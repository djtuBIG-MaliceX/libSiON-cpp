/*
 * Part of libSiON-cpp, forked from GDSiON software synthesizer
 * Copyright (c) 2024-2026 Yuri Sizov, James Alan Nguyen and contributors
 * Based on SiON Flash Software Synthesizer (C) 2008-2016 keim_at_Si
 * Provided under MIT License.
 */

#ifndef MML_SYSTEM_COMMAND_H
#define MML_SYSTEM_COMMAND_H

//#include <godot_cpp/classes/ref_counted.hpp>
//#include <godot_cpp/variant/string.hpp>



class MMLSystemCommand {
	//GDCLASS(MMLSystemCommand, RefCounted)

protected:
	static void _bind_methods() {}

public:
	// For the given MML string "#ABC5{def}ghi;"...

	sion::String command; // Command name; always starts with "#", e.g. command = "#ABC"
	int number = 0; // Number after command, e.g. number = 5
	sion::String content; // sion::String inside {..}, e.g. content = "def"
	sion::String postfix; // sion::String at the end of the command, e.g. postfix = "ghi"

	MMLSystemCommand() {}
	~MMLSystemCommand() {}
};

#endif // MML_SYSTEM_COMMAND_H
