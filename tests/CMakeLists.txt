# Unit tests — validates DSP against academic benchmarks

add_executable(valvra_tests
    test_koren_triode.cpp
    test_jiles_atherton.cpp
    test_cathode_bounce.cpp
    test_power_supply_sag.cpp
    test_component_variation.cpp
    test_tube_stage.cpp
    test_tube_amp_chain.cpp
    test_polyphase_oversampler.cpp
)

target_link_libraries(valvra_tests
    PRIVATE
        valvra_dsp
        Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(valvra_tests)

if(TARGET valvra_process)
    add_executable(valvra_cli_tests
        test_cli_regression.cpp
    )

    target_link_libraries(valvra_cli_tests
        PRIVATE
            Catch2::Catch2WithMain
    )

    target_compile_features(valvra_cli_tests PRIVATE cxx_std_20)
    target_compile_definitions(valvra_cli_tests PRIVATE
        VALVRA_PROCESS_PATH="$<TARGET_FILE:valvra_process>"
    )
    add_dependencies(valvra_cli_tests valvra_process)
    catch_discover_tests(valvra_cli_tests)
endif()

if(VALVRA_BUILD_PLUGIN)
    add_executable(valvra_plugin_tests
        test_plugin_processor_mono.cpp
        ${CMAKE_SOURCE_DIR}/src/plugin/PluginProcessor.cpp
        ${CMAKE_SOURCE_DIR}/src/plugin/PluginEditor.cpp
    )

    target_include_directories(valvra_plugin_tests
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src/plugin
    )

    target_compile_definitions(valvra_plugin_tests
        PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
            JUCE_DISPLAY_SPLASH_SCREEN=0
            JUCE_REPORT_APP_USAGE=0
    )

    target_link_libraries(valvra_plugin_tests
        PRIVATE
            valvra_dsp
            Catch2::Catch2WithMain
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_dsp
    )

    target_compile_features(valvra_plugin_tests PRIVATE cxx_std_20)
    catch_discover_tests(valvra_plugin_tests)
endif()
