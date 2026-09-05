/***************************************************/
/* Part of GDSiON software synthesizer             */
/* Copyright (c) 2024 Yuri Sizov and contributors  */
/* Provided under MIT                              */
/***************************************************/

#ifndef SION_COMPAT_MATH_H
#define SION_COMPAT_MATH_H

#include <cmath>
#include <cstdint>

// Math helpers replacing the godot-cpp Math namespace. Keep float->double
// promotion behavior identical to Godot (all computations are double).
struct Math {
	static double sin(double p_value) { return std::sin(p_value); }
	static double cos(double p_value) { return std::cos(p_value); }
	static double log(double p_value) { return std::log(p_value); }
	static double pow(double p_base, double p_exp) { return std::pow(p_base, p_exp); }
	static double sqrt(double p_value) { return std::sqrt(p_value); }
	static double sinh(double p_value) { return std::sinh(p_value); }
	static double fmod(double p_x, double p_y) { return std::fmod(p_x, p_y); }
	static bool is_nan(double p_value) { return std::isnan(p_value); }

	// Same formula as Godot 4.x.
	static double linear2db(double p_linear) {
		return std::log(p_linear) * 6.0 / std::log(10.0);
	}
};

#endif // SION_COMPAT_MATH_H
