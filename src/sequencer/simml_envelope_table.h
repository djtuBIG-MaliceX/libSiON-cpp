/***************************************************/
/* Part of GDSiON software synthesizer             */
/* Copyright (c) 2024 Yuri Sizov and contributors  */
/* Provided under MIT                              */
/***************************************************/

#ifndef SIMML_ENVELOPE_TABLE_H
#define SIMML_ENVELOPE_TABLE_H

////#include <godot_cpp/classes/ref_counted.hpp>
////#include <godot_cpp/templates/vector.hpp>
////#include <godot_cpp/variant/string.hpp>
////#include "templates/singly_linked_list.h"
#include <forward_list>



class SiMMLEnvelopeTable {
	////GDCLASS(SiMMLEnvelopeTable, RefCounted)

	std::forward_list<int> *_data = nullptr;

protected:
	static void _bind_methods() {}

public:
	std::forward_list<int> *get_data() const { return _data; }
	void set_data(std::forward_list<int> *p_data);

	std::forward_list<int>::Element *get_head() const;
	std::forward_list<int>::Element *get_tail() const;

	//

	void parse_mml(std::string p_table_numbers, std::string p_postfix, int p_max_index = 65536);
	void from_vector(std::vector<int> p_table, int p_loop_point = -1);
	// NOTE: Original code can implicitly create the destination vector and return it. We require creating it ahead of the call.
	void to_vector(int p_length, std::vector<int> *r_destination, int p_min = -65536, int p_max = 65536);

	void copy_from(const SiMMLEnvelopeTable &p_source);

	SiMMLEnvelopeTable(std::vector<int> p_table = std::vector<int>(), int p_loop_point = -1);
	virtual ~SiMMLEnvelopeTable();
};

#endif // SIMML_ENVELOPE_TABLE_H
