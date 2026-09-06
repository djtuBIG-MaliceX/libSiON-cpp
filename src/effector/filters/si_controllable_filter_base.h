/*
 * Part of libSiON-cpp, forked from GDSiON software synthesizer
 * Copyright (c) 2024-2026 Yuri Sizov, James Alan Nguyen and contributors
 * Based on SiON Flash Software Synthesizer (C) 2008-2016 keim_at_Si
 * Provided under MIT License.
 */

#ifndef SI_CONTROLLABLE_FILTER_BASE_H
#define SI_CONTROLLABLE_FILTER_BASE_H

//#include <godot_cpp/templates/vector.hpp>
#include "effector/si_effect_base.h"
//#include "templates/singly_linked_list.h"

class SiControllableFilterBase : public SiEffectBase {
	//GDCLASS(SiControllableFilterBase, SiEffectBase)

	// These are referencing external data, and we don't want to mess the internal cursor
	// in that data. So we keep our own pointers.
	SinglyLinkedList<int>::Element *_cutoff_ptr = nullptr;
	SinglyLinkedList<int>::Element *_resonance_ptr = nullptr;

	int _lfo_step = 0;
	int _lfo_residue_step = 0;

	virtual void _process_lfo(std::vector<double> *r_buffer, int p_start_index, int p_length) {}

protected:

	double _p0_right = 0;
	double _p1_right = 0;
	double _p0_left = 0;
	double _p1_left = 0;

	int _cutoff_index = 0;
	double _resonance = 0;

public:
	void set_params(int p_cutoff = 255, int p_resonance = 255, double p_fps = 20);
	void set_params_manually(double p_cutoff, double p_resonance);

	double get_cutoff() const;
	void set_cutoff(double p_value);

	double get_resonance() const { return _resonance; }
	void set_resonance(double p_value);

	//

	virtual int prepare_process() override;
	virtual int process(int p_channels, std::vector<double> *r_buffer, int p_start_index, int p_length) override;

	virtual void set_by_mml(std::vector<double> p_args) override;
	virtual void reset() override;

	SiControllableFilterBase();
	~SiControllableFilterBase() {}
};

#endif // SI_CONTROLLABLE_FILTER_BASE_H
