/*
 * Part of libSiON-cpp, forked from GDSiON software synthesizer
 * Copyright (c) 2024-2026 Yuri Sizov, James Alan Nguyen and contributors
 * Based on SiON Flash Software Synthesizer (C) 2008-2016 keim_at_Si
 * Provided under MIT License.
 */

#include "sion_core.h"

#include "chip/channels/siopm_channel_fm.h"
#include "chip/siopm_ref_table.h"
#include "sequencer/base/mml_parser.h"
#include "sequencer/base/mml_sequencer.h"
#include "sequencer/simml_ref_table.h"
#include "sequencer/simml_track.h"
#include "templates/singly_linked_list.h"



namespace sion {

void initialize() {
	SinglyLinkedList<int>::initialize_pool();
	SinglyLinkedList<double>::initialize_pool();

	MMLParser::initialize();
	MMLSequencer::initialize();
	SiOPMRefTable::initialize();
	SiMMLRefTable::initialize();
	SiMMLTrack::initialize();
}

void finalize() {
	SinglyLinkedList<int>::finalize_pool();
	SinglyLinkedList<double>::finalize_pool();

	SiOPMChannelFM::finalize_pool();
	SiMMLTrack::finalize();
	SiMMLRefTable::finalize();
	SiOPMRefTable::finalize();
	MMLSequencer::finalize();
	MMLParser::finalize();
}

} // namespace sion
