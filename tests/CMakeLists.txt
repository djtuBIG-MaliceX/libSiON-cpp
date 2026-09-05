####################################################
# Part of GDSiON software synthesizer              #
# Copyright (c) 2024 Yuri Sizov and contributors   #
# Provided under MIT                               #
####################################################

# Native tests. The Phase 1 milestone is a compile-only MML smoke test; the
# golden-based suites (mml-compilation, voices-sound-consistency) land in
# Phase 4.

function(libsion_add_test name source)
	add_executable(${name} ${source})
	target_link_libraries(${name} PRIVATE SiONcpp::SiONcpp)
	if(MSVC)
		target_compile_options(${name} PRIVATE "/FI${CMAKE_CURRENT_SOURCE_DIR}/../src/compat/sion_compat.h")
	else()
		target_compile_options(${name} PRIVATE "-include${CMAKE_CURRENT_SOURCE_DIR}/../src/compat/sion_compat.h")
	endif()
	add_test(NAME ${name} COMMAND ${name})
endfunction()

libsion_add_test(smoke_mml_compile smoke_mml_compile.cpp)

# Phase 2 milestone: standalone SiONDriver offline render + realtime chunk streaming.
if(LIBSION_DRIVER_EXPERIMENTAL)
	libsion_add_test(smoke_driver_render smoke_driver_render.cpp)

	# Phase 4 goldens: audio determinism gate (int32 captures vs *.dat).
	libsion_add_test(voices_sound_consistency voices_sound_consistency.cpp)
	target_compile_definitions(voices_sound_consistency PRIVATE
		LIBSION_VOICES_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/run/voices-sound-consistency/data")

	# Phase 4 goldens: MML compilation error output over the vendored SiONMML corpus.
	libsion_add_test(mml_compilation mml_compilation.cpp)
	target_compile_definitions(mml_compilation PRIVATE
		LIBSION_MML_MANIFEST="${CMAKE_CURRENT_SOURCE_DIR}/data/mml_manifest.tsv"
		LIBSION_MML_INPUT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/mml"
		LIBSION_MML_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/run/mml-compilation/outputs")
endif()
