/*
 * Part of libSiON-cpp, forked from GDSiON software synthesizer
 * Copyright (c) 2024-2026 Yuri Sizov, James Alan Nguyen and contributors
 * Based on SiON Flash Software Synthesizer (C) 2008-2016 keim_at_Si
 * Provided under MIT License.
 */

#ifndef SI_EFFECT_WAVE_SHAPER_H
#define SI_EFFECT_WAVE_SHAPER_H

#include "effector/si_effect_base.h"

class SiEffectWaveShaper : public SiEffectBase {
	//GDCLASS(SiEffectWaveShaper, SiEffectBase)

	int _coefficient = 0;
	double _output_level = 0;

protected:

public:
	void set_params(double p_distortion = 0.5, double p_output_level = 1.0);

	//

	virtual int prepare_process() override;
	virtual int process(int p_channels, std::vector<double> *r_buffer, int p_start_index, int p_length) override;

	virtual void set_by_mml(std::vector<double> p_args) override;
	virtual void reset() override;

	SiEffectWaveShaper(double p_distortion = 0.5, double p_output_level = 1.0);
	~SiEffectWaveShaper() {}
};

#endif // SI_EFFECT_WAVE_SHAPER_H
