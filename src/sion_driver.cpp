/***************************************************/
/* Part of GDSiON software synthesizer             */
/* Copyright (c) 2024 Yuri Sizov and contributors  */
/* Provided under MIT                              */
/***************************************************/

#include "sion_driver.h"

#include "sion_data.h"
#include "sion_enums.h"
#include "sion_voice.h"
#include "chip/channels/siopm_channel_base.h"
#include "chip/siopm_channel_params.h"
#include "chip/siopm_ref_table.h"
#include "chip/siopm_sound_chip.h"
#include "chip/wave/siopm_wave_pcm_data.h"
#include "chip/wave/siopm_wave_pcm_table.h"
#include "chip/wave/siopm_wave_sampler_data.h"
#include "chip/wave/siopm_wave_table.h"
#include "effector/si_effector.h"
#include "events/sion_event.h"
#include "events/sion_track_event.h"
#include "sequencer/base/mml_event.h"
#include "sequencer/base/mml_executor.h"
#include "sequencer/base/mml_parser.h"
#include "sequencer/base/mml_parser_settings.h"
#include "sequencer/base/mml_sequence.h"
#include "sequencer/base/mml_sequence_group.h"
#include "sequencer/base/mml_sequencer.h"
#include "sequencer/simml_envelope_table.h"
#include "sequencer/simml_ref_table.h"
#include "sequencer/simml_sequencer.h"
#include "sequencer/simml_track.h"
#include "utils/fader_util.h"
#include "utils/transformer_util.h"

#include <algorithm>

// TODO: Extract somewhere more manageable?
const char *SiONDriver::VERSION = "0.7.0.0"; // Original code was last versioned as 0.6.6.0.
const char *SiONDriver::VERSION_FLAVOR = "beta8";

SiONDriver *SiONDriver::_mutex = nullptr;
bool SiONDriver::_allow_multiple_drivers = false;

// Data.

Ref<SiOPMWaveTable> SiONDriver::set_wave_table(int p_index, std::vector<double> p_table) {
	int bits = -1;
	for (int i = p_table.size(); i > 0; i >>= 1) {
		bits += 1;
	}

	if (bits < 2) {
		return Ref<SiOPMWaveTable>();
	}

	std::vector<int> wave_data = TransformerUtil::transform_pcm_data(p_table, 1);
	wave_data.resize(1 << bits); // TODO zeroed?

	Ref<SiOPMWaveTable> wave_table = new SiOPMWaveTable(wave_data);
	SiOPMRefTable::get_instance()->register_wave_table(p_index, wave_table);
	return wave_table;
}

Ref<SiOPMWavePCMData> SiONDriver::set_pcm_wave(int p_index, const Ref<SampleData> &p_data, double p_sampling_note, int p_key_range_from, int p_key_range_to, int p_src_channel_num, int p_channel_num) {
	Ref<SiMMLVoice> pcm_voice = SiOPMRefTable::get_instance()->get_global_pcm_voice(p_index & (SiOPMRefTable::PCM_DATA_MAX - 1));
	Ref<SiOPMWavePCMTable> pcm_table = pcm_voice->get_wave_data();
	Ref<SiOPMWavePCMData> pcm_data = new SiOPMWavePCMData(p_data, (int)(p_sampling_note * 64), p_src_channel_num, p_channel_num);

	pcm_table->set_key_range_data(pcm_data, p_key_range_from, p_key_range_to);
	return pcm_data;
}

Ref<SiOPMWaveSamplerData> SiONDriver::set_sampler_wave(int p_index, const Ref<SampleData> &p_data, bool p_ignore_note_off, int p_pan, int p_src_channel_num, int p_channel_num) {
	return SiOPMRefTable::get_instance()->register_sampler_data(p_index, p_data, p_ignore_note_off, p_pan, p_src_channel_num, p_channel_num);
}

void SiONDriver::set_pcm_voice(int p_index, const Ref<SiONVoice> &p_voice) {
	SiOPMRefTable::get_instance()->set_global_pcm_voice(p_index & (SiOPMRefTable::PCM_DATA_MAX - 1), p_voice);
}

void SiONDriver::set_sampler_table(int p_bank, const Ref<SiOPMWaveSamplerTable> &p_table) {
	SiOPMRefTable::get_instance()->sampler_tables[p_bank & (SiOPMRefTable::SAMPLER_TABLE_MAX - 1)] = p_table;
}

void SiONDriver::set_envelope_table(int p_index, std::vector<int> p_table, int p_loop_point) {
	SiMMLRefTable::get_instance()->register_master_envelope_table(p_index, new SiMMLEnvelopeTable(p_table, p_loop_point));
}

void SiONDriver::set_voice(int p_index, const Ref<SiONVoice> &p_voice) {
	ERR_FAIL_COND_MSG(!p_voice->is_suitable_for_fm_voice(), "SiONDriver: Cannot register a voice that is not suitable to be an FM voice.");

	SiMMLRefTable::get_instance()->register_master_voice(p_index, p_voice);
}

void SiONDriver::clear_all_user_tables() {
	SiOPMRefTable::get_instance()->reset_all_user_tables();
	SiMMLRefTable::get_instance()->reset_all_user_tables();
}

SiMMLTrack *SiONDriver::create_user_controllable_track(int p_track_id) {
	int internal_track_id = (p_track_id & SiMMLTrack::TRACK_ID_FILTER) | SiMMLTrack::USER_CONTROLLED;

	return sequencer->create_controllable_track(internal_track_id, false);
}

void SiONDriver::notify_user_defined_track(int p_event_trigger_id, int p_note) {
	SiONTrackEvent *event = new SiONTrackEvent(SiONTrackEvent::USER_DEFINED, this, nullptr, sequencer->get_stream_writing_residue(), p_note, p_event_trigger_id);
	_track_event_queue.push_back(event);
}

// Background sound.

void SiONDriver::_set_background_sample(const Ref<SampleData> &p_sound) {
	_background_sample = p_sound;
	if (_background_sample.is_valid()) {
		_background_sample_data = new SiOPMWaveSamplerData(_background_sample, true);
	} else {
		_background_sample_data = Ref<SiOPMWaveSamplerData>();
	}

	if (_is_streaming) {
		_start_background_sample();
	}
}

