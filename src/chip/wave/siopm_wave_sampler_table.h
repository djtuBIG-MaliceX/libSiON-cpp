/*
 * Part of libSiON-cpp, forked from GDSiON software synthesizer
 * Copyright (c) 2024-2026 Yuri Sizov, James Alan Nguyen and contributors
 * Based on SiON Flash Software Synthesizer (C) 2008-2016 keim_at_Si
 * Provided under MIT License.
 */

#ifndef SIOPM_WAVE_SAMPLER_TABLE_H
#define SIOPM_WAVE_SAMPLER_TABLE_H

//#include <godot_cpp/templates/vector.hpp>
#include "chip/wave/siopm_wave_base.h"
#include "chip/wave/siopm_wave_sampler_data.h"



class SiOPMWaveSamplerTable : public SiOPMWaveBase {
	//GDCLASS(SiOPMWaveSamplerTable, SiOPMWaveBase)

	// Stencil table; search sample in stencil table before seaching in this instance's own table.
	Ref<SiOPMWaveSamplerTable> _stencil;
	std::vector<Ref<SiOPMWaveSamplerData>> _table;

protected:
	static void _bind_methods() {}

public:
	Ref<SiOPMWaveSamplerTable> get_stencil() const { return _stencil; }
	void set_stencil(const Ref<SiOPMWaveSamplerTable> &p_table) { _stencil = p_table; }

	Ref<SiOPMWaveSamplerData> get_sample(int p_sample_number) const;
	void set_sample(const Ref<SiOPMWaveSamplerData> &p_sample, int p_key_range_from = 0, int p_key_range_to = -1);

	void clear();

	SiOPMWaveSamplerTable();
	~SiOPMWaveSamplerTable();
};

#endif // SIOPM_WAVE_SAMPLER_TABLE_H
