/***************************************************/
/* Part of GDSiON software synthesizer             */
/* Copyright (c) 2024 Yuri Sizov and contributors  */
/* Provided under MIT                              */
/***************************************************/

#ifndef SION_COMPAT_CONTAINERS_H
#define SION_COMPAT_CONTAINERS_H

#include <algorithm>
#include <cstring>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <vector>

// Reference counting pointer, compatible with std::shared_ptr but with the
// Godot-style validity checks that the codebase relies on.
template <class T>
class Ref : public std::shared_ptr<T> {
	using Base = std::shared_ptr<T>;

public:
	using Base::Base;
	using Base::operator=;

	// Godot's Ref allowed checked downcasts between related types
	// (e.g. Ref<AudioStream> -> Ref<AudioStreamWAV>), yielding null on
	// mismatch. Mirror that through dynamic_pointer_cast.
	template <class U, class = std::enable_if_t<!std::is_convertible_v<U *, T *>>>
	Ref(const Ref<U> &p_other) :
			Base(std::dynamic_pointer_cast<T>(p_other)) {}

	template <class U, class = std::enable_if_t<!std::is_convertible_v<U *, T *>>>
	Ref &operator=(const Ref<U> &p_other) {
		Base::operator=(std::dynamic_pointer_cast<T>(p_other));
		return *this;
	}

	void instantiate() { Base::reset(new T()); }

	// Non-owning alias view. Godot's intrusive refcount made `fn(this)` with a
	// const Ref& parameter safe (temporary + INCREF); a plain shared_ptr wrap
	// would double-own, so use this for synchronous calls where the true owner
	// outlives the call.
	static Ref borrow(T *p_ptr) {
		Ref r;
		r.reset(p_ptr, [](T *) {});
		return r;
	}

	// Godot's Ref accepted raw pointers and related-type downcasts directly;
	// mirror both so `x = new T()` and base->derived Ref conversions keep
	// compiling exactly as they did against godot-cpp.
	Ref(T *p_ptr) :
			Base(p_ptr) {}

	template <class U, class = std::enable_if_t<std::is_convertible_v<U *, T *> && !std::is_same_v<U, T>>>
	Ref(U *p_ptr) :
			Base(p_ptr) {}

	Ref &operator=(T *p_ptr) {
		Base::reset(p_ptr);
		return *this;
	}

	template <class U, class = std::enable_if_t<std::is_convertible_v<U *, T *> && !std::is_same_v<U, T>>>
	Ref &operator=(U *p_ptr) {
		Base::reset(p_ptr);
		return *this;
	}

	bool is_valid() const { return Base::get() != nullptr; }
	bool is_null() const { return Base::get() == nullptr; }
};

// Key/value pair view used when iterating a HashMap, mirroring the
// iteration idiom of the original Godot-based code.
template <class K, class V>
struct KeyValue {
	const K &key;
	V &value;
};

template <class K, class V>
class HashMap {
	std::unordered_map<K, V> _map;

public:
	struct iterator {
		typename std::unordered_map<K, V>::iterator _it;

		KeyValue<K, V> operator*() const { return { _it->first, _it->second }; }
		bool operator==(const iterator &p_other) const { return _it == p_other._it; }
		bool operator!=(const iterator &p_other) const { return _it != p_other._it; }
		iterator &operator++() { ++_it; return *this; }
		iterator operator++(int) { iterator c = *this; ++_it; return c; }
	};

	struct const_iterator {
		typename std::unordered_map<K, V>::const_iterator _it;

		KeyValue<K, const V> operator*() const { return { _it->first, _it->second }; }
		bool operator==(const const_iterator &p_other) const { return _it == p_other._it; }
		bool operator!=(const const_iterator &p_other) const { return _it != p_other._it; }
		const_iterator &operator++() { ++_it; return *this; }
		const_iterator operator++(int) { const_iterator c = *this; ++_it; return c; }
	};

	V &operator[](const K &p_key) { return _map[p_key]; }
	const V &operator[](const K &p_key) const {
		auto it = _map.find(p_key);
		static const V missing = V();
		return (it != _map.end()) ? it->second : missing;
	}

	void insert(const K &p_key, const V &p_value, bool p_overwrite = true) {
		if (p_overwrite) {
			_map[p_key] = p_value;
		} else {
			_map.emplace(p_key, p_value);
		}
	}

	bool has(const K &p_key) const { return _map.find(p_key) != _map.end(); }

