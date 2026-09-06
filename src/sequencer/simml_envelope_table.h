/*
 * Part of libSiON-cpp, forked from GDSiON software synthesizer
 * Copyright (c) 2024-2026 Yuri Sizov, James Alan Nguyen and contributors
 * Based on SiON Flash Software Synthesizer (C) 2008-2016 keim_at_Si
 * Provided under MIT License.
 */

#ifndef SIMML_ENVELOPE_TABLE_H
#define SIMML_ENVELOPE_TABLE_H

////#include <godot_cpp/classes/ref_counted.hpp>
////#include <godot_cpp/templates/vector.hpp>
////#include <godot_cpp/variant/string.hpp>
////#include "templates/singly_linked_list.h"



class SiMMLEnvelopeTable {
	////GDCLASS(SiMMLEnvelopeTable, RefCounted)

	SinglyLinkedList<int> *_data = nullptr;

protected:
	static void _bind_methods() {}

public:
	SinglyLinkedList<int> *get_data() const { return _data; }
	void set_data(SinglyLinkedList<int> *p_data);

	SinglyLinkedList<int>::Element *get_head() const;
	SinglyLinkedList<int>::Element *get_tail() const;

	//

	void parse_mml(sion::String p_table_numbers, sion::String p_postfix, int p_max_index = 65536);
	void from_vector(std::vector<int> p_table, int p_loop_point = -1);
	// NOTE: Original code can implicitly create the destination vector and return it. We require creating it ahead of the call.
	void to_vector(int p_length, std::vector<int> *r_destination, int p_min = -65536, int p_max = 65536);

	void copy_from(const Ref<SiMMLEnvelopeTable> &p_source);

	SiMMLEnvelopeTable(std::vector<int> p_table = std::vector<int>(), int p_loop_point = -1);
	virtual ~SiMMLEnvelopeTable();
};

#endif // SIMML_ENVELOPE_TABLE_H
