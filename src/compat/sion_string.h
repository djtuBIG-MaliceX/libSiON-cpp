/***************************************************/
/* Part of GDSiON software synthesizer             */
/* Copyright (c) 2024 Yuri Sizov and contributors  */
/* Provided under MIT                              */
/***************************************************/

#ifndef SION_COMPAT_STRING_H
#define SION_COMPAT_STRING_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

// A std::string-based stand-in for Godot's String class, providing the small
// set of Godot-flavored helpers the codebase relies on. Backed by bytes
// instead of UTF-32 codepoints; MML payloads are ASCII so offsets behave the
// same as the original UTF-32 implementation for all real inputs.

namespace sion {

class String : public std::string {
	using Base = std::string;

	static bool _is_hex_prefixed(const char *p_str) {
		while (*p_str == ' ' || *p_str == '\t') {
			p_str++;
		}
		if (p_str[0] == '+' || p_str[0] == '-') {
			p_str++;
		}
		return p_str[0] == '0' && (p_str[1] == 'x' || p_str[1] == 'X');
	}

public:
	using Base::Base;

	String(const Base &p_str) :
			Base(p_str) {}
	String(Base &&p_str) :
			Base(std::move(p_str)) {}

	// --- Introspection.

	int length() const { return static_cast<int>(size()); }
	bool is_empty() const { return empty(); }

	// Returns the byte at p_pos as a codepoint (inputs are ASCII).
	int unicode_at(size_t p_pos) const {
		return (p_pos < size()) ? static_cast<int>(static_cast<unsigned char>(at(p_pos))) : 0;
	}

	bool begins_with(const String &p_str) const {
		return size() >= p_str.size() && compare(0, p_str.size(), p_str) == 0;
	}

	bool ends_with(const String &p_str) const {
		return size() >= p_str.size() && compare(size() - p_str.size(), p_str.size(), p_str) == 0;
	}

	bool contains(const String &p_str) const {
		return Base::find(p_str) != Base::npos;
	}

	// --- Conversions. Godot semantics: parse a leading number, ignore the
	// rest; hex literals with a 0x prefix are recognized by to_int.

	int64_t to_int() const {
		return strtoll(c_str(), nullptr, _is_hex_prefixed(c_str()) ? 16 : 10);
	}

	double to_float() const {
		return strtod(c_str(), nullptr);
	}

	int64_t hex_to_int() const {
		return strtoll(c_str(), nullptr, 16);
	}

	bool is_valid_int() const {
		const char *c = c_str();
		if (*c == '+' || *c == '-') {
			c++;
		}
		bool any = false;
		while (*c) {
			if (*c < '0' || *c > '9') {
				return false;
			}
			any = true;
			c++;
		}
		return any;
	}

	bool is_valid_float() const {
		const char *c = c_str();
		if (*c == '+' || *c == '-') {
			c++;
		}
		bool digits_before = false, digits_after = false;
		while (*c >= '0' && *c <= '9') {
			digits_before = true;
			c++;
		}
		if (*c == '.') {
			c++;
			while (*c >= '0' && *c <= '9') {
				digits_after = true;
				c++;
			}
		}
		if (!digits_before && !digits_after) {
			return false;
		}
		if (*c == 'e' || *c == 'E') {
			c++;
			if (*c == '+' || *c == '-') {
				c++;
			}
			bool digits_exp = false;
			while (*c >= '0' && *c <= '9') {
				digits_exp = true;
				c++;
			}
			if (!digits_exp) {
				return false;
			}
		}
		return *c == '\0';
	}

	// --- Searching. Godot semantics: -1 when absent (hides std::string::find).

	int64_t find(const String &p_str, size_t p_from = 0) const {
		size_t r = Base::find(static_cast<const Base &>(p_str), p_from);
		return (r == Base::npos) ? -1 : static_cast<int64_t>(r);
	}

	int64_t find(char p_char, size_t p_from = 0) const {
		size_t r = Base::find(p_char, p_from);
		return (r == Base::npos) ? -1 : static_cast<int64_t>(r);
	}

	// --- Slicing. Godot semantics: never throws, clamps instead.

	String substr(size_t p_from = 0, size_t p_len = Base::npos) const {
		if (p_from > size()) {
			return String();
		}
		return String(Base::substr(p_from, p_len));
	}

	String left(size_t p_count) const {
		return substr(0, p_count);
	}

	String right(size_t p_count) const {
		return (p_count >= size()) ? *this : String(substr(size() - p_count));
	}

	// --- Mutation helpers (Godot style). insert/replace are non-mutating and
	// return the new string; erase mutates in place, deleting one char.

	String insert(size_t p_pos, const String &p_str) const {
		String result(*this);
		Base &base = result;
		base.insert(p_pos, static_cast<const Base &>(p_str));
		return result;
	}

