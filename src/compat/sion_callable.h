/***************************************************/
/* Part of GDSiON software synthesizer             */
/* Copyright (c) 2024 Yuri Sizov and contributors  */
/* Provided under MIT                              */
/***************************************************/

#ifndef SION_COMPAT_CALLABLE_H
#define SION_COMPAT_CALLABLE_H

#include <functional>
#include <type_traits>
#include <utility>

// Minimal stand-in for Godot's Callable, matching the only usage shape left
// in the library (the fader util callback). Phase 2 replaces driver-facing
// signal plumbing with plain std::function members.

class Callable {
	std::function<void(double)> _callable;

public:
	Callable() = default;
	Callable(std::nullptr_t) {}

	template <class F, class = std::enable_if_t<!std::is_same_v<std::decay_t<F>, Callable>>>
	Callable(F &&p_callable) :
			_callable(std::forward<F>(p_callable)) {}

	bool is_valid() const { return static_cast<bool>(_callable); }

	void call(double p_argument) const {
		if (_callable) {
			_callable(p_argument);
		}
	}
};

#endif // SION_COMPAT_CALLABLE_H
