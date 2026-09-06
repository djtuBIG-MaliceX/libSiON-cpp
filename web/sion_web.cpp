/*
 * Part of libSiON-cpp, forked from GDSiON software synthesizer
 * Copyright (c) 2024-2026 Yuri Sizov, James Alan Nguyen and contributors
 * Based on SiON Flash Software Synthesizer (C) 2008-2016 keim_at_Si
 * Provided under MIT License.
 */

// C ABI facade over SiONDriver for the Emscripten/WASM build. The AudioWorklet
// glue (sion_worklet.js) drives this API: init once, play(mml), then call
// update() + render(frames) from every audio quantum and read back the static
// interleaved-stereo buffer (fixed address, safe across memory growth).

#include "sion_core.h"
#include "sion_driver.h"

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

} // extern "C"
