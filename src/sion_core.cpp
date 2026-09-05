/***************************************************/
/* Part of GDSiON software synthesizer             */
/* Copyright (c) 2024 Yuri Sizov and contributors  */
/* Provided under MIT                              */
/***************************************************/

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
