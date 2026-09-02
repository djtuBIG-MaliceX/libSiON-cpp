/***************************************************/
/* Part of GDSiON software synthesizer             */
/* Copyright (c) 2024 Yuri Sizov and contributors  */
/* Provided under MIT                              */
/***************************************************/

#ifndef SIMML_DATA_H
#define SIMML_DATA_H

//#include <godot_cpp/core/object.hpp>
//#include <godot_cpp/templates/vector.hpp>
#include "sequencer/base/mml_data.h"



class SiMMLEnvelopeTable;
class SiMMLVoice;
class SiOPMChannelParams;
class SiOPMWaveSamplerTable;
class SiOPMWaveTable;

class SiMMLData : public MMLData {
	//GDCLASS(SiMMLData, MMLData)

protected:
	static void _bind_methods() {}

	std::vector<std::shared_ptr<SiMMLEnvelopeTable>> _envelope_tables;
	std::vector<std::shared_ptr<SiOPMWaveTable>> _wave_tables;
	std::vector<std::shared_ptr<SiOPMWaveSamplerTable>> _sampler_tables;

	std::vector<std::shared_ptr<SiMMLVoice>> _fm_voices;
	std::vector<std::shared_ptr<SiMMLVoice>> _pcm_voices;

public:
	// Static so it can be called when there is no SiMMLData instance available.
	static void clear_ref_stencils();
	void register_ref_stencils();

	// Tables.

	std::vector<std::shared_ptr<SiMMLEnvelopeTable>> get_envelope_tables() const { return _envelope_tables; }

	std::shared_ptr<SiMMLEnvelopeTable> get_envelope_table(int p_index) const;
	void set_envelope_table(int p_index, const std::shared_ptr<SiMMLEnvelopeTable> &p_envelope);
	std::shared_ptr<SiOPMWaveTable> get_wave_table(int p_index) const;
	std::shared_ptr<SiOPMWaveTable> set_wave_table(int p_index, std::vector<double> *p_data);
	std::shared_ptr<SiOPMWaveSamplerTable> get_sampler_table(int p_index) const;
	void set_sampler_table(int p_index, const std::shared_ptr<SiOPMWaveSamplerTable> &p_sampler);

	// Voices.

	std::shared_ptr<SiMMLVoice> initialize_voice(int p_index);
	void set_voice(int p_index, const std::shared_ptr<SiMMLVoice> &p_voice);
	std::shared_ptr<SiMMLVoice> get_pcm_voice(int p_index);

	//

	virtual void clear() override;

	SiMMLData();
	virtual ~SiMMLData();
};

#endif // SIMML_DATA_H
