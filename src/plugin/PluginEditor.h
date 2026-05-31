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

#include <array>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_opengl/juce_opengl.h>

#include "PluginProcessor.h"
#include "SignatureViews.h"

namespace valvra {

/// Custom LookAndFeel — dark vintage chassis with warm tube-glow accents.
class VintageLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    VintageLookAndFeel();
    void setUiScale(float scale) noexcept
    {
        uiScale_ = juce::jlimit(1.0f, 2.0f, scale);
    }

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

    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;

private:
    float uiScale_ { 1.0f };
};

class ChainBuilderView final : public juce::Component,
                               private juce::Timer
{
public:
    explicit ChainBuilderView(ValvraProcessor& proc);
    void setUiScale(float scale) noexcept;
    void paint(juce::Graphics& g) override;

#if defined(VALVRA_ENABLE_UI_TEST_HOOKS) && VALVRA_ENABLE_UI_TEST_HOOKS
    struct DebugNodeBands
    {
        juce::Rectangle<float> node;
        juce::Rectangle<float> titleBand;
        juce::Rectangle<float> detailBand;
    };

    std::vector<DebugNodeBands> debugNodeBands() const;
#endif

private:
    void timerCallback() override;

    ValvraProcessor& processor_;
    int lastPresetIndex_ { -1 };
    float uiScale_ { 1.0f };
};

// ─────────────────────────────────────────────────────────────────────────────
// GainReductionMeter — small vertical bar showing the True Peak limiter's
// current gain reduction in dB.  Idle when not engaged; warm-orange glow
// scaled to abs(GR_dB) up to ~12 dB so the user has immediate visual
// feedback of how hard the limiter is working.
// ─────────────────────────────────────────────────────────────────────────────
class GainReductionMeter final
    : public juce::Component,
      private juce::Timer
{
public:
    explicit GainReductionMeter(ValvraProcessor& proc)
        : processor_ { proc }
    {
        startTimerHz(30);
    }

    void setUiScale(float scale) noexcept
    {
        uiScale_ = juce::jlimit(1.0f, 2.0f, scale);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour(juce::Colour::fromRGB(14, 14, 16));
        const float radius = 3.0f * uiScale_;
        g.fillRoundedRectangle(r, radius);

        // Title: "GR"
        g.setColour(juce::Colours::white.withAlpha(0.55f));
        g.setFont(juce::FontOptions(8.5f * uiScale_).withStyle("Bold"));
        g.drawText("GR", r.reduced(3.0f * uiScale_).removeFromTop(11.0f * uiScale_),
                   juce::Justification::centred, false);

        auto bar = r.reduced(6.0f * uiScale_, 14.0f * uiScale_);
        g.setColour(juce::Colours::white.withAlpha(0.07f));
        g.drawRoundedRectangle(bar, 2.0f * uiScale_, 1.0f);

        // GR is reported as a non-positive dB value (0 = no reduction,
        // negative when limiting).  We map [0, -12] to [0, 1] of the bar
        // height; clamps to [0, 1] for safety.
        const float grAbs = juce::jlimit(0.0f, 12.0f, -lastGr_);
        const float t = grAbs / 12.0f;

        auto fill = bar.removeFromBottom(bar.getHeight() * t);
        g.setColour(juce::Colour::fromRGB(255, 140, 26).withAlpha(0.85f));
        g.fillRoundedRectangle(fill, 2.0f * uiScale_);

        // Numeric readout
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.setFont(juce::FontOptions(8.5f * uiScale_));
        g.drawText(juce::String::formatted("%.1f", -grAbs),
                   r.reduced(2.0f * uiScale_).removeFromBottom(11.0f * uiScale_),
                   juce::Justification::centred, false);
    }

private:
    void timerCallback() override
    {
        const float gr = processor_.gainReductionDb();
        if (std::abs(gr - lastGr_) > 0.05f)
        {
            lastGr_ = gr;
            repaint();
        }
    }

    ValvraProcessor& processor_;
    float lastGr_ { 0.0f };
    float uiScale_ { 1.0f };
};