	V get(const K &p_key, const V &p_default = V()) const {
		auto it = _map.find(p_key);
		return (it != _map.end()) ? it->second : p_default;
	}

	void erase(const K &p_key) { _map.erase(p_key); }
	void erase(iterator p_it) { _map.erase(p_it._it); }

	iterator find(const K &p_key) { return iterator { _map.find(p_key) }; }

	int size() const { return static_cast<int>(_map.size()); }
	bool is_empty() const { return _map.empty(); }
	bool empty() const { return _map.empty(); }
	void clear() { _map.clear(); }

	iterator begin() { return iterator { _map.begin() }; }
	iterator end() { return iterator { _map.end() }; }
	// Godot-era code iterates const maps binding `const KeyValue<K, V> &`,
	// which the original (intrusive, non-owning) pair view allowed. Mirror that
	// permissiveness; call sites only read through these iterators.
	iterator begin() const { return iterator { const_cast<std::unordered_map<K, V> &>(_map).begin() }; }
	iterator end() const { return iterator { const_cast<std::unordered_map<K, V> &>(_map).end() }; }
};

// Doubly-linked list with Godot-style Element pointers, preserving the
// front()/next()/erase(Element*) idioms used across the codebase.
template <class T>
class List {
public:
	class Element {
		friend class List<T>;

		Element *_prev = nullptr;
		Element *_next = nullptr;
		T _value;

	public:
		Element *next() const { return _next; }
		Element *prev() const { return _prev; }
		Element *self() { return this; }

		const T *operator->() const { return &_value; }
		T *operator->() { return &_value; }
		const T &value() const { return _value; }
		T &value() { return _value; }

		// Godot's List::Element did not define get() — pointer values in this
		// codebase call it as a value accessor, so expose the stored value.
		const T &get() const { return _value; }
		T &get() { return _value; }
		void set(const T &p_value) { _value = p_value; }
	};

private:
	Element *_first = nullptr;
	Element *_last = nullptr;
	int _size = 0;

public:
	Element *front() const { return _first; }
	Element *back() const { return _last; }

	int size() const { return _size; }
	bool is_empty() const { return _size == 0; }
	bool empty() const { return _size == 0; }

	Element *push_back(const T &p_value) {
		Element *elem = new Element();
		elem->_value = p_value;

		if (_last) {
			elem->_prev = _last;
			_last->_next = elem;
			_last = elem;
		} else {
			_first = elem;
			_last = elem;
		}
		_size++;
		return elem;
	}

	Element *push_front(const T &p_value) {
		Element *elem = new Element();
		elem->_value = p_value;

		if (_first) {
			elem->_next = _first;
			_first->_prev = elem;
			_first = elem;
		} else {
			_first = elem;
			_last = elem;
		}
		_size++;
		return elem;
	}

	void append(const T &p_value) { push_back(p_value); }
	void append_front(const T &p_value) { push_front(p_value); }

	void pop_front() {
		if (!_first) {
			return;
		}

		Element *elem = _first;
		_first = elem->_next;

		if (_first) {
			_first->_prev = nullptr;
		} else {
			_last = nullptr;
		}
		delete elem;
		_size--;
	}

	void pop_back() {
		if (!_last) {
			return;
		}

		Element *elem = _last;
		_last = elem->_prev;

		if (_last) {
			_last->_next = nullptr;
		} else {
			_first = nullptr;
		}
		delete elem;
		_size--;
	}

	Element *insert_before(Element *p_element, const T &p_value) {
		if (!p_element) {
			push_back(p_value);
			return _last;
		}

		Element *elem = new Element();
		elem->_value = p_value;
		elem->_next = p_element;
		elem->_prev = p_element->_prev;

		if (p_element->_prev) {
			p_element->_prev->_next = elem;
		} else {
			_first = elem;
		}
		p_element->_prev = elem;
		_size++;

		return elem;
	}

	Element *insert_after(Element *p_element, const T &p_value) {
		if (!p_element) {
			push_front(p_value);
			return _first;
		}

		Element *elem = new Element();
		elem->_value = p_value;
		elem->_prev = p_element;
		elem->_next = p_element->_next;

		if (p_element->_next) {
			p_element->_next->_prev = elem;
		} else {
			_last = elem;
		}
		p_element->_next = elem;
		_size++;

		return elem;
	}

