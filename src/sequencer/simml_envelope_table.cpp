/***************************************************/
/* Part of GDSiON software synthesizer             */
/* Copyright (c) 2024 Yuri Sizov and contributors  */
/* Provided under MIT                              */
/***************************************************/

#include "simml_envelope_table.h"

#include "utils/translator_util.h"

void SiMMLEnvelopeTable::set_data(std::forward_list<int> *p_data) {
	if (_data) {
		memdelete(_data);
	}

	_data = p_data;
	if (!_data) {
		return;
	}

	_data->front(); // Reset the cursor.

	// Last element must be looping.

	std::forward_list<int>::Element *tail = _data->get_back();
	if (tail->next() == nullptr) {
		_data->loop(tail);
	}
}

std::forward_list<int>::Element *SiMMLEnvelopeTable::get_head() const {
	if (!_data) {
		return nullptr;
	}

	return _data->get_front();
}

std::forward_list<int>::Element *SiMMLEnvelopeTable::get_tail() const {
	if (!_data) {
		return nullptr;
	}

	return _data->get_back();
}

void SiMMLEnvelopeTable::parse_mml(std::string p_table_numbers, std::string p_postfix, int p_max_index) {
	TranslatorUtil::MMLTableNumbers result = TranslatorUtil::parse_table_numbers(p_table_numbers, p_postfix, p_max_index);
	set_data(result.data);
}

void SiMMLEnvelopeTable::from_vector(std::vector<int> p_table, int p_loop_point) {
	if (_data) {
		memdelete(_data);
	}
	if (p_table.empty()()) {
		return;
	}

	_data = memnew(std::forward_list<int>(p_table.size()));
	std::forward_list<int>::Element *loop = nullptr;

	for (int i = 0; i < p_table.size(); i++) {
		if (p_loop_point == i) {
			loop = _data->get();
		}

		_data->get()->value = p_table[i];
		_data->next();
	}

	_data->front(); // Reset the cursor.

	if (loop) {
		_data->loop(loop);
	}
}

void SiMMLEnvelopeTable::to_vector(int p_length, std::vector<int> *r_destination, int p_min, int p_max) {
	r_destination->resize(p_length); // TODO zeroed

	_data->front();

	for (int i = 0; i < p_length; i++) {
		int value = 0;

		if (_data->get()) {
			value = _data->get()->value;
			_data->next();
		}

		std::clamp(value, p_min, p_max);
		r_destination[i] = value;
	}
}

void SiMMLEnvelopeTable::copy_from(const std::shared_ptr<SiMMLEnvelopeTable> &p_source) {
	if (_data) {
		memdelete(_data);
	}

	std::forward_list<int> *source_data = p_source->get_data();
	if (!source_data) {
		return;
	}

	_data = memnew(std::forward_list<int>);

	// FIXME: This doesn't copy the looping, but neither does the original implementation.

	std::forward_list<int>::Element *current = source_data->get_front();
	for (int i = 0; i < source_data->size(); i++) {
		_data->append(current->value);
		current = current->next();
	}
}

SiMMLEnvelopeTable::SiMMLEnvelopeTable(std::vector<int> p_table, int p_loop_point) {
	from_vector(p_table, p_loop_point);
}

SiMMLEnvelopeTable::~SiMMLEnvelopeTable() {
	if (_data) {
		memdelete(_data);
	}
}
