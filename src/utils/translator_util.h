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

	static std::vector<int> _split_data_string(const Ref<SiOPMChannelParams> &p_params, sion::String p_data_string, int p_channel_param_count, int p_operator_param_count, const sion::String &p_command);
	static void _check_operator_count(const Ref<SiOPMChannelParams> &p_params, int p_data_length, int p_channel_param_count, int p_operator_param_count, const sion::String &p_command);

	static int _sanitize_param_loop(int p_value, int p_min, int p_max, const sion::String &p_label);
	static int _sanitize_param_clamp(int p_value, int p_min, int p_max, const sion::String &p_label);
	static int _get_params_algorithm(int (&p_algorithms)[4][16], int p_operator_count, int p_data_value, int p_max_value, const sion::String &p_command);

	static void _set_siopm_params_by_array(const Ref<SiOPMChannelParams> &p_params, std::vector<int> p_data);
	static void _set_opl_params_by_array(const Ref<SiOPMChannelParams> &p_params, std::vector<int> p_data);
	static void _set_opm_params_by_array(const Ref<SiOPMChannelParams> &p_params, std::vector<int> p_data);
	static void _set_opn_params_by_array(const Ref<SiOPMChannelParams> &p_params, std::vector<int> p_data);
	static void _set_opx_params_by_array(const Ref<SiOPMChannelParams> &p_params, std::vector<int> p_data);
	static void _set_ma3_params_by_array(const Ref<SiOPMChannelParams> &p_params, std::vector<int> p_data);
	static void _set_al_params_by_array(const Ref<SiOPMChannelParams> &p_params, std::vector<int> p_data);

	static int _get_algorithm_index(int p_operator_count, int p_algorithm, int (&p_table)[4][16], const sion::String &p_command);
	static int _get_ma3_from_pg_type(int p_pulse_generator_type, const sion::String &p_command);
	static int _get_nearest_dt2(int p_detune);
	static int _balance_total_levels(int p_level0, int p_level1);

	struct OperatorParamsSizes {
		int pg_type = 1;
		int total_level = 2;
		int detune2 = 1;
		int phase = 1;
		int fixed_pitch = 1;
	};

	static sion::String _format_mml_comment(const sion::String &p_comment, const sion::String &p_line_end);
	static sion::String _format_mml_digit(int p_value, int p_padded = 0);
	static OperatorParamsSizes _get_operator_params_sizes(const Ref<SiOPMChannelParams> &p_params);

