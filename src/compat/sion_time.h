/*
 * Part of libSiON-cpp, forked from GDSiON software synthesizer
 * Copyright (c) 2024-2026 Yuri Sizov, James Alan Nguyen and contributors
 * Based on SiON Flash Software Synthesizer (C) 2008-2016 keim_at_Si
 * Provided under MIT License.
 */

#ifndef SION_COMPAT_TIME_H
#define SION_COMPAT_TIME_H

#include <chrono>
#include <cstdint>

// Tick counters compatible with the `Time::get_singleton()->get_ticks_msec()`
// call shape left by the GDExtension port.
class Time {
public:
	static Time *get_singleton() {
		static Time singleton;
		return &singleton;
	}

	int64_t get_ticks_usec() const {
		using namespace std::chrono;
		return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
	}

	int64_t get_ticks_msec() const {
		return get_ticks_usec() / 1000;
	}
};

#endif // SION_COMPAT_TIME_H
