/*
 * Part of libSiON-cpp, forked from GDSiON software synthesizer
 * Copyright (c) 2024-2026 Yuri Sizov, James Alan Nguyen and contributors
 * Based on SiON Flash Software Synthesizer (C) 2008-2016 keim_at_Si
 * Provided under MIT License.
 */

// C ABI facade over SiONDriver for the Emscripten/WASM build. The page-side
// engine (sion_engine.js) drives this API: init once, play(mml), then call
// update() + render(frames) from every audio quantum and read back the static
// interleaved-stereo buffer (fixed address, safe across memory growth).
// sion_web_capture_channels() additionally refreshes a fixed-address
// snapshot of the per-channel visualizer state (see VIS_* layout below,
// mirrored in sion_engine.js / index.html).

#include "sion_core.h"
#include "sion_driver.h"
#include "chip/channels/siopm_channel_base.h"
#include "sequencer/simml_sequencer.h"
#include "sequencer/simml_track.h"

#include <algorithm>
#include <cstdint>
#include <string>

#ifdef _WIN32
#define SION_WEB_EXPORT __declspec(dllexport)
#else
#define SION_WEB_EXPORT __attribute__((visibility("default")))
#endif

namespace {

SiONDriver *g_driver = nullptr;
std::string g_last_error;

// Render output with a fixed address (static segment), so JS can keep a plain
// byte offset into the WASM heap without re-querying after memory growth.
constexpr int RENDER_BUFFER_MAX_FRAMES = 8192;
float g_render_buffer[RENDER_BUFFER_MAX_FRAMES * 2];

// ---- Channel telemetry for the page visualizer -------------------------------
// Row = one hardware channel: MML tracks routed to the same (%module, %number)
// target by the `%` command share a row, so a polyphonic row lists every
// sounding note at once (macros/event triggers that stack notes on one channel
// show up as multiple keys). Channels without an explicit number are
// auto-allocated and always get their own row. Rows are capped at
// VIS_MAX_CHANNELS; anything above is ignored by the page.

constexpr int VIS_MAX_CHANNELS = 26;
constexpr int VIS_NOTES_PER_ROW = 8;
// int32 layout per row: channel_type, channel_number, module_type, program,
// note_count, reserved; then per note (up to VIS_NOTES_PER_ROW): pitch (pitch
// index in 1/64 semitones, live - includes bend/portamento/note envelopes),
// volume (permille of track output level), pan (0..127), flags, sweep target
// (pitch index, valid while SWEEP flag is set). Mirrored in sion_engine.js.
constexpr int VIS_ROW_INTS = 6 + VIS_NOTES_PER_ROW * 5;

enum VisNoteFlag {
	VIS_FLAG_KEY_ON = 1 << 0,
	VIS_FLAG_SWEEP = 1 << 1,  // portamento (`po`) / pitch-bend (`*`) sweep in flight
	VIS_FLAG_MUTE = 1 << 2,
	VIS_FLAG_AUDIBLE = 1 << 3, // channel not idling (includes release tails)
};

struct VisNote {
	int32_t pitch = 0;
	int32_t volume = 0;
	int32_t pan = 64;
	int32_t flags = 0;
	int32_t sweep_target = 0;
};

struct VisRow {
	int32_t channel_type = -1; // SiOPMChannelManager::ChannelType
	int32_t channel_number = -1; // explicit %number, -1 = auto-allocated
	int32_t module_type = -1; // SiONModuleType
	int32_t program = 0; // tone/voice index (program number)
	int order = 0; // insertion order, tie-break for sorting
	int note_count = 0;
	VisNote notes[VIS_NOTES_PER_ROW];
};

int32_t g_channel_rows[VIS_MAX_CHANNELS * VIS_ROW_INTS];

} // namespace

