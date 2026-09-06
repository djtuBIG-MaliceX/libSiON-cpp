###########################################################################
# Part of libSiON-cpp, forked from GDSiON software synthesizer            #
# Copyright (c) 2024-2026 Yuri Sizov, James Alan Nguyen and contributors/ #
# Based on SiON Flash Software Synthesizer (C) 2008-2016 keim_at_Si       #
# Provided under MIT License.                                             #
###########################################################################

# Emscripten target: builds the player module (sion_wasm.mjs + sion_wasm.wasm)
# consumed by web/sion_worklet.js, and stages index.html / test scripts next to
# it so build_web can be served as-is.

add_executable(sion_wasm sion_web.cpp)
target_link_libraries(sion_wasm PRIVATE SiONcpp::SiONcpp)

# The compat umbrella is force-included in the library target privately; the
# facade includes driver headers directly, so it needs it too.
target_compile_options(sion_wasm PRIVATE "-include" "${CMAKE_SOURCE_DIR}/src/compat/sion_compat.h")

set_target_properties(sion_wasm PROPERTIES
	OUTPUT_NAME "sion_wasm"
	SUFFIX ".js"
)

target_link_options(sion_wasm PRIVATE
	"--no-entry"
	"-sMODULARIZE=1"
	"-sEXPORT_NAME=createSiONModule"
	# Classic (non-ESM) glue: loaded with <script src> by the page, and with
	# require() by test_node.js. AudioWorklet hosting was ruled out — Chrome's
	# WorkletGlobalScope exposes no importScripts/fetch/self and forbids
	# dynamic import(), so only SharedArrayBuffer streaming could host WASM
	# there (needs COOP/COEP headers); the page renders via ScriptProcessorNode.
	"-sENVIRONMENT=web,node"
	"-sEXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString,HEAPF32"
	"-sEXPORTED_FUNCTIONS=[_sion_web_init,_sion_web_play,_sion_web_stop,_sion_web_update,_sion_web_render,_sion_web_buffer,_sion_web_is_streaming,_sion_web_set_volume,_sion_web_last_error]"
	"-sALLOW_MEMORY_GROWTH=1"
)

# Stage the page + glue into the build dir on every build (so editing them
# needs no relink) — the dir is then directly servable, e.g.
# `python -m http.server` from it, then open index.html.
file(GLOB LIBSION_WEB_ASSETS CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/index.html"
	"${CMAKE_CURRENT_SOURCE_DIR}/sion_engine.js"
	"${CMAKE_CURRENT_SOURCE_DIR}/test_node.js"
)
add_custom_target(sion_web_stage ALL
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different
		${LIBSION_WEB_ASSETS} "$<TARGET_FILE_DIR:sion_wasm>"
	DEPENDS sion_wasm
 COMMENT "Staging web player assets"
)
