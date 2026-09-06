/*
 * Part of libSiON-cpp, forked from GDSiON software synthesizer
 * Copyright (c) 2024-2026 Yuri Sizov, James Alan Nguyen and contributors
 * Based on SiON Flash Software Synthesizer (C) 2008-2016 keim_at_Si
 * Provided under MIT License.
 */

#ifndef SIOPM_WAVE_TABLE_H
#define SIOPM_WAVE_TABLE_H

//#include <godot_cpp/templates/list.hpp>
//#include <godot_cpp/templates/vector.hpp>
#include "sion_enums.h"
#include "chip/wave/siopm_wave_base.h"



class SiOPMWaveTable : public SiOPMWaveBase {
	//GDCLASS(SiOPMWaveTable, SiOPMWaveBase)

	std::vector<int> _wavelet;
	int _fixed_bits = 0;
	SiONPitchTableType _default_pitch_table_type = SiONPitchTableType::PITCH_TABLE_OPM;

protected:
	static void _bind_methods() {}

public:
	std::vector<int> get_wavelet() const { return _wavelet; }
	int get_fixed_bits() const { return _fixed_bits; }
	SiONPitchTableType get_default_pitch_table_type() const { return _default_pitch_table_type; }

	//

	void initialize(std::vector<int> p_wavelet, SiONPitchTableType p_default_pt_type = SiONPitchTableType::PITCH_TABLE_OPM);
	void copy_from(const Ref<SiOPMWaveTable> &p_source);

	SiOPMWaveTable(std::vector<int> p_wavelet = std::vector<int>(), SiONPitchTableType p_default_pitch_table_type = SiONPitchTableType::PITCH_TABLE_OPM);
	~SiOPMWaveTable() {}
};

#endif // SIOPM_WAVE_TABLE_H
