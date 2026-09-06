/*
 * Part of libSiON-cpp, forked from GDSiON software synthesizer
 * Copyright (c) 2024-2026 Yuri Sizov, James Alan Nguyen and contributors
 * Based on SiON Flash Software Synthesizer (C) 2008-2016 keim_at_Si
 * Provided under MIT License.
 */

#include "mml_event.h"

//#include <godot_cpp/core/memory.hpp>
//#include <godot_cpp/variant/string.hpp>
#include "sequencer/base/mml_parser.h"
#include <string>



int MMLEvent::get_id_from_mml(sion::String p_mml) {
	if (p_mml == "c" || p_mml == "d" || p_mml == "e" || p_mml == "f" || p_mml == "g" || p_mml == "a" || p_mml == "b") {
		return MMLEvent::NOTE;
	}
	if (p_mml == "r") {
		return MMLEvent::REST;
	}
	if (p_mml == "q") {
		return MMLEvent::QUANT_RATIO;
	}
	if (p_mml == "@q") {
		return MMLEvent::QUANT_COUNT;
	}
	if (p_mml == "v") {
		return MMLEvent::VOLUME;
	}
	if (p_mml == "@v") {
		return MMLEvent::FINE_VOLUME;
	}
	if (p_mml == "%") {
		return MMLEvent::MOD_TYPE;
	}
	if (p_mml == "@") {
		return MMLEvent::MOD_PARAM;
	}
	if (p_mml == "@i") {
		return MMLEvent::INPUT_PIPE;
	}
	if (p_mml == "@o") {
		return MMLEvent::OUTPUT_PIPE;
	}
	if (p_mml == "(" || p_mml == ")") {
		return MMLEvent::VOLUME_SHIFT;
	}
	if (p_mml == "&") {
		return MMLEvent::SLUR;
	}
	if (p_mml == "&&") {
		return MMLEvent::SLUR_WEAK;
	}
	if (p_mml == "*") {
		return MMLEvent::PITCHBEND;
	}
	if (p_mml == ",") {
		return MMLEvent::PARAMETER;
	}
	if (p_mml == "$") {
		return MMLEvent::REPEAT_ALL;
	}
	if (p_mml == "[") {
		return MMLEvent::REPEAT_BEGIN;
	}
	if (p_mml == "]") {
		return MMLEvent::REPEAT_END;
	}
	if (p_mml == "|") {
		return MMLEvent::REPEAT_BREAK;
	}
	if (p_mml == "t") {
		return MMLEvent::TEMPO;
	}

	return 0;
}

// Helpers.

MMLEvent *MMLEvent::get_parameters(std::vector<int> *r_params, int p_length) const {
	MMLEvent *event = const_cast<MMLEvent *>(this);

	int i = 0;
	while (i < p_length) {
		(*r_params)[i] = event->data;
		i++;

		if (event->next == nullptr || event->next->id != EventID::PARAMETER) {
			break;
		}

		event = event->next;
	}
	while (i < p_length) {
		(*r_params)[i] = INT32_MIN;
		i++;
	}

	return event;
}

// Object management.

void MMLEvent::initialize(int p_id, int p_data, int p_length) {
	id = p_id & 0x7f;
	data = p_data; // Prefer values below 0xffffff.
	length = p_length;

	next = nullptr;
	jump = nullptr;
}

sion::String MMLEvent::as_text() const {
	return "#" + itos(id) + "{" + itos(data) + "," + itos(length) + "}";
}

sion::String MMLEvent::_to_string() const {
	sion::String chain_str = "";
	chain_str += "next=" + (next ? itos(next->id) : "null") + ", ";
	chain_str += "jump=" + (jump ? itos(jump->id) : "null");

	return vformat("MMLEvent: id=%d, data=%d, len=%d, %s", id, length, data, chain_str);
}

MMLEvent::MMLEvent(int p_id, int p_data, int p_length) {
	if (p_id > 1) {
		initialize(p_id, p_data, p_length);
	}
}

MMLEvent::~MMLEvent() {
	next = nullptr;
	jump = nullptr;
}