void SiONDriver::set_background_sample(const Ref<SampleData> &p_sound, double p_mix_level, double p_loop_point) {
	set_background_sample_volume(p_mix_level);
	_background_loop_point = p_loop_point;
	_set_background_sample(p_sound);
}

void SiONDriver::clear_background_sample() {
	_background_loop_point = -1;
	_set_background_sample(Ref<SampleData>());
}

void SiONDriver::_start_background_sample() {
	int start_frame = 0;
	int end_frame = 0;

	// Currently fading out -> stop fade out track.
	if (_background_fade_out_track) {
		_background_fade_out_track->set_disposable();
		_background_fade_out_track->key_off(0, true);
		_background_fade_out_track = nullptr;
	}

	// Background sound is playing now -> fade out.
	if (_background_track) {
		_background_fade_out_track = _background_track;
		_background_track = nullptr;
		start_frame = 0;
	} else {
		start_frame = _background_fade_out_frames + _background_fade_gap_frames;
	}

	// Play sound with fade in.
	if (_background_sample_data.is_valid()) {
		_background_voice->set_wave_data(_background_sample_data);
		if (_background_loop_point != -1) {
			_background_sample_data->slice(-1, -1, _background_loop_point * 44100);
		}

		_background_track = sequencer->create_controllable_track(SiMMLTrack::DRIVER_BACKGROUND, false);
		_background_track->set_expression(128);
		_background_voice->update_track_voice(_background_track);
		_background_track->key_on(60, 0, (_background_fade_out_frames + _background_fade_gap_frames) * _buffer_length);

		end_frame = _background_total_fade_frames;
	} else {
		_background_voice->set_wave_data(Ref<SiOPMWaveBase>());
		_background_loop_point = -1;
		end_frame = _background_fade_out_frames + _background_fade_gap_frames;
	}

	// Set up the fader.
	if (end_frame - start_frame > 0) {
		_background_fader->set_fade(start_frame, end_frame, end_frame - start_frame);
	} else {
		// Stop fade out immediately.
		if (_background_fade_out_track) {
			_background_fade_out_track->set_disposable();
			_background_fade_out_track->key_off(0, true);
			_background_fade_out_track = nullptr;
		}
	}
}

void SiONDriver::_fade_background_callback(double p_value) {
	double fade_out = 0;
	double fade_in = 0;

	if (_background_fade_out_track) {
		if (_background_fade_out_frames > 0) {
			fade_out = 1.0 - p_value / _background_fade_out_frames;
			fade_out = std::clamp(fade_out, 0.0, 1.0);
		}

		_background_fade_out_track->set_expression(fade_out * 128);
	}

	if (_background_track) {
		if (_background_fade_in_frames > 0) {
			fade_in = 1.0 - (_background_total_fade_frames - p_value) / _background_fade_in_frames;
			fade_in = std::clamp(fade_in, 0.0, 1.0);
		} else {
			fade_in = 1.0;
		}

		_background_track->set_expression(fade_in * 128);
	}

	if (_background_fade_out_track && (fade_out == 0 || fade_in == 1)) {
		_background_fade_out_track->set_disposable();
		_background_fade_out_track->key_off(0, true);
		_background_fade_out_track = nullptr;
	}
}

double SiONDriver::get_background_sample_fade_out_time() const {
	return _background_fade_out_frames * _buffer_length / _sample_rate;
}

void SiONDriver::set_background_sample_fade_out_time(double p_time) {
	double ratio = _sample_rate / _buffer_length;

	_background_fade_out_frames = p_time * ratio;
	_background_total_fade_frames = _background_fade_out_frames + _background_fade_in_frames + _background_fade_gap_frames;
}

double SiONDriver::get_background_sample_fade_in_time() const {
	return _background_fade_in_frames * _buffer_length / _sample_rate;
}

void SiONDriver::set_background_sample_fade_in_times(double p_time) {
	double ratio = _sample_rate / _buffer_length;

	_background_fade_in_frames = p_time * ratio;
	_background_total_fade_frames = _background_fade_out_frames + _background_fade_in_frames + _background_fade_gap_frames;
}

double SiONDriver::get_background_sample_fade_gap_time() const {
	return _background_fade_gap_frames * _buffer_length / _sample_rate;
}

void SiONDriver::set_background_sample_fade_gap_time(double p_time) {
	double ratio = _sample_rate / _buffer_length;

	_background_fade_gap_frames = p_time * ratio;
	_background_total_fade_frames = _background_fade_out_frames + _background_fade_in_frames + _background_fade_gap_frames;
}

double SiONDriver::get_background_sample_volume() const {
	return _background_voice->get_channel_params()->get_master_volume(0);
}

void SiONDriver::set_background_sample_volume(double p_value) {
	_background_voice->get_channel_params()->set_master_volume(0, p_value);
	if (_background_track) {
		_background_track->set_master_volume(p_value * 128);
	}
	if (_background_fade_out_track) {
		_background_fade_out_track->set_master_volume(p_value * 128);
	}
}

// Sound parameters.

// Streaming only.
int SiONDriver::get_track_count() const {
	return sequencer->get_tracks().size();
}

int SiONDriver::get_max_track_count() const {
	return sequencer->get_max_track_count();
}

void SiONDriver::set_max_track_count(int p_value) {
	ERR_FAIL_COND_MSG(p_value < 1, "SiONDriver: Max track limit cannot be lower than 1.");

	sequencer->set_max_track_count(p_value);
}

double SiONDriver::get_volume() const {
	return _master_volume;
}

void SiONDriver::set_volume(double p_value) {
	ERR_FAIL_COND_MSG(p_value < 0 || p_value > 1, "SiONDriver: Volume must be between 0.0 and 1.0 (inclusive).");

	// Master volume is applied as a linear gain by render_chunk(). In the Godot version this
	// was delegated to the AudioStreamPlayer node.
	_master_volume = p_value;
}

double SiONDriver::get_bpm() const {
	return sequencer->get_effective_bpm();
}

void SiONDriver::set_bpm(double p_value) {
	// Scholars debate whether BPM even has the upper limit. At some point it definitely turns into tone for most people,
	// with no discernible beat in earshot. But having no limit at all for the API feels strange. Besides, we don't have
	// infinitely scalable performance. So as a compromise you can set the BPM to up to 4000 beats per minute.
	// You're welcome!
	ERR_FAIL_COND_MSG(p_value < 1 || p_value > 4000, "SiONDriver: BPM must be between 1 and 4000 (inclusive).");

	sequencer->set_effective_bpm(p_value);
}

