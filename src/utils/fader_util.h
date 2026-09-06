/*
 * Part of libSiON-cpp, forked from GDSiON software synthesizer
 * Copyright (c) 2024-2026 Yuri Sizov, James Alan Nguyen and contributors
 * Based on SiON Flash Software Synthesizer (C) 2008-2016 keim_at_Si
 * Provided under MIT License.
 */

#ifndef SION_FADER_UTIL_H
#define SION_FADER_UTIL_H

//#include <godot_cpp/variant/callable.hpp>
#include <functional>



class FaderUtil {

	double _end = 0;
	double _step = 0;
	int _counter = 0;
	double _value = 0;

	std::function<void(double)> _callback;

public:
	bool is_active() const;
	bool is_incrementing() const;
	double get_value() const { return _value; }
	void set_callback(const std::function<void(double)> &p_callback) { _callback = p_callback; }

	// Return true if the end value has been reached.
	bool execute();
	void stop();

	void set_fade(double p_value_from = 0, double p_value_to = 1, int p_frames = 60);

	FaderUtil(const std::function<void(double)> &p_callback = nullptr, double p_value_from = 0, double p_value_to = 1, int p_frames = 60);
	~FaderUtil() {}
};

#endif // SION_FADER_UTIL_H
