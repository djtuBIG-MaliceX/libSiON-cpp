/*
 * Part of libSiON-cpp, forked from GDSiON software synthesizer
 * Copyright (c) 2024-2026 Yuri Sizov, James Alan Nguyen and contributors
 * Based on SiON Flash Software Synthesizer (C) 2008-2016 keim_at_Si
 * Provided under MIT License.
 */

#ifndef SION_COMPAT_ERRORS_H
#define SION_COMPAT_ERRORS_H

#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>

// Error reporting that mirrors the exact message text Godot printed while
// this code was a GDExtension. The mml-compilation golden outputs consist of
// the `ERROR: ` first lines, so the formatted shape below must be kept in
// sync with Godot 4.x `err_print_error`:
//
//   ERROR: <message>
//   <error>                       (only when both message and error exist)
//     at: <function> (<file>:<line>)
//
// The golden files only ever capture the `ERROR: ` line, which carries the
// custom message when one exists, otherwise the error string.

namespace sion {

// A fully formatted, multi-line error string (as Godot would print it).
typedef std::function<void(const std::string &p_output)> ErrorOutput;

inline ErrorOutput &error_output() {
	static ErrorOutput output = [](const std::string &p_output) {
		fputs(p_output.c_str(), stderr);
		fflush(stderr);
	};
	return output;
}

inline std::string &warnings_capture() {
	static std::string capture;
	return capture;
}

inline const char *rel_location(const char *p_file) {
	const char *base = p_file;
	for (const char *c = p_file; *c; c++) {
		if ((c[0] == '/' || c[0] == '\\') && c[1] == 's' && c[2] == 'r' && c[3] == 'c' && (c[4] == '/' || c[4] == '\\')) {
			base = c + 1;
			break;
		}
	}
	return base;
}

// Mirrors Godot's `err_print_error` + `print_error_with_message` formatting.
inline void err_print(const char *p_func, const char *p_file, int p_line, const std::string &p_error, const std::string &p_message, bool p_warning = false) {
	std::string body = p_message;
	if (!body.empty()) {
		body += "\n";
	}
	body += p_error;

	std::string out;
	if (p_warning) {
		out = "WARNING: ";
	} else {
		out = "ERROR: ";
	}
	out += body;
	out += "\n  at: ";
	out += p_func;
	out += " (";
	out += rel_location(p_file);
	out += ":";
	out += std::to_string(p_line);
	out += ")\n";

	error_output()(out);
}

// --- vformat supporting the subset used by the codebase (%s, %d, %%).

inline void vformat_expand(std::string &r_out, const char *p_fmt) {
	while (*p_fmt) {
		if (p_fmt[0] == '%' && p_fmt[1] == '%') {
			r_out += '%';
			p_fmt += 2;
		} else {
			r_out += *p_fmt++;
		}
	}
}

// Enum arguments: Godot passes bound enums through Variant as integers, so
// both specs render the numeric value.
template <class A>
inline std::enable_if_t<std::is_enum_v<std::decay_t<A>>> vformat_write(std::string &r_out, char p_spec, const A &p_arg) {
	(void)p_spec;
	char buf[64];
	snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(static_cast<typename std::underlying_type<std::decay_t<A>>::type>(p_arg)));
	r_out += buf;
}

// Object-like/string arguments.
template <class A>
inline std::enable_if_t<!std::is_arithmetic_v<std::decay_t<A>> && !std::is_enum_v<std::decay_t<A>> && !std::is_pointer_v<std::decay_t<A>>> vformat_write(std::string &r_out, char p_spec, const A &p_arg) {
	if (p_spec == 's') {
		r_out += static_cast<std::string>(p_arg);
	} else {
		char buf[64];
		snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(static_cast<int64_t>(0))); // %d on a string is a bug; keep compiling.
		r_out += buf;
	}
}

// Arithmetic arguments: Godot's vformat is untyped and stringifies whatever
// spec pairs with whatever value. Mirror that instead of failing to compile.
template <class A>
inline std::enable_if_t<std::is_arithmetic_v<std::decay_t<A>> && !std::is_same_v<std::decay_t<A>, bool>> vformat_write(std::string &r_out, char p_spec, A p_arg) {
	char buf[64];
	if constexpr (std::is_floating_point_v<A>) {
		snprintf(buf, sizeof(buf), "%.14g", static_cast<double>(p_arg));
	} else {
		snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(p_arg));
	}
	r_out += buf;
}

inline void vformat_write(std::string &r_out, char p_spec, const std::string &p_arg) {
	if (p_spec == 's') {
		r_out += p_arg;
	} else {
		r_out += "0"; // %d on a string is a bug; keep compiling.
	}
}

