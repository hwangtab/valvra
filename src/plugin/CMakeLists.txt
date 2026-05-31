# ─────────────────────────────────────────────────────────────────────────────
# Valvra plugin target — VST3 + AU + Standalone
#
# Uses JUCE 8 CMake helpers (juce_add_plugin). The core DSP engine lives in
# valvra_dsp (src/dsp) and is framework-independent — the plugin layer only
# handles parameter binding, MIDI, DAW host callbacks, and the UI.
# ─────────────────────────────────────────────────────────────────────────────

function(add_valvra_plugin
    TARGET_NAME
    PRODUCT_NAME
    BUNDLE_ID
    PLUGIN_CODE
    VST3_CATEGORY
    DEFAULT_EXPANSION_MODE
    DEFAULT_EXPANSION_AMOUNT
    DEFAULT_EXPANSION_MIX)
    juce_add_plugin(${TARGET_NAME}
        PLUGIN_MANUFACTURER_CODE  Vlvr
        PLUGIN_CODE               ${PLUGIN_CODE}
        FORMATS                   VST3 AU Standalone
        PRODUCT_NAME              ${PRODUCT_NAME}
        COMPANY_NAME              "Valvra Open Source"
        BUNDLE_ID                 ${BUNDLE_ID}
        VST3_CATEGORIES           "${VST3_CATEGORY}"
        AU_MAIN_TYPE              "kAudioUnitType_Effect"
        IS_SYNTH                  FALSE
        NEEDS_MIDI_INPUT          FALSE
        NEEDS_MIDI_OUTPUT         FALSE
        IS_MIDI_EFFECT            FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD   FALSE
    )

    target_sources(${TARGET_NAME} PRIVATE
        PluginProcessor.cpp
        PluginEditor.cpp
    )

    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_compile_definitions(${TARGET_NAME}
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
            JUCE_REPORT_APP_USAGE=0
            VALVRA_USE_RTNEURAL=$<BOOL:${VALVRA_USE_RTNEURAL}>
            VALVRA_DEFAULT_EXPANSION_MODE=${DEFAULT_EXPANSION_MODE}
            VALVRA_DEFAULT_EXPANSION_AMOUNT=${DEFAULT_EXPANSION_AMOUNT}
            VALVRA_DEFAULT_EXPANSION_MIX=${DEFAULT_EXPANSION_MIX}
    )

    target_link_libraries(${TARGET_NAME}
        PRIVATE
            valvra_dsp
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_dsp
            juce::juce_opengl
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
    )

    if(VALVRA_USE_RTNEURAL AND TARGET RTNeural)
        target_link_libraries(${TARGET_NAME} PRIVATE RTNeural)
    endif()
endfunction()

add_valvra_plugin(
    ValvraPlugin
    "Valvra"
    org.valvra.plugin
    Vlv1
    "Distortion"
    0
    0.0
    1.0
)
add_valvra_plugin(
    ValvraCompressorPlugin
    "Valvra Compressor"
    org.valvra.compressor
    Vlc1
    "Dynamics"
    1
    0.70
    1.0
)
add_valvra_plugin(
    ValvraTapePlugin
    "Valvra Tape"
    org.valvra.tape
    Vlt1
    "Distortion"
    3
    0.75
    1.0
)

if(MSVC)
    set_source_files_properties(PluginProcessor.cpp PluginEditor.cpp
        PROPERTIES COMPILE_OPTIONS /W4
    )
else()
    set_source_files_properties(PluginProcessor.cpp PluginEditor.cpp
        PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Wpedantic"
    )
endif()
