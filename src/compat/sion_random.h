/***************************************************/
/* Part of GDSiON software synthesizer             */
/* Copyright (c) 2024 Yuri Sizov and contributors  */
/* Provided under MIT                              */
/***************************************************/

#ifndef SION_COMPAT_RANDOM_H
#define SION_COMPAT_RANDOM_H

#include <chrono>
#include <cstdint>

// PCG32 random generator + Godot-compatible RandomNumberGenerator facade.
// The core algorithm is the O'Neill PCG-XSH-RR 64/32 reference (the same one
// Godot vendors at thirdparty/misc/pcg.cpp), so value sequences match the
// original GDExtension behavior for any given seed.

namespace sion {

class PCG32 {
	uint64_t _state = 0;
	uint64_t _inc = 0;

public:
	static constexpr uint64_t MULTIPLIER = 6364136223846793005ULL;
	static constexpr uint64_t DEFAULT_INCREMENT = 1442695040888963407ULL;

	uint32_t next() {
		uint64_t old = _state;
		_state = old * MULTIPLIER + _inc;

		uint32_t xs = static_cast<uint32_t>(((old >> 18u) ^ old) >> 27u);
		uint32_t rot = static_cast<uint32_t>(old >> 59u);
		return (xs >> rot) | (xs << ((~rot + 1) & 31));
	}

	void seed(uint64_t p_state, uint64_t p_seq) {
		_state = 0;
		_inc = (p_seq << 1u) | 1u;
		next();
		_state += p_state;
		next();
	}

	// Rejection-sampled bounded draw (pcg32_boundedrand_r parity).
	uint32_t bounded(uint32_t p_bound) {
		if (p_bound == 0) {
			return 0;
		}
		uint32_t threshold = (~static_cast<uint32_t>(0)) % p_bound;
		while (true) {
			uint32_t r = next();
			if (r >= threshold) {
				return r % p_bound;
			}
		}
	}
};

class RandomNumberGenerator {
	PCG32 _rng;
	uint64_t _seed = 0;

public:
	RandomNumberGenerator() {
		randomize();
	}

	void randomize() {
		uint64_t ticks = static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
		set_seed(ticks);
	}

	void set_seed(uint64_t p_seed) {
		_seed = p_seed;
		_rng.seed(p_seed, PCG32::DEFAULT_INCREMENT);
	}

	uint64_t get_seed() const { return _seed; }

	uint32_t randi() { return _rng.next(); }

	// Godot parity: inclusive on both ends.
	int32_t randi_range(int32_t p_from, int32_t p_to) {
		if (p_from < p_to) {
			return static_cast<int32_t>(_rng.bounded(static_cast<uint32_t>(p_to - p_from) + 1u)) + p_from;
		} else if (p_to < p_from) {
			return static_cast<int32_t>(_rng.bounded(static_cast<uint32_t>(p_from - p_to) + 1u)) + p_to;
		}
		return p_to;
	}

	float randf() {
		return static_cast<float>(_rng.next()) / static_cast<float>(UINT32_MAX);
	}

	// Godot parity: two draws stitched into a 64-bit mantissa.
	double randd() {
		uint64_t a = _rng.next();
		a %= 1ull << 32;
		uint64_t b = _rng.next();
		b %= 1ull << 32;
		return static_cast<double>(a * static_cast<double>(1ull << 32) + static_cast<double>(b)) * 5.421010862497356e-20; // ldexp(x, -64)
	}
};

} // namespace sion

// Godot exposes these globally; library code uses them unqualified.`r
using ::sion::RandomNumberGenerator;
using ::sion::PCG32;

#endif // SION_COMPAT_RANDOM_H
