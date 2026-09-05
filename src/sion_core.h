/***************************************************/
/* Part of GDSiON software synthesizer             */
/* Copyright (c) 2024 Yuri Sizov and contributors  */
/* Provided under MIT                              */
/***************************************************/

#pragma once

namespace sion {

// Must be called before using any SiON functionality. Replaces the Godot
// module initialization (register_types.cpp) for standalone builds.
void initialize();
void finalize();

} // namespace sion