// ─────────────────────────────────────────────────────────────────────────────
// MasteringPanel — TP limiter + TPDF dither controls + GR meter (docs/20 §4.8).
//
// Off by default: a user only adding tube colour to a track shouldn't have
// the master section subtly reshape transients.  Flip on for the "this is
// the last plugin in the chain" mastering use case.
// ─────────────────────────────────────────────────────────────────────────────
class MasteringPanel final : public juce::Component,
                             private juce::Timer
{
public:
    explicit MasteringPanel(ValvraProcessor& proc);
    void setUiScale(float scale) noexcept;
    void paint(juce::Graphics& g) override;
    void resized() override;

#if defined(VALVRA_ENABLE_UI_TEST_HOOKS) && VALVRA_ENABLE_UI_TEST_HOOKS
    struct DebugLayout
    {
        juce::Rectangle<int> analogSection;
        juce::Rectangle<int> safetySection;
        juce::Rectangle<int> meterSection;
        juce::Rectangle<int> lufsMeterBounds;
        juce::Rectangle<int> calibrationMeterBounds;
        juce::Rectangle<int> tpMeterBounds;
        juce::Rectangle<int> grHistoryBounds;
        juce::Rectangle<int> grMeterBounds;
    };

    DebugLayout debugLayout() const;
#endif

private:
    void timerCallback() override;
    void syncAnalogModeFromProcessor();
    void writeAnalogMode(int modeIndex);

    ValvraProcessor& processor_;

    juce::ComboBox     analogModeBox_;
    juce::Slider       analogAmountKnob_;
    juce::Slider       analogMixKnob_;
    juce::Slider       realismKnob_;
    juce::Slider       inputTrimKnob_;
    juce::ComboBox     levelMatchModeBox_;
    juce::ComboBox     targetProfileBox_;
    juce::TextButton   calibrateButton_ { "Cal -18" };
    juce::TextButton   analyzeMatchButton_ { "Analyze" };
    juce::ToggleButton tpToggle_       { "True Peak" };
    juce::ComboBox     tpModeBox_;
    juce::Label        tpModeLabel_    { {}, "Mode" };
    juce::Slider       ceilingKnob_;
    juce::Label        ceilingLabel_   { {}, "Ceiling" };
    juce::Slider       lookaheadKnob_;
    juce::Label        lookaheadLabel_ { {}, "Look" };
    juce::ToggleButton ditherToggle_   { "TPDF Dither" };
    juce::ComboBox     depthBox_;
    juce::Label        depthLabel_     { {}, "Depth" };
    juce::ComboBox     msModeBox_;
    juce::Label        msModeLabel_    { {}, "Routing" };
    GainReductionMeter grMeter_;

    using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttach  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttach>  analogAmountAttach_;
    std::unique_ptr<SliderAttach>  analogMixAttach_;
    std::unique_ptr<SliderAttach>  realismAttach_;
    std::unique_ptr<SliderAttach>  inputTrimAttach_;
    std::unique_ptr<ComboAttach>   levelMatchAttach_;
    std::unique_ptr<ComboAttach>   targetProfileAttach_;
    std::unique_ptr<ButtonAttach>  tpToggleAttach_;
    std::unique_ptr<ComboAttach>   tpModeAttach_;
    std::unique_ptr<SliderAttach>  ceilingAttach_;
    std::unique_ptr<SliderAttach>  lookaheadAttach_;
    std::unique_ptr<ButtonAttach>  ditherToggleAttach_;
    std::unique_ptr<ComboAttach>   depthAttach_;
    std::unique_ptr<ComboAttach>   msModeAttach_;
    std::array<float, 120> grHistory_ {};
    std::size_t grWrite_ { 0 };
    float uiScale_ { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MasteringPanel)
};

