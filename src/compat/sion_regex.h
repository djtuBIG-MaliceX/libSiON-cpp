/*
 * Part of libSiON-cpp, forked from GDSiON software synthesizer
 * Copyright (c) 2024-2026 Yuri Sizov, James Alan Nguyen and contributors
 * Based on SiON Flash Software Synthesizer (C) 2008-2016 keim_at_Si
 * Provided under MIT License.
 */

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

	// Literal replacement of up to p_count matches (negative = all).
	sion::String sub(const std::string &p_subject, const sion::String &p_replacement, int64_t p_count = -1) const;

	// SiON semantics: true means replace ALL matches (the old Godot binding's
	// bool-as-variant coercion ate this as count=1 — upstream parser bug we
	// deliberately do NOT replicate; comment/comment-like stripping must be global).
	sion::String sub(const std::string &p_subject, const sion::String &p_replacement, bool p_all) const {
		return sub(p_subject, p_replacement, p_all ? (int64_t)-1 : (int64_t)0);
	}
};

#endif // SION_COMPAT_REGEX_H
