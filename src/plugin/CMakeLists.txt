# ─────────────────────────────────────────────────────────────────────────────
# Valvra plugin target — VST3 + AU + Standalone
#
# Uses JUCE 8 CMake helpers (juce_add_plugin). The core DSP engine lives in
# valvra_dsp (src/dsp) and is framework-independent — the plugin layer only
# handles parameter binding, MIDI, DAW host callbacks, and the UI.
# ─────────────────────────────────────────────────────────────────────────────

juce_add_plugin(ValvraPlugin
    PLUGIN_MANUFACTURER_CODE  Vlvr
    PLUGIN_CODE               Vlv1
    FORMATS                   VST3 AU Standalone
    PRODUCT_NAME              "Valvra"
    COMPANY_NAME              "Valvra Open Source"
    BUNDLE_ID                 org.valvra.plugin
    VST3_CATEGORIES           "Fx|Distortion"
    AU_MAIN_TYPE              "kAudioUnitType_Effect"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   FALSE
)

target_sources(ValvraPlugin PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp
)

target_include_directories(ValvraPlugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(ValvraPlugin
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
)

target_link_libraries(ValvraPlugin
    PRIVATE
        valvra_dsp
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
