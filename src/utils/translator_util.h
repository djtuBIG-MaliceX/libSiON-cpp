/***************************************************/
/* Part of GDSiON software synthesizer             */
/* Copyright (c) 2024 Yuri Sizov and contributors  */
/* Provided under MIT                              */
/***************************************************/

#ifndef SION_TRANSLATOR_UTIL_H
#define SION_TRANSLATOR_UTIL_H

// //#include <godot_cpp/templates/hash_map.hpp>
// //#include <godot_cpp/templates/list.hpp>
// //#include <godot_cpp/templates/vector.hpp>
// //#include <godot_cpp/variant/string.hpp>
#include "sequencer/base/mml_system_command.h"
//#include "templates/singly_linked_list.h"



class SiMMLEnvelopeTable;
class SiMMLVoice;
class SiONVoice;
class SiOPMChannelParams;
class SiOPMWavePCMTable;
class SiOPMWaveSamplerTable;

class TranslatorUtil {

	static std::vector<int> _split_data_string(const std::shared_ptr<SiOPMChannelParams> &p_params, std::string p_data_string, int p_channel_param_count, int p_operator_param_count, const std::string &p_command);
	static void _check_operator_count(const std::shared_ptr<SiOPMChannelParams> &p_params, int p_data_length, int p_channel_param_count, int p_operator_param_count, const std::string &p_command);

	static int _sanitize_param_loop(int p_value, int p_min, int p_max, const std::string &p_label);
	static int _sanitize_param_clamp(int p_value, int p_min, int p_max, const std::string &p_label);
	static int _get_params_algorithm(int (&p_algorithms)[4][16], int p_operator_count, int p_data_value, int p_max_value, const std::string &p_command);

	static void _set_siopm_params_by_array(const std::shared_ptr<SiOPMChannelParams> &p_params, std::vector<int> p_data);
	static void _set_opl_params_by_array(const std::shared_ptr<SiOPMChannelParams> &p_params, std::vector<int> p_data);
	static void _set_opm_params_by_array(const std::shared_ptr<SiOPMChannelParams> &p_params, std::vector<int> p_data);
	static void _set_opn_params_by_array(const std::shared_ptr<SiOPMChannelParams> &p_params, std::vector<int> p_data);
	static void _set_opx_params_by_array(const std::shared_ptr<SiOPMChannelParams> &p_params, std::vector<int> p_data);
	static void _set_ma3_params_by_array(const std::shared_ptr<SiOPMChannelParams> &p_params, std::vector<int> p_data);
	static void _set_al_params_by_array(const std::shared_ptr<SiOPMChannelParams> &p_params, std::vector<int> p_data);

	static int _get_algorithm_index(int p_operator_count, int p_algorithm, int (&p_table)[4][16], const std::string &p_command);
	static int _get_ma3_from_pg_type(int p_pulse_generator_type, const std::string &p_command);
	static int _get_nearest_dt2(int p_detune);
	static int _balance_total_levels(int p_level0, int p_level1);

	struct OperatorParamsSizes {
		int pg_type = 1;
		int total_level = 2;
		int detune2 = 1;
		int phase = 1;
		int fixed_pitch = 1;
	};

	static std::string _format_mml_comment(const std::string &p_comment, const std::string &p_line_end);
	static std::string _format_mml_digit(int p_value, int p_padded = 0);
	static OperatorParamsSizes _get_operator_params_sizes(const std::shared_ptr<SiOPMChannelParams> &p_params);

public:
	// Channel params.

	static void parse_siopm_params(const std::shared_ptr<SiOPMChannelParams> &p_params, const std::string &p_data_string);
	static void parse_opl_params(const std::shared_ptr<SiOPMChannelParams> &p_params, const std::string &p_data_string);
	static void parse_opm_params(const std::shared_ptr<SiOPMChannelParams> &p_params, const std::string &p_data_string);
	static void parse_opn_params(const std::shared_ptr<SiOPMChannelParams> &p_params, const std::string &p_data_string);
	static void parse_opx_params(const std::shared_ptr<SiOPMChannelParams> &p_params, const std::string &p_data_string);
	static void parse_ma3_params(const std::shared_ptr<SiOPMChannelParams> &p_params, const std::string &p_data_string);
	static void parse_al_params(const std::shared_ptr<SiOPMChannelParams> &p_params, const std::string &p_data_string);

