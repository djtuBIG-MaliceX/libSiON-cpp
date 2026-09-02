/***************************************************/
/* Part of GDSiON software synthesizer             */
/* Copyright (c) 2024 Yuri Sizov and contributors  */
/* Provided under MIT                              */
/***************************************************/

#ifndef SION_TRANSFORMER_UTIL_H
#define SION_TRANSFORMER_UTIL_H

////#include <godot_cpp/templates/vector.hpp>
#include <vector>



// Based partially on the original code's SiONUtil class.
class TransformerUtil {

	static void _amplify_log_data(std::vector<int> *r_src, int p_gain);

public:
	// Logarithmic transformation of wave PCM data.
	static std::vector<int> transform_pcm_data(std::vector<double> p_source, int p_src_channel_count = 2, int p_channel_count = 0, bool p_maximize = true);

	static std::vector<double> transform_sampler_data(std::vector<double> p_source, int p_src_channel_count = 2, int p_channel_count = 0);

	static std::vector<double> wave_color_to_vector(uint32_t p_color, int p_wave_type = 0);
};

#endif // SION_TRANSFORMER_UTIL_H
