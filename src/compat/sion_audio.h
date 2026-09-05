/***************************************************/
/* Part of GDSiON software synthesizer             */
/* Copyright (c) 2024 Yuri Sizov and contributors  */
/* Provided under MIT                              */
/***************************************************/

#ifndef SION_COMPAT_AUDIO_H
#define SION_COMPAT_AUDIO_H

#include <algorithm>
#include <cstdint>
#include <vector>

#include "compat/sion_containers.h"

// Replacements for the Godot audio/container types that leaked into the
// waveform ingestion paths (PackedByteArray, AudioStreamWAV) and the stream
// buffers (Vector2 pairs). DSP math never touches these directly; they only
// carry raw PCM bytes/floats into the wave data classes.

// --- Packed array stand-ins.

using PackedInt32Array = std::vector<int32_t>;
using PackedFloat32Array = std::vector<float>;

// Godot's packed float buffer supports element-wise math operators, which
// the effector DSP relies on directly. Same semantics, same name.
class PackedFloat64Array : public std::vector<double> {
	using Base = std::vector<double>;

public:
	using Base::Base;
	using Base::operator=;

	PackedFloat64Array(const Base &p_other) :
			Base(p_other) {}
	PackedFloat64Array(Base &&p_other) :
			Base(std::move(p_other)) {}

	void fill(double p_value) { std::fill(begin(), end(), p_value); }

	void append_array(const PackedFloat64Array &p_other) {
		insert(end(), p_other.begin(), p_other.end());
	}

	PackedFloat64Array &operator+=(const PackedFloat64Array &p_other) {
		for (size_t i = 0; i < size() && i < p_other.size(); i++) {
			at(i) += p_other.at(i);
		}
		return *this;
	}

	PackedFloat64Array &operator-=(const PackedFloat64Array &p_other) {
		for (size_t i = 0; i < size() && i < p_other.size(); i++) {
			at(i) -= p_other.at(i);
		}
		return *this;
	}

	PackedFloat64Array &operator*=(double p_scale) {
		for (double &v : *this) {
			v *= p_scale;
		}
		return *this;
	}

	PackedFloat64Array &operator/=(double p_scale) {
		for (double &v : *this) {
			v /= p_scale;
		}
		return *this;
	}
};

class PackedByteArray : public std::vector<uint8_t> {
	using Base = std::vector<uint8_t>;

public:
	using Base::Base;
	using Base::operator=;

	PackedByteArray(const Base &p_other) :
			Base(p_other) {}
	PackedByteArray(Base &&p_other) :
			Base(std::move(p_other)) {}

	// Little-endian decoding, matching Godot's PackedByteArray on all
	// platforms SiON WAV payloads are produced on.
	int decode_s8(int p_offset) const {
		return static_cast<int>(static_cast<int8_t>(at(static_cast<size_t>(p_offset))));
	}

	int decode_s16(int p_offset) const {
		uint16_t raw = static_cast<uint16_t>(at(static_cast<size_t>(p_offset))) |
				(static_cast<uint16_t>(at(static_cast<size_t>(p_offset + 1))) << 8);
		return static_cast<int>(static_cast<int16_t>(raw));
	}
};

// --- Minimal Vector2 for the streaming event buffers.

struct Vector2 {
	double x = 0;
	double y = 0;
};

using PackedVector2Array = std::vector<Vector2>;

// --- Audio stream stand-ins (only WAV payloads are consumed).

class AudioStream {
public:
	virtual ~AudioStream() = default;
};

class AudioStreamWAV : public AudioStream {public:
	enum Format {
		FORMAT_NONE,
		FORMAT_8_BITS,
		FORMAT_16_BITS,
		FORMAT_IMA_ADPCM,
	};

private:
	Format _format = FORMAT_NONE;
	PackedByteArray _data;
	int _mix_rate = 44100;
	bool _stereo = false;

public:
	Format get_format() const { return _format; }
	void set_format(Format p_format) { _format = p_format; }

	const PackedByteArray &get_data() const { return _data; }
	void set_data(const PackedByteArray &p_data) { _data = p_data; }

	int get_mix_rate() const { return _mix_rate; }
	void set_mix_rate(int p_rate) { _mix_rate = p_rate; }

	bool is_stereo() const { return _stereo; }
	void set_stereo(bool p_stereo) { _stereo = p_stereo; }
};

// --- Concrete replacement for the Variant parameters that carried PCM or
// sampler payloads into SiOPMWavePCMData / SiOPMWaveSamplerData.

class SampleData {
public:
	enum Type {
		NIL,
		INT32_ARRAY,
		FLOAT32_ARRAY,
		WAVE,
	};

	Type type = NIL;
	PackedInt32Array int32_samples;
	PackedFloat32Array float_samples;
	Ref<AudioStreamWAV> wave;

	static Ref<SampleData> from_floats(const PackedFloat32Array &p_samples) {
		Ref<SampleData> data(new SampleData());
		data->type = FLOAT32_ARRAY;
		data->float_samples = p_samples;
		return data;
	}

	static Ref<SampleData> from_int32s(const PackedInt32Array &p_samples) {
		Ref<SampleData> data(new SampleData());
		data->type = INT32_ARRAY;
		data->int32_samples = p_samples;
		return data;
	}

	static Ref<SampleData> from_wave(const Ref<AudioStreamWAV> &p_wave) {
		Ref<SampleData> data(new SampleData());
		data->type = WAVE;
		data->wave = p_wave;
		return data;
	}
};

// --- Placeholder types referenced by the not-yet-libified SiONDriver header.
// Phase 2 removes these along with the Godot audio plumbing.

class AudioStreamGenerator : public AudioStream {
public:
	void set_mix_rate(double p_rate) { mix_rate = p_rate; }
	double get_mix_rate() const { return mix_rate; }
	double mix_rate = 44100.0;
};

class AudioStreamGeneratorPlayback {};
class AudioStreamPlayer {};

// Minimal stand-in keeping the legacy SiONDriver header parseable; real
// payloads now flow through SampleData. Removed with the Phase 2 driver.
class Variant {
public:
	enum Type {
		NIL,
		BOOL,
		INT,
		FLOAT,
		STRING,
		OBJECT,
		PACKED_BYTE_ARRAY,
		PACKED_INT32_ARRAY,
		PACKED_FLOAT32_ARRAY,
	};

private:
	Type _type = NIL;

public:
	Variant() = default;

	Type get_type() const { return _type; }
};

#endif // SION_COMPAT_AUDIO_H
