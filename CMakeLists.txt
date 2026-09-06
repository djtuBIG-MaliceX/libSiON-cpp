####################################################
# Part of GDSiON software synthesizer              #
# Copyright (c) 2024 Yuri Sizov and contributors   #
# Provided under MIT                               #
####################################################

cmake_minimum_required(VERSION 3.20)

# C is required because PortAudio (CLI backend) skips its own project() call
# when nested, leaving the C language disabled for its targets otherwise.
project(libSiON-cpp VERSION 0.7.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static deps (PortAudio, PCRE2) end up inside PIE executables on Linux — their
# own CMake scripts don't all request position-independent code.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(LIBSION_BUILD_CLI "Build the sion-cpp-play command-line tool" OFF)
option(LIBSION_BUILD_TESTS "Build native tests" OFF)
option(LIBSION_DRIVER_EXPERIMENTAL "Include SiONDriver in the library" ON)
option(LIBSION_BUILD_WEB "Build the Emscripten WebAssembly player module" ${EMSCRIPTEN})

if(LIBSION_BUILD_WEB AND NOT EMSCRIPTEN)
	message(FATAL_ERROR "LIBSION_BUILD_WEB requires the Emscripten toolchain (configure with emcmake)")
endif()

if(POLICY CMP0135)
	cmake_policy(SET CMP0135 NEW)
endif()

include(FetchContent)

# --- Dependencies

# PCRE2 powers the RegEx shim (same engine Godot wrapped, for exact match
# semantics like the (?s) inline flag used throughout the MML parsers).
set(PCRE2_BUILD_PCRE2GREP OFF CACHE BOOL "" FORCE)
set(PCRE2_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(PCRE2_SUPPORT_LIBEDIT OFF CACHE BOOL "" FORCE)
set(PCRE2_SUPPORT_LIBREADLINE OFF CACHE BOOL "" FORCE)
set(PCRE2_SUPPORT_LIBZ OFF CACHE BOOL "" FORCE)
set(PCRE2_SUPPORT_LIBBZ2 OFF CACHE BOOL "" FORCE)
# sljit has no WASM backend; PCRE2 falls back to the interpreter there.
if(EMSCRIPTEN)
	set(PCRE2_SUPPORT_JIT OFF CACHE BOOL "" FORCE)
else()
	set(PCRE2_SUPPORT_JIT ON CACHE BOOL "" FORCE)
endif()
set(PCRE2_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

set(_libsion_policy_min_saved "${CMAKE_POLICY_VERSION_MINIMUM}")
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

FetchContent_Declare(
	pcre2
	GIT_REPOSITORY https://github.com/PCRE2Project/pcre2.git
	GIT_TAG pcre2-10.44
	GIT_SHALLOW ON
)
FetchContent_MakeAvailable(pcre2)

if(NOT "${_libsion_policy_min_saved}" STREQUAL "")
	set(CMAKE_POLICY_VERSION_MINIMUM "${_libsion_policy_min_saved}")
endif()

if(NOT EMSCRIPTEN)
	find_package(Threads QUIET)
endif()

# --- Library

file(GLOB_RECURSE LIBSION_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
list(REMOVE_ITEM LIBSION_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/register_types.cpp")
if(NOT LIBSION_DRIVER_EXPERIMENTAL)
	list(REMOVE_ITEM LIBSION_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/sion_driver.cpp")
endif()
list(REMOVE_ITEM LIBSION_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/compat/sion_regex.cpp")

add_library(SiONcpp STATIC
	${LIBSION_SOURCES}
	src/compat/sion_regex.cpp
)
add_library(SiONcpp::SiONcpp ALIAS SiONcpp)

target_include_directories(SiONcpp PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(SiONcpp PUBLIC pcre2-8-static)
if(Threads_FOUND)
	target_link_libraries(SiONcpp PUBLIC Threads::Threads)
endif()

# Force-include the compat umbrella so shims are always visible.
if(MSVC)
	target_compile_options(SiONcpp PRIVATE "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/compat/sion_compat.h")
else()
	target_compile_options(SiONcpp PRIVATE "-include" "${CMAKE_CURRENT_SOURCE_DIR}/src/compat/sion_compat.h")
endif()

# --- Subdirectories

if(LIBSION_BUILD_WEB)
	add_subdirectory(web)
endif()

if(LIBSION_BUILD_CLI)
	add_subdirectory(cli)
endif()

if(LIBSION_BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()
