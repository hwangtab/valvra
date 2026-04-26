// ─────────────────────────────────────────────────────────────────────────────
// PluginEditor.cpp — Valvra GUI (Tier 2 with signature views + vintage L&F)
// ─────────────────────────────────────────────────────────────────────────────
#include "PluginEditor.h"
#include "FactoryPresets.h"

namespace valvra {

// ─────────────────────────────────────────────────────────────────────────────
// VintageLookAndFeel
// ─────────────────────────────────────────────────────────────────────────────
VintageLookAndFeel::VintageLookAndFeel()
{
    // Dark chassis base
    setColour(juce::ResizableWindow::backgroundColourId,
              juce::Colour::fromRGB(24, 24, 28));
    setColour(juce::Label::textColourId,
              juce::Colours::white.withAlpha(0.82f));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxTextColourId,
              juce::Colours::white.withAlpha(0.75f));
    setColour(juce::ComboBox::backgroundColourId,
              juce::Colour::fromRGB(38, 38, 44));
    setColour(juce::ComboBox::outlineColourId,
              juce::Colours::white.withAlpha(0.18f));
    setColour(juce::ComboBox::arrowColourId,
              juce::Colour::fromRGB(255, 140, 26));
    setColour(juce::ComboBox::textColourId,
              juce::Colours::white.withAlpha(0.85f));
    setColour(juce::PopupMenu::backgroundColourId,
              juce::Colour::fromRGB(28, 28, 32));
    setColour(juce::PopupMenu::highlightedBackgroundColourId,
              juce::Colour::fromRGB(255, 140, 26).withAlpha(0.35f));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour(juce::TextButton::buttonColourId,
              juce::Colour::fromRGB(42, 42, 48));
    setColour(juce::TextButton::buttonOnColourId,
              juce::Colour::fromRGB(255, 140, 26).withAlpha(0.55f));
    setColour(juce::TextButton::textColourOffId,
              juce::Colours::white.withAlpha(0.85f));
    setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    setColour(juce::ToggleButton::textColourId,
              juce::Colours::white.withAlpha(0.85f));
    setColour(juce::ToggleButton::tickColourId,
              juce::Colour::fromRGB(255, 140, 26));
    setColour(juce::ToggleButton::tickDisabledColourId,
              juce::Colours::white.withAlpha(0.25f));
}

void VintageLookAndFeel::drawRotarySlider(juce::Graphics& g,
                                          int x, int y, int width, int height,
                                          float sliderPos,
                                          float startAng, float endAng,
                                          juce::Slider& /*slider*/)
{
    const float radius = juce::jmin(width, height) * 0.42f;
    const float centreX = x + width * 0.5f;
    const float centreY = y + height * 0.5f;
    const float angle = startAng + sliderPos * (endAng - startAng);

    const juce::Rectangle<float> bounds { centreX - radius, centreY - radius,
                                          radius * 2.0f, radius * 2.0f };

    // Outer ring — "metal bezel"
    g.setGradientFill(juce::ColourGradient(
        juce::Colour::fromRGB(72, 72, 80), centreX, centreY - radius,
        juce::Colour::fromRGB(22, 22, 24), centreX, centreY + radius,
        false));
    g.fillEllipse(bounds);

    // Inner knob face
    const float innerR = radius * 0.84f;
    const juce::Rectangle<float> inner {
        centreX - innerR, centreY - innerR, innerR * 2.0f, innerR * 2.0f };
    g.setGradientFill(juce::ColourGradient(
        juce::Colour::fromRGB(48, 48, 54), centreX, centreY - innerR,
        juce::Colour::fromRGB(20, 20, 22), centreX, centreY + innerR,
        false));
    g.fillEllipse(inner);

    // Warm glow pool in the centre proportional to slider position
    const float glowAlpha = 0.25f + 0.5f * sliderPos;
    g.setColour(juce::Colour::fromRGB(255, 140, 26).withAlpha(glowAlpha));
    g.fillEllipse(inner.reduced(innerR * 0.65f));

    // Indicator line
    const float indicatorLen = innerR * 0.75f;
    const float ix = centreX + std::cos(angle - juce::MathConstants<float>::halfPi)
                                 * indicatorLen;
    const float iy = centreY + std::sin(angle - juce::MathConstants<float>::halfPi)
                                 * indicatorLen;
    g.setColour(juce::Colours::white);
    g.drawLine(centreX, centreY, ix, iy, 2.2f);

    // Arc showing travel
    juce::Path arc;
    arc.addCentredArc(centreX, centreY, radius, radius, 0.0f,
                      startAng, angle, true);
    g.setColour(juce::Colour::fromRGB(255, 140, 26).withAlpha(0.8f));
    g.strokePath(arc, juce::PathStrokeType(2.0f));
}

void VintageLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                              juce::Button& b,
                                              const juce::Colour& bg,
                                              bool /*over*/, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced(1.0f);
    const auto base = down
        ? bg.darker(0.15f)
        : (b.getToggleState() ? bg.brighter(0.25f) : bg);
    g.setGradientFill(juce::ColourGradient(
        base.brighter(0.12f), r.getX(), r.getY(),
        base.darker(0.12f),   r.getX(), r.getBottom(),
        false));
    g.fillRoundedRectangle(r, 3.0f);
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawRoundedRectangle(r, 3.0f, 1.0f);
}

juce::Font VintageLookAndFeel::getLabelFont(juce::Label&)
{
    return juce::Font(juce::FontOptions(11.0f));
}

juce::Font VintageLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return juce::Font(juce::FontOptions(12.0f));
}

// ─────────────────────────────────────────────────────────────────────────────
// ValvraEditor
// ─────────────────────────────────────────────────────────────────────────────
ValvraEditor::ValvraEditor(ValvraProcessor& proc)
    : juce::AudioProcessorEditor(&proc)
    , processor_(proc)
    , hysteresisView_(proc)
    , harmonicView_(proc)
    , nullTestToggle_(proc)
{
    setLookAndFeel(&lnf_);

    presetBox_.addItemList(
        { "V72 Preamp", "Marshall", "Culture Vulture", "RNDI DI" }, 1);
    addAndMakeVisible(presetBox_);
    addAndMakeVisible(presetLabel_);
    presetLabel_.attachToComponent(&presetBox_, false);
    presetLabel_.setJustificationType(juce::Justification::centredLeft);

    auto configureKnob = [this](juce::Slider& s, juce::Label& label)
    {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 16);
        addAndMakeVisible(s);
        addAndMakeVisible(label);
        label.setJustificationType(juce::Justification::centred);
        label.attachToComponent(&s, false);
    };
    configureKnob(driveKnob_,  driveLabel_);
    configureKnob(outputKnob_, outputLabel_);
    configureKnob(mixKnob_,    mixLabel_);

    oversampleBox_.addItemList(
        { "Low (1x)", "Medium (2x)", "High (4x)", "Ultra (8x)" }, 1);
    addAndMakeVisible(oversampleBox_);
    addAndMakeVisible(oversampleLabel_);
    oversampleLabel_.attachToComponent(&oversampleBox_, false);
    oversampleLabel_.setJustificationType(juce::Justification::centredLeft);

    // Factory preset selector.  Item IDs are 1-based; IDs 1..N map to
    // factoryPresets()[0..N-1].  ID 0 is reserved as the "no selection" hint.
    const auto& presets = factoryPresets();
    factoryPresetBox_.addItem("— Choose a Preset —", -1);
    factoryPresetBox_.addSeparator();
    std::string currentCategory;
    for (std::size_t i = 0; i < presets.size(); ++i)
    {
        if (presets[i].category != currentCategory)
        {
            currentCategory = presets[i].category;
            factoryPresetBox_.addSectionHeading(currentCategory);
        }
        factoryPresetBox_.addItem(presets[i].name,
                                  static_cast<int>(i) + 1);
    }
    factoryPresetBox_.setSelectedId(-1, juce::dontSendNotification);
    factoryPresetBox_.onChange = [this]
    {
        const int id = factoryPresetBox_.getSelectedId();
        if (id >= 1)
        {
            processor_.loadFactoryPreset(id - 1);
            updateSeedLabel();
        }
    };
    addAndMakeVisible(factoryPresetBox_);
    addAndMakeVisible(factoryPresetLabel_);
    factoryPresetLabel_.attachToComponent(&factoryPresetBox_, false);
    factoryPresetLabel_.setJustificationType(juce::Justification::centredLeft);

    // Signature views
    addAndMakeVisible(hysteresisView_);
    addAndMakeVisible(harmonicView_);
    addAndMakeVisible(nullTestToggle_);

    // Reroll
    rerollButton_.onClick = [this]
    {
        processor_.reroll();
        updateSeedLabel();
    };
    addAndMakeVisible(rerollButton_);
    addAndMakeVisible(seedLabel_);
    seedLabel_.setJustificationType(juce::Justification::centredLeft);
    updateSeedLabel();

    auto& p = processor_.parameters();
    presetAttach_     = std::make_unique<ComboAttach>(p, "preset",     presetBox_);
    driveAttach_      = std::make_unique<SliderAttach>(p, "drive",     driveKnob_);
    outputAttach_     = std::make_unique<SliderAttach>(p, "outputDb",  outputKnob_);
    mixAttach_        = std::make_unique<SliderAttach>(p, "mix",       mixKnob_);
    oversampleAttach_ = std::make_unique<ComboAttach>(p, "oversample", oversampleBox_);

    setSize(700, 620);
}

