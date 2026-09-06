/*
 * Part of libSiON-cpp, forked from GDSiON software synthesizer
 * Copyright (c) 2024-2026 Yuri Sizov, James Alan Nguyen and contributors
 * Based on SiON Flash Software Synthesizer (C) 2008-2016 keim_at_Si
 * Provided under MIT License.
 */

#ifndef SI_CONTROLLABLE_FILTER_HIGH_PASS_H
#define SI_CONTROLLABLE_FILTER_HIGH_PASS_H

//#include <godot_cpp/templates/vector.hpp>
#include "effector/filters/si_controllable_filter_base.h"



class SiControllableFilterHighPass : public SiControllableFilterBase {
	//GDCLASS(SiControllableFilterHighPass, SiControllableFilterBase)

	virtual void _process_lfo(std::vector<double> *r_buffer, int p_start_index, int p_length) override;

protected:
	static void _bind_methods() {}

public:
	SiControllableFilterHighPass(double p_cutoff = 1, double p_resonance = 0);
	~SiControllableFilterHighPass() {}
};

#endif // SI_CONTROLLABLE_FILTER_HIGH_PASS_H