// Streaming and rendering.

void SiONDriver::set_note_on_exception_mode(ExceptionMode p_mode) {
	ERR_FAIL_INDEX(p_mode, NEM_MAX);

	_note_on_exception_mode = p_mode;
}

double SiONDriver::get_streaming_position() const {
	return sequencer->get_processed_sample_count() * 1000.0 / _sample_rate;
}

void SiONDriver::set_start_position(double p_value) {
	_start_position = p_value;
	if (sequencer->is_ready_to_process()) {
		sequencer->reset_all_tracks();
		sequencer->process_dummy(_start_position * _sample_rate * 0.001);
	}
}

bool SiONDriver::_parse_system_command(const List<Ref<MMLSystemCommand>> &p_system_commands) {
	bool effect_set = false;

	for (const Ref<MMLSystemCommand> &command : p_system_commands) {
		if (command->command == "#EFFECT") {
			effect_set = true;
			effector->parse_global_effect_mml(command->number, command->content, command->postfix);
		} else if (command->command == "#WAVCOLOR" || command->command == "#WAVC") {
			uint32_t wave_color = command->content.hex_to_int();
			set_wave_table(command->number, TransformerUtil::wave_color_to_vector(wave_color));
		}
	}

	return effect_set;
}

void SiONDriver::_prepare_compile(sion::String p_mml, const Ref<SiONData> &p_data) {
	ERR_FAIL_COND(p_data.is_null());

	p_data->clear();
	_data = p_data;
	_mml_string = p_mml;
	sequencer->prepare_compile(_data, _mml_string);

	_job_progress = 0.01;
	_performance_stats.compiling_time = 0;
	_current_job_type = JobType::COMPILE;
}

void SiONDriver::_prepare_render(const Ref<SiONData> &p_data, int p_buffer_size, int p_buffer_channel_num, bool p_reset_effector) {
	_prepare_process(p_data, p_reset_effector);

	_render_buffer.clear();
	_render_buffer.resize(p_buffer_size); // TODO zeroed

	_render_buffer_channel_num = (p_buffer_channel_num == 2 ? 2 : 1);
	_render_buffer_size_max = p_buffer_size;
	_render_buffer_index = 0;

	_job_progress = 0.01;
	_performance_stats.rendering_time = 0;
	_current_job_type = JobType::RENDER;
}

bool SiONDriver::_rendering() {
	// Processing.
	sound_chip->begin_process();
	effector->begin_process();
	sequencer->process();
	effector->end_process();
	sound_chip->end_process();

	bool finished = false;

	// Limit the rendering length.
	int rendering_length = _buffer_length << 1;
	int buffer_extension = _buffer_length << (_render_buffer_channel_num - 1);

	if (_render_buffer_size_max != 0 && _render_buffer_size_max < (_render_buffer_index + buffer_extension)) {
		buffer_extension = _render_buffer_size_max - _render_buffer_index;
		finished = true;
	}

	// Extend the buffer.
	if (_render_buffer.size() < (_render_buffer_index + buffer_extension)) {
		_render_buffer.resize(_render_buffer_index + buffer_extension); // TODO zeroed
	}

	// Read the output.
	std::vector<double> *output_buffer = sound_chip->get_output_buffer_ptr();

	if (_render_buffer_channel_num == 2) {
		for (int i = 0, j = _render_buffer_index; i < rendering_length && j < _render_buffer.size(); i++, j++) {
			_render_buffer[j] = (*output_buffer)[i];
		}
	} else {
		for (int i = 0, j = _render_buffer_index; i < rendering_length && j < _render_buffer.size(); i += 2, j++) {
			_render_buffer[j] = (*output_buffer)[i];
		}
	}

	// Increment the index.
	_render_buffer_index += buffer_extension;

	return (finished || (_render_buffer_size_max == 0 && sequencer->is_finished()));
}

// Processes a single `_buffer_length`-frames block of audio and appends interleaved stereo
// samples (with master/fader volume applied) to p_block. This is the standalone replacement
// for the Godot-era `_streaming()` method; event dispatch, fader and auto-stop bookkeeping
// are all preserved.
void SiONDriver::_stream_block(std::vector<double> &r_block) {
	_in_streaming_process = true;

	int start_time = Time::get_singleton()->get_ticks_msec();
	_performance_stats.streaming_time = start_time;

	// Processing.
	sound_chip->begin_process();
	effector->begin_process();
	sequencer->process();
	effector->end_process();
	sound_chip->end_process();

	// Calculate an average processing time.

	const int frame_time = Time::get_singleton()->get_ticks_msec() - start_time;
	SinglyLinkedList<int>::Element *frame_record = _performance_stats.processing_time_data->get();
	_performance_stats.processing_time_data->next();

	_performance_stats.total_processing_time -= frame_record->value;
	frame_record->value = frame_time;
	_performance_stats.total_processing_time += frame_record->value;
	_performance_stats.update_average_processing_time();

	// Write samples. Master and fader volume replace the AudioStreamPlayer gain of the Godot version.
	const double gain = _master_volume * _fader_volume;
	std::vector<double> *output_buffer = sound_chip->get_output_buffer_ptr();
	for (int i = 0; i < (int)output_buffer->size(); i += 2) {
		r_block.push_back((*output_buffer)[i] * gain);
		r_block.push_back((*output_buffer)[i + 1] * gain);
	}

	// Dispatch events.
	if (_stream_event_enabled) {
		PackedVector2Array stream_buffer;
		for (int i = 0; i < (int)r_block.size(); i += 2) {
			stream_buffer.push_back(Vector2(r_block[i], r_block[i + 1]));
		}
		_dispatch_event(new SiONEvent(SiONEvent::STREAMING, this, stream_buffer));
	}
	if (!_is_finish_sequence_dispatched && sequencer->is_sequence_finished()) {
		_dispatch_event(new SiONEvent(SiONEvent::SEQUENCE_FINISHED, this));
		_is_finish_sequence_dispatched = true;
	}

	bool finished = false;
	if (_fader->execute()) {
		PackedVector2Array stream_buffer;
		for (int i = 0; i < (int)r_block.size(); i += 2) {
			stream_buffer.push_back(Vector2(r_block[i], r_block[i + 1]));
		}
		sion::String event_type = (_fader->is_incrementing() ? SiONEvent::FADE_IN_COMPLETED : SiONEvent::FADE_OUT_COMPLETED);
		_dispatch_event(new SiONEvent(event_type, this, stream_buffer));
		finished = !_fader->is_incrementing();
	} else {
		finished = sequencer->is_finished();
	}

	if (finished && _auto_stop) {
		stop();
	}

	_in_streaming_process = false;
}