ValvraEditor::~ValvraEditor()
{
    setLookAndFeel(nullptr);
}

void ValvraEditor::paint(juce::Graphics& g)
{
    // Chassis background with subtle vertical gradient
    g.setGradientFill(juce::ColourGradient(
        juce::Colour::fromRGB(32, 32, 36), 0.0f, 0.0f,
        juce::Colour::fromRGB(16, 16, 18), 0.0f, static_cast<float>(getHeight()),
        false));
    g.fillAll();

    // Title
    g.setColour(juce::Colour::fromRGB(255, 140, 26));
    g.setFont(juce::FontOptions(26.0f).withStyle("Bold"));
    g.drawText("VALVRA", getLocalBounds().removeFromTop(40),
               juce::Justification::centred, false);

    g.setColour(juce::Colours::white.withAlpha(0.55f));
    g.setFont(juce::FontOptions(10.0f));
    g.drawText("LIVING TUBE AMP COLOUR",
               getLocalBounds().withTrimmedTop(42).removeFromTop(16),
               juce::Justification::centred, false);

    // Subtle "power glow" indicator bar under the title
    auto glowArea = juce::Rectangle<float>(0.0f, 60.0f,
                                           static_cast<float>(getWidth()), 2.0f);
    g.setGradientFill(juce::ColourGradient(
        juce::Colours::transparentBlack, 0.0f, 61.0f,
        juce::Colour::fromRGB(255, 140, 26).withAlpha(0.35f),
        glowArea.getCentreX(), 61.0f, true));
    g.fillRect(glowArea);
}

void ValvraEditor::resized()
{
    auto area = getLocalBounds().reduced(14);
    area.removeFromTop(60);  // title

    // Row 0 — Factory preset selector (full-width on its own row so the
    // category section headings have room to breathe in the popup menu)
    auto row0 = area.removeFromTop(50);
    factoryPresetBox_.setBounds(row0.reduced(6, 22));

    // Row 1 — Mode + Quality
    auto row1 = area.removeFromTop(50);
    presetBox_.setBounds(row1.removeFromLeft(260).reduced(6, 22));
    row1.removeFromLeft(30);
    oversampleBox_.setBounds(row1.removeFromLeft(260).reduced(6, 22));

    // Row 2 — Drive / Output / Mix knobs
    auto row2 = area.removeFromTop(150);
    const int knobCol = row2.getWidth() / 3;
    auto kArea = row2.reduced(10, 20);
    driveKnob_.setBounds(kArea.removeFromLeft(knobCol));
    outputKnob_.setBounds(kArea.removeFromLeft(knobCol));
    mixKnob_.setBounds(kArea);

    // Row 3 — Signature visualizations (hysteresis + harmonics)
    area.removeFromTop(12);
    auto row3 = area.removeFromTop(220);
    hysteresisView_.setBounds(row3.removeFromLeft(row3.getWidth() / 2).reduced(4));
    harmonicView_.setBounds(row3.reduced(4));

    // Bottom row — Reroll + seed label + Null-test toggle
    area.removeFromTop(8);
    auto bottom = area.removeFromTop(40);
    rerollButton_.setBounds(bottom.removeFromLeft(100).reduced(2));
    seedLabel_.setBounds(bottom.removeFromLeft(230).reduced(4));
    nullTestToggle_.setBounds(bottom.removeFromRight(140).reduced(2));
}

void ValvraEditor::updateSeedLabel()
{
    seedLabel_.setText(
        "Seed: 0x" + juce::String::toHexString(
            static_cast<juce::int64>(processor_.currentSeed())).toUpperCase(),
        juce::dontSendNotification);
}

} // namespace valvra
