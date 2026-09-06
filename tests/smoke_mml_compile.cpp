/*
 * Part of libSiON-cpp, forked from GDSiON software synthesizer
 * Copyright (c) 2024-2026 Yuri Sizov, James Alan Nguyen and contributors
 * Based on SiON Flash Software Synthesizer (C) 2008-2016 keim_at_Si
 * Provided under MIT License.
 */

// Phase 1 milestone check: compile MML through the sequencer without any
// Godot runtime, capturing the Godot-parity error sink.

#include "chip/siopm_sound_chip.h"
#include "sequencer/base/mml_sequence_group.h"
#include "sequencer/simml_data.h"
#include "sequencer/simml_sequencer.h"
#include "sion_core.h"
#include "sion_data.h"

#include <cstdio>
#include <string>

static std::string g_error_capture;

// Runs a full compile cycle the way SiONDriver does: data + sequencer over a
// shared sound chip. All objects must be released before sion::finalize().
static bool compile_mml(SiOPMSoundChip *p_chip, const char *p_mml, std::string &r_errors, int *r_sequences = nullptr) {
	const size_t capture_start = g_error_capture.size();
	bool prepared = false;
	int sequence_count = 0;

	{
		Ref<SiONData> data;
		data.instantiate();
		data->clear();

		SiMMLSequencer sequencer(p_chip);
		prepared = sequencer.prepare_compile(data, sion::String(p_mml));
		if (prepared) {
			sequencer.compile(0);
			sequence_count = data->get_sequence_group()->get_sequence_count();
		}
	}

	if (r_sequences) {
		*r_sequences = sequence_count;
	}
	r_errors = g_error_capture.substr(capture_start);
	return prepared;
}

int main() {
	setvbuf(stdout, nullptr, _IONBF, 0);
	sion::error_output() = [](const std::string &p_output) { g_error_capture += p_output; };

	int failures = 0;

	{
		sion::initialize();

		SiOPMSoundChip chip;

		// A well-formed tune must compile without emitting any errors and
		// produce at least one track with events.
		std::string errors;
		int sequences = 0;
		bool ok = compile_mml(&chip, "t150 l8 o4 cdefgab>c<", errors, &sequences);
		if (!ok) {
			std::printf("FAIL: valid MML was rejected by the parser\n");
			failures++;
		} else if (sequences < 1) {
			std::printf("FAIL: valid MML produced no sequences (%d)\n", sequences);
			failures++;
		} else if (!errors.empty()) {
			std::printf("FAIL: valid MML emitted errors:\n%s", errors.c_str());
			failures++;
		} else {
			std::printf("PASS: valid MML compiled clean\n");
		}

		// An out-of-range command argument must report the golden error text
		// and keep the parser alive (no crash, error captured).
		compile_mml(&chip, "t150 l8 o20 c", errors);
		if (errors.find("ERROR: MMLParser: Command 'o' has argument (20) outside of valid range (0 : 9).") == std::string::npos) {
			std::printf("FAIL: broken MML did not report the expected error, got:\n%s", errors.c_str());
			failures++;
		} else {
			std::printf("PASS: broken MML reported the golden error\n");
		}

		sion::finalize();
	}

	if (failures > 0) {
		std::printf("SMOKE: %d failure(s)\n", failures);
		return 1;
	}
	std::printf("SMOKE: all checks passed\n");
	return 0;
}
