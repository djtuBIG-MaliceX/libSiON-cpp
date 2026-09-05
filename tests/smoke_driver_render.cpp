/***************************************************/
/* Part of GDSiON software synthesizer             */
/* Copyright (c) 2024 Yuri Sizov and contributors  */
/* Provided under MIT                              */
/***************************************************/

// Phase 2 milestone check: the standalone SiONDriver compiles MML, renders
// audio offline and streams realtime chunks — all without a Godot runtime.

#include "sion_core.h"
#include "sion_data.h"
#include "sion_driver.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

static std::string g_error_capture;

int main() {
	setvbuf(stdout, nullptr, _IONBF, 0);
	sion::error_output() = [](const std::string &p_output) { g_error_capture += p_output; };

	int failures = 0;

	{
		sion::initialize();

		SiONDriver driver(2048, 2, 44100, 0);

		// --- Lifecycle bookkeeping (port of tests/run/driver-lifecycle.gd; the Godot
		//     AudioStreamPlayer checks are gone with the Node lifecycle).
		if (driver.get_buffer_length() != 2048 || driver.get_channel_num() != 2 ||
				driver.get_sample_rate() != 44100 || driver.get_bitrate() != 0) {
			std::printf("FAIL: driver defaults (buffer=%d channels=%d rate=%f bitrate=%f)\n",
					driver.get_buffer_length(), driver.get_channel_num(),
					driver.get_sample_rate(), driver.get_bitrate());
			failures++;
		}
		driver.set_bpm(120);
		if (driver.get_bpm() != 120) {
			std::printf("FAIL: driver bpm roundtrip (got %f)\n", driver.get_bpm());
			failures++;
		}

		driver.stream(false);
		if (!driver.is_streaming() || driver.is_paused()) {
			std::printf("FAIL: stream() state (streaming=%d paused=%d)\n",
					(int)driver.is_streaming(), (int)driver.is_paused());
			failures++;
		}
		driver.update(); // Releases the startup suspend, like the first frame did under Godot.
		driver.stop();
		if (driver.is_streaming()) {
			std::printf("FAIL: stop() left driver streaming\n");
			failures++;
		}

		const char *mml = "t150 l8 o4 cdefgab>c<";

		// --- Immediate compile through the driver.
		size_t before = g_error_capture.size();
		Ref<SiONData> data = driver.compile(mml);

		bool compiled = data.is_valid();

		if (!compiled) {
			std::printf("FAIL: driver compile returned no data\n");
			failures++;
		} else if (g_error_capture.size() != before) {
			std::printf("FAIL: driver compile emitted errors:\n%s", g_error_capture.substr(before).c_str());
			failures++;
		} else {
			std::printf("PASS: driver compiled MML clean\n");
		}

		// --- Offline render must produce non-silent audio.
		const int total_samples = 44100 * 2; // 1 second of stereo samples.
		PackedFloat64Array buffer = driver.render(data, total_samples, 2, true);

		if ((int)buffer.size() < total_samples) {
			std::printf("FAIL: render returned %zu samples, expected %d\n", buffer.size(), total_samples);
			failures++;
		} else {
			double peak = 0;
			for (double value : buffer) {
				peak = std::max(peak, std::abs(value));
			}
			if (peak < 0.01) {
				std::printf("FAIL: render produced silence (peak %f)\n", peak);
				failures++;
			} else {
				std::printf("PASS: offline render peak %f\n", peak);
			}
		}

		// --- Realtime streaming: update() pump + render_chunk().
		int started = 0;
		int stopped = 0;
		driver.on_event = [&](const Ref<SiONEvent> &p_event) {
			if (p_event->get_event_type() == SiONEvent::STREAM_STARTED) {
				started++;
			} else if (p_event->get_event_type() == SiONEvent::STREAM_STOPPED) {
				stopped++;
			}
		};

		driver.play(data);
		if (!driver.is_streaming()) {
			std::printf("FAIL: play() did not start streaming\n");
			failures++;
		}

		// First pump dispatches STREAM_STARTED and lifts the suspend flag.
		driver.update();
		if (started != 1) {
			std::printf("FAIL: expected one stream_started event, got %d\n", started);
			failures++;
		}

		std::vector<float> chunk(2048 * 2, -1.0f);
		double peak = 0;
		for (int i = 0; i < 64; i++) { // ~3 seconds worth of blocks.
			driver.render_chunk(chunk.data(), 2048);
			for (float value : chunk) {
				peak = std::max(peak, std::abs((double)value));
			}
		}
		if (peak < 0.01) {
			std::printf("FAIL: render_chunk produced silence (peak %f)\n", peak);
			failures++;
		} else {
			std::printf("PASS: render_chunk peak %f\n", peak);
		}

		driver.stop();
		if (stopped != 1 || driver.is_streaming()) {
			std::printf("FAIL: stop() misbehaved (stopped=%d streaming=%d)\n", stopped, (int)driver.is_streaming());
			failures++;
		} else {
			std::printf("PASS: stream stop bookkeeping\n");
		}

		if (failures == 0) {
			std::printf("SMOKE: all driver checks passed\n");
		}
	}

	sion::finalize();

	if (failures > 0) {
		std::printf("SMOKE: %d failure(s)\n", failures);
		return 1;
	}
	return 0;
}