void SiONDriver::render_chunk(float *p_buffer, int p_frames) {
	ERR_FAIL_NULL(p_buffer);
	if (p_frames <= 0) {
		return;
	}

	int remaining = p_frames * 2;
	float *out = p_buffer;

	while (remaining > 0) {
		if (_chunk_position >= _chunk_buffer.size()) {
			_chunk_buffer.clear();
			_chunk_position = 0;

			if (!_is_streaming || _is_paused || _suspend_streaming) {
				// Zero-fill when there is nothing to render. This mirrors the Godot behavior where
				// silence was pushed to the playback device instead of processing new frames.
				std::fill(p_buffer, p_buffer + p_frames * 2, 0.0f);
				return;
			}

			_stream_block(_chunk_buffer);
		}

		int available = (int)(_chunk_buffer.size() - _chunk_position);
		int to_copy = std::min(available, remaining);
		for (int i = 0; i < to_copy; i++) {
			out[i] = (float)_chunk_buffer[_chunk_position + i];
		}
		_chunk_position += to_copy;
		out += to_copy;
		remaining -= to_copy;

		// stop() may have been requested from the event callbacks above; do not render new blocks.
		if (!_is_streaming) {
			std::fill(out, p_buffer + p_frames * 2, 0.0f);
			break;
		}
	}
}

Ref<SiONData> SiONDriver::compile(sion::String p_mml) {
	stop();

	int start_time = Time::get_singleton()->get_ticks_msec();
	Ref<SiONData> temp_data;
	temp_data.instantiate();
	_prepare_compile(p_mml, temp_data);

	_job_progress = sequencer->compile(0); // 0 ensures that the process is completed within the same frame.
	_performance_stats.compiling_time = Time::get_singleton()->get_ticks_msec() - start_time;
	_mml_string = "";

	if (on_compilation_finished) {
		on_compilation_finished(_data);
	}
	return _data;
}

int SiONDriver::queue_compile(sion::String p_mml) {
	ERR_FAIL_COND_V_MSG(p_mml.empty(), _job_queue.size(), "SiONDriver: Cannot queue a compile task, the MML string is empty.");

	Ref<SiONData> sion_data;
	sion_data.instantiate();

	SiONDriverJob compile_job;
	compile_job.type = JobType::COMPILE;
	compile_job.data = sion_data;
	compile_job.mml_string = p_mml;
	compile_job.channel_num = 2;

	_job_queue.push_back(compile_job);
	return _job_queue.size();
}

PackedFloat64Array SiONDriver::render(const Ref<SiONData> &p_data, int p_buffer_size, int p_buffer_channel_num, bool p_reset_effector) {
	stop();

	int start_time = Time::get_singleton()->get_ticks_msec();
	_prepare_render(p_data, p_buffer_size, p_buffer_channel_num, p_reset_effector);

	while (true) { // Render everything.
		if (_rendering()) {
			break;
		}
	}
	_performance_stats.rendering_time = Time::get_singleton()->get_ticks_msec() - start_time;

	PackedFloat64Array buffer;
	for (double value : _render_buffer) {
		buffer.push_back(value);
	}

	if (on_render_finished) {
		on_render_finished(buffer);
	}
	return buffer;
}

PackedFloat64Array SiONDriver::render_mml(const sion::String &p_mml, int p_buffer_size, int p_buffer_channel_num, bool p_reset_effector) {
	Ref<SiONData> data = compile(p_mml);
	return render(data, p_buffer_size, p_buffer_channel_num, p_reset_effector);
}

int SiONDriver::queue_render(const Ref<SiONData> &p_data, int p_buffer_size, int p_buffer_channel_num, bool p_reset_effector) {
	ERR_FAIL_COND_V_MSG(p_data.is_null(), _job_queue.size(), "SiONDriver: Cannot queue a render task, the data object is empty.");
	ERR_FAIL_COND_V_MSG(p_buffer_size <= 0, _job_queue.size(), "SiONDriver: Cannot queue a render task, the buffer size must be a positive number.");

	SiONDriverJob render_job;
	render_job.type = JobType::RENDER;
	render_job.data = p_data;
	render_job.buffer_size = p_buffer_size;
	render_job.channel_num = p_buffer_channel_num;
	render_job.reset_effector = p_reset_effector;

	_job_queue.push_back(render_job);
	return _job_queue.size();
}

int SiONDriver::queue_render(const sion::String &p_mml, int p_buffer_size, int p_buffer_channel_num, bool p_reset_effector) {
	ERR_FAIL_COND_V_MSG(p_mml.empty(), _job_queue.size(), "SiONDriver: Cannot queue a render task, the MML string is empty.");
	ERR_FAIL_COND_V_MSG(p_buffer_size <= 0, _job_queue.size(), "SiONDriver: Cannot queue a render task, the buffer size must be a positive number.");

	// Data is shared between the two tasks.
	Ref<SiONData> sion_data;
	sion_data.instantiate();

	// Queue compilation first.
	SiONDriverJob compile_job;
	compile_job.type = JobType::COMPILE;
	compile_job.data = sion_data;
	compile_job.mml_string = p_mml;
	compile_job.channel_num = 2;

	_job_queue.push_back(compile_job);

	return queue_render(sion_data, p_buffer_size, p_buffer_channel_num, p_reset_effector);
}

// Playback.

void SiONDriver::_prepare_stream(const Ref<SiONData> &p_data, bool p_reset_effector) {
	_prepare_process(p_data, p_reset_effector);

	_performance_stats.total_processing_time = 0;
	_performance_stats.processing_time_data->reset();

	_chunk_buffer.clear();
	_chunk_position = 0;

	_is_paused = false;
	_is_finish_sequence_dispatched = p_data.is_null();

	// Start streaming.
	_is_streaming = true;
	_suspend_streaming = true;

	_set_processing_immediate();
}

void SiONDriver::stream(bool p_reset_effector) {
	stop();
	_prepare_stream(Ref<SiONData>(), p_reset_effector);
}

