/*
 * Part of libSiON-cpp, forked from GDSiON software synthesizer
 * Copyright (c) 2024-2026 Yuri Sizov, James Alan Nguyen and contributors
 * Based on SiON Flash Software Synthesizer (C) 2008-2016 keim_at_Si
 * Provided under MIT License.
 */

#include "compat/sion_regex.h"
#include "compat/sion_errors.h"

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

sion::String RegExMatch::get_string(int p_group) const {
	if (p_group < 0 || p_group >= (int)_starts.size() || _starts[p_group] < 0) {
		return std::string();
	}
	return _subject.substr(_starts[p_group], _ends[p_group] - _starts[p_group]);
}

std::vector<int> RegExMatch::get_group(int p_group) const {
	if (p_group < 0 || p_group >= (int)_starts.size()) {
		return { -1, -1 };
	}
	return { _starts[p_group], _ends[p_group] };
}

RegEx::~RegEx() {
	if (_code) {
		pcre2_code_free_8((pcre2_code *)_code);
	}
}

Ref<RegEx> RegEx::create_from_string(const std::string &p_pattern) {
	int errcode = 0;
	PCRE2_SIZE erroffset = 0;

	pcre2_code *code = pcre2_compile_8(
			(PCRE2_SPTR8)p_pattern.c_str(),
			PCRE2_ZERO_TERMINATED,
			0,
			&errcode,
			&erroffset,
			nullptr);

	if (!code) {
		PCRE2_UCHAR8 buffer[512];
		pcre2_get_error_message_8(errcode, buffer, sizeof(buffer));
		ERR_PRINT(vformat("RegEx: Compilation failed at offset %d: %s", (int)erroffset, (const char *)buffer));
		return nullptr;
	}

	Ref<RegEx> regex(new RegEx());
	regex->_code = code;
	return regex;
}

static Ref<RegExMatch> _build_match(pcre2_match_data_8 *p_match_data, const std::string &p_subject) {
	Ref<RegExMatch> match(new RegExMatch());

	PCRE2_SIZE *ovector = pcre2_get_ovector_pointer_8(p_match_data);
	uint32_t count = pcre2_get_ovector_count_8(p_match_data);

	match->_subject = p_subject;
	match->_starts.resize(count);
	match->_ends.resize(count);

	for (uint32_t i = 0; i < count; i++) {
		if (ovector[i * 2] == PCRE2_UNSET) {
			match->_starts[i] = -1;
			match->_ends[i] = -1;
		} else {
			match->_starts[i] = (int)ovector[i * 2];
			match->_ends[i] = (int)ovector[i * 2 + 1];
		}
	}

	return match;
}

Ref<RegExMatch> RegEx::search(const std::string &p_subject, int p_offset) const {
	if (!_code) {
		return nullptr;
	}
	if (p_offset < 0) {
		p_offset = 0;
	}
	if (p_offset > (int)p_subject.size()) {
		return nullptr;
	}

	pcre2_match_data_8 *match_data = pcre2_match_data_create_from_pattern_8((const pcre2_code *)_code, nullptr);

	int rc = pcre2_match_8((const pcre2_code *)_code,
			(PCRE2_SPTR8)p_subject.c_str(),
			(PCRE2_SIZE)p_subject.size(),
			(PCRE2_SIZE)p_offset,
			0,
			match_data,
			nullptr);

	Ref<RegExMatch> match;
	if (rc >= 0) {
		match = _build_match(match_data, p_subject);
	}

	pcre2_match_data_free_8(match_data);
	return match;
}

std::vector<Ref<RegExMatch>> RegEx::search_all(const std::string &p_subject, int p_offset) const {
	std::vector<Ref<RegExMatch>> matches;

	if (!_code) {
		return matches;
	}
	if (p_offset < 0) {
		p_offset = 0;
	}

	pcre2_match_data_8 *match_data = pcre2_match_data_create_from_pattern_8((const pcre2_code *)_code, nullptr);

	PCRE2_SIZE offset = (PCRE2_SIZE)p_offset;
	const PCRE2_SIZE length = (PCRE2_SIZE)p_subject.size();

	while (offset <= length) {
		int rc = pcre2_match_8((const pcre2_code *)_code,
				(PCRE2_SPTR8)p_subject.c_str(),
				length,
				offset,
				0,
				match_data,
				nullptr);

		if (rc < 0) {
			break; // No more matches.
		}

		matches.push_back(_build_match(match_data, p_subject));

		PCRE2_SIZE *ovector = pcre2_get_ovector_pointer_8(match_data);
		PCRE2_SIZE next = ovector[1];
		if (ovector[0] == ovector[1]) {
			next += 1; // Empty match: step one forward to avoid looping.
		}
		offset = next;
	}

	pcre2_match_data_free_8(match_data);
	return matches;
}

sion::String RegEx::sub(const std::string &p_subject, const sion::String &p_replacement, int64_t p_count) const {
	if (!_code || p_count == 0) {
		return sion::String(p_subject);
	}

	sion::String result;
	pcre2_match_data_8 *match_data = pcre2_match_data_create_from_pattern_8((const pcre2_code *)_code, nullptr);

	PCRE2_SIZE offset = 0;
	const PCRE2_SIZE length = (PCRE2_SIZE)p_subject.size();
	int64_t replaced = 0;

	while (offset <= length) {
		int rc = pcre2_match_8((const pcre2_code *)_code,
				(PCRE2_SPTR8)p_subject.c_str(),
				length,
				offset,
				0,
				match_data,
				nullptr);

		if (rc < 0) {
			break; // No more matches.
		}

		PCRE2_SIZE *ovector = pcre2_get_ovector_pointer_8(match_data);

		result += p_subject.substr(offset, ovector[0] - offset);
		result += p_replacement;
		replaced++;

		if (ovector[0] == ovector[1]) {
			// Empty match: copy one character forward to avoid looping.
			if (offset < length) {
				result += p_subject[offset];
			}
			offset += 1;
		} else {
			offset = ovector[1];
		}

		if (p_count >= 0 && replaced >= p_count) {
			break;
		}
	}

	pcre2_match_data_free_8(match_data);
	result += p_subject.substr(std::min<PCRE2_SIZE>(offset, length));
	return result;
}