	static void set_siopm_params(const std::shared_ptr<SiOPMChannelParams> &p_params, std::vector<int> p_data);
	static void set_opl_params(const std::shared_ptr<SiOPMChannelParams> &p_params, std::vector<int> p_data);
	static void set_opm_params(const std::shared_ptr<SiOPMChannelParams> &p_params, std::vector<int> p_data);
	static void set_opn_params(const std::shared_ptr<SiOPMChannelParams> &p_params, std::vector<int> p_data);
	static void set_opx_params(const std::shared_ptr<SiOPMChannelParams> &p_params, std::vector<int> p_data);
	static void set_ma3_params(const std::shared_ptr<SiOPMChannelParams> &p_params, std::vector<int> p_data);
	static void set_al_params(const std::shared_ptr<SiOPMChannelParams> &p_params, std::vector<int> p_data);

	static std::vector<int> get_siopm_params(const std::shared_ptr<SiOPMChannelParams> &p_params);
	static std::vector<int> get_opl_params(const std::shared_ptr<SiOPMChannelParams> &p_params);
	static std::vector<int> get_opm_params(const std::shared_ptr<SiOPMChannelParams> &p_params);
	static std::vector<int> get_opn_params(const std::shared_ptr<SiOPMChannelParams> &p_params);
	static std::vector<int> get_opx_params(const std::shared_ptr<SiOPMChannelParams> &p_params);
	static std::vector<int> get_ma3_params(const std::shared_ptr<SiOPMChannelParams> &p_params);
	static std::vector<int> get_al_params(const std::shared_ptr<SiOPMChannelParams> &p_params);

	static std::string get_siopm_params_as_mml(const std::shared_ptr<SiOPMChannelParams> &p_params, std::string p_separator = " ", std::string p_line_end = "\n", std::string p_comment = std::string());
	static std::string get_opl_params_as_mml(const std::shared_ptr<SiOPMChannelParams> &p_params, std::string p_separator = " ", std::string p_line_end = "\n", std::string p_comment = std::string());
	static std::string get_opm_params_as_mml(const std::shared_ptr<SiOPMChannelParams> &p_params, std::string p_separator = " ", std::string p_line_end = "\n", std::string p_comment = std::string());
	static std::string get_opn_params_as_mml(const std::shared_ptr<SiOPMChannelParams> &p_params, std::string p_separator = " ", std::string p_line_end = "\n", std::string p_comment = std::string());
	static std::string get_opx_params_as_mml(const std::shared_ptr<SiOPMChannelParams> &p_params, std::string p_separator = " ", std::string p_line_end = "\n", std::string p_comment = std::string());
	static std::string get_ma3_params_as_mml(const std::shared_ptr<SiOPMChannelParams> &p_params, std::string p_separator = " ", std::string p_line_end = "\n", std::string p_comment = std::string());
	static std::string get_al_params_as_mml(const std::shared_ptr<SiOPMChannelParams> &p_params, std::string p_separator = " ", std::string p_line_end = "\n", std::string p_comment = std::string());

	static void parse_voice_setting(const std::shared_ptr<SiMMLVoice> &p_voice, std::string p_mml, std::vector<std::shared_ptr<SiMMLEnvelopeTable>> p_envelopes = std::vector<std::shared_ptr<SiMMLEnvelopeTable>>());
	static std::string get_voice_setting_as_mml(const std::shared_ptr<SiMMLVoice> &p_voice);

	//

	static List<std::shared_ptr<MMLSystemCommand>> extract_system_command(std::string p_mml);

	struct MMLTableNumbers {
		std::forward_list<int> *data = nullptr;
		int length = 0;
		bool repeated = false;
	};

	static MMLTableNumbers parse_table_numbers(std::string p_table_numbers, std::string p_postfix, int p_max_index = 65536);

	static void parse_wav(std::string p_table_numbers, std::string p_postfix, std::vector<double> *r_data);
	static void parse_wavb(std::string p_hex, std::vector<double> *r_data);

	// TODO: The sound reference table is mostly needed for passing a map of Flash Sound objects. Some code changes may be needed in places that utilize that.
	static bool parse_sampler_wave(const std::shared_ptr<SiOPMWaveSamplerTable> &p_table, int p_note_number, std::string p_mml, HashMap<std::string, Variant> p_sound_ref_table);
	static bool parse_pcm_wave(const std::shared_ptr<SiOPMWavePCMTable> &p_table, std::string p_mml, HashMap<std::string, Variant> p_sound_ref_table);
	static bool parse_pcm_voice(const std::shared_ptr<SiMMLVoice> &p_voice, std::string p_mml, std::string p_postfix, std::vector<std::shared_ptr<SiMMLEnvelopeTable>> p_envelopes = std::vector<std::shared_ptr<SiMMLEnvelopeTable>>());
};

#endif // SION_TRANSLATOR_UTIL_H
