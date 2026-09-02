/***************************************************/
/* Part of GDSiON software synthesizer             */
/* Copyright (c) 2024 Yuri Sizov and contributors  */
/* Provided under MIT                              */
/***************************************************/

#ifndef SIOPM_STREAM_H
#define SIOPM_STREAM_H

////#include <godot_cpp/templates/vector.hpp>
////#include "templates/singly_linked_list.h"
#include <vector>
#include <forward_list>



class SiOPMStream {

	int channels = 2;
	std::vector<double> buffer;

public:
	int get_channel_count() const { return channels; }
	void set_channel_count(int p_value) { channels = p_value; }

	std::vector<double> get_buffer() const { return buffer; }
	std::vector<double> *get_buffer_ptr() { return &buffer; }
	void set_buffer(std::vector<double> p_buffer) { buffer = p_buffer; }

	void resize(int p_length);
	void clear();
	void limit();
	void quantize(int p_bitrate);

	void write(std::forward_list<int>::Element *p_data_start, int p_offset, int p_length, double p_volume, int p_pan);
	void write_stereo(std::forward_list<int>::Element *p_left_start, std::forward_list<int>::Element *p_right_start, int p_offset, int p_length, double p_volume, int p_pan);
	void write_from_vector(std::vector<double> *p_data, int p_start_data, int p_start_buffer, int p_length, double p_volume, int p_pan, int p_sample_channel_count);

	SiOPMStream() {}
	~SiOPMStream() {}
};

#endif // SIOPM_STREAM_H
