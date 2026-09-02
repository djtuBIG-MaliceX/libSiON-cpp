/***************************************************/
/* Part of GDSiON software synthesizer             */
/* Copyright (c) 2024 Yuri Sizov and contributors  */
/* Provided under MIT                              */
/***************************************************/

#include "simml_data.h"
#include <memory>

#include "chip/siopm_ref_table.h"
#include "chip/wave/siopm_wave_pcm_table.h"
#include "chip/wave/siopm_wave_sampler_table.h"
#include "chip/wave/siopm_wave_table.h"
#include "sequencer/simml_envelope_table.h"
#include "sequencer/simml_ref_table.h"
#include "sequencer/simml_voice.h"



void SiMMLData::clear_ref_stencils() {
	// Bank 2 and 3 are not available at this time.
	SiOPMRefTable::get_instance()->clear_sampler_table_stencil(0);
	SiOPMRefTable::get_instance()->clear_sampler_table_stencil(1);

	SiOPMRefTable::get_instance()->clear_stencil_custom_wave_tables();
	SiOPMRefTable::get_instance()->clear_stencil_pcm_voices();

	SiMMLRefTable::get_instance()->clear_stencil_envelopes();
	SiMMLRefTable::get_instance()->clear_stencil_voices();
}

void SiMMLData::register_ref_stencils() {
	// Bank 2 and 3 are not available at this time.
	SiOPMRefTable::get_instance()->set_sampler_table_stencil(0, _sampler_tables[0]);
	SiOPMRefTable::get_instance()->set_sampler_table_stencil(1, _sampler_tables[1]);

	SiOPMRefTable::get_instance()->set_stencil_custom_wave_tables(_wave_tables);
	SiOPMRefTable::get_instance()->set_stencil_pcm_voices(_pcm_voices);

	SiMMLRefTable::get_instance()->set_stencil_envelopes(_envelope_tables);
	SiMMLRefTable::get_instance()->set_stencil_voices(_fm_voices);
}

// Tables.

std::shared_ptr<SiMMLEnvelopeTable> SiMMLData::get_envelope_table(int p_index) const {
	//////ERR_FAIL_INDEX_V(p_index, SiMMLRefTable::ENVELOPE_TABLE_MAX, nullptr);

	return _envelope_tables[p_index];
}

void SiMMLData::set_envelope_table(int p_index, const std::shared_ptr<SiMMLEnvelopeTable> &p_envelope) {
	//////ERR_FAIL_INDEX(p_index, SiMMLRefTable::ENVELOPE_TABLE_MAX);

	_envelope_tables[p_index] = p_envelope;
}

std::shared_ptr<SiOPMWaveTable> SiMMLData::get_wave_table(int p_index) const {
	int index = p_index & (SiOPMRefTable::WAVE_TABLE_MAX - 1);
	return _wave_tables[index];
}

std::shared_ptr<SiOPMWaveTable> SiMMLData::set_wave_table(int p_index, std::vector<double> *p_data) {
	int index = p_index & (SiOPMRefTable::WAVE_TABLE_MAX - 1);

	std::vector<int> log_table;
	log_table.resize(p_data->size());  // TODO zeroed?
	for (int i = 0; i < p_data->size(); i++) {
		log_table[i] = SiOPMRefTable::calculate_log_table_index((*p_data)[i]);
	}

	_wave_tables[index] = std::shared_ptr<SiOPMWaveTable>(memnew(SiOPMWaveTable(log_table)));
	return _wave_tables[index];
}

std::shared_ptr<SiOPMWaveSamplerTable> SiMMLData::get_sampler_table(int p_index) const {
	////ERR_FAIL_INDEX_V(p_index, SiOPMRefTable::SAMPLER_TABLE_MAX, nullptr);

	return _sampler_tables[p_index];
}

void SiMMLData::set_sampler_table(int p_index, const std::shared_ptr<SiOPMWaveSamplerTable> &p_sampler) {
	////ERR_FAIL_INDEX(p_index, SiOPMRefTable::SAMPLER_TABLE_MAX);

	_sampler_tables[p_index] = p_sampler;
}

// Voices.

std::shared_ptr<SiMMLVoice> SiMMLData::initialize_voice(int p_index) {
	////ERR_FAIL_INDEX_V(p_index, SiMMLRefTable::VOICE_MAX, std::shared_ptr<SiMMLVoice>());

	std::shared_ptr<SiMMLVoice> voice;
	voice.instantiate();
	_fm_voices[p_index] = voice;

	return voice;
}

void SiMMLData::set_voice(int p_index, const std::shared_ptr<SiMMLVoice> &p_voice) {
	////ERR_FAIL_INDEX(p_index, SiMMLRefTable::VOICE_MAX);
	////ERR_FAIL_COND_MSG(!p_voice->is_suitable_for_fm_voice(), "SiMMLData: Cannot set voice data which is not suitable for FM voices.");

	_fm_voices[p_index] = p_voice;
}

std::shared_ptr<SiMMLVoice> SiMMLData::get_pcm_voice(int p_index) {
	int index = p_index & (SiOPMRefTable::PCM_DATA_MAX - 1);
	if (_pcm_voices[index].is_null()) {
		std::shared_ptr<SiMMLVoice> voice = SiMMLVoice::create_blank_pcm_voice(index);
		_pcm_voices[index] = voice;
	}

	return _pcm_voices[index];
}

//

void SiMMLData::clear() {
	MMLData::clear();

	for (int i = 0; i < SiMMLRefTable::ENVELOPE_TABLE_MAX; i++) {
		_envelope_tables[i] = std::shared_ptr<SiMMLEnvelopeTable>();
	}

	for (int i = 0; i < SiMMLRefTable::VOICE_MAX; i++) {
		_fm_voices[i] = std::shared_ptr<SiMMLVoice>();
	}

	for (int i = 0; i < SiOPMRefTable::WAVE_TABLE_MAX; i++) {
		_wave_tables[i] = std::shared_ptr<SiOPMWaveTable>();
	}

	for (int i = 0; i < SiOPMRefTable::PCM_DATA_MAX; i++) {
		if (_pcm_voices[i].is_valid()) {
			std::shared_ptr<SiOPMWavePCMTable> pcm_table = _pcm_voices[i]->get_wave_data();
			if (pcm_table.is_valid()) {
				pcm_table->clear();
			}

			_pcm_voices[i] = std::shared_ptr<SiMMLVoice>();
		}
	}

	for (int i = 0; i < SiOPMRefTable::SAMPLER_TABLE_MAX; i++) {
		_sampler_tables[i] = std::shared_ptr<SiOPMWaveSamplerTable>();
	}
}

SiMMLData::SiMMLData() {
	_envelope_tables.resize(SiMMLRefTable::ENVELOPE_TABLE_MAX); // TODO zeroed
	_wave_tables.resize(SiOPMRefTable::WAVE_TABLE_MAX); // TODO zeroed
	_sampler_tables.resize(SiOPMRefTable::SAMPLER_TABLE_MAX); // TODO zeroed

	_fm_voices.resize(SiMMLRefTable::VOICE_MAX); // TODO zeroed
	_pcm_voices.resize(SiOPMRefTable::PCM_DATA_MAX); // TODO zeroed

	for (int i = 0; i < SiOPMRefTable::SAMPLER_TABLE_MAX; i++) {
		_sampler_tables[i] = std::shared_ptr<SiOPMWaveSamplerTable>(memnew(SiOPMWaveSamplerTable));
	}
}

SiMMLData::~SiMMLData() {
	_envelope_tables.clear();
	_wave_tables.clear();
	_sampler_tables.clear();
}