template <size_t N>
inline void vformat_write(std::string &r_out, char p_spec, const char (&p_arg)[N]) {
	vformat_write(r_out, p_spec, static_cast<const char *>(p_arg));
}

inline void vformat_write(std::string &r_out, char p_spec, const char *p_arg) {
	if (p_spec == 's') {
		r_out += p_arg ? p_arg : "";
	} else {
		r_out += "0";
	}
}

inline void vformat_write(std::string &r_out, char p_spec, bool p_arg) {
	// Godot renders booleans through "-1"/"0" in both specs.
	r_out += p_arg ? "-1" : "0";
}

template <class A, class... Rest>
inline void vformat_expand(std::string &r_out, const char *p_fmt, const A &p_arg, const Rest &...p_rest) {
	while (*p_fmt) {
		if (p_fmt[0] == '%' && p_fmt[1] == '%') {
			r_out += '%';
			p_fmt += 2;
			continue;
		}
		if (p_fmt[0] == '%') {
			const char spec = p_fmt[1];
			if (spec >= '0' && spec <= '9') {
				r_out += *p_fmt++;
				continue; // Width digit; consume and look again.
			}
			vformat_write(r_out, spec, p_arg);
			vformat_expand(r_out, p_fmt + 2, p_rest...);
			return;
		}
		r_out += *p_fmt++;
	}
	// More arguments than placeholders: drop the extras like Godot does not,
	// but nothing in the codebase depends on leftovers.
}

template <class... Args>
inline std::string vformat(const std::string &p_fmt, const Args &...p_args) {
	std::string out;
	vformat_expand(out, p_fmt.c_str(), p_args...);
	return out;
}

} // namespace sion

// Godot exposes vformat as a global; code calls it unqualified.
using ::sion::vformat;

// --- Godot-compatible error macros. Keep the literal strings identical to
// godot-cpp error_macros.hpp (golden test parity).

#define ERR_PRINT(m_message)                                                                                                                    \
	do {                                                                                                                                        \
		::sion::err_print(__func__, __FILE__, __LINE__, static_cast<std::string>(m_message), std::string());                                    \
	} while (0)

#define WARN_PRINT(m_message)                                                                                                                   \
	do {                                                                                                                                        \
		::sion::err_print(__func__, __FILE__, __LINE__, std::string(), static_cast<std::string>(m_message), true);                              \
	} while (0)

