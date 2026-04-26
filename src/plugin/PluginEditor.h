// ─────────────────────────────────────────────────────────────────────────────
// PluginEditor.h — Valvra plugin GUI (Tier 2)
//
// Layout:
//   ┌─ [ VALVRA | Living Tube Amp Colour ] ────────────────────────────┐
//   │  Mode ▾            Quality ▾                                     │
//   │                                                                  │
//   │  [Drive]   [Output]   [Mix]                                      │
//   │                                                                  │
//   │  ┌── B-H Hysteresis ──┐   ┌── Harmonics (dBc) ──┐                │
//   │  │                    │   │                      │               │
//   │  └────────────────────┘   └──────────────────────┘               │
//   │                                                                  │
//   │  [Reroll]  Seed: 0xDEADBEEF          [□ Null Test]               │
//   └──────────────────────────────────────────────────────────────────┘
//
// References: docs/16 (UI/UX spec), docs/20 §4.7 (signature features)
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "SignatureViews.h"

namespace valvra {

/// Custom LookAndFeel — dark vintage chassis with warm tube-glow accents.
class VintageLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    VintageLookAndFeel();

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider&) override;

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool isMouseOverButton,
                              bool isButtonDown) override;

    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
};

class ValvraEditor final : public juce::AudioProcessorEditor
{
public:
    explicit ValvraEditor(ValvraProcessor& proc);
    ~ValvraEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    ValvraProcessor& processor_;
    VintageLookAndFeel lnf_;

    // Parameters
    juce::ComboBox presetBox_;
    juce::Slider   driveKnob_;
    juce::Slider   outputKnob_;
    juce::Slider   mixKnob_;
    juce::ComboBox oversampleBox_;

    // Factory preset loader
    juce::ComboBox     factoryPresetBox_;
    juce::Label        factoryPresetLabel_ { {}, "Factory" };

    // Signature features
    HysteresisLoopView hysteresisView_;
    HarmonicMeterView  harmonicView_;
    NullTestToggle     nullTestToggle_;
    juce::TextButton   rerollButton_ { "Reroll" };
    juce::Label        seedLabel_;

    // Labels
    juce::Label presetLabel_     { {}, "Mode" };
    juce::Label driveLabel_      { {}, "Drive" };
    juce::Label outputLabel_     { {}, "Output" };
    juce::Label mixLabel_        { {}, "Mix" };
    juce::Label oversampleLabel_ { {}, "Quality" };

    using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttach  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<ComboAttach>  presetAttach_;
    std::unique_ptr<SliderAttach> driveAttach_;
    std::unique_ptr<SliderAttach> outputAttach_;
    std::unique_ptr<SliderAttach> mixAttach_;
    std::unique_ptr<ComboAttach>  oversampleAttach_;

    void updateSeedLabel();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ValvraEditor)
};

} // namespace valvra
