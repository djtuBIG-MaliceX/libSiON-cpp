/***************************************************/
/* Part of GDSiON software synthesizer             */
/* Copyright (c) 2024 Yuri Sizov and contributors  */
/* Provided under MIT                              */
/***************************************************/

// Port of tests/run/mml-compilation.gd from the Godot test project.
//
// The original fetched mmltalks_mml.json, stored every tune as
// "<mml_string.hash()>.mml" (djb2 over UTF-32 codepoints) and compiled each
// one in a scratch Godot process, comparing the captured "ERROR: ..." lines
// against committed golden outputs.
//
// Here the corpus is vendored under tests/data/mml (index-named inputs plus a
// manifest carrying the golden hash mapping), and compilation runs in-process
// through the error_output() sink. A fresh driver is created per tune to
// approximate the original one-process-per-tune isolation.

#include "sion_core.h"
#include "sion_data.h"
#include "sion_driver.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifndef LIBSION_MML_MANIFEST
	#error "LIBSION_MML_MANIFEST must be defined by the build system"
#endif
#ifndef LIBSION_MML_INPUT_DIR
	#error "LIBSION_MML_INPUT_DIR must be defined by the build system"
#endif
#ifndef LIBSION_MML_GOLDEN_DIR
	#error "LIBSION_MML_GOLDEN_DIR must be defined by the build system"
#endif

static std::string g_error_capture;

static bool read_file(const std::string &p_path, std::string &r_out) {
	std::ifstream f(p_path, std::ios::binary);
	if (!f) {
		return false;
	}
	std::ostringstream ss;
	ss << f.rdbuf();
	r_out = ss.str();
	return true;
}

// Mirrors TestBase._extract_godot_errors(): keep only lines starting with "ERROR: ",
// after normalizing CRLF.
static std::string extract_error_lines(const std::string &p_buffer) {
	std::string normalized;
	normalized.reserve(p_buffer.size());
	for (char c : p_buffer) {
		if (c != '\r') {
			normalized.push_back(c);
		}
	}

	std::string errors;
	std::istringstream stream(normalized);
	std::string line;
	while (std::getline(stream, line)) {
		if (line.rfind("ERROR: ", 0) == 0) {
			errors += line + "\n";
		}
	}
	return errors;
}

int main() {
	setvbuf(stdout, nullptr, _IONBF, 4096);
	sion::error_output() = [](const std::string &p_output) { g_error_capture += p_output; };

	int failures = 0;
	int checked = 0;

	std::string manifest;
	if (!read_file(LIBSION_MML_MANIFEST, manifest)) {
		std::printf("MML: cannot read manifest %s\n", LIBSION_MML_MANIFEST);
		return 1;
	}

	{
		sion::initialize();

		const std::string input_dir = LIBSION_MML_INPUT_DIR;
		const std::string golden_dir = LIBSION_MML_GOLDEN_DIR;

		std::istringstream lines(manifest);
		std::string row;
		while (std::getline(lines, row)) {
			if (row.empty()) {
				continue;
			}

			// index \t hash \t has_error \t title \t author
			size_t f1 = row.find('\t');
			size_t f2 = row.find('\t', f1 + 1);
			size_t f3 = row.find('\t', f2 + 1);
			std::string index = row.substr(0, f1);
			std::string hash = row.substr(f1 + 1, f2 - f1 - 1);
			bool has_error_golden = (row[f2 + 1] == '1');
			std::string title = (f3 != std::string::npos) ? row.substr(f3 + 1) : "";

			std::string mml;
			if (!read_file(input_dir + "/" + index + ".mml", mml)) {
				std::printf("FAIL: [input missing - %s] %s.mml\n", title.c_str(), index.c_str());
				failures++;
				continue;
			}

			size_t before = g_error_capture.size();

			SiONDriver *driver = new SiONDriver(2048, 2, 44100, 0);
			driver->compile(mml);
			delete driver; // ~SiONDriver stops streaming if still active.

			std::string received = extract_error_lines(g_error_capture.substr(before));

			std::string expected;
			if (has_error_golden) {
				std::string golden;
				if (!read_file(golden_dir + "/" + hash + ".txt", golden)) {
					std::printf("FAIL: [golden missing - %s] %s.txt\n", title.c_str(), hash.c_str());
					failures++;
					continue;
				}
				expected = extract_error_lines(golden);
			}

			checked++;
			if (received != expected) {
				std::printf("FAIL: [compile errors - %s] idx=%s\n--- expected ---\n%s--- received ---\n%s",
						title.c_str(), index.c_str(), expected.c_str(), received.c_str());
				failures++;
			}
		}

		std::printf("MML: checked %d tunes, %d failure(s)\n", checked, failures);
	}

	sion::finalize();

	if (failures > 0) {
		return 1;
	}
	return 0;
}
