/***************************************************/
/* Part of GDSiON software synthesizer             */
/* Copyright (c) 2024 Yuri Sizov and contributors  */
/* Provided under MIT                              */
/***************************************************/

#ifndef SIOPM_WAVE_PCM_TABLE_H
#define SIOPM_WAVE_PCM_TABLE_H

//#include <godot_cpp/templates/vector.hpp>
#include "chip/wave/siopm_wave_base.h"
#include "chip/wave/siopm_wave_pcm_data.h"



class SiOPMWavePCMTable : public SiOPMWaveBase {
	//GDCLASS(SiOPMWavePCMTable, SiOPMWaveBase)

	// PCM wave data assign table for each note.
	std::vector<std::shared_ptr<SiOPMWavePCMData>> _note_data_map;
	std::vector<double> _note_volume_map;
	std::vector<int> _note_pan_map;

protected:
	static void _bind_methods() {}

public:
	std::shared_ptr<SiOPMWavePCMData> get_note_data(int p_note) const;
	double get_note_volume(int p_note) const;
	int get_note_pan(int p_note) const;

	void set_key_range_data(const std::shared_ptr<SiOPMWavePCMData> &p_pcm_data, int p_key_range_from = 0, int p_key_range_to = 127);
	void set_key_scale_volume(int p_center_note = 64, double p_key_range = 0, double p_volume_range = 0);
	void set_key_scale_pan(int p_center_note = 64, double p_key_range = 0, double p_pan_width = 0);

	void clear();

	SiOPMWavePCMTable();
	~SiOPMWavePCMTable();
};

#endif // SIOPM_WAVE_PCM_TABLE_H
