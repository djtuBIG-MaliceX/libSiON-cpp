/*
 * Part of libSiON-cpp, forked from GDSiON software synthesizer
 * Copyright (c) 2024-2026 Yuri Sizov, James Alan Nguyen and contributors
 * Based on SiON Flash Software Synthesizer (C) 2008-2016 keim_at_Si
 * Provided under MIT License.
 */

#ifndef SI_FILTER_NOTCH_H
#define SI_FILTER_NOTCH_H

#include "effector/filters/si_filter_base.h"

class SiFilterNotch : public SiFilterBase {
	//GDCLASS(SiFilterNotch, SiFilterBase)

protected:

public:
	void set_params(double p_frequency = 3000, double p_band = 1);

	virtual void set_by_mml(std::vector<double> p_args) override;
	virtual void reset() override;

	SiFilterNotch(double p_frequency = 3000, double p_band = 1);
	~SiFilterNotch() {}
};

#endif // SI_FILTER_NOTCH_H
