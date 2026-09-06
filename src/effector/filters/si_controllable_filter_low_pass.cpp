/*
 * Part of libSiON-cpp, forked from GDSiON software synthesizer
 * Copyright (c) 2024-2026 Yuri Sizov, James Alan Nguyen and contributors
 * Based on SiON Flash Software Synthesizer (C) 2008-2016 keim_at_Si
 * Provided under MIT License.
 */

#include "si_controllable_filter_low_pass.h"

#include "chip/siopm_ref_table.h"

void SiControllableFilterLowPass::_process_lfo(std::vector<double> *r_buffer, int p_start_index, int p_length) {
	double cutoff = SiOPMRefTable::get_instance()->filter_cutoff_table[_cutoff_index];
	double feedback = _resonance * SiOPMRefTable::get_instance()->filter_feedback_table[_cutoff_index];

	for (int i = p_start_index; i < (p_start_index + p_length); ) {
		double value_left = (*r_buffer)[i];
		_p0_left += cutoff * (value_left - _p0_left + feedback * (_p0_left - _p1_left));
		_p1_left += cutoff * (_p0_left - _p1_left);
		(*r_buffer)[i] = _p1_left;
		i++;

		double value_right = (*r_buffer)[i];
		_p0_right += cutoff * (value_right - _p0_right + feedback * (_p0_right - _p1_right));
		_p1_right += cutoff * (_p0_right - _p1_right);
		(*r_buffer)[i] = _p1_right;
		i++;
	}
}

SiControllableFilterLowPass::SiControllableFilterLowPass(double p_cutoff, double p_resonance) :
		SiControllableFilterBase() {
	reset();
	set_params_manually(p_cutoff, p_resonance);
}
