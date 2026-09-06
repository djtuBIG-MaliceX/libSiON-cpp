/*
 * Part of libSiON-cpp, forked from GDSiON software synthesizer
 * Copyright (c) 2024-2026 Yuri Sizov, James Alan Nguyen and contributors
 * Based on SiON Flash Software Synthesizer (C) 2008-2016 keim_at_Si
 * Provided under MIT License.
 */

#include "beats_per_minute.h"

//#include <godot_cpp/core/math.hpp>

#include "sequencer/base/mml_sequencer.h"



bool BeatsPerMinute::update(double p_bpm, int p_sample_rate) {
	double bpm = std::clamp(p_bpm, 1.0, 511.0);

	if (bpm == _bpm && p_sample_rate == _sample_rate) {
		return false;
	}

	_bpm = bpm;
	_sample_rate = p_sample_rate;

	_tick_per_sample = _resolution * _bpm / (_sample_rate * 240);
	_beat_16th_per_sample = _bpm / (_sample_rate * 15); // 60 / 4
	_sample_per_beat_16th = 1.0 / _beat_16th_per_sample;
	_sample_per_tick = (1.0 / _tick_per_sample) * (1 << MMLSequencer::FIXED_BITS);

	return true;
}

BeatsPerMinute::BeatsPerMinute(double p_bpm, int p_sample_rate, int p_resolution) {
	_resolution = p_resolution;
	update(p_bpm, p_sample_rate);
}