void SiONDriver::play(const Ref<SiONData> &p_data, bool p_reset_effector) {
	stop();
	_prepare_stream(p_data, p_reset_effector);
}

void SiONDriver::play_mml(const sion::String &p_mml, bool p_reset_effector) {
	Ref<SiONData> data = compile(p_mml);
	play(data, p_reset_effector);
}

void SiONDriver::stop() {
	if (!_is_streaming) {
		return;
	}
	if (_in_streaming_process) {
		_preserve_stop = true;
		return;
	}

	_preserve_stop = false;
	_is_paused = false;
	_is_streaming = false;

	clear_data(); // Original SiON doesn't do that, but that seems like an oversight.
	clear_background_sample();
	_clear_processing();

	_fader->stop();
	_fader_volume = 1;

	_chunk_buffer.clear();
	_chunk_position = 0;

	sequencer->stop_sequence();

	_dispatch_event(new SiONEvent(SiONEvent::STREAM_STOPPED, this));

	_performance_stats.streaming_latency = 0;
}

void SiONDriver::reset() {
	sequencer->reset_all_tracks();
}

void SiONDriver::pause() {
	if (_is_streaming) {
		_is_paused = true;
	}
}

void SiONDriver::resume() {
	_is_paused = false;
}

SiMMLTrack *SiONDriver::_find_or_create_track(int p_track_id, double p_delay, double p_quant, bool p_disposable, int *r_delay_samples) {
	ERR_FAIL_COND_V_MSG(p_delay < 0, nullptr, "SiONDriver: Playback delay cannot be less than zero.");

	int internal_track_id = (p_track_id & SiMMLTrack::TRACK_ID_FILTER) | SiMMLTrack::DRIVER_NOTE;
	double delay_samples = sequencer->calculate_sample_delay(0, p_delay, p_quant);

	SiMMLTrack *track = nullptr;

	// Check track ID conflicts.
	if (_note_on_exception_mode != NEM_IGNORE) {
		// Find a track with the same sound timings.
		track = sequencer->find_active_track(internal_track_id, delay_samples);

		if (track && _note_on_exception_mode == NEM_REJECT) {
			return nullptr;
		}
		if (track && _note_on_exception_mode == NEM_SHIFT) {
			int step = sequencer->calculate_sample_length(p_quant);
			while (track) {
				delay_samples += step;
				track = sequencer->find_active_track(internal_track_id, delay_samples);
			}
		}
	}

	*r_delay_samples = delay_samples;

	if (track) {
		return track;
	}

	track = sequencer->create_controllable_track(internal_track_id, p_disposable);
	ERR_FAIL_NULL_V_MSG(track, nullptr, "SiONDriver: Failed to allocate a track for playback. Pushing the limits?");
	return track;
}

SiMMLTrack *SiONDriver::sample_on(int p_sample_number, double p_length, double p_delay, double p_quant, int p_track_id, bool p_disposable) {
	ERR_FAIL_COND_V_MSG(!_is_streaming, nullptr, "SiONDriver: Driver is not streaming, you must call SiONDriver.stream() first.");
	ERR_FAIL_COND_V_MSG(p_length < 0, nullptr, "SiONDriver: Sample length cannot be less than zero.");

	int delay_samples = 0;
	SiMMLTrack *track = _find_or_create_track(p_delay, p_quant, p_track_id, p_disposable, &delay_samples);
	if (!track) {
		return nullptr;
	}

	track->set_channel_module_type(SiONModuleType::MODULE_SAMPLE, 0);
	track->key_on(p_sample_number, _convert_event_length(p_length), delay_samples);

	return track;
}

SiMMLTrack *SiONDriver::note_on(int p_note, const Ref<SiONVoice> &p_voice, double p_length, double p_delay, double p_quant, int p_track_id, bool p_disposable) {
	ERR_FAIL_COND_V_MSG(!_is_streaming, nullptr, "SiONDriver: Driver is not streaming, you must call SiONDriver.stream() first.");
	ERR_FAIL_COND_V_MSG(p_length < 0, nullptr, "SiONDriver: Note length cannot be less than zero.");

	int delay_samples = 0;
	SiMMLTrack *track = _find_or_create_track(p_delay, p_quant, p_track_id, p_disposable, &delay_samples);
	if (!track) {
		return nullptr;
	}

	if (p_voice.is_valid()) {
		p_voice->update_track_voice(track);
	}
	track->key_on(p_note, _convert_event_length(p_length), delay_samples);

	return track;
}

SiMMLTrack *SiONDriver::note_on_with_bend(int p_note, int p_note_to, double p_bend_length, const Ref<SiONVoice> &p_voice, double p_length, double p_delay, double p_quant, int p_track_id, bool p_disposable) {
	ERR_FAIL_COND_V_MSG(!_is_streaming, nullptr, "SiONDriver: Driver is not streaming, you must call SiONDriver.stream() first.");
	ERR_FAIL_COND_V_MSG(p_length < 0, nullptr, "SiONDriver: Note length cannot be less than zero.");
	ERR_FAIL_COND_V_MSG(p_bend_length < 0, nullptr, "SiONDriver: Pitch bending length cannot be less than zero.");

	int delay_samples = 0;
	SiMMLTrack *track = _find_or_create_track(p_delay, p_quant, p_track_id, p_disposable, &delay_samples);
	if (!track) {
		return nullptr;
	}

	if (p_voice.is_valid()) {
		p_voice->update_track_voice(track);
	}
	track->key_on(p_note, _convert_event_length(p_length), delay_samples);
	track->bend_note(p_note_to, _convert_event_length(p_bend_length));

	return track;
}

std::vector<SiMMLTrack *> SiONDriver::note_off(int p_note, int p_track_id, double p_delay, double p_quant, bool p_stop_immediately) {
	ERR_FAIL_COND_V_MSG(!_is_streaming, std::vector<SiMMLTrack *>(), "SiONDriver: Driver is not streaming, you must call SiONDriver.stream() first.");
	ERR_FAIL_COND_V_MSG(p_delay < 0, std::vector<SiMMLTrack *>(), "SiONDriver: Note off delay cannot be less than zero.");

	int internal_track_id = (p_track_id & SiMMLTrack::TRACK_ID_FILTER) | SiMMLTrack::DRIVER_NOTE;
	int delay_samples = sequencer->calculate_sample_delay(0, p_delay, p_quant);

	std::vector<SiMMLTrack *> tracks;
	for (SiMMLTrack *track : sequencer->get_tracks()) {
		if (track->get_internal_track_id() != internal_track_id) {
			continue;
		}

		if (p_note == -1 || (p_note == track->get_note() && track->get_channel()->is_note_on())) {
			track->key_off(delay_samples, p_stop_immediately);
			tracks.push_back(track);
		} else if (track->get_executor()->get_waiting_note() == p_note) {
			// This track is waiting for this note to start.
			track->key_on(p_note, 1, delay_samples);
			tracks.push_back(track);
		}
	}

	return tracks;
}