#define ERR_FAIL_COND(m_condition)                                                                                                              \
	do {                                                                                                                                        \
		if (m_condition) {                                                                                                                      \
			::sion::err_print(__func__, __FILE__, __LINE__, "Condition \"" #m_condition "\" is true.", std::string());                          \
			return;                                                                                                                             \
		}                                                                                                                                       \
	} while (0)

#define ERR_FAIL_COND_MSG(m_condition, m_message)                                                                                               \
	do {                                                                                                                                        \
		if (m_condition) {                                                                                                                      \
			::sion::err_print(__func__, __FILE__, __LINE__, "Condition \"" #m_condition "\" is true.", static_cast<std::string>(m_message));    \
			return;                                                                                                                             \
		}                                                                                                                                       \
	} while (0)

#define ERR_CONTINUE_MSG(m_condition, m_message)                                                                                                \
	do {                                                                                                                                        \
		if (m_condition) {                                                                                                                      \
			::sion::err_print(__func__, __FILE__, __LINE__, "Condition \"" #m_condition "\" is true. Continued.", static_cast<std::string>(m_message)); \
			continue;                                                                                                                           \
		}                                                                                                                                       \
	} while (0)

#define ERR_FAIL_COND_V(m_condition, m_retval)                                                                                                  \
	do {                                                                                                                                        \
		if (m_condition) {                                                                                                                      \
			::sion::err_print(__func__, __FILE__, __LINE__, "Condition \"" #m_condition "\" is true. Returning: " #m_retval, std::string());    \
			return m_retval;                                                                                                                    \
		}                                                                                                                                       \
	} while (0)

#define ERR_FAIL_COND_V_MSG(m_condition, m_retval, m_message)                                                                                   \
	do {                                                                                                                                        \
		if (m_condition) {                                                                                                                      \
			::sion::err_print(__func__, __FILE__, __LINE__, "Condition \"" #m_condition "\" is true. Returning: " #m_retval, static_cast<std::string>(m_message)); \
			return m_retval;                                                                                                                    \
		}                                                                                                                                       \
	} while (0)

#define ERR_FAIL_INDEX(m_index, m_size)                                                                                                         \
	do {                                                                                                                                        \
		if ((m_index) < 0 || (m_index) >= (m_size)) {                                                                                           \
			::sion::err_print(__func__, __FILE__, __LINE__, ::sion::vformat("Index " #m_index " = %d is out of bounds (" #m_size " = %d).", static_cast<int>(m_index), static_cast<int>(m_size)), std::string()); \
			return;                                                                                                                             \
		}                                                                                                                                       \
	} while (0)

#define ERR_FAIL_INDEX_V(m_index, m_size, m_retval)                                                                                             \
	do {                                                                                                                                        \
		if ((m_index) < 0 || (m_index) >= (m_size)) {                                                                                           \
			::sion::err_print(__func__, __FILE__, __LINE__, ::sion::vformat("Index " #m_index " = %d is out of bounds (" #m_size " = %d).", static_cast<int>(m_index), static_cast<int>(m_size)), std::string()); \
			return m_retval;                                                                                                                    \
		}                                                                                                                                       \
	} while (0)

#define ERR_FAIL_INDEX_MSG(m_index, m_size, m_message)                                                                                          \
	do {                                                                                                                                        \
		if ((m_index) < 0 || (m_index) >= (m_size)) {                                                                                           \
			::sion::err_print(__func__, __FILE__, __LINE__, ::sion::vformat("Index " #m_index " = %d is out of bounds (" #m_size " = %d).", static_cast<int>(m_index), static_cast<int>(m_size)), static_cast<std::string>(m_message)); \
			return;                                                                                                                             \
		}                                                                                                                                       \
	} while (0)

#define ERR_FAIL_INDEX_V_MSG(m_index, m_size, m_retval, m_message)                                                                              \
	do {                                                                                                                                        \
		if ((m_index) < 0 || (m_index) >= (m_size)) {                                                                                           \
			::sion::err_print(__func__, __FILE__, __LINE__, ::sion::vformat("Index " #m_index " = %d is out of bounds (" #m_size " = %d).", static_cast<int>(m_index), static_cast<int>(m_size)), static_cast<std::string>(m_message)); \
			return m_retval;                                                                                                                    \
		}                                                                                                                                       \
	} while (0)

#define ERR_FAIL_NULL(m_param)                                                                                                                  \
	do {                                                                                                                                        \
		if ((m_param) == nullptr) {                                                                                                             \
			::sion::err_print(__func__, __FILE__, __LINE__, "Parameter \"" #m_param "\" is null.", std::string());                              \
			return;                                                                                                                             \
		}                                                                                                                                       \
	} while (0)

#define ERR_FAIL_NULL_MSG(m_param, m_message)                                                                                                   \
	do {                                                                                                                                        \
		if ((m_param) == nullptr) {                                                                                                             \
			::sion::err_print(__func__, __FILE__, __LINE__, "Parameter \"" #m_param "\" is null.", static_cast<std::string>(m_message));        \
			return;                                                                                                                             \
		}                                                                                                                                       \
	} while (0)

#define ERR_FAIL_NULL_V(m_param, m_retval)                                                                                                      \
	do {                                                                                                                                        \
		if ((m_param) == nullptr) {                                                                                                             \
			::sion::err_print(__func__, __FILE__, __LINE__, "Parameter \"" #m_param "\" is null. Returning: " #m_retval, std::string());        \
			return m_retval;                                                                                                                    \
		}                                                                                                                                       \
	} while (0)

#define ERR_FAIL_NULL_V_MSG(m_param, m_retval, m_message)                                                                                       \
	do {                                                                                                                                        \
		if ((m_param) == nullptr) {                                                                                                             \
			::sion::err_print(__func__, __FILE__, __LINE__, "Parameter \"" #m_param "\" is null. Returning: " #m_retval, static_cast<std::string>(m_message)); \
			return m_retval;                                                                                                                    \
		}                                                                                                                                       \
	} while (0)

#define ERR_FAIL()                                                                                                                              \
	do {                                                                                                                                        \
		::sion::err_print(__func__, __FILE__, __LINE__, "Method/function failed.", std::string());                                              \
		return;                                                                                                                                 \
	} while (0)

#define ERR_FAIL_MSG(m_message)                                                                                                                 \
	do {                                                                                                                                        \
		::sion::err_print(__func__, __FILE__, __LINE__, "Method/function failed.", static_cast<std::string>(m_message));                        \
		return;                                                                                                                                 \
	} while (0)

#define ERR_FAIL_V(m_retval)                                                                                                                    \
	do {                                                                                                                                        \
		::sion::err_print(__func__, __FILE__, __LINE__, "Method/function failed. Returning: " #m_retval, std::string());                        \
		return m_retval;                                                                                                                        \
	} while (0)

#define ERR_FAIL_V_MSG(m_retval, m_message)                                                                                                     \
	do {                                                                                                                                        \
		::sion::err_print(__func__, __FILE__, __LINE__, "Method/function failed. Returning: " #m_retval, static_cast<std::string>(m_message));  \
		return m_retval;                                                                                                                        \
	} while (0)

#endif // SION_COMPAT_ERRORS_H
