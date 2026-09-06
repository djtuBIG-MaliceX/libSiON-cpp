/***************************************************/
/* Part of GDSiON software synthesizer             */
/* Copyright (c) 2024 Yuri Sizov and contributors  */
/* Provided under MIT                              */
/***************************************************/

#ifndef LIBSION_WAV_WRITER_H
#define LIBSION_WAV_WRITER_H

#include "cli_util.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

// Minimal 16-bit PCM WAV writer for offline rendering output.
class WavWriter {
	static void write_u32(FILE *p_file, uint32_t p_value) {
		uint8_t bytes[4] = {
			(uint8_t)(p_value & 0xFF),
			(uint8_t)((p_value >> 8) & 0xFF),
			(uint8_t)((p_value >> 16) & 0xFF),
			(uint8_t)((p_value >> 24) & 0xFF),
		};
		fwrite(bytes, 1, 4, p_file);
	}

	static void write_u16(FILE *p_file, uint16_t p_value) {
		uint8_t bytes[2] = { (uint8_t)(p_value & 0xFF), (uint8_t)((p_value >> 8) & 0xFF) };
		fwrite(bytes, 1, 2, p_file);
	}

public:
	// p_samples is interleaved multi-channel float audio in [-1, +1].
	static bool write(const std::string &p_path, const double *p_samples, size_t p_sample_count, int p_channels, int p_sample_rate) {
		FILE *file = cli_fopen(p_path, "wb");
		if (!file) {
			return false;
		}

		const uint32_t data_size = (uint32_t)(p_sample_count * sizeof(int16_t));
		const uint32_t byte_rate = (uint32_t)p_sample_rate * p_channels * 2;

		fwrite("RIFF", 1, 4, file);
		write_u32(file, 36 + data_size);
		fwrite("WAVE", 1, 4, file);

		fwrite("fmt ", 1, 4, file);
		write_u32(file, 16);                  // PCM chunk size.
		write_u16(file, 1);                   // Audio format: PCM.
		write_u16(file, (uint16_t)p_channels);
		write_u32(file, (uint32_t)p_sample_rate);
		write_u32(file, byte_rate);
		write_u16(file, (uint16_t)(p_channels * 2)); // Block align.
		write_u16(file, 16);                  // Bits per sample.

		fwrite("data", 1, 4, file);
		write_u32(file, data_size);

		// Stream int16 frames without holding a second full buffer around.
		int16_t frame;
		for (size_t i = 0; i < p_sample_count; i++) {
			double clamped = std::clamp(p_samples[i], -1.0, 1.0);
			frame = (int16_t)std::lround(clamped * 32767.0);
			fwrite(&frame, sizeof(int16_t), 1, file);
		}

		fclose(file);
		return true;
	}
};

#endif // LIBSION_WAV_WRITER_H
