# ─────────────────────────────────────────────────────────────────────────────
# Valvra — Sensational Tube Amp Coloration Plugin
# Open-source GPL-3 tube amp emulator with time-varying dynamics +
# Monte Carlo instance variation + real-time Jiles-Atherton hysteresis
# References:
#   docs/20-mvp-scope-decision.md (v2.0 sensational scope)
#   docs/24-tube-amp-target-hardware.md (V72/Marshall/CV/RNDI modes)
#   docs/07-implementation-strategies.md (DSP engine)
# ─────────────────────────────────────────────────────────────────────────────

cmake_minimum_required(VERSION 3.25)

project(Valvra
    VERSION 0.0.1
    DESCRIPTION "Tube amp coloration with time-varying dynamics, Monte Carlo variation, Jiles-Atherton transformer"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─────────────────────────────────────────────────────────────────────────────
# Build options
# ─────────────────────────────────────────────────────────────────────────────
option(VALVRA_BUILD_TESTS    "Build DSP unit tests"            ON)
option(VALVRA_BUILD_PLUGIN   "Build VST3/AU/Standalone plugin" ON)
option(VALVRA_BUILD_CLI      "Build valvra_process CLI for validation" ON)
option(VALVRA_BUILD_BENCHES  "Build micro-benchmarks"          OFF)
option(VALVRA_USE_CHOWDSP    "Fetch chowdsp_utils (needed for plugin, optional for core DSP)" ${VALVRA_BUILD_PLUGIN})
option(VALVRA_USE_RTNEURAL   "Enable RTNeural runtime model loading" OFF)
option(VALVRA_ENABLE_FIT_PIPELINE_TEST
       "Enable external refit pipeline smoke test in CTest"
       ON)

# Release flags
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Warning and optimization flags are target-scoped below. Keep this file from
# adding global compile options so third-party JUCE/Catch sources don't inherit
# project warning policy.

# ─────────────────────────────────────────────────────────────────────────────
# Dependencies (fetched at configure time — no manual clone required)
# ─────────────────────────────────────────────────────────────────────────────
include(FetchContent)

# JUCE 8 (GPL-3) — plugin framework
if(VALVRA_BUILD_PLUGIN)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

# chowdsp_utils — WDF, oversampling, DSP primitives (GPL-3/BSD mixed modules)
# Most of chowdsp_utils assumes JUCE is present; only fetched when plugin target is active.
if(VALVRA_USE_CHOWDSP)
    FetchContent_Declare(
        chowdsp_utils
        GIT_REPOSITORY https://github.com/Chowdhury-DSP/chowdsp_utils.git
        GIT_TAG        master
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(chowdsp_utils)
endif()

# RTNeural — optional neural inference backend (BSD-3).
if(VALVRA_USE_RTNEURAL)
    # Prefer STL backend for minimal dependency footprint in this repo.
    set(RTNEURAL_STL ON CACHE BOOL "Use RTNeural STL backend" FORCE)
    FetchContent_Declare(
        RTNeural
        GIT_REPOSITORY https://github.com/jatinchowdhury18/RTNeural.git
        GIT_TAG        main
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(RTNeural)
endif()

# Catch2 — unit tests (BSL-1.0)
if(VALVRA_BUILD_TESTS)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.5.2
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(Catch2)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Core DSP library (plugin-framework independent)
# ─────────────────────────────────────────────────────────────────────────────
add_subdirectory(src/dsp)

# ─────────────────────────────────────────────────────────────────────────────
# Plugin target
# ─────────────────────────────────────────────────────────────────────────────
if(VALVRA_BUILD_PLUGIN)
    add_subdirectory(src/plugin)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Standalone CLI (for headless validation and Python scripts)
# ─────────────────────────────────────────────────────────────────────────────
if(VALVRA_BUILD_CLI)
    add_subdirectory(src/cli)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Tests
# ─────────────────────────────────────────────────────────────────────────────
if(VALVRA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Benchmarks (off by default — compile/run only when explicitly requested,
# since they pull in JUCE just to instrument the plugin's hot path).
# ─────────────────────────────────────────────────────────────────────────────
if(VALVRA_BUILD_BENCHES)
    add_subdirectory(bench)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Summary
# ─────────────────────────────────────────────────────────────────────────────
message(STATUS "")
message(STATUS "Valvra ${PROJECT_VERSION} configuration:")
message(STATUS "  C++ standard:   ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "  Build plugin:   ${VALVRA_BUILD_PLUGIN}")
message(STATUS "  Build tests:    ${VALVRA_BUILD_TESTS}")
message(STATUS "  Build benches:  ${VALVRA_BUILD_BENCHES}")
message(STATUS "  Use RTNeural:   ${VALVRA_USE_RTNEURAL}")
message(STATUS "")
