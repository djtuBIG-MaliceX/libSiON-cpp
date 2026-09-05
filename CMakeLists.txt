####################################################
# Part of GDSiON software synthesizer              #
# Copyright (c) 2024 Yuri Sizov and contributors   #
# Provided under MIT                               #
####################################################

cmake_minimum_required(VERSION 3.20)

project(gdsion VERSION 0.7.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GDSION_BUILD_CLI "Build the gdsion-play command-line tool" OFF)
option(GDSION_BUILD_TESTS "Build native tests" OFF)
option(GDSION_DRIVER_EXPERIMENTAL "Include SiONDriver in the library (WIP during libification)" OFF)

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
set(PCRE2_SUPPORT_JIT ON CACHE BOOL "" FORCE)
set(PCRE2_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

set(_gdsion_policy_min_saved "${CMAKE_POLICY_VERSION_MINIMUM}")
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

FetchContent_Declare(
	pcre2
	GIT_REPOSITORY https://github.com/PCRE2Project/pcre2.git
	GIT_TAG pcre2-10.44
	GIT_SHALLOW ON
)
FetchContent_MakeAvailable(pcre2)

if(NOT "${_gdsion_policy_min_saved}" STREQUAL "")
	set(CMAKE_POLICY_VERSION_MINIMUM "${_gdsion_policy_min_saved}")
endif()

find_package(Threads QUIET)

# --- Library

file(GLOB_RECURSE GDSION_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
list(REMOVE_ITEM GDSION_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/register_types.cpp")
if(NOT GDSION_DRIVER_EXPERIMENTAL)
	list(REMOVE_ITEM GDSION_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/sion_driver.cpp")
endif()
list(REMOVE_ITEM GDSION_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/compat/sion_regex.cpp")

add_library(gdsion STATIC
	${GDSION_SOURCES}
	src/compat/sion_regex.cpp
)
add_library(gdsion::gdsion ALIAS gdsion)

target_include_directories(gdsion PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(gdsion PUBLIC pcre2-8-static)
if(Threads_FOUND)
	target_link_libraries(gdsion PUBLIC Threads::Threads)
endif()

# Force-include the compat umbrella so shims are always visible.
if(MSVC)
	target_compile_options(gdsion PRIVATE "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/compat/sion_compat.h")
else()
	target_compile_options(gdsion PRIVATE "-include${CMAKE_CURRENT_SOURCE_DIR}/src/compat/sion_compat.h")
endif()

# --- Subdirectories

if(GDSION_BUILD_CLI)
	add_subdirectory(cli)
endif()

if(GDSION_BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()