std::vector<SiMMLTrack *> SiONDriver::sequence_on(const Ref<SiONData> &p_data, const Ref<SiONVoice> &p_voice, double p_length, double p_delay, double p_quant, int p_track_id, bool p_disposable) {
	ERR_FAIL_COND_V(p_data.is_null(), std::vector<SiMMLTrack *>());
	ERR_FAIL_COND_V_MSG(p_length < 0, std::vector<SiMMLTrack *>(), "SiONDriver: Sequence length cannot be less than zero.");
	ERR_FAIL_COND_V_MSG(p_delay < 0, std::vector<SiMMLTrack *>(), "SiONDriver: Sequence delay cannot be less than zero.");

	int internal_track_id = (p_track_id & SiMMLTrack::TRACK_ID_FILTER) | SiMMLTrack::DRIVER_SEQUENCE;
	int delay_samples = sequencer->calculate_sample_delay(0, p_delay, p_quant);
	int length_samples = sequencer->calculate_sample_length(p_length);

	std::vector<SiMMLTrack *> tracks;

	MMLSequence *sequence = p_data->get_sequence_group()->get_head_sequence();
	while (sequence) {
		if (sequence->is_active()) {
			SiMMLTrack *track = sequencer->create_controllable_track(internal_track_id, p_disposable);
			ERR_FAIL_NULL_V_MSG(track, tracks, "SiONDriver: Failed to allocate a track for playback. Pushing the limits?");

			track->sequence_on(p_data, sequence, length_samples, delay_samples);
			if (p_voice.is_valid()) {
				p_voice->update_track_voice(track);
			}

			tracks.push_back(track);
		}

		sequence = sequence->get_next_sequence();
	}

	return tracks;
}

std::vector<SiMMLTrack *> SiONDriver::sequence_off(int p_track_id, double p_delay, double p_quant, bool p_stop_with_reset) {
	ERR_FAIL_COND_V_MSG(p_delay < 0, std::vector<SiMMLTrack *>(), "SiONDriver: Sequence off delay cannot be less than zero.");

	int internal_track_id = (p_track_id & SiMMLTrack::TRACK_ID_FILTER) | SiMMLTrack::DRIVER_SEQUENCE;
	int delay_samples = sequencer->calculate_sample_delay(0, p_delay, p_quant);

	std::vector<SiMMLTrack *> tracks;
	for (SiMMLTrack *track : sequencer->get_tracks()) {
		if (track->get_internal_track_id() != internal_track_id) {
			continue;
		}

		track->sequence_off(delay_samples, p_stop_with_reset);
		tracks.push_back(track);
	}

	return tracks;
}

void SiONDriver::_fade_callback(double p_value) {
	_fader_volume = p_value;

	if (!_fading_event_enabled) {
		return;
	}

	_dispatch_event(new SiONEvent(SiONEvent::FADING, this));
}

void SiONDriver::fade_in(double p_time) {
	_fader->set_fade(0, 1, p_time * _sample_rate / _buffer_length);
}

void SiONDriver::fade_out(double p_time) {
	_fader->set_fade(1, 0, p_time * _sample_rate / _buffer_length);
}

// Processing.

void SiONDriver::_set_processing_queue() {
	ERR_FAIL_COND_MSG(_current_frame_processing != FrameProcessingType::NONE, vformat("SiONDriver: Cannot begin processing the queue, driver is busy (%d).", _current_frame_processing));
	_current_frame_processing = FrameProcessingType::PROCESSING_QUEUE;
}

void SiONDriver::_set_processing_immediate() {
	ERR_FAIL_COND_MSG(_current_frame_processing != FrameProcessingType::NONE, vformat("SiONDriver: Cannot begin immediate processing, driver is busy (%d).", _current_frame_processing));
	_current_frame_processing = FrameProcessingType::PROCESSING_IMMEDIATE;

	_performance_stats.frame_timestamp = Time::get_singleton()->get_ticks_msec();
}

void SiONDriver::_clear_processing() {
	_current_frame_processing = FrameProcessingType::NONE;
}

void SiONDriver::_prepare_process(const Ref<SiONData> &p_data, bool p_reset_effector) {
	if (p_data.is_valid()) {
		// Passing a null Ref keeps the previously compiled data, mirroring the old Variant-based
		// nil/string/object discrimination.
		_data = p_data;
	}

	// Order of operations below is critical.

	sound_chip->initialize(_channel_num, _bitrate, _buffer_length);  // Initialize DSP.
	sound_chip->reset();                                             // Reset all channels.

	if (p_reset_effector) {                                          // Initialize or reset effectors.
		effector->initialize();
	} else {
		effector->reset();
	}

	sequencer->prepare_process(_data, _sample_rate, _buffer_length); // Set sequencer tracks (should be called after sound_chip::reset()).
	if (_data.is_valid()) {
		_parse_system_command(_data->get_system_commands());         // Parse #EFFECT command (should be called after effector::reset()).
	}

	effector->prepare_process();                                     // Set effector connections.
	_track_event_queue.clear();                                      // Clear event queue.

	//

	// Set position if we don't start from the top.
	if (_data.is_valid() && _start_position > 0) {
		sequencer->process_dummy(_start_position * _sample_rate * 0.001);
	}

	if (_background_sample_data.is_valid()) {
		_start_background_sample();
	}

	if (_timer_interval_event->get_length() > 0) {
		sequencer->set_global_sequence(_timer_sequence);
	}
}

void SiONDriver::update() {
	if (_current_frame_processing != FrameProcessingType::NONE) {
		_process_frame();
	}
}

void SiONDriver::_process_frame() {
	switch (_current_frame_processing) {
		case FrameProcessingType::PROCESSING_QUEUE: {
			_process_frame_queue();
		} break;

		case FrameProcessingType::PROCESSING_IMMEDIATE: {
			_process_frame_immediate();
		} break;

		default: break; // Silences enum warnings.
	}
}