public:
	// Channel params.

	static void parse_siopm_params(const Ref<SiOPMChannelParams> &p_params, const sion::String &p_data_string);
	static void parse_opl_params(const Ref<SiOPMChannelParams> &p_params, const sion::String &p_data_string);
	static void parse_opm_params(const Ref<SiOPMChannelParams> &p_params, const sion::String &p_data_string);
	static void parse_opn_params(const Ref<SiOPMChannelParams> &p_params, const sion::String &p_data_string);
	static void parse_opx_params(const Ref<SiOPMChannelParams> &p_params, const sion::String &p_data_string);
	static void parse_ma3_params(const Ref<SiOPMChannelParams> &p_params, const sion::String &p_data_string);
	static void parse_al_params(const Ref<SiOPMChannelParams> &p_params, const sion::String &p_data_string);

	static void set_siopm_params(const Ref<SiOPMChannelParams> &p_params, std::vector<int> p_data);
	static void set_opl_params(const Ref<SiOPMChannelParams> &p_params, std::vector<int> p_data);
	static void set_opm_params(const Ref<SiOPMChannelParams> &p_params, std::vector<int> p_data);
	static void set_opn_params(const Ref<SiOPMChannelParams> &p_params, std::vector<int> p_data);
	static void set_opx_params(const Ref<SiOPMChannelParams> &p_params, std::vector<int> p_data);
	static void set_ma3_params(const Ref<SiOPMChannelParams> &p_params, std::vector<int> p_data);
	static void set_al_params(const Ref<SiOPMChannelParams> &p_params, std::vector<int> p_data);

	static std::vector<int> get_siopm_params(const Ref<SiOPMChannelParams> &p_params);
	static std::vector<int> get_opl_params(const Ref<SiOPMChannelParams> &p_params);
	static std::vector<int> get_opm_params(const Ref<SiOPMChannelParams> &p_params);
	static std::vector<int> get_opn_params(const Ref<SiOPMChannelParams> &p_params);
	static std::vector<int> get_opx_params(const Ref<SiOPMChannelParams> &p_params);
	static std::vector<int> get_ma3_params(const Ref<SiOPMChannelParams> &p_params);
	static std::vector<int> get_al_params(const Ref<SiOPMChannelParams> &p_params);

	static sion::String get_siopm_params_as_mml(const Ref<SiOPMChannelParams> &p_params, sion::String p_separator = " ", sion::String p_line_end = "\n", sion::String p_comment = sion::String());
	static sion::String get_opl_params_as_mml(const Ref<SiOPMChannelParams> &p_params, sion::String p_separator = " ", sion::String p_line_end = "\n", sion::String p_comment = sion::String());
	static sion::String get_opm_params_as_mml(const Ref<SiOPMChannelParams> &p_params, sion::String p_separator = " ", sion::String p_line_end = "\n", sion::String p_comment = sion::String());
	static sion::String get_opn_params_as_mml(const Ref<SiOPMChannelParams> &p_params, sion::String p_separator = " ", sion::String p_line_end = "\n", sion::String p_comment = sion::String());
	static sion::String get_opx_params_as_mml(const Ref<SiOPMChannelParams> &p_params, sion::String p_separator = " ", sion::String p_line_end = "\n", sion::String p_comment = sion::String());
	static sion::String get_ma3_params_as_mml(const Ref<SiOPMChannelParams> &p_params, sion::String p_separator = " ", sion::String p_line_end = "\n", sion::String p_comment = sion::String());
	static sion::String get_al_params_as_mml(const Ref<SiOPMChannelParams> &p_params, sion::String p_separator = " ", sion::String p_line_end = "\n", sion::String p_comment = sion::String());

	static void parse_voice_setting(const Ref<SiMMLVoice> &p_voice, sion::String p_mml, std::vector<Ref<SiMMLEnvelopeTable>> p_envelopes = std::vector<Ref<SiMMLEnvelopeTable>>());
	static sion::String get_voice_setting_as_mml(const Ref<SiMMLVoice> &p_voice);

	//

	static List<Ref<MMLSystemCommand>> extract_system_command(sion::String p_mml);

	struct MMLTableNumbers {
		SinglyLinkedList<int> *data = nullptr;
		int length = 0;
		bool repeated = false;
	};

	static MMLTableNumbers parse_table_numbers(sion::String p_table_numbers, sion::String p_postfix, int p_max_index = 65536);

	static void parse_wav(sion::String p_table_numbers, sion::String p_postfix, std::vector<double> *r_data);
	static void parse_wavb(sion::String p_hex, std::vector<double> *r_data);

	// TODO: The sound reference table is mostly needed for passing a map of Flash Sound objects. Some code changes may be needed in places that utilize that.
	static bool parse_sampler_wave(const Ref<SiOPMWaveSamplerTable> &p_table, int p_note_number, sion::String p_mml, HashMap<sion::String, Ref<SampleData>> p_sound_ref_table);
	static bool parse_pcm_wave(const Ref<SiOPMWavePCMTable> &p_table, sion::String p_mml, HashMap<sion::String, Ref<SampleData>> p_sound_ref_table);
	static bool parse_pcm_voice(const Ref<SiMMLVoice> &p_voice, sion::String p_mml, sion::String p_postfix, std::vector<Ref<SiMMLEnvelopeTable>> p_envelopes = std::vector<Ref<SiMMLEnvelopeTable>>());
};

#endif // SION_TRANSLATOR_UTIL_H