// ─────────────────────────────────────────────────────────────────────────────
// StageEditorPanel — per-stage tube / topology / drive / bias editor.
//
// Lives below the ChainBuilderView in the editor.  A single set of widgets
// is multiplexed across the 4 stages via a stage-selector combo box: the
// panel's currently-selected stage decides which set of stage{1..4}_*
// AVPTS parameters the visible widgets read from and write back to.
//
// We deliberately do NOT use AVPTS attachments here because the target
// parameter changes whenever the user moves the stage selector; rolling
// our own minimal binding is simpler than re-attaching attachments on
// every selection change.  A polling timer catches external automation
// changes (host preset recalls, DAW automation lanes) so the visible
// widgets always reflect the current parameter state.
//
// Reference: docs/26 §6 (per-stage editor minimum acceptance).
// ─────────────────────────────────────────────────────────────────────────────
class StageEditorPanel final : public juce::Component,
                               private juce::Timer
{
public:
    explicit StageEditorPanel(ValvraProcessor& proc);
    void setUiScale(float scale) noexcept;
    void paint(juce::Graphics& g) override;
    void resized() override;

#if defined(VALVRA_ENABLE_UI_TEST_HOOKS) && VALVRA_ENABLE_UI_TEST_HOOKS
    struct DebugLayout
    {
        juce::Rectangle<int> stageSelector;
        juce::Rectangle<int> tubeBox;
        juce::Rectangle<int> topologyBox;
        juce::Rectangle<int> driveSlider;
        juce::Rectangle<int> biasSlider;
        juce::Rectangle<int> driveValue;
        juce::Rectangle<int> biasValue;
    };

    DebugLayout debugLayout() const noexcept;
#endif

private:
    void timerCallback() override;

    int  selectedStage() const noexcept;
    void syncFromParams();

    void writeChoice(const char* paramId, int value);
    void writeFloat (const char* paramId, double value);

    ValvraProcessor& processor_;

    juce::ComboBox stageSelector_;
    juce::ComboBox tubeBox_;
    juce::ComboBox topologyBox_;
    juce::Slider   driveSlider_;
    juce::Slider   biasSlider_;

    juce::Label stageLabel_    { {}, "Editing" };
    juce::Label tubeLabel_     { {}, "Tube" };
    juce::Label topologyLabel_ { {}, "Topology" };
    juce::Label driveLabel_    { {}, "Drive trim" };
    juce::Label biasLabel_     { {}, "Bias offset" };
    juce::Label driveValueLabel_ { {}, "0.0 dB" };
    juce::Label biasValueLabel_  { {}, "0.000 V" };
    float uiScale_ { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StageEditorPanel)
};

class ValvraEditor final : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit ValvraEditor(ValvraProcessor& proc);
    ~ValvraEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void mouseDown(const juce::MouseEvent& e) override;

#if defined(VALVRA_ENABLE_UI_TEST_HOOKS) && VALVRA_ENABLE_UI_TEST_HOOKS
    enum class DebugTab : int
    {
        Sound = 0,
        Analysis,
        Output
    };

    void debugSetUiScale(float scale);
    void debugSelectTab(DebugTab tab);
    juce::Rectangle<int> debugBoundsForNamedComponent(const juce::String& name) const;
    StageEditorPanel::DebugLayout debugStageEditorLayout() const noexcept;
    MasteringPanel::DebugLayout debugMasteringLayout() const;
    std::vector<ChainBuilderView::DebugNodeBands> debugChainBuilderNodeBands() const;
#endif