void SiONDriver::_process_frame_queue() {
	int start_time = Time::get_singleton()->get_ticks_msec();

	switch (_current_job_type) {
		case JobType::COMPILE: {
			_job_progress = sequencer->compile(_queue_interval);
			_performance_stats.compiling_time += Time::get_singleton()->get_ticks_msec() - start_time;
		} break;

		case JobType::RENDER: {
			_job_progress += (1 - _job_progress) * 0.5; // I guess we can't correctly track this?

			int rendering_time = Time::get_singleton()->get_ticks_msec() - start_time;
			while (rendering_time <= _queue_interval) {
				if (_rendering()) {
					_job_progress = 1;
					break;
				}

				rendering_time = Time::get_singleton()->get_ticks_msec() - start_time;
			}

			_performance_stats.rendering_time += Time::get_singleton()->get_ticks_msec() - start_time;
		} break;

		default: break; // Silences enum warnings.
	}

	// Finish the job and prepare the next one.
	if (_job_progress == 1) {
		switch (_current_job_type) {
			case JobType::COMPILE: {
				if (on_compilation_finished) {
					on_compilation_finished(_data);
				}
			} break;

			case JobType::RENDER: {
				if (on_render_finished) {
					PackedFloat64Array buffer;
					for (double value : _render_buffer) {
						buffer.push_back(value);
					}
					on_render_finished(buffer);
				}
			} break;

			default: break; // Silences enum warnings.
		}

		if (_prepare_next_job()) {
			return; // Queue is finished.
		}
	}

	_dispatch_event(new SiONEvent(SiONEvent::QUEUE_EXECUTING, this));
}

void SiONDriver::_process_frame_immediate() {
	// Calculate the framerate.
	int t = Time::get_singleton()->get_ticks_msec();
	_performance_stats.frame_rate = t - _performance_stats.frame_timestamp;
	_performance_stats.frame_timestamp = t;

	// This is true at the start of streaming.
	if (_suspend_streaming) {
		_suspend_streaming = false;

		// In the original code this event is cancellable and this means users can
		// react to it to trigger an immediate stop to streaming. If this is needed
		// in this implementation, you can just call stop() while reacting to the callback.
		_dispatch_event(new SiONEvent(SiONEvent::STREAM_STARTED, this));
		return;
	}

	if (_preserve_stop) {
		stop();
	}

	// Process events and keep the ones which are still remaining.
	if (_track_event_queue.size() > 0) {
		List<Ref<SiONTrackEvent>> remaining_events;
		for (const Ref<SiONTrackEvent> &event : _track_event_queue) {
			if (event->decrement_timer(_performance_stats.frame_rate)) {
				_dispatch_event(event);
				continue;
			}

			remaining_events.push_back(event);
		}

		_track_event_queue = remaining_events;
	}
}

bool SiONDriver::_prepare_next_job() {
	_data = Ref<SiONData>();
	_mml_string = "";

	_current_job_type = JobType::NO_JOB;
	if (_job_queue.size() == 0) {
		_queue_length = 0;
		_clear_processing();

		_dispatch_event(new SiONEvent(SiONEvent::QUEUE_COMPLETED, this));
		return true; // Finished.
	}

	SiONDriverJob job = _job_queue.front()->get();
	_job_queue.pop_front();

	switch (job.type) {
		case JobType::COMPILE: {
			if (job.mml_string.empty()) {
				WARN_PRINT("SiONDriver: Invalid compile job queued up, missing MML string.");
				return _prepare_next_job(); // Skip this job.
			}

			_prepare_compile(job.mml_string, job.data);
		} break;

		case JobType::RENDER: {
			if (job.buffer_size <= 0) {
				WARN_PRINT("SiONDriver: Invalid render job queued up, buffer size must be a positive number.");
				return _prepare_next_job(); // Skip this job;
			}

			_prepare_render(job.data, job.buffer_size, job.channel_num, job.reset_effector);
		} break;

		default: {
			WARN_PRINT("SiONDriver: Unknown job queued up.");
			return _prepare_next_job(); // Skip this job.
		} break;
	}

	return false; // Not finished yet.
}

void SiONDriver::_cancel_all_jobs() {
	_data = Ref<SiONData>();
	_mml_string = "";

	_current_job_type = JobType::NO_JOB;
	_job_progress = 0;
	_job_queue.clear();
	_queue_length = 0;
	_clear_processing();

	_dispatch_event(new SiONEvent(SiONEvent::QUEUE_CANCELLED, this));
}

double SiONDriver::get_queue_total_progress() const {
	if (_queue_length == 0) {
		return 1.0;
	}
	if (_queue_length == _job_queue.size()) {
		return 0.0;
	}

	return (_queue_length - _job_queue.size() - 1.0 + _job_progress) / _queue_length;
}

int SiONDriver::get_queue_length() const {
	return _job_queue.size();
}

bool SiONDriver::is_queue_executing() const {
	return (_job_progress > 0 && _job_progress < 1);
}

int SiONDriver::start_queue(int p_interval) {
	stop();

	_queue_length = _job_queue.size();
	if (_queue_length > 0) {
		_queue_interval = p_interval;
		_prepare_next_job();
		_set_processing_queue();
	}

	return _queue_length;
}

// Events.

double SiONDriver::_convert_event_length(double p_length) const {
	// Driver methods expect length in 1/16ths of a beat. The event length is in resolution units.
	// Note: In the original implementation this was mistakenly interpreted as 1/16th of
	// the sequencer's note resolution, which only translated to 1/4th of the beat.

	double beat_resolution = (double)sequencer->get_parser_settings()->resolution / 4.0;
	return p_length * beat_resolution * 0.0625;
}

void SiONDriver::_dispatch_event(const Ref<SiONEvent> &p_event) {
	// This method exists as a proxy. Original implementation relied on native events and signals,
	// whereas we route everything through the single on_event callback. Event objects are plain
	// data carriers; their type string identifies what happened.

	sion::String signal_name = p_event->get_event_type();
	ERR_FAIL_COND(signal_name.empty());

	if (on_event) {
		on_event(p_event);
	}
}

