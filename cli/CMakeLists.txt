####################################################
# Part of GDSiON software synthesizer              #
# Copyright (c) 2024 Yuri Sizov and contributors   #
# Provided under MIT                               #
####################################################

# gdsion-play: play MML via PortAudio or export rendered audio to WAV.

include(FetchContent)

find_package(PortAudio CONFIG QUIET)

if(NOT PortAudio_FOUND)
	set(PA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set(PA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
	set(PA_BUILD_SHARED OFF CACHE BOOL "" FORCE)
	set(PA_BUILD_STATIC ON CACHE BOOL "" FORCE)

	FetchContent_Declare(
		portaudio
		URL https://github.com/PortAudio/portaudio/archive/refs/tags/v19.7.0.tar.gz
	)
	FetchContent_MakeAvailable(portaudio)
endif()

add_executable(gdsion-play main.cpp wav_writer.h)
target_link_libraries(gdsion-play PRIVATE gdsion::gdsion)

# Prefer the static PortAudio flavor for a self-contained CLI binary.
if(TARGET portaudio_static)
	target_link_libraries(gdsion-play PRIVATE portaudio_static)
elseif(TARGET PortAudio::PortAudioStatic)
	target_link_libraries(gdsion-play PRIVATE PortAudio::PortAudioStatic)
elseif(TARGET portaudio)
	target_link_libraries(gdsion-play PRIVATE portaudio)
elseif(TARGET PortAudio::PortAudio)
	target_link_libraries(gdsion-play PRIVATE PortAudio::PortAudio)
else()
	message(FATAL_ERROR "PortAudio target not found.")
endif()

if(MSVC)
	target_compile_options(gdsion-play PRIVATE "/FI${CMAKE_CURRENT_SOURCE_DIR}/../src/compat/sion_compat.h")
else()
	target_compile_options(gdsion-play PRIVATE "-include${CMAKE_CURRENT_SOURCE_DIR}/../src/compat/sion_compat.h")
endif()

set_target_properties(gdsion-play PROPERTIES OUTPUT_NAME "gdsion-play")
