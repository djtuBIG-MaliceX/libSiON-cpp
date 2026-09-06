/*
 * Part of libSiON-cpp, forked from GDSiON software synthesizer
 * Copyright (c) 2024-2026 Yuri Sizov, James Alan Nguyen and contributors
 * Based on SiON Flash Software Synthesizer (C) 2008-2016 keim_at_Si
 * Provided under MIT License.
 */

#ifndef SIOPM_WAVE_BASE_H
#define SIOPM_WAVE_BASE_H

//#include <godot_cpp/classes/ref_counted.hpp>
//#include <godot_cpp/classes/audio_stream.hpp>
//#include <godot_cpp/templates/vector.hpp>
#include "sion_enums.h"



class SiOPMWaveBase {
	//GDCLASS(SiOPMWaveBase, RefCounted)

	SiONModuleType _module_type = SiONModuleType::MODULE_MAX;

protected:
	static void _bind_methods() {}

	std::vector<double> _extract_wave_data(const Ref<AudioStream> &p_stream, int *r_channel_count);

public:
	SiONModuleType get_module_type() const { return _module_type; }

	SiOPMWaveBase(SiONModuleType p_module_type = SiONModuleType::MODULE_MAX);
	virtual ~SiOPMWaveBase() {}
};

#endif // SIOPM_WAVE_BASE_H