private:
    void timerCallback() override;
    enum class MainTab : int
    {
        Sound = 0,
        Analysis,
        Mastering
    };
    enum class SoundControlMode : int
    {
        Quick = 0,
        Advanced
    };
    void selectTab(MainTab tab);
    void refreshTabButtons();
    void applyTabVisibility();
    void setSoundControlMode(SoundControlMode mode);
    void refreshSoundControlButtons();
    void syncQuickControlsFromParams();
    void applyQuickMacroTargets();
    float readParamValue(const char* paramId, float fallback = 0.0f) const;
    void writeFloatParam(const char* paramId, float value);
    void writeChoiceParam(const char* paramId, int index);

    ValvraProcessor& processor_;
    VintageLookAndFeel lnf_;
    juce::OpenGLContext openGLContext_;
    std::unique_ptr<juce::FileChooser> neuralModelChooser_;
    juce::Image vintageTexture_;

    // Parameters
    juce::ComboBox presetBox_;
    juce::Slider   driveKnob_;
    juce::Slider   outputKnob_;
    juce::Slider   mixKnob_;
    juce::Slider   neuralBlendKnob_;
    juce::Slider   expansionAmountKnob_;
    juce::Slider   expansionMixKnob_;
    juce::ComboBox oversampleBox_;
    juce::ComboBox mcDistributionBox_;
    juce::ToggleButton mcLockToggle_ { "Lock" };
    juce::ComboBox cvModeBox_;
    juce::ComboBox expansionModeBox_;
    juce::ComboBox stageCountBox_;
    juce::ComboBox inputTrafoBox_;
    juce::ComboBox outputTrafoBox_;

    // Factory preset loader
    juce::ComboBox     factoryPresetBox_;
    juce::Label        factoryPresetLabel_ { {}, "Factory" };
    juce::ComboBox     uiScaleBox_;
    juce::Label        uiScaleLabel_ { {}, "UI Scale" };
    juce::TextButton   tabSoundButton_    { "Sound" };
    juce::TextButton   tabAnalysisButton_ { "Analysis" };
    juce::TextButton   tabMasterButton_   { "Output" };
    juce::TextButton   quickModeButton_   { "Quick" };
    juce::TextButton   advancedModeButton_{ "Advanced" };

    // Signature features
    HysteresisLoopView   hysteresisView_;
    HarmonicMeterView    harmonicView_;
    NullTestToggle       nullTestToggle_;
    ChainBuilderView     chainBuilderView_;
    StageEditorPanel     stageEditor_;
    MasteringPanel       masteringPanel_;
    DriftRecorderView    driftView_;
    RerollTimelinePanel  rerollTimeline_;
    juce::TextButton     rerollButton_ { "Reroll" };
    juce::TextButton     warmupButton_ { "Warmup" };
    juce::TextButton     abButton_     { "A | B" };
    juce::TextButton     copyAToBButton_ { "A->B" };
    juce::TextButton     copyBToAButton_ { "B->A" };
    juce::TextButton     resetABButton_  { "Reset AB" };
    juce::TextButton     snapshotCButton_ { "C" };
    juce::TextButton     snapshotDButton_ { "D" };
    juce::TextButton     snapshotEButton_ { "E" };
    juce::TextButton     undoABButton_    { "Undo" };
    juce::TextButton     redoABButton_    { "Redo" };
    juce::TextButton     loadNeuralButton_   { "Load NN" };
    juce::TextButton     unloadNeuralButton_ { "Unload NN" };
    juce::ToggleButton   blindABToggle_  { "Blind" };
    juce::Label          seedLabel_;
    juce::Label          neuralModelLabel_;

    juce::Slider quickWarmKnob_;
    juce::Slider quickToneKnob_;
    juce::Slider quickPunchKnob_;
    juce::Slider quickGlueKnob_;
    juce::Slider quickWidthKnob_;
    juce::Slider quickOutputKnob_;
    juce::Label  quickWarmLabel_   { {}, "Warm" };
    juce::Label  quickToneLabel_   { {}, "Tone" };
    juce::Label  quickPunchLabel_  { {}, "Punch" };
    juce::Label  quickGlueLabel_   { {}, "Glue" };
    juce::Label  quickWidthLabel_  { {}, "Width" };
    juce::Label  quickOutputLabel_ { {}, "Output" };

    // Labels
    juce::Label presetLabel_     { {}, "Mode" };
    juce::Label driveLabel_      { {}, "Drive" };
    juce::Label outputLabel_     { {}, "Output" };
    juce::Label mixLabel_        { {}, "Mix" };
    juce::Label neuralBlendLabel_{ {}, "Neural" };
    juce::Label expansionAmountLabel_{ {}, "Expansion" };
    juce::Label expansionMixLabel_{ {}, "X Mix" };
    juce::Label oversampleLabel_ { {}, "Quality" };
    juce::Label mcDistributionLabel_ { {}, "Monte Carlo" };
    juce::Label cvModeLabel_ { {}, "CV Mode" };
    juce::Label expansionModeLabel_ { {}, "Engine" };
    juce::Label stageCountLabel_ { {}, "Stages" };
    juce::Label inputTrafoLabel_ { {}, "Input Trafo" };
    juce::Label outputTrafoLabel_ { {}, "Output Trafo" };

    using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttach  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<ComboAttach>  presetAttach_;
    std::unique_ptr<SliderAttach> driveAttach_;
    std::unique_ptr<SliderAttach> outputAttach_;
    std::unique_ptr<SliderAttach> mixAttach_;
    std::unique_ptr<SliderAttach> neuralBlendAttach_;
    std::unique_ptr<ComboAttach>  oversampleAttach_;
    std::unique_ptr<ComboAttach>  mcDistributionAttach_;
    std::unique_ptr<ButtonAttach> mcLockAttach_;
    std::unique_ptr<ComboAttach>  cvModeAttach_;
    std::unique_ptr<ComboAttach>  expansionModeAttach_;
    std::unique_ptr<SliderAttach> expansionAmountAttach_;
    std::unique_ptr<SliderAttach> expansionMixAttach_;
    std::unique_ptr<ComboAttach>  stageCountAttach_;
    std::unique_ptr<ComboAttach>  inputTrafoAttach_;
    std::unique_ptr<ComboAttach>  outputTrafoAttach_;

    void updateSeedLabel();
    void refreshABButtonLabel();
    void refreshABControls();
    void refreshNeuralModelLabel();
    void rebuildVintageTexture();
    void applyUiScale(float scale);

    MainTab activeTab_ { MainTab::Sound };
    SoundControlMode soundControlMode_ { SoundControlMode::Quick };
    bool suppressQuickCallbacks_ { false };
    float uiScale_ { 1.0f };
    static constexpr int kBaseWidth  = 980;
    static constexpr int kBaseHeight = 760;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ValvraEditor)
};

} // namespace valvra