extern "C" {

SION_WEB_EXPORT int sion_web_init(int p_sample_rate, int p_buffer_length) {
	if (g_driver != nullptr) {
		return 1;
	}
	sion::initialize();
	g_driver = SiONDriver::create(p_buffer_length, 2, p_sample_rate, 0);
	if (g_driver == nullptr) {
		return 0;
	}
	// Sequence end stops the stream so the page can report "finished" and free
	// CPU; the JS side replays on demand (loop mode included).
	g_driver->set_auto_stop(true);
	return 1;
}

SION_WEB_EXPORT int sion_web_play(const char *p_mml) {
	if (g_driver == nullptr) {
		return 0;
	}
	g_last_error.clear();

	const sion::ErrorOutput ErrorSink = sion::error_output();
	sion::error_output() = [](const std::string &p_output) {
		g_last_error += p_output;
	};
	g_driver->play_mml(p_mml != nullptr ? p_mml : "");
	sion::error_output() = ErrorSink;

	return 1;
}

SION_WEB_EXPORT void sion_web_stop() {
	if (g_driver != nullptr) {
		g_driver->stop();
	}
}

SION_WEB_EXPORT void sion_web_update() {
	if (g_driver != nullptr) {
		g_driver->update();
	}
}

// Renders p_frames interleaved stereo frames into the static buffer.
SION_WEB_EXPORT int sion_web_render(int p_frames) {
	if (g_driver == nullptr || p_frames <= 0) {
		return 0;
	}
	const int frames = p_frames > RENDER_BUFFER_MAX_FRAMES ? RENDER_BUFFER_MAX_FRAMES : p_frames;
	g_driver->render_chunk(g_render_buffer, frames);
	return frames;
}

// Byte address of the static render buffer (interleaved float32, stereo).
SION_WEB_EXPORT const float *sion_web_buffer() {
	return g_render_buffer;
}

SION_WEB_EXPORT int sion_web_is_streaming() {
	return (g_driver != nullptr && g_driver->is_streaming()) ? 1 : 0;
}

SION_WEB_EXPORT void sion_web_set_volume(double p_value) {
	if (g_driver != nullptr) {
		g_driver->set_volume(p_value);
	}
}

// Captured error output of the last play() call (Godot-formatted lines).
SION_WEB_EXPORT const char *sion_web_last_error() {
	return g_last_error.c_str();
}

// Rebuilds the visualizer snapshot from the live sequencer tracks. Returns the
// number of rows written to sion_web_channel_data() (capped at 26). Read-only
// with respect to DSP state; call once per animation frame from the page.
SION_WEB_EXPORT int sion_web_capture_channels() {
	// Rows represent live playback only: when not streaming, sequencer tracks may
	// still linger (freed on the next play), so report an empty visualizer.
	if (g_driver == nullptr || g_driver->get_sequencer() == nullptr || !g_driver->is_streaming()) {
		return 0;
	}

	const std::vector<SiMMLTrack *> tracks = g_driver->get_sequencer()->get_tracks();

	VisRow rows[VIS_MAX_CHANNELS];
	int row_count = 0;

	for (SiMMLTrack *track : tracks) {
		SiOPMChannelBase *channel = track->get_channel();
		if (channel == nullptr || track->is_finished()) {
			continue;
		}

		const int channel_type = (int)channel->get_channel_type();
		const int channel_number = track->get_channel_number();

		int row = -1;
		for (int i = 0; i < row_count; i++) {
			if (channel_number >= 0 && rows[i].channel_type == channel_type && rows[i].channel_number == channel_number) {
				row = i;
				break;
			}
		}
		if (row < 0) {
			if (row_count == VIS_MAX_CHANNELS) {
				continue; // Over the display cap: ignore, per page design.
			}
			row = row_count++;
			rows[row].channel_type = channel_type;
			rows[row].channel_number = channel_number;
			rows[row].module_type = (int)track->get_module_type();
			rows[row].program = track->get_program_number();
			rows[row].order = row;
		}

		if (rows[row].note_count == VIS_NOTES_PER_ROW) {
			continue;
		}

		VisNote &note = rows[row].notes[rows[row].note_count++];
		note.pitch = channel->get_pitch();
		const int volume = (int)(track->get_output_level() * 1000.0);
		note.volume = volume < 0 ? 0 : (volume > 2000 ? 2000 : volume);
		note.pan = track->get_pan();
		note.flags = 0;
		note.sweep_target = 0;
		if (channel->is_note_on()) {
			note.flags |= VIS_FLAG_KEY_ON;
		}
		if (!channel->is_idling()) {
			note.flags |= VIS_FLAG_AUDIBLE;
		}
		if (channel->is_mute()) {
			note.flags |= VIS_FLAG_MUTE;
		}
		if (track->is_pitch_sweeping()) {
			note.flags |= VIS_FLAG_SWEEP;
			note.sweep_target = track->get_pitch_sweep_target();
		}
	}

	std::stable_sort(rows, rows + row_count, [](const VisRow &a, const VisRow &b) {
		if (a.channel_type != b.channel_type) {
			return a.channel_type < b.channel_type;
		}
		const int ka = a.channel_number < 0 ? INT32_MAX : a.channel_number;
		const int kb = b.channel_number < 0 ? INT32_MAX : b.channel_number;
		if (ka != kb) {
			return ka < kb;
		}
		return a.order < b.order;
	});

	for (int i = 0; i < VIS_MAX_CHANNELS; i++) {
		int32_t *dst = &g_channel_rows[i * VIS_ROW_INTS];
		if (i >= row_count) {
			dst[0] = -1; // Unused row marker.
			for (int k = 1; k < VIS_ROW_INTS; k++) {
				dst[k] = 0;
			}
			continue;
		}
		const VisRow &r = rows[i];
		dst[0] = r.channel_type;
		dst[1] = r.channel_number;
		dst[2] = r.module_type;
		dst[3] = r.program;
		dst[4] = r.note_count;
		dst[5] = 0;
		for (int n = 0; n < VIS_NOTES_PER_ROW; n++) {
			int32_t *p = dst + 6 + n * 5;
			if (n < r.note_count) {
				const VisNote &note = r.notes[n];
				p[0] = note.pitch;
				p[1] = note.volume;
				p[2] = note.pan;
				p[3] = note.flags;
				p[4] = note.sweep_target;
			} else {
				for (int k = 0; k < 5; k++) {
					p[k] = 0;
				}
			}
		}
	}

	return row_count;
}

// Byte address of the static channel snapshot (VIS_ROW_INTS int32 per row).
SION_WEB_EXPORT const int32_t *sion_web_channel_data() {
	return g_channel_rows;
}

} // extern "C"
