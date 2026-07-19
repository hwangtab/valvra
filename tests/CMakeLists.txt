# Unit tests — validates DSP against academic benchmarks

add_executable(valvra_tests
    test_koren_triode.cpp
    test_koren_pentode.cpp
    test_jiles_atherton.cpp
    test_cathode_bounce.cpp
    test_power_supply_sag.cpp
    test_component_variation.cpp
    test_tube_stage.cpp
    test_tube_amp_chain.cpp
    test_polyphase_oversampler.cpp
    test_true_peak_limiter.cpp
    test_push_pull_stage.cpp
    test_compound_topologies.cpp
    test_expansion_rack.cpp
    test_golden_render.cpp
)

target_link_libraries(valvra_tests
    PRIVATE
        valvra_dsp
        Catch2::Catch2WithMain
)

if(MSVC)
    target_compile_options(valvra_tests PRIVATE /W4)
else()
    target_compile_options(valvra_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

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
    if(MSVC)
        target_compile_options(valvra_cli_tests PRIVATE /W4)
    else()
        target_compile_options(valvra_cli_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_dependencies(valvra_cli_tests valvra_process)
    catch_discover_tests(valvra_cli_tests)

    if(VALVRA_ENABLE_FIT_PIPELINE_TEST)
        find_package(Python3 COMPONENTS Interpreter)
        if(Python3_Interpreter_FOUND)
            set(VALVRA_FIT_PIPELINE_SUMMARY
                "${CMAKE_BINARY_DIR}/artifacts/external_refit_pipeline_summary_ctest.json")
            set(VALVRA_FIT_PIPELINE_VERIFY
                "${CMAKE_BINARY_DIR}/artifacts/fit_verify_summary_ctest.json")
            add_test(
                NAME valvra_external_refit_pipeline_smoke
                COMMAND ${Python3_EXECUTABLE}
                        ${CMAKE_SOURCE_DIR}/scripts/run_external_refit_pipeline.py
                        --dataset-dir ${CMAKE_BINARY_DIR}/artifacts/external_ctest
                        --pattern {mode}_external_capture_v1.npz
                        --bootstrap-external-from-internal
                        --coarse-trials 0
                        --max-nfev 1
                        --hill-iters 0
                        --verify-json ${VALVRA_FIT_PIPELINE_VERIFY}
                        --summary-json ${VALVRA_FIT_PIPELINE_SUMMARY}
            )
            set_tests_properties(valvra_external_refit_pipeline_smoke PROPERTIES
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                # The physics overhaul (per-sample node solvers + factor-
                # scaled oversampling FIRs) lengthened the full 5-mode
                # refit render from ~13 to ~25 min on an 8-core arm64.
                TIMEOUT 3600
                LABELS "fit_pipeline;plugin_stress"
            )
        endif()
    endif()
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
            JUCE_REPORT_APP_USAGE=0
            VALVRA_DISABLE_EDITOR_OPENGL=1
            VALVRA_ENABLE_UI_TEST_HOOKS=1
            VALVRA_USE_RTNEURAL=$<BOOL:${VALVRA_USE_RTNEURAL}>
    )

    target_link_libraries(valvra_plugin_tests
        PRIVATE
            valvra_dsp
            Catch2::Catch2WithMain
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_dsp
            juce::juce_opengl
    )
    if(VALVRA_USE_RTNEURAL AND TARGET RTNeural)
        target_link_libraries(valvra_plugin_tests PRIVATE RTNeural)
    endif()

    target_compile_features(valvra_plugin_tests PRIVATE cxx_std_20)
    if(MSVC)
        set_source_files_properties(
            test_plugin_processor_mono.cpp
            ${CMAKE_SOURCE_DIR}/src/plugin/PluginProcessor.cpp
            ${CMAKE_SOURCE_DIR}/src/plugin/PluginEditor.cpp
            PROPERTIES COMPILE_OPTIONS /W4
        )
    else()
        set_source_files_properties(
            test_plugin_processor_mono.cpp
            ${CMAKE_SOURCE_DIR}/src/plugin/PluginProcessor.cpp
            ${CMAKE_SOURCE_DIR}/src/plugin/PluginEditor.cpp
            PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Wpedantic;-Werror=reorder-init-list"
        )
    endif()
    # CTest labels for CI sharding:
    #   - plugin: regular plugin-level processor tests (non-stress)
    #   - plugin_stress: heavy transition/continuity stress subset
    #
    # Partition by Catch tag to avoid duplicate registration.
    catch_discover_tests(valvra_plugin_tests
        TEST_SPEC "~[stress]"
        PROPERTIES LABELS "plugin"
    )
    catch_discover_tests(valvra_plugin_tests
        TEST_SPEC "[stress]"
        PROPERTIES LABELS "plugin_stress"
    )
endif()
