/*
 * Part of libSiON-cpp, forked from GDSiON software synthesizer
 * Copyright (c) 2024-2026 Yuri Sizov, James Alan Nguyen and contributors
 * Based on SiON Flash Software Synthesizer (C) 2008-2016 keim_at_Si
 * Provided under MIT License.
 */

#ifndef SI_EFFECT_SPEAKER_SIMULATOR_H
#define SI_EFFECT_SPEAKER_SIMULATOR_H

#include "effector/si_effect_base.h"

class SiEffectSpeakerSimulator : public SiEffectBase {
	//GDCLASS(SiEffectSpeakerSimulator, SiEffectBase)

	double _spring_coef = 0.96;

	double _diaphragm_pos_left = 0;
	double _diaphragm_pos_right = 0;
	double _previous_left = 0;
	double _previous_right = 0;

protected:

public:
	void set_params(double p_hardness = 0.2);

	//

	virtual int prepare_process() override;
	virtual int process(int p_channels, std::vector<double> *r_buffer, int p_start_index, int p_length) override;

	virtual void set_by_mml(std::vector<double> p_args) override;
	virtual void reset() override;

	SiEffectSpeakerSimulator(double p_hardness = 0.2);
	~SiEffectSpeakerSimulator() {}
};

#endif // SI_EFFECT_SPEAKER_SIMULATOR_H
