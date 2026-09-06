/*
 * Part of libSiON-cpp, forked from GDSiON software synthesizer
 * Copyright (c) 2024-2026 Yuri Sizov, James Alan Nguyen and contributors
 * Based on SiON Flash Software Synthesizer (C) 2008-2016 keim_at_Si
 * Provided under MIT License.
 */

#pragma once

namespace sion {

// Must be called before using any SiON functionality. Replaces the Godot
// module initialization (register_types.cpp) for standalone builds.
void initialize();
void finalize();

} // namespace sion