	void erase(Element *p_element) {
		if (!p_element) {
			return;
		}

		if (p_element->_prev) {
			p_element->_prev->_next = p_element->_next;
		} else {
			_first = p_element->_next;
		}
		if (p_element->_next) {
			p_element->_next->_prev = p_element->_prev;
		} else {
			_last = p_element->_prev;
		}
		delete p_element;
		_size--;
	}

	bool erase(const T &p_value) {
		Element *elem = _first;
		while (elem) {
			if (elem->_value == p_value) {
				erase(elem);
				return true;
			}
			elem = elem->_next;
		}
		return false;
	}

	void sort() {
		std::vector<Element *> elems;
		for (Element *elem = _first; elem; elem = elem->_next) {
			elems.push_back(elem);
		}

		std::stable_sort(elems.begin(), elems.end(), [](const Element *a, const Element *b) {
			return a->value() < b->value();
		});

		relink(elems);
	}

	// Godot's List::sort_custom, stable like the engine's merge sort.
	template <class C>
	void sort_custom() {
		C comparator;

		std::vector<Element *> elems;
		for (Element *elem = _first; elem; elem = elem->_next) {
			elems.push_back(elem);
		}

		std::stable_sort(elems.begin(), elems.end(), [&comparator](const Element *a, const Element *b) {
			return comparator(a->value(), b->value());
		});

		relink(elems);
	}

	void relink(const std::vector<Element *> &elems) {
		Element *prev = nullptr;
		for (Element *elem : elems) {
			elem->_prev = prev;
			if (prev) {
				prev->_next = elem;
			} else {
				_first = elem;
			}
			prev = elem;
		}
		if (prev) {
			prev->_next = nullptr;
			_last = prev;
		}
	}

	Element *find(const T &p_value) const {
		Element *elem = _first;
		while (elem) {
			if (elem->_value == p_value) {
				return elem;
			}
			elem = elem->_next;
		}
		return nullptr;
	}

	Element *at_index(int p_index) const {
		Element *elem = _first;
		for (int i = 0; i < p_index && elem; i++) {
			elem = elem->_next;
		}
		return elem;
	}

	// Random access, matching the godot-cpp List pinned by GDSiON. Use with
	// care: O(n), never for iteration.
	T &operator[](int p_index) { return at_index(p_index)->_value; }
	const T &operator[](int p_index) const { return at_index(p_index)->_value; }
	T &get(int p_index) { return operator[](p_index); }
	const T &get(int p_index) const { return operator[](p_index); }

	void reverse() {
		Element *f = _first;
		Element *b = _last;
		int s = _size / 2;
		for (int i = 0; i < s; i++) {
			std::swap(f->_value, b->_value);
			f = f->_next;
			b = b->_prev;
		}
	}

	void clear() {
		Element *elem = _first;
		while (elem) {
			Element *next = elem->_next;
			delete elem;
			elem = next;
		}
		_first = nullptr;
		_last = nullptr;
		_size = 0;
	}

	// --- Range-based iteration support ---

	struct iterator {
		Element *_e = nullptr;
		T &operator*() const { return _e->value(); }
		T *operator->() const { return &_e->value(); }
		bool operator==(const iterator &p_o) const { return _e == p_o._e; }
		bool operator!=(const iterator &p_o) const { return _e != p_o._e; }
		iterator &operator++() { _e = _e->next(); return *this; }
		iterator operator++(int) { iterator c = *this; _e = _e ? _e->next() : nullptr; return c; }
	};

	struct const_iterator {
		const Element *_e = nullptr;
		const T &operator*() const { return _e->value(); }
		const T *operator->() const { return &_e->value(); }
		bool operator==(const const_iterator &p_o) const { return _e == p_o._e; }
		bool operator!=(const const_iterator &p_o) const { return _e != p_o._e; }
		const_iterator &operator++() { _e = _e->next(); return *this; }
		const_iterator operator++(int) { const_iterator c = *this; _e = _e ? _e->next() : nullptr; return c; }
	};

	iterator begin() { return iterator { _first }; }
	iterator end() { return iterator { nullptr }; }
	const_iterator begin() const { return const_iterator { _first }; }
	const_iterator end() const { return const_iterator { nullptr }; }

	List() = default;

	List(const List &p_other) {
		for (const T &v : p_other) {
			push_back(v);
		}
	}

	List &operator=(const List &p_other) {
		if (this != &p_other) {
			clear();
			for (const T &v : p_other) {
				push_back(v);
			}
		}
		return *this;
	}

	~List() { clear(); }
};

#endif // SION_COMPAT_CONTAINERS_H
