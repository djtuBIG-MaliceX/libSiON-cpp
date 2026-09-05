/***************************************************/
/* Part of GDSiON software synthesizer             */
/* Copyright (c) 2024 Yuri Sizov and contributors  */
/* Provided under MIT                              */
/***************************************************/

#include "sion_track_event.h"

#include "sion_data.h"
#include "sion_driver.h"
#include "chip/channels/siopm_channel_base.h"
#include "sequencer/simml_track.h"
#include "sequencer/simml_sequencer.h"

const char *SiONTrackEvent::NOTE_ON_STREAM = "note_on_stream";
const char *SiONTrackEvent::NOTE_OFF_STREAM = "note_off_stream";
const char *SiONTrackEvent::NOTE_ON_FRAME = "note_on_frame";
const char *SiONTrackEvent::NOTE_OFF_FRAME = "note_off_frame";
const char *SiONTrackEvent::STREAMING_BEAT = "streaming_beat";
const char *SiONTrackEvent::BPM_CHANGED = "bpm_changed";
const char *SiONTrackEvent::USER_DEFINED = "user_defined_event";

bool SiONTrackEvent::decrement_timer(int p_frame_rate) {
	_frame_trigger_timer -= p_frame_rate;
	return (_frame_trigger_timer <= 0);
}

//

SiONTrackEvent::SiONTrackEvent(sion::String p_type, SiONDriver *p_driver, SiMMLTrack *p_track, int p_buffer_index, int p_note, int p_event_trigger_id) :
		SiONEvent(p_type, p_driver) {
	_track = p_track;

	if (_track) {
		_note = _track->get_note();
		_event_trigger_id = _track->get_event_trigger_id();
		_buffer_index = _track->get_channel()->get_buffer_index();
	} else {
		_note = p_note;
		_event_trigger_id = p_event_trigger_id;
		_buffer_index = p_buffer_index;
	}

	_frame_trigger_delay = (double)_buffer_index / p_driver->get_sequencer()->get_sample_rate() + p_driver->get_streaming_latency();
	_frame_trigger_timer = _frame_trigger_delay;
}
