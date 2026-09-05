/***************************************************/
/* Part of GDSiON software synthesizer             */
/* Copyright (c) 2024 Yuri Sizov and contributors  */
/* Provided under MIT                              */
/***************************************************/

#ifndef SI_EFFECTOR_H
#define SI_EFFECTOR_H

//#include <godot_cpp/core/object.hpp>
//#include <godot_cpp/templates/hash_map.hpp>
//#include <godot_cpp/templates/list.hpp>
//#include <godot_cpp/templates/vector.hpp>
#include "effector/si_effect_base.h"



class SiEffectStream;
class SiOPMSoundChip;

class SiEffector {
	//GDCLASS(SiEffector, Object)

	static HashMap<sion::String, List<Ref<SiEffectBase>>> _effect_instances;

	SiOPMSoundChip *_sound_chip = nullptr;

	SiEffectStream *_master_effect = nullptr;;
	List<SiEffectStream *> _free_effect_streams;
	std::vector<SiEffectStream *> _local_effects;
	// Expected to be SiOPMSoundChip::STREAM_SEND_SIZE at most.
	std::vector<SiEffectStream *> _global_effects;
	int _global_effect_count = 0;

	SiEffectStream *_get_global_stream(int p_slot);
	SiEffectStream *_alloc_stream(int p_depth);

protected:

public:
	// Effects.

	int get_global_effect_count() const { return _global_effect_count; }

	template <class T>
	static void register_effect(const sion::String &p_name);
	static Ref<SiEffectBase> get_effect_instance(const sion::String &p_name);
	template <class T>
	static Ref<T> create_effect_instance();

	// Slots and connections.

	std::vector<SiEffectBase> get_slot_effects(int p_slot) const;
	void add_slot_effect(int p_slot, const Ref<SiEffectBase> &p_effect);
	void set_slot_effects(int p_slot, const std::vector<SiEffectBase> &p_effects);
	void clear_slot_effects(int p_slot);

	SiEffectStream *create_local_effect(int p_depth, List<Ref<SiEffectBase>> p_effects);
	void delete_local_effect(SiEffectStream *p_effect);

	void parse_global_effect_mml(int p_slot, sion::String p_mml, sion::String p_postfix);

	// Processing.

	void prepare_process();
	void begin_process();
	void end_process();
	void reset();

	//

	void initialize();

	SiEffector(SiOPMSoundChip *p_chip = nullptr);
	~SiEffector();
};

#endif // SI_EFFECTOR_H
