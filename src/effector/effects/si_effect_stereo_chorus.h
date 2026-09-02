/***************************************************/
/* Part of GDSiON software synthesizer             */
/* Copyright (c) 2024 Yuri Sizov and contributors  */
/* Provided under MIT                              */
/***************************************************/

#ifndef SI_EFFECT_STEREO_CHORUS_H
#define SI_EFFECT_STEREO_CHORUS_H

//#include <godot_cpp/templates/vector.hpp>
#include "effector/si_effect_base.h"
#include <vector>



class SiEffectStereoChorus : public SiEffectBase {
	//GDCLASS(SiEffectStereoChorus, SiEffectBase)

	static const int DELAY_BUFFER_BITS = 12;
	static const int DELAY_BUFFER_FILTER = (1 << DELAY_BUFFER_BITS) - 1;

	std::vector<double> _delay_buffer_left;
	std::vector<double> _delay_buffer_right;

	int _pointer_read = 0;
	int _pointer_write = 0;
	double _feedback = 0;
	double _depth = 0;
	double _wet = 0;

	int _lfo_phase = 0;
	int _lfo_step = 0;
	int _lfo_residue_step = 0;
	int _phase_invert = 0;
	std::vector<int> _phase_table;

	void _process_channel(std::vector<double> *r_buffer, int p_buffer_index, std::vector<double> *r_delay_buffer, int p_delay);
	void _process_lfo(std::vector<double> *r_buffer, int p_start_index, int p_length);

protected:
	static void _bind_methods();

public:
	void set_params(double p_delay_time = 20, double p_feedback = 0.2, double p_frequency = 4, double p_depth = 20, double p_wet = 0.5, bool p_invert_phase = true);

	//

	virtual int prepare_process() override;
	virtual int process(int p_channels, std::vector<double> *r_buffer, int p_start_index, int p_length) override;

	virtual void set_by_mml(std::vector<double> p_args) override;
	virtual void reset() override;

	SiEffectStereoChorus(double p_delay_time = 20, double p_feedback = 0.2, double p_frequency = 4, double p_depth = 20, double p_wet = 0.5, bool p_invert_phase = true);
	~SiEffectStereoChorus() {}
};

#endif // SI_EFFECT_STEREO_CHORUS_H
