/*
 * Part of libSiON-cpp, forked from GDSiON software synthesizer
 * Copyright (c) 2024-2026 Yuri Sizov, James Alan Nguyen and contributors
 * Based on SiON Flash Software Synthesizer (C) 2008-2016 keim_at_Si
 * Provided under MIT License.
 */

#ifndef SION_COMPAT_H
#define SION_COMPAT_H

// Umbrella header force-included into every translation unit of the library,
// CLI and tests, so the compatibility shims are always available.

#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "compat/sion_containers.h"
#include "templates/singly_linked_list.h"
#include "compat/sion_errors.h"
#include "compat/sion_math.h"
#include "compat/sion_string.h"
#include "compat/sion_regex.h"
#include "compat/sion_audio.h"
#include "compat/sion_random.h"
#include "compat/sion_time.h"

// Godot branch hints.
#ifndef likely
#define likely(x) (!!(x))
#endif
#ifndef unlikely
#define unlikely(x) (!!(x))
#endif

#endif // SION_COMPAT_H
