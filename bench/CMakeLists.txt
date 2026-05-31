# ─────────────────────────────────────────────────────────────────────────────
# valvra_bench — wall-time benchmark of the full plugin signal path.
#
# Re-compiles PluginProcessor.cpp + PluginEditor.cpp into the bench binary
# (mirroring valvra_plugin_tests) so the benchmark exercises the same code
# the DAW host runs, including JUCE's parameter wiring and bus management.
# ─────────────────────────────────────────────────────────────────────────────

if(NOT VALVRA_BUILD_PLUGIN)
    message(STATUS "valvra_bench skipped (VALVRA_BUILD_PLUGIN is OFF)")
    return()
endif()

add_executable(valvra_bench
    valvra_bench.cpp
    ${CMAKE_SOURCE_DIR}/src/plugin/PluginProcessor.cpp
    ${CMAKE_SOURCE_DIR}/src/plugin/PluginEditor.cpp
)

target_include_directories(valvra_bench
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/plugin
)

# Same JUCE-stripping defines as the plugin / tests so we don't drag in
# the curl + web-browser stack for a CPU benchmark.
target_compile_definitions(valvra_bench PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0
    VALVRA_USE_RTNEURAL=$<BOOL:${VALVRA_USE_RTNEURAL}>
)

if(MSVC)
    set_source_files_properties(
        valvra_bench.cpp
        ${CMAKE_SOURCE_DIR}/src/plugin/PluginProcessor.cpp
        ${CMAKE_SOURCE_DIR}/src/plugin/PluginEditor.cpp
        PROPERTIES COMPILE_OPTIONS /W4
    )
else()
    set_source_files_properties(
        valvra_bench.cpp
        ${CMAKE_SOURCE_DIR}/src/plugin/PluginProcessor.cpp
        ${CMAKE_SOURCE_DIR}/src/plugin/PluginEditor.cpp
        PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Wpedantic"
    )
endif()

target_link_libraries(valvra_bench
    PRIVATE
        valvra_dsp
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_opengl
)

if(VALVRA_USE_RTNEURAL AND TARGET RTNeural)
    target_link_libraries(valvra_bench PRIVATE RTNeural)
endif()

target_compile_features(valvra_bench PRIVATE cxx_std_20)
