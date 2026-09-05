/***************************************************/
/* Part of GDSiON software synthesizer             */
/* Copyright (c) 2024 Yuri Sizov and contributors  */
/* Provided under MIT                              */
/***************************************************/

#ifndef SION_COMPAT_REGEX_H
#define SION_COMPAT_REGEX_H

#include <string>
#include <vector>

#include "compat/sion_containers.h"
#include "compat/sion_string.h"

// A PCRE2-backed stand-in for Godot's RegEx/RegExMatch classes. Godot wraps
// PCRE2, so matching semantics (including inline flags like (?s)) match the
// original GDExtension behavior exactly.

class RegExMatch {
	friend class RegEx;

public:
	// Populated by the matching code in sion_regex.cpp.
	std::vector<int> _starts; // Byte offsets of group starts (-1 if unmatched).
	std::vector<int> _ends;   // Byte offsets of group ends.
	std::string _subject;
	int get_group_count() const { return _starts.empty() ? 0 : static_cast<int>(_starts.size()) - 1; }

	bool is_empty() const { return _starts.empty(); }

	sion::String get_string(int p_group = 0) const;
	sion::String get_subject() const { return sion::String(_subject); }

	// Returns the {start, end} byte range of the group, or {-1, -1} if unmatched.
	std::vector<int> get_group(int p_group = 0) const;

	int get_index(int p_group = 0) const { return get_group(p_group)[0]; }
	int get_start(int p_group = 0) const { return get_group(p_group)[0]; }
	int get_end(int p_group = 0) const { return get_group(p_group)[1]; }
};

class RegEx {
	void *_code = nullptr; // pcre2_code *

public:
	~RegEx();

	// Returns null on compile error (mirrors Godot's empty Ref behavior).
	static Ref<RegEx> create_from_string(const std::string &p_pattern);

	bool is_valid() const { return _code != nullptr; }

	// Returns null if there is no match.
	Ref<RegExMatch> search(const std::string &p_subject, int p_offset = 0) const;

	// All non-overlapping matches from p_offset on, mirroring Godot's
	// iteration (empty matches advance the cursor by one).
	std::vector<Ref<RegExMatch>> search_all(const std::string &p_subject, int p_offset = 0) const;

	// Literal replacement of up to p_count matches (negative = all). Godot's
	// binding takes an int64 count, so callers passing a bool convert true
	// to a count of 1; keep that exact shape for behavioral parity.
	sion::String sub(const std::string &p_subject, const sion::String &p_replacement, int64_t p_count = -1) const;
};

#endif // SION_COMPAT_REGEX_H
