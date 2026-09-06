/*
 * Part of libSiON-cpp, forked from GDSiON software synthesizer
 * Copyright (c) 2024-2026 Yuri Sizov, James Alan Nguyen and contributors
 * Based on SiON Flash Software Synthesizer (C) 2008-2016 keim_at_Si
 * Provided under MIT License.
 */

// Based on Bjarne Stroustrup's C++ Style and Technique FAQ
// https://www.stroustrup.com/bs_faq2.html#constraints

template<class T, class B> struct derived_from {
	static void constraints(T* p) {
		B* pb = p;
		(void)pb;
	}
	derived_from() {
		void(*p)(T*) = constraints;
	}
};
