/***************************************************/
/* Part of GDSiON software synthesizer             */
/* Copyright (c) 2024 Yuri Sizov and contributors  */
/* Provided under MIT                              */
/***************************************************/

// Port of tests/run/voices-sound-consistency.gd from the Godot test project.
//
// The original test drives the driver through the Godot frame loop: it enables
// stream events, waits for one timer tick (which fires inside the sequencer
// phase of the first *processed* block, right when note_on lands with zero
// delay), then captures exactly two streaming buffers of 2048 stereo frames
// each and converts the left channel to int32 (`int(sample.x * 32767)`).
//
// Reproduced here deterministically: prepare a fresh (null-data) stream, lift
// the start suspend via update(), key the note on before the first processed
// block, render two blocks through the realtime chunk path and compare against
// the committed *.dat goldens (raw little-endian int32 samples).
//
// THIS IS THE AUDIO DETERMINISM GATE: any DSP change breaks it.

#include "sion_core.h"
#include "sion_driver.h"
#include "utils/sion_voice_preset_util.h"

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#ifndef GDSION_VOICES_DATA_DIR
	#error "GDSION_VOICES_DATA_DIR must be defined by the build system"
#endif

static std::string g_error_capture;
static size_t g_errors_before_voice = 0;

static const int SAMPLE_LENGTH = 2; // In 1/16th of a beat, as in the GD test.
static const int NOTE_VALUE = 60;
static const int SAMPLE_SCALE = 32767;
static const int BLOCK_FRAMES = 2048;
static const int CAPTURE_BLOCKS = 2; // Golden files are always exactly 2 blocks long.

static std::vector<int32_t> load_golden(const std::string &p_path) {
	std::vector<int32_t> samples;

	FILE *f = nullptr;
#ifdef _MSC_VER
	fopen_s(&f, p_path.c_str(), "rb");
#else
	f = fopen(p_path.c_str(), "rb");
#endif
	if (!f) {
		return samples;
	}

	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);

	size_t count = (size_t)(size > 0 ? size / 4 : 0);
	samples.resize(count);
	if (count > 0 && fread(samples.data(), 4, count, f) != count) {
		samples.clear();
	}
	fclose(f);
	return samples;
}

int main() {
	setvbuf(stdout, nullptr, _IONBF, 4096);
	sion::error_output() = [](const std::string &p_output) { g_error_capture += p_output; };

	int failures = 0;
	int checked = 0;

	{
		sion::initialize();

		SiONVoicePresetUtil *voice_preset_util = SiONVoicePresetUtil::generate_voices();
		std::vector<sion::String> voice_list = voice_preset_util->get_voice_preset_keys();

		const std::string data_dir = GDSION_VOICES_DATA_DIR;

		SiONDriver driver(2048, 2, 44100, 0);
		std::vector<float> chunk(BLOCK_FRAMES * 2);
		std::vector<int32_t> captured;
		captured.reserve(CAPTURE_BLOCKS * BLOCK_FRAMES);

		for (const sion::String &voice_name : voice_list) {
			Ref<SiONVoice> voice = voice_preset_util->get_voice_preset(voice_name);
			if (voice.is_null()) {
				std::printf("FAIL: [preset lookup - %s]\n", voice_name.c_str());
				failures++;
				continue;
			}

			g_errors_before_voice = g_error_capture.size();
			captured.clear();

			// Fresh silent stream per voice (mirrors driver.stream(false) which stops
			// and re-prepares everything), then lift the startup suspend exactly like
			// the frame that preceded the first timer tick did in the GD test.
			driver.stream(false);
			driver.update();

			driver.note_on(NOTE_VALUE, voice, 1 * SAMPLE_LENGTH);

			for (int block = 0; block < CAPTURE_BLOCKS; block++) {
				driver.render_chunk(chunk.data(), BLOCK_FRAMES);
				for (int i = 0; i < BLOCK_FRAMES; i++) {
					// Mirrors `int(sample.x * SAMPLE_SCALE)` over float32 stream samples.
					captured.push_back((int32_t)((double)chunk[i * 2] * SAMPLE_SCALE));
				}
			}

			driver.stop();

			std::string golden_path = data_dir + "/" + std::string(voice_name.c_str()) + ".dat";
			std::vector<int32_t> reference = load_golden(golden_path);

			checked++;

			if (reference.empty()) {
				std::printf("FAIL: [golden missing - %s] %s\n", voice_name.c_str(), golden_path.c_str());
				failures++;
				continue;
			}
			if (captured.size() != reference.size()) {
				std::printf("FAIL: [sample length - %s] got %zu want %zu\n",
						voice_name.c_str(), captured.size(), reference.size());
				failures++;
				continue;
			}

			int mismatch = -1;
			for (size_t i = 0; i < captured.size(); i++) {
				if (captured[i] != reference[i]) {
					mismatch = (int)i;
					break;
				}
			}
			if (mismatch >= 0) {
				std::printf("FAIL: [sample data - %s] first diff at %d: got %d want %d\n",
						voice_name.c_str(), mismatch, captured[mismatch], reference[mismatch]);
				failures++;
			}

			if (g_error_capture.size() != g_errors_before_voice) {
				std::printf("FAIL: [errors emitted - %s]\n%s", voice_name.c_str(),
						g_error_capture.substr(g_errors_before_voice).c_str());
				failures++;
			}
		}

		std::printf("VOICES: checked %d voices, %d failure(s)\n", checked, failures);

		delete voice_preset_util;
	}

	sion::finalize();

	if (failures > 0) {
		return 1;
	}
	return 0;
}
