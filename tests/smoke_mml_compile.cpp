/***************************************************/
/* Part of GDSiON software synthesizer             */
/* Copyright (c) 2024 Yuri Sizov and contributors  */
/* Provided under MIT                              */
/***************************************************/

// Phase 1 milestone check: compile MML through the sequencer without any
// Godot runtime, capturing the Godot-parity error sink.

#include "sequencer/simml_data.h"
#include "sequencer/simml_sequencer.h"
#include "sion_data.h"
#include "templates/singly_linked_list.h"

#include <cstdio>
#include <string>

static std::string g_error_capture;

static Ref<SiONData> compile_mml(const char *p_mml, std::string &r_errors) {
	r_errors.clear();

	Ref<SiONData> data;
	data.instantiate();
	std::printf("[step] data instantiated\n");
	std::fflush(stdout);
	data->clear();
	std::printf("[step] data cleared\n");
	std::fflush(stdout);

	Ref<SiMMLSequencer> sequencer;
	sequencer.instantiate();
	std::printf("[step] sequencer instantiated\n");
	std::fflush(stdout);

	if (!sequencer->prepare_compile(data, sion::String(p_mml))) {
		return nullptr;
	}
	std::printf("[step] prepared\n");
	std::fflush(stdout);
	sequencer->compile(0);
	std::printf("[step] compiled\n");
	std::fflush(stdout);

	return data;
}

int main() {
	sion::error_output() = [](const std::string &p_output) { g_error_capture += p_output; };

	SinglyLinkedList<int>::initialize_pool();
	SinglyLinkedList<double>::initialize_pool();

	int failures = 0;

	// A well-formed tune must compile without emitting any errors.
	std::string errors;
	Ref<SiONData> good = compile_mml("t150 l8 o4 cdefgab>c<", errors);
	if (good.is_null()) {
		std::printf("FAIL: valid MML did not produce data\n");
		failures++;
	} else if (!errors.empty()) {
		std::printf("FAIL: valid MML emitted errors:\n%s", errors.c_str());
		failures++;
	} else {
		std::printf("PASS: valid MML compiled clean\n");
	}

	// An unknown user-defined event must report the golden error text and
	// keep the parser alive (no crash, error captured).
	Ref<SiONData> bad = compile_mml("t150 l8 o4 $unknown_event c", errors);
	if (errors.find("ERROR: MMLParser: Unknown user-defined event: '$unknown_event'.") == std::string::npos) {
		std::printf("FAIL: broken MML did not report the expected error, got:\n%s", errors.c_str());
		failures++;
	} else {
		std::printf("PASS: broken MML reported the golden error (data=%s)\n",
				bad.is_null() ? "null as expected after prepare fail" : "produced");
	}

	SinglyLinkedList<int>::finalize_pool();
	SinglyLinkedList<double>::finalize_pool();

	if (failures > 0) {
		std::printf("SMOKE: %d failure(s)\n", failures);
		return 1;
	}
	std::printf("SMOKE: all checks passed\n");
	return 0;
}
