/*
 * Part of libSiON-cpp, forked from GDSiON software synthesizer
 * Copyright (c) 2024-2026 Yuri Sizov, James Alan Nguyen and contributors
 * Based on SiON Flash Software Synthesizer (C) 2008-2016 keim_at_Si
 * Provided under MIT License.
 */

#ifndef SI_EFFECT_DOWNSAMPLER_H
#define SI_EFFECT_DOWNSAMPLER_H

#include "effector/si_effect_base.h"
#include <vector>

class SiEffectDownsampler : public SiEffectBase {
	//GDCLASS(SiEffectDownsampler, SiEffectBase)

	int _frequency_shift = 0;
	double _bit_conv0 = 1;
	double _bit_conv1 = 1;
	int _channel_count = 2;

	void _process_mono(std::vector<double> *r_buffer, int p_start_index, int p_length);
	void _process_stereo(std::vector<double> *r_buffer, int p_start_index, int p_length);

protected:

public:
	void set_params(int p_frequency_shift = 0, int p_bitrate = 16, int p_channel_count = 2);

	//

	virtual int prepare_process() override;
	virtual int process(int p_channels, std::vector<double> *r_buffer, int p_start_index, int p_length) override;

	virtual void set_by_mml(std::vector<double> p_args) override;
	virtual void reset() override;

	SiEffectDownsampler(int p_frequency_shift = 0, int p_bitrate = 16, int p_channel_count = 2);
	~SiEffectDownsampler() {}
};

#endif // SI_EFFECT_DOWNSAMPLER_H
