// ─────────────────────────────────────────────────────────────────────────────
// FactoryPresets.h — curated parameter snapshots shipped with the plugin.
//
// A preset is just a (name, parameter-map, seed) tuple.  Loading a preset:
//   1) sets AudioParameterValueTreeState values → the UI & chain update
//   2) stores a fixed seed → reproducible character per preset
//
// These cover the common use cases a friend would reach for: drum bus glue,
// vocal warmth, bass DI, mix-bus master colour, etc.  Keeping them in header
// form (no JSON load at startup) avoids a new runtime dependency.
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace valvra {

/// Mode index — must match the order in PluginProcessor's "preset"
/// AudioParameterChoice (V72=0, ConsoleOutput=1, ...) so saved-state
/// integers continue to map to the right chain config.  ConsoleOutput
/// was previously labelled "Marshall" and tuned as a guitar-amp output
/// stage; it is now a class-A1 console-style power-tube colour suitable
/// for mix and mastering use.  Push Drive past ~2 to recover the older
/// guitar-crunch territory.
enum class PresetMode : int {
    V72            = 0,
    ConsoleOutput  = 1,
    CultureVulture = 2,
    RNDI           = 3,
    HiFi300B       = 4,
};

struct FactoryPreset
{
    std::string  name;
    std::string  category;      ///< "Drums", "Vocal", "Bass", …
    PresetMode   mode;
    float        drive;         ///< matches kParamDrive range [0, 3]
    float        outputDb;      ///< matches kParamOutputDb range [−24, +24]
    float        mix;           ///< [0, 1]
    int          oversampleIdx; ///< 0=1x, 1=2x, 2=4x, 3=8x, 4=16x
    std::uint64_t seed;
    int          cvModeIdx { 1 }; ///< 0=T, 1=P1, 2=P2
};

inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> kPresets = {
        // ── Drums ─────────────────────────────────────────────────────────
        {
            "Drum Bus Glue", "Drums",
            PresetMode::V72, 0.8f, -1.0f, 1.0f, 2, 0xD70BAA55ULL
        },
        {
            "Punchy Kick", "Drums",
            PresetMode::ConsoleOutput, 1.4f, -4.0f, 0.85f, 2, 0x12340042ULL
        },
        {
            "Snare Body", "Drums",
            PresetMode::V72, 1.2f, -2.0f, 0.7f, 2, 0x53194155ULL
        },
        {
            "Room Mics Vulture", "Drums",
            PresetMode::CultureVulture, 1.6f, -6.0f, 0.6f, 2, 0xCA11EEUL, 2
        },

        // ── Vocals ────────────────────────────────────────────────────────
        {
            "Lead Vocal Warmth", "Vocal",
            PresetMode::V72, 0.6f, 0.0f, 1.0f, 2, 0x110CA11ULL
        },
        {
            "Vocal Double", "Vocal",
            PresetMode::CultureVulture, 0.9f, -2.0f, 0.9f, 2, 0xD081E42ULL, 0
        },
        {
            "Rap Vocal Aggro", "Vocal",
            PresetMode::ConsoleOutput, 2.0f, -5.0f, 1.0f, 2, 0xAAF19421ULL
        },

        // ── Bass / DI ─────────────────────────────────────────────────────
        {
            "Bass DI Push", "Bass",
            PresetMode::RNDI, 1.2f, -1.0f, 1.0f, 2, 0xBA55ULL
        },
        {
            "Bass Amp Color", "Bass",
            PresetMode::V72, 1.0f, 0.0f, 0.85f, 2, 0x5E3D1ULL
        },

        // ── Mix / Master ──────────────────────────────────────────────────
        {
            "Mix Bus Subtle", "Master",
            PresetMode::V72, 0.4f, 0.0f, 0.5f, 2, 0xFEED42ULL
        },
        {
            "Master Glue", "Master",
            PresetMode::V72, 0.35f, 0.0f, 0.7f, 3, 0xBEEFFULL
        },
        {
            "Guitar Crunch", "Guitar",
            PresetMode::ConsoleOutput, 2.8f, -3.0f, 1.0f, 3, 0x77AAEEULL
        },
        {
            "Console Master Drive", "Master",
            PresetMode::ConsoleOutput, 0.9f, 0.0f, 0.65f, 3, 0xC0501FULL
        },
    };
    return kPresets;
}

} // namespace valvra
