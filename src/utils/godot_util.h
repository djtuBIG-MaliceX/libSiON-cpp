/*
 * Part of libSiON-cpp, forked from GDSiON software synthesizer
 * Copyright (c) 2024-2026 Yuri Sizov, James Alan Nguyen and contributors
 * Based on SiON Flash Software Synthesizer (C) 2008-2016 keim_at_Si
 * Provided under MIT License.
 */

#ifndef SION_GODOT_UTIL_H
#define SION_GODOT_UTIL_H

//#include <godot_cpp/templates/vector.hpp>
//#include <godot_cpp/variant/string.hpp>
//#include <godot_cpp/variant/typed_array.hpp>



std::vector<sion::String> split_string_by_regex(const sion::String &p_string, const sion::String &p_regex);

template <class T, size_t S>
std::vector<T> make_vector(T (&p_array)[S]) {
	std::vector<T> vector;
	vector.resize(S); // TODO zeroed

	for (int i = 0; i < S; i++) {
		vector[i] = p_array[i];
	}

	return vector;
}

// Ideally std::vectors should be natively convertable with std::vectors, but it's not implemented.

template <class T>
std::vector<T> make_vector_from_typed_array(const std::vector<T> &p_array) {
	std::vector<T> data;

	for (int i = 0; i < p_array.size(); i++) {
		data.push_back(p_array[i]);
	}

	return data;
}

template <class T>
std::vector<T> make_typed_array_from_vector(const std::vector<T> &p_data) {
	std::vector<T> array;

	for (const T &item : p_data) {
		array.push_back(item);
	}

	return array;
}

#endif // SION_GODOT_UTIL_H