	void erase(size_t p_pos = 0, size_t p_len = 1) {
		Base::erase(p_pos, p_len);
	}

	String replace(const String &p_what, const String &p_forwhat) const {
		if (p_what.empty()) {
			return *this;
		}
		String result;
		size_t pos = 0;
		while (true) {
			size_t found = Base::find(static_cast<const Base &>(p_what), pos);
			if (found == Base::npos) {
				result += Base::substr(pos);
				break;
			}
			result += Base::substr(pos, found - pos);
			result += p_forwhat;
			pos = found + p_what.size();
		}
		return result;
	}

	String repeat(uint64_t p_count) const {
		String result;
		for (uint64_t i = 0; i < p_count; i++) {
			result += *this;
		}
		return result;
	}

	// --- Case conversion (ASCII-only, sufficient for MML identifiers).

	String to_lower() const {
		String result(*this);
		for (char &c : result) {
			if (c >= 'A' && c <= 'Z') {
				c += ('a' - 'A');
			}
		}
		return result;
	}

	String to_upper() const {
		String result(*this);
		for (char &c : result) {
			if (c >= 'a' && c <= 'z') {
				c -= ('a' - 'A');
			}
		}
		return result;
	}

	// --- Padding. Grows the string to p_digits characters with leading
	// zeros, keeping a leading sign in place. Godot parity.

	String pad_zeros(int p_digits) const {
		if (length() >= p_digits) {
			return *this;
		}

		String result;
		size_t offset = 0;
		if (!empty() && (at(0) == '-' || at(0) == '+')) {
			result += at(0);
			offset = 1;
		}
		result += String(static_cast<size_t>(p_digits - length() + offset), '0');
		result += Base::substr(offset);
		return result;
	}

	// --- Splitting and joining. Godot semantics.

	std::vector<String> split(const String &p_delim, bool p_allow_empty = true, size_t p_maxsplit = 0) const {
		std::vector<String> result;
		if (empty()) {
			return result;
		}
		if (p_delim.empty()) {
			result.push_back(*this);
			return result;
		}

		size_t splits = 0;
		size_t pos = 0;
		while (true) {
			size_t found = Base::find(static_cast<const Base &>(p_delim), pos);
			if (found == Base::npos || (p_maxsplit > 0 && splits >= p_maxsplit)) {
				String piece = Base::substr(pos);
				if (p_allow_empty || !piece.empty()) {
					result.push_back(piece);
				}
				break;
			}
			String piece = Base::substr(pos, found - pos);
			if (p_allow_empty || !piece.empty()) {
				result.push_back(piece);
			}
			splits++;
			pos = found + p_delim.size();
		}
		return result;
	}

	String get_slice(const String &p_delim, int64_t p_slice) const {
		if (p_delim.empty() || p_slice < 0) {
			return String();
		}

		int64_t index = 0;
		size_t pos = 0;
		while (true) {
			size_t found = Base::find(static_cast<const Base &>(p_delim), pos);
			if (found == Base::npos) {
				return (index == p_slice) ? String(Base::substr(pos)) : String();
			}
			if (index == p_slice) {
				return String(Base::substr(pos, found - pos));
			}
			index++;
			pos = found + p_delim.size();
		}
	}

	String join(const std::vector<String> &p_parts) const {
		String result;
		for (size_t i = 0; i < p_parts.size(); i++) {
			if (i > 0) {
				result += *this;
			}
			result += p_parts[i];
		}
		return result;
	}

	String strip_edges(bool p_left = true, bool p_right = true) const {
		size_t begin = 0;
		size_t end = size();
		if (p_left) {
			while (begin < end && (at(begin) == ' ' || at(begin) == '\t' || at(begin) == '\n' || at(begin) == '\r')) {
				begin++;
			}
		}
		if (p_right) {
			while (end > begin && (at(end - 1) == ' ' || at(end - 1) == '\t' || at(end - 1) == '\n' || at(end - 1) == '\r')) {
				end--;
			}
		}
		return String(Base::substr(begin, end - begin));
	}
};

} // namespace sion

// --- Free helpers matching Godot's global itos/rtos.

inline sion::String itos(int64_t p_value) {
	return sion::String(std::to_string(p_value));
}

inline sion::String rtos(double p_value) {
	char buffer[64];
	snprintf(buffer, sizeof(buffer), "%.5f", p_value);
	return sion::String(buffer);
}

namespace std {

template <>
struct hash<sion::String> {
	size_t operator()(const sion::String &p_str) const {
		return hash<string>()(static_cast<const string &>(p_str));
	}
};

} // namespace std

#endif // SION_COMPAT_STRING_H
