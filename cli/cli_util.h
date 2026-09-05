/***************************************************/
/* Part of GDSiON software synthesizer             */
/* Copyright (c) 2024 Yuri Sizov and contributors  */
/* Provided under MIT                              */
/***************************************************/

#ifndef LIBSION_CLI_UTIL_H
#define LIBSION_CLI_UTIL_H

// Console and path helpers so the tool behaves correctly with UTF-8 text
// (usage output, non-ASCII file names) on every platform.

#include <cstdio>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

inline std::wstring cli_utf8_to_wide(const std::string &p_utf8) {
	if (p_utf8.empty()) {
		return std::wstring();
	}
	int length = MultiByteToWideChar(CP_UTF8, 0, p_utf8.c_str(), (int)p_utf8.size(), nullptr, 0);
	std::wstring wide(length, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, p_utf8.c_str(), (int)p_utf8.size(), &wide[0], length);
	return wide;
}

inline std::string cli_wide_to_utf8(const wchar_t *p_wide) {
	int length = WideCharToMultiByte(CP_UTF8, 0, p_wide, -1, nullptr, 0, nullptr, nullptr);
	if (length <= 0) {
		return std::string();
	}
	std::string utf8(length - 1, '\0');
	WideCharToMultiByte(CP_UTF8, 0, p_wide, -1, &utf8[0], length, nullptr, nullptr);
	return utf8;
}

// The ANSI CRT entry point would have already mangled non-ASCII arguments
// through the legacy code page, so rebuild them from the wide command line.
inline std::vector<std::string> cli_build_arguments(int p_argc, char **p_argv) {
	int count = 0;
	LPWSTR *wide = CommandLineToArgvW(GetCommandLineW(), &count);
	if (wide == nullptr) {
		return std::vector<std::string>(p_argv, p_argv + p_argc);
	}
	std::vector<std::string> args;
	args.reserve(count);
	for (int i = 0; i < count; i++) {
		args.push_back(cli_wide_to_utf8(wide[i]));
	}
	LocalFree(wide);
	return args;
}

inline FILE *cli_fopen(const std::string &p_path, const char *p_mode) {
	return _wfopen(cli_utf8_to_wide(p_path).c_str(), cli_utf8_to_wide(p_mode).c_str());
}

#else // !_WIN32

inline std::vector<std::string> cli_build_arguments(int p_argc, char **p_argv) {
	return std::vector<std::string>(p_argv, p_argv + p_argc);
}

inline FILE *cli_fopen(const std::string &p_path, const char *p_mode) {
	return fopen(p_path.c_str(), p_mode);
}

#endif // _WIN32

#endif // LIBSION_CLI_UTIL_H