void SiONDriver::_note_on_callback(SiMMLTrack *p_track) {
	_publish_note_event(p_track, p_track->get_event_trigger_type_on(), SiONTrackEvent::NOTE_ON_FRAME, SiONTrackEvent::NOTE_ON_STREAM);
}

void SiONDriver::_note_off_callback(SiMMLTrack *p_track) {
	_publish_note_event(p_track, p_track->get_event_trigger_type_off(), SiONTrackEvent::NOTE_OFF_FRAME, SiONTrackEvent::NOTE_OFF_STREAM);
}

void SiONDriver::_publish_note_event(SiMMLTrack *p_track, int p_type, sion::String p_frame_event, sion::String p_stream_event) {
	// Frame event; dispatch later.
	if (p_type & 1) {
		Ref<SiONTrackEvent> event = new SiONTrackEvent(p_frame_event, this, p_track);
		_track_event_queue.push_back(event);
		return;
	}

	// Stream event; dispatch immediately.
	if (p_type & 2) {
		Ref<SiONTrackEvent> event = new SiONTrackEvent(p_stream_event, this, p_track);
		_dispatch_event(event);
		return;
	}
}

void SiONDriver::_tempo_changed_callback(int p_buffer_index, bool p_dummy) {
	Ref<SiONTrackEvent> event = new SiONTrackEvent(SiONTrackEvent::BPM_CHANGED, this, nullptr, p_buffer_index);

	if (p_dummy && _notify_change_bpm_on_position_changed) {
		_dispatch_event(event);
	} else {
		_track_event_queue.push_back(event);
	}
}

void SiONDriver::_beat_callback(int p_buffer_index, int p_beat_counter) {
	if (!_beat_event_enabled) {
		return;
	}

	Ref<SiONTrackEvent> event = new SiONTrackEvent(SiONTrackEvent::STREAMING_BEAT, this, nullptr, p_buffer_index, 0, p_beat_counter);
	_track_event_queue.push_back(event);
}

void SiONDriver::set_beat_callback_interval(double p_length_16th) {
	ERR_FAIL_COND_MSG(p_length_16th < 0, "SiONDriver: Beat callback interval value cannot be less than zero.");

	int filter = 1;
	double length = p_length_16th;

	while (length > 1.5) {
		filter <<= 1;
		length *= 0.5;
	}

	sequencer->set_beat_callback_filter(filter - 1);
}

void SiONDriver::set_timer_interval(double p_length) {
	ERR_FAIL_COND_MSG(p_length < 0, "SiONDriver: Timer interval value cannot be less than zero.");

	_timer_interval_event->set_length(_convert_event_length(p_length));

	if (p_length > 0) {
		sequencer->set_timer_callback([this]() {
			if (on_timer_interval) {
				on_timer_interval();
			}
		});
	} else {
		sequencer->set_timer_callback(nullptr);
	}
}

//

SiONDriver *SiONDriver::create(int p_buffer_length, int p_channel_num, int p_sample_rate, int p_bitrate) {
	return new SiONDriver(p_buffer_length, p_channel_num, p_sample_rate, p_bitrate);
}

SiONDriver::SiONDriver(int p_buffer_length, int p_channel_num, int p_sample_rate, int p_bitrate) {
	ERR_FAIL_COND_MSG(!_allow_multiple_drivers && _mutex, "SiONDriver: Only one driver instance is allowed.");
	_mutex = this;

	ERR_FAIL_COND_MSG((p_buffer_length != 2048 && p_buffer_length != 4096 && p_buffer_length != 8192), "SiONDriver: Buffer length can only be 2048, 4096, or 8192.");
	ERR_FAIL_COND_MSG((p_channel_num != 1 && p_channel_num != 2), "SiONDriver: Channel number can only be 1 (mono) or 2 (stereo).");
	ERR_FAIL_COND_MSG((p_sample_rate != 44100), "SiONDriver: Sampling rate can only be 44100.");

	sound_chip = new SiOPMSoundChip;
	effector = new SiEffector(sound_chip);
	sequencer = new SiMMLSequencer(sound_chip);
	sequencer->set_note_on_callback([this](SiMMLTrack *p_track) { _note_on_callback(p_track); });
	sequencer->set_note_off_callback([this](SiMMLTrack *p_track) { _note_off_callback(p_track); });
	sequencer->set_tempo_changed_callback([this](int p_buffer_index, bool p_dummy) { _tempo_changed_callback(p_buffer_index, p_dummy); });
	sequencer->set_beat_callback([this](int p_buffer_index, int p_beat_counter) { _beat_callback(p_buffer_index, p_beat_counter); });

	// Main sound.
	{
		_fader = new FaderUtil;
		_fader->set_callback([this](double p_value) { _fade_callback(p_value); });
	}

	// Background sound.
	{
		_background_voice = new SiONVoice(SiONModuleType::MODULE_SAMPLE);
		_background_voice->set_update_volumes(true);
		_background_fader = new FaderUtil;
		_background_fader->set_callback([this](double p_value) { _fade_background_callback(p_value); });
	}

	// FIXME: Implement SMF/MIDI support.
	//_midi_module = new MIDIModule;
	//_midi_converter = new SiONDataConverterSMF(nullptr, _midi_module);

	{
		_buffer_length = p_buffer_length;
		_channel_num = p_channel_num;
		_sample_rate = p_sample_rate;
		_bitrate = p_bitrate;
	}

	{
		_timer_sequence = new MMLSequence;
		_timer_sequence->initialize();
		_timer_sequence->append_new_event(MMLEvent::REPEAT_ALL, 0);
		_timer_sequence->append_new_event(MMLEvent::TIMER, 0);
		_timer_interval_event = _timer_sequence->append_new_event(MMLEvent::GLOBAL_WAIT, 0, 0);
	}

	_performance_stats.processing_time_data = new SinglyLinkedList<int>(TIME_AVERAGING_COUNT, 0, true);
	_performance_stats.total_processing_time_ratio = _sample_rate / (_buffer_length * TIME_AVERAGING_COUNT);
}

SiONDriver::~SiONDriver() {
	if (_is_streaming) {
		_in_streaming_process = false; // Force-clean shutdown, bypass the deferred stop guard.
		stop();
	}

	if (_mutex == this) {
		_mutex = nullptr;
	}

	_timer_interval_event = nullptr;
	delete _timer_sequence;

	delete _fader;
	delete _background_fader;

	delete sequencer;
	delete effector;
	delete sound_chip;
}
