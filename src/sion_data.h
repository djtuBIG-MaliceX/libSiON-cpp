/***************************************************/
/* Part of GDSiON software synthesizer             */
/* Copyright (c) 2024 Yuri Sizov and contributors  */
/* Provided under MIT                              */
/***************************************************/

#ifndef SION_DATA_H
#define SION_DATA_H

//#include <godot_cpp/core/object.hpp>
#include "sequencer/simml_data.h"
#include <memory>



class SiONVoice;
class SiOPMWavePCMData;
class SiOPMWaveSamplerData;
class SiOPMWaveSamplerTable;

// Contains musical score and voice settings data of SiON.
class SiONData : public SiMMLData {
	//GDCLASS(SiONData, SiMMLData)

protected:
	static void _bind_methods() {}

public:
	std::shared_ptr<SiOPMWavePCMData> set_pcm_wave(int p_index, const Variant &p_data, double p_sampling_note = 69, int p_key_range_from = 0, int p_key_range_to = 127, int p_src_channel_count = 2, int p_channel_count = 0);
	void set_pcm_voice(int p_index, const std::shared_ptr<SiONVoice> &p_voice);

	std::shared_ptr<SiOPMWaveSamplerData> set_sampler_wave(int p_index, const Variant &p_data, bool p_ignore_note_off = false, int p_pan = 0, int p_src_channel_count = 2, int p_channel_count = 0);
	void set_sampler_table(int p_bank, const std::shared_ptr<SiOPMWaveSamplerTable> &p_table);

	SiONData() {}
	virtual ~SiONData() {}
};

#endif // SION_DATA_H
