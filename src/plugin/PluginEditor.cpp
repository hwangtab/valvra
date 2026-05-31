// ─────────────────────────────────────────────────────────────────────────────
// PluginEditor.cpp — Valvra GUI (Tier 2 with signature views + vintage L&F)
// ─────────────────────────────────────────────────────────────────────────────
#include "PluginEditor.h"
#include "FactoryPresets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <random>

namespace valvra {

namespace {

struct MasteringPanelLayout final
{
    juce::Rectangle<float> titleBounds;
    juce::Rectangle<int> statusBounds;

    juce::Rectangle<float> analogSection;
    juce::Rectangle<float> safetySection;
    juce::Rectangle<float> meterSection;

    juce::Rectangle<int> analogModeBounds;
    juce::Rectangle<int> analogAmountBounds;
    juce::Rectangle<int> analogMixBounds;
    juce::Rectangle<int> realismBounds;
    juce::Rectangle<int> inputTrimBounds;
    juce::Rectangle<int> levelMatchBounds;
    juce::Rectangle<int> targetProfileBounds;
    juce::Rectangle<int> calibrateBounds;
    juce::Rectangle<int> analyzeMatchBounds;

    juce::Rectangle<int> tpToggleBounds;
    juce::Rectangle<int> tpModeBounds;
    juce::Rectangle<int> ceilingBounds;
    juce::Rectangle<int> lookaheadBounds;
    juce::Rectangle<int> ditherToggleBounds;
    juce::Rectangle<int> depthBounds;
    juce::Rectangle<int> routingBounds;
    juce::Rectangle<int> grMeterBounds;

    juce::Rectangle<float> lufsMeterBounds;
    juce::Rectangle<float> calibrationMeterBounds;
    juce::Rectangle<float> tpMeterBounds;
    juce::Rectangle<float> grHistoryBounds;
};

MasteringPanelLayout makeMasteringPanelLayout(juce::Rectangle<int> bounds, float uiScale)
{
    MasteringPanelLayout l {};
    auto px = [uiScale](int v) { return static_cast<int>(std::round(v * uiScale)); };

    auto content = bounds.reduced(px(10), px(8));
    l.titleBounds = content.removeFromTop(px(14)).toFloat();
    l.statusBounds = content.removeFromTop(px(14));
    content.removeFromTop(px(6));

    l.analogSection = content.removeFromTop(px(124)).toFloat();
    content.removeFromTop(px(8));
    l.safetySection = content.removeFromTop(px(150)).toFloat();
    content.removeFromTop(px(8));
    l.meterSection = content.toFloat();

    {
        auto analog = l.analogSection.toNearestInt().reduced(px(10));
        analog.removeFromTop(px(24));
        const int gap = px(10);
        const int colW = juce::jmax(px(82), (analog.getWidth() - gap * 5) / 6);

        auto modeCol = analog.removeFromLeft(colW);
        analog.removeFromLeft(gap);
        auto amountCol = analog.removeFromLeft(colW);
        analog.removeFromLeft(gap);
        auto mixCol = analog.removeFromLeft(colW);
        analog.removeFromLeft(gap);
        auto realismCol = analog.removeFromLeft(colW);
        analog.removeFromLeft(gap);
        auto inputCol = analog.removeFromLeft(colW);
        analog.removeFromLeft(gap);
        auto matchCol = analog;

        modeCol.removeFromTop(px(15));
        amountCol.removeFromTop(px(12));
        mixCol.removeFromTop(px(12));
        realismCol.removeFromTop(px(12));
        inputCol.removeFromTop(px(12));
        matchCol.removeFromTop(px(15));

        l.analogModeBounds = modeCol.removeFromTop(px(30)).reduced(px(2), px(2));
        l.analogAmountBounds = amountCol.reduced(px(22), 0);
        l.analogMixBounds = mixCol.reduced(px(22), 0);
        l.realismBounds = realismCol.reduced(px(22), 0);
        l.inputTrimBounds = inputCol.reduced(px(22), 0);
        l.levelMatchBounds = matchCol.removeFromTop(px(28)).reduced(px(2), px(2));
        matchCol.removeFromTop(px(6));
        l.targetProfileBounds = matchCol.removeFromTop(px(28)).reduced(px(2), px(2));
        matchCol.removeFromTop(px(6));
        auto buttons = matchCol.removeFromTop(px(28));
        l.calibrateBounds = buttons.removeFromLeft((buttons.getWidth() - gap) / 2).reduced(px(2), px(2));
        buttons.removeFromLeft(gap);
        l.analyzeMatchBounds = buttons.reduced(px(2), px(2));
    }

    {
        auto safety = l.safetySection.toNearestInt().reduced(px(10));
        safety.removeFromTop(px(24));
        const int gap = px(10);
        const int colW = juce::jmax(px(78), (safety.getWidth() - gap * 3) / 4);

        auto limiterCol = safety.removeFromLeft(colW);
        safety.removeFromLeft(gap);
        auto ditherCol = safety.removeFromLeft(colW);
        safety.removeFromLeft(gap);
        auto routingCol = safety.removeFromLeft(colW);
        safety.removeFromLeft(gap);
        auto grCol = safety;

        limiterCol.removeFromTop(px(14));
        auto limiterTop = limiterCol.removeFromTop(px(30));
        l.tpToggleBounds = limiterTop.removeFromLeft(px(92)).reduced(px(2), px(4));
        l.tpModeBounds = limiterTop.reduced(px(2), px(3));
        limiterCol.removeFromTop(px(5));
        auto limiterKnobs = limiterCol;
        l.ceilingBounds = limiterKnobs.removeFromLeft(limiterKnobs.getWidth() / 2).reduced(px(4), 0);
        l.lookaheadBounds = limiterKnobs.reduced(px(4), 0);

        ditherCol.removeFromTop(px(14));
        l.ditherToggleBounds = ditherCol.removeFromTop(px(30)).reduced(px(2), px(4));
        ditherCol.removeFromTop(px(5));
        l.depthBounds = ditherCol.removeFromTop(px(30)).reduced(px(2), px(2));

        routingCol.removeFromTop(px(49));
        l.routingBounds = routingCol.removeFromTop(px(30)).reduced(px(2), px(2));

        grCol.removeFromTop(px(14));
        auto grBounds = grCol.withSizeKeepingCentre(px(44), grCol.getHeight());
        l.grMeterBounds = grBounds.reduced(px(2), 0);
    }

    {
        auto meters = l.meterSection.reduced(static_cast<float>(px(10)),
                                             static_cast<float>(px(10)));
        meters = meters.withTrimmedTop(static_cast<float>(px(24)));
        l.lufsMeterBounds = meters.removeFromLeft(meters.getWidth() * 0.32f);
        l.calibrationMeterBounds = meters.removeFromLeft(meters.getWidth() * 0.27f).reduced(6.0f * uiScale, 0.0f);
        l.tpMeterBounds = meters.removeFromLeft(meters.getWidth() * 0.34f).reduced(6.0f * uiScale, 0.0f);
        l.grHistoryBounds = meters.reduced(6.0f * uiScale, 0.0f);
    }

    return l;
}

} // namespace

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
    return juce::Font(juce::FontOptions(11.0f * uiScale_));
}

juce::Font VintageLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    const float target = static_cast<float>(buttonHeight) * 0.45f;
    const float minPt = 10.0f * uiScale_;
    const float maxPt = 14.0f * uiScale_;
    return juce::Font(juce::FontOptions(juce::jlimit(minPt, maxPt, target)));
}

juce::Font VintageLookAndFeel::getPopupMenuFont()
{
    return juce::Font(juce::FontOptions(13.0f * uiScale_));
}

juce::Font VintageLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return juce::Font(juce::FontOptions(12.0f * uiScale_));
}

// ─────────────────────────────────────────────────────────────────────────────
// ChainBuilderView — minimal visual chain builder baseline
// ─────────────────────────────────────────────────────────────────────────────
namespace {

struct ChainStageLabel
{
    const char* title;
    const char* detail;
};

struct ChainPresetLabels
{
    const char* name;
    const char* input;
    std::array<ChainStageLabel, 4> stages;
    int stageCount;
    const char* output;
};

const ChainPresetLabels& chainLabelsForPreset(int preset)
{
    static const ChainPresetLabels v72 {
        "V72 Preamp",
        "Marinair input",
        {{
            { "Stage 1", "12AX7 CC" },
            { "Stage 2", "12AU7 CC" },
            { "", "" },
            { "", "" }
        }},
        2,
        "Marinair output"
    };
    static const ChainPresetLabels marshall {
        "Console Output",
        "Line input",
        {{
            { "Stage 1", "12AX7 CC" },
            { "Stage 2", "12AX7 CC" },
            { "Stage 3", "EL34 PP (class-A1)" },
            { "", "" }
        }},
        3,
        "UTC OPT"
    };
    static const ChainPresetLabels cv {
        "Culture Vulture",
        "UTC input",
        {{
            { "Stage 1", "EF86 triode" },
            { "Stage 2", "6AS6 var-mu" },
            { "Stage 3", "12AU7 follower" },
            { "", "" }
        }},
        3,
        "UTC output"
    };
    static const ChainPresetLabels rndi {
        "RNDI DI",
        "Hi-Z direct",
        {{
            { "Stage 1", "12AX7 follower" },
            { "Stage 2", "12AU7 CC" },
            { "", "" },
            { "", "" }
        }},
        2,
        "Jensen output"
    };
    static const ChainPresetLabels hifi {
        "HiFi 300B SE",
        "Line input",
        {{
            { "Stage 1", "6SN7 CC" },
            { "Stage 2", "6SN7 follower" },
            { "Stage 3", "300B SE power" },
            { "", "" }
        }},
        3,
        "Lundahl output"
    };

    switch (preset)
    {
        case 1: return marshall;
        case 2: return cv;
        case 3: return rndi;
        case 4: return hifi;
        case 0:
        default: return v72;
    }
}

const char* transformerOverrideLabel(int choice, const char* presetLabel)
{
    switch (choice)
    {
        case 1: return "Off";
        case 2: return "Marinair";
        case 3: return "UTC A-12";
        case 4: return "Jensen";
        case 5: return "Lundahl";
        case 0:
        default: return presetLabel;
    }
}

void drawChainNode(juce::Graphics& g,
                   juce::Rectangle<float> r,
                   const juce::String& title,
                   const juce::String& detail,
                   bool transformer,
                   float uiScale)
{
    const auto fill = transformer
        ? juce::Colour::fromRGB(28, 42, 46)
        : juce::Colour::fromRGB(42, 34, 28);
    const auto stroke = transformer
        ? juce::Colour::fromRGB(80, 200, 220).withAlpha(0.35f)
        : juce::Colour::fromRGB(255, 140, 26).withAlpha(0.45f);

    g.setColour(fill);
    g.fillRoundedRectangle(r, 4.0f * uiScale);
    g.setColour(stroke);
    g.drawRoundedRectangle(r, 4.0f * uiScale, 1.0f);

    auto textArea = r.reduced(6.0f * uiScale, 4.0f * uiScale);
    const float titleH = juce::jmin(18.0f * uiScale, textArea.getHeight() * 0.52f);
    auto titleBand = textArea.removeFromTop(titleH);
    textArea.removeFromTop(2.0f * uiScale);
    auto detailBand = textArea;

    g.setColour(juce::Colours::white.withAlpha(0.86f));
    const float titleFont = juce::jlimit(8.0f * uiScale, 11.0f * uiScale, titleBand.getHeight() * 0.72f);
    g.setFont(juce::FontOptions(titleFont).withStyle("Bold"));
    g.drawText(title, titleBand, juce::Justification::centred, false);

    if (detailBand.getHeight() >= (7.5f * uiScale))
    {
        g.setColour(juce::Colours::white.withAlpha(0.56f));
        const float detailFont = juce::jlimit(7.0f * uiScale, 9.0f * uiScale, detailBand.getHeight() * 0.70f);
        g.setFont(juce::FontOptions(detailFont));
        g.drawText(detail, detailBand, juce::Justification::centred, false);
    }
}

#if defined(VALVRA_ENABLE_UI_TEST_HOOKS) && VALVRA_ENABLE_UI_TEST_HOOKS
ChainBuilderView::DebugNodeBands computeChainNodeBands(juce::Rectangle<float> r,
                                                       float uiScale)
{
    ChainBuilderView::DebugNodeBands out {};
    out.node = r;
    auto textArea = r.reduced(6.0f * uiScale, 4.0f * uiScale);
    const float titleH = juce::jmin(18.0f * uiScale, textArea.getHeight() * 0.52f);
    out.titleBand = textArea.removeFromTop(titleH);
    textArea.removeFromTop(2.0f * uiScale);
    out.detailBand = textArea;
    return out;
}
#endif

} // namespace

ChainBuilderView::ChainBuilderView(ValvraProcessor& proc)
    : processor_ { proc }
{
    startTimerHz(12);
}

void ChainBuilderView::setUiScale(float scale) noexcept
{
    uiScale_ = juce::jlimit(1.0f, 2.0f, scale);
    repaint();
}

void ChainBuilderView::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float px = uiScale_;

    g.setColour(juce::Colour::fromRGB(14, 14, 16));
    g.fillRoundedRectangle(r, 4.0f * px);

    const int preset = static_cast<int>(
        processor_.parameters().getRawParameterValue("preset")->load());
    const auto& labels = chainLabelsForPreset(preset);
    const int stageChoice = static_cast<int>(
        processor_.parameters().getRawParameterValue("stageCount")->load());
    const int stageCount = (stageChoice == 0)
        ? labels.stageCount
        : juce::jlimit(1, 4, stageChoice);
    const int inputTrafoChoice = static_cast<int>(
        processor_.parameters().getRawParameterValue("inputTrafo")->load());
    const int outputTrafoChoice = static_cast<int>(
        processor_.parameters().getRawParameterValue("outputTrafo")->load());

    auto content = r.reduced(8.0f * px);
    const float headerH = juce::jmax(16.0f * px, juce::jmin(22.0f * px, content.getHeight() * 0.24f));
    auto header = content.removeFromTop(headerH);
    content.removeFromTop(6.0f * px);
    auto lane = content;

    g.setColour(juce::Colour::fromRGB(255, 140, 26));
    g.setFont(juce::FontOptions(11.0f * px).withStyle("Bold"));
    g.drawText("Chain Builder", header, juce::Justification::centredLeft, false);

    g.setColour(juce::Colours::white.withAlpha(0.48f));
    g.setFont(juce::FontOptions(9.0f * px));
    const juce::String modeText = juce::String(labels.name)
        + " | " + juce::String(stageCount) + " stage"
        + (stageCount == 1 ? "" : "s");
    g.drawText(modeText, header, juce::Justification::centredRight, false);
    const int numStages = stageCount;
    const int nodeCount = numStages + 2;
    const float gap = 6.0f * px;
    const float nodeW =
        (lane.getWidth() - gap * static_cast<float>(nodeCount - 1))
        / static_cast<float>(nodeCount);

    juce::Rectangle<float> node = lane.removeFromLeft(nodeW);
    drawChainNode(g, node, "Input",
                  transformerOverrideLabel(inputTrafoChoice, labels.input),
                  true,
                  px);

    for (int i = 0; i < stageCount; ++i)
    {
        const auto& stage = i < labels.stageCount
            ? labels.stages[static_cast<std::size_t>(i)]
            : labels.stages[static_cast<std::size_t>(labels.stageCount - 1)];
        const float x1 = node.getRight();
        const float y = node.getCentreY();
        g.setColour(juce::Colours::white.withAlpha(0.22f));
        g.drawLine(x1 + 1.0f * px, y, x1 + gap - 1.0f * px, y, 1.0f);

        lane.removeFromLeft(gap);
        node = lane.removeFromLeft(nodeW);
        drawChainNode(g, node, stage.title, stage.detail, false, px);
    }

    const float x1 = node.getRight();
    const float y = node.getCentreY();
    g.setColour(juce::Colours::white.withAlpha(0.22f));
    g.drawLine(x1 + 1.0f * px, y, x1 + gap - 1.0f * px, y, 1.0f);

    lane.removeFromLeft(gap);
    node = lane.removeFromLeft(nodeW);
    drawChainNode(g, node, "Output",
                  transformerOverrideLabel(outputTrafoChoice, labels.output),
                  true,
                  px);
}

void ChainBuilderView::timerCallback()
{
    const int preset = static_cast<int>(
        processor_.parameters().getRawParameterValue("preset")->load());
    const int stageChoice = static_cast<int>(
        processor_.parameters().getRawParameterValue("stageCount")->load());
    const int inputTrafoChoice = static_cast<int>(
        processor_.parameters().getRawParameterValue("inputTrafo")->load());
    const int outputTrafoChoice = static_cast<int>(
        processor_.parameters().getRawParameterValue("outputTrafo")->load());
    const int viewKey = preset
        | (stageChoice << 4)
        | (inputTrafoChoice << 8)
        | (outputTrafoChoice << 12);
    if (viewKey == lastPresetIndex_)
        return;
    lastPresetIndex_ = viewKey;
    repaint();
}

#if defined(VALVRA_ENABLE_UI_TEST_HOOKS) && VALVRA_ENABLE_UI_TEST_HOOKS
std::vector<ChainBuilderView::DebugNodeBands> ChainBuilderView::debugNodeBands() const
{
    std::vector<DebugNodeBands> bands;
    auto r = getLocalBounds().toFloat();
    const float px = uiScale_;

    const int preset = static_cast<int>(
        processor_.parameters().getRawParameterValue("preset")->load());
    const auto& labels = chainLabelsForPreset(preset);
    const int stageChoice = static_cast<int>(
        processor_.parameters().getRawParameterValue("stageCount")->load());
    const int stageCount = (stageChoice == 0)
        ? labels.stageCount
        : juce::jlimit(1, 4, stageChoice);

    auto content = r.reduced(8.0f * px);
    const float headerH = juce::jmax(16.0f * px, juce::jmin(22.0f * px, content.getHeight() * 0.24f));
    content.removeFromTop(headerH);
    content.removeFromTop(6.0f * px);
    auto lane = content;

    const int nodeCount = stageCount + 2;
    const float gap = 6.0f * px;
    const float nodeW =
        (lane.getWidth() - gap * static_cast<float>(nodeCount - 1))
        / static_cast<float>(nodeCount);

    juce::Rectangle<float> node = lane.removeFromLeft(nodeW);
    bands.push_back(computeChainNodeBands(node, px));

    for (int i = 0; i < stageCount; ++i)
    {
        lane.removeFromLeft(gap);
        node = lane.removeFromLeft(nodeW);
        bands.push_back(computeChainNodeBands(node, px));
    }

    lane.removeFromLeft(gap);
    node = lane.removeFromLeft(nodeW);
    bands.push_back(computeChainNodeBands(node, px));
    return bands;
}
#endif

// ─────────────────────────────────────────────────────────────────────────────
// MasteringPanel — True Peak limiter + TPDF dither + GR meter (docs/20 §4.8)
// ─────────────────────────────────────────────────────────────────────────────
MasteringPanel::MasteringPanel(ValvraProcessor& proc)
    : processor_ { proc }
    , grMeter_   { proc }
{
    startTimerHz(12);

    analogModeBox_.addItem("Off", 1);
    analogModeBox_.addItem("Opto Glue", 2);
    analogModeBox_.addItem("FET Punch", 3);
    analogModeBox_.addItem("Tape Print", 4);
    analogModeBox_.setTextWhenNothingSelected("Creative FX active");
    analogModeBox_.setTooltip(
        "Analog master bus engine. Creative Synth FX remains available from the Sound page.");
    analogModeBox_.onChange = [this]
    {
        const int id = analogModeBox_.getSelectedId();
        if (id >= 1 && id <= 4)
            writeAnalogMode(id - 1);
    };
    addAndMakeVisible(analogModeBox_);

    auto configureAnalogKnob = [this](juce::Slider& s, const juce::String& tip)
    {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 16);
        s.setTextValueSuffix(" %");
        s.setTooltip(tip);
        s.textFromValueFunction = [](double v)
        {
            return juce::String(static_cast<int>(std::round(v * 100.0)));
        };
        s.valueFromTextFunction = [](const juce::String& t)
        {
            return t.getDoubleValue() / 100.0;
        };
        addAndMakeVisible(s);
    };
    configureAnalogKnob(analogAmountKnob_, "Analog engine character/intensity.");
    configureAnalogKnob(analogMixKnob_, "Analog engine wet/dry blend.");
    configureAnalogKnob(realismKnob_,
                        "Subtle hardware imperfections: loading, feedback memory, leakage and noise.");

    inputTrimKnob_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    inputTrimKnob_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 16);
    inputTrimKnob_.setTextValueSuffix(" dB");
    inputTrimKnob_.setTooltip(
        "Lineup trim before Drive. Use Cal -18 to put the analog input near hardware sweet spot.");
    addAndMakeVisible(inputTrimKnob_);

    levelMatchModeBox_.addItemList({ "Off", "Mode Trim", "Analyze Match" }, 1);
    levelMatchModeBox_.setTooltip(
        "Post-color gain compensation. Mode Trim uses built-in offsets; Analyze Match writes a measured trim.");
    addAndMakeVisible(levelMatchModeBox_);

    targetProfileBox_.addItemList({ "Auto", "V72", "Console Output", "Culture Vulture", "RNDI", "HiFi 300B" }, 1);
    targetProfileBox_.setTooltip("Hardware target profile used by the range meter.");
    addAndMakeVisible(targetProfileBox_);

    calibrateButton_.setTooltip("Set Input Trim from the recent input RMS toward -18 dBFS.");
    calibrateButton_.onClick = [this] { processor_.calibrateInputToMinus18(); };
    addAndMakeVisible(calibrateButton_);

    analyzeMatchButton_.setTooltip("Measure recent pre-output input/output level and write Analyze Match trim.");
    analyzeMatchButton_.onClick = [this] { processor_.analyzeLevelMatch(); };
    addAndMakeVisible(analyzeMatchButton_);

    addAndMakeVisible(tpToggle_);
    tpToggle_.setTooltip("Legacy on/off switch for true-peak safety. "
                         "Use Mode for Off, Soft, or Brick-wall.");

    tpModeBox_.addItemList({ "Off", "Soft", "Brick-wall" }, 1);
    tpModeBox_.setTooltip("True Peak safety mode: Off, soft ceiling, or "
                          "4x detection brick-wall limiting.");
    addAndMakeVisible(tpModeBox_);
    addAndMakeVisible(tpModeLabel_);
    tpModeLabel_.attachToComponent(&tpModeBox_, false);
    tpModeLabel_.setJustificationType(juce::Justification::centredLeft);
    tpModeLabel_.setFont(juce::FontOptions(9.5f));
    tpModeLabel_.setVisible(false);

    ceilingKnob_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    ceilingKnob_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 14);
    ceilingKnob_.setTextValueSuffix(" dB");
    addAndMakeVisible(ceilingKnob_);
    addAndMakeVisible(ceilingLabel_);
    ceilingLabel_.attachToComponent(&ceilingKnob_, false);
    ceilingLabel_.setJustificationType(juce::Justification::centred);
    ceilingLabel_.setFont(juce::FontOptions(9.5f));
    ceilingLabel_.setVisible(false);

    lookaheadKnob_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    lookaheadKnob_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 56, 14);
    lookaheadKnob_.setTextValueSuffix(" ms");
    addAndMakeVisible(lookaheadKnob_);
    addAndMakeVisible(lookaheadLabel_);
    lookaheadLabel_.attachToComponent(&lookaheadKnob_, false);
    lookaheadLabel_.setJustificationType(juce::Justification::centred);
    lookaheadLabel_.setFont(juce::FontOptions(9.5f));
    lookaheadLabel_.setVisible(false);

    addAndMakeVisible(ditherToggle_);
    ditherToggle_.setTooltip(
        "Triangular-PDF dither at the absolute output stage. "
        "Use only when Valvra is the last plugin in the chain.");

    depthBox_.addItemList({ "16-bit", "20-bit", "24-bit" }, 1);
    addAndMakeVisible(depthBox_);
    addAndMakeVisible(depthLabel_);
    depthLabel_.attachToComponent(&depthBox_, false);
    depthLabel_.setJustificationType(juce::Justification::centredLeft);
    depthLabel_.setFont(juce::FontOptions(9.5f));
    depthLabel_.setVisible(false);

    msModeBox_.addItemList({ "Stereo", "Mid/Side" }, 1);
    msModeBox_.setTooltip(
        "Mid/Side: decode L+R into Mid (centre) and Side (stereo width), "
        "process each through its own chain, re-encode.  Useful for "
        "applying tube colour to the centre image while keeping the "
        "sides cleaner.  Mono-compatible (sums to the Mid chain alone).");
    addAndMakeVisible(msModeBox_);
    addAndMakeVisible(msModeLabel_);
    msModeLabel_.attachToComponent(&msModeBox_, false);
    msModeLabel_.setJustificationType(juce::Justification::centredLeft);
    msModeLabel_.setFont(juce::FontOptions(9.5f));
    msModeLabel_.setVisible(false);

    addAndMakeVisible(grMeter_);

    auto& p = processor_.parameters();
    analogAmountAttach_ = std::make_unique<SliderAttach>(p, "expansionAmount", analogAmountKnob_);
    analogMixAttach_    = std::make_unique<SliderAttach>(p, "expansionMix",    analogMixKnob_);
    realismAttach_      = std::make_unique<SliderAttach>(p, "realismAmount",   realismKnob_);
    inputTrimAttach_    = std::make_unique<SliderAttach>(p, "inputTrimDb",     inputTrimKnob_);
    levelMatchAttach_   = std::make_unique<ComboAttach> (p, "levelMatchMode",  levelMatchModeBox_);
    targetProfileAttach_= std::make_unique<ComboAttach> (p, "targetProfile",   targetProfileBox_);
    tpToggleAttach_     = std::make_unique<ButtonAttach>(p, "tpEnabled",      tpToggle_);
    tpModeAttach_       = std::make_unique<ComboAttach> (p, "tpMode",         tpModeBox_);
    ceilingAttach_      = std::make_unique<SliderAttach>(p, "tpCeilingDb",    ceilingKnob_);
    lookaheadAttach_    = std::make_unique<SliderAttach>(p, "tpLookaheadMs",  lookaheadKnob_);
    ditherToggleAttach_ = std::make_unique<ButtonAttach>(p, "ditherEnabled",  ditherToggle_);
    depthAttach_        = std::make_unique<ComboAttach> (p, "ditherDepth",    depthBox_);
    msModeAttach_       = std::make_unique<ComboAttach> (p, "msMode",         msModeBox_);
    setUiScale(1.0f);
    syncAnalogModeFromProcessor();
}

void MasteringPanel::setUiScale(float scale) noexcept
{
    uiScale_ = juce::jlimit(1.0f, 2.0f, scale);

    const int textW = static_cast<int>(std::round(64.0f * uiScale_));
    const int textH = static_cast<int>(std::round(14.0f * uiScale_));
    const int lookW = static_cast<int>(std::round(56.0f * uiScale_));
    const int analogW = static_cast<int>(std::round(70.0f * uiScale_));
    const int analogH = static_cast<int>(std::round(16.0f * uiScale_));
    analogAmountKnob_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, analogW, analogH);
    analogMixKnob_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, analogW, analogH);
    realismKnob_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, analogW, analogH);
    inputTrimKnob_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, analogW, analogH);
    ceilingKnob_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, textW, textH);
    lookaheadKnob_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, lookW, textH);

    const auto labelFont = juce::FontOptions(9.5f * uiScale_);
    tpModeLabel_.setFont(labelFont);
    ceilingLabel_.setFont(labelFont);
    lookaheadLabel_.setFont(labelFont);
    depthLabel_.setFont(labelFont);
    msModeLabel_.setFont(labelFont);

    grMeter_.setUiScale(uiScale_);
    resized();
    repaint();
}

void MasteringPanel::paint(juce::Graphics& g)
{
    const auto layout = makeMasteringPanelLayout(getLocalBounds(), uiScale_);
    auto r = getLocalBounds().toFloat();
    const float px = uiScale_;
    g.setColour(juce::Colour::fromRGB(14, 14, 16));
    g.fillRoundedRectangle(r, 4.0f * px);

    const auto panelFill = juce::Colour::fromRGB(10, 10, 12);
    const auto panelStroke = juce::Colours::white.withAlpha(0.07f);
    auto drawSection = [&](juce::Rectangle<float> bounds, const char* title)
    {
        g.setColour(panelFill.withAlpha(0.72f));
        g.fillRoundedRectangle(bounds, 4.0f * px);
        g.setColour(panelStroke);
        g.drawRoundedRectangle(bounds, 4.0f * px, 1.0f);
        g.setColour(juce::Colour::fromRGB(255, 140, 26));
        g.setFont(juce::FontOptions(11.0f * px).withStyle("Bold"));
        g.drawText(title, bounds.reduced(8.0f * px).removeFromTop(14.0f * px),
                   juce::Justification::centredLeft, false);
    };

    g.setColour(juce::Colour::fromRGB(255, 140, 26));
    g.setFont(juce::FontOptions(11.0f * px).withStyle("Bold"));
    g.drawText("Output Stage", layout.titleBounds.toNearestInt(),
               juce::Justification::topLeft, false);

    g.setColour(juce::Colours::white.withAlpha(0.45f));
    g.setFont(juce::FontOptions(9.0f * px));
    const auto ms = processor_.readMasteringState();
    const juce::String statText = juce::String::formatted(
        "I %.1f LUFS | TP %.1f dBTP | Corr %.2f",
        static_cast<double>(ms.integratedLufs),
        static_cast<double>(ms.truePeakDbtp),
        static_cast<double>(ms.correlation));
    g.drawFittedText(statText,
                     layout.statusBounds,
                     juce::Justification::topRight,
                     1,
                     0.85f);

    auto analog = layout.analogSection;
    auto safety = layout.safetySection;
    auto meters = layout.meterSection;
    drawSection(analog, "Analog Output Rack");
    drawSection(safety, "Final Safety");
    drawSection(meters, "Meters");

    g.setColour(juce::Colours::white.withAlpha(0.58f));
    g.setFont(juce::FontOptions(8.5f * px));
    auto label = [&](const char* text, juce::Rectangle<float> b)
    {
        g.drawText(text, b.toNearestInt(), juce::Justification::centredLeft, false);
    };
    auto a = analog.reduced(10.0f * px).withTrimmedTop(24.0f * px);
    const float aW = a.getWidth() / 6.0f;
    label("Analog Engine", a.removeFromLeft(aW).removeFromTop(12.0f * px));
    label("Character", a.removeFromLeft(aW).removeFromTop(12.0f * px));
    label("Blend", a.removeFromLeft(aW).removeFromTop(12.0f * px));
    label("Realism", a.removeFromLeft(aW).removeFromTop(12.0f * px));
    label("Input Trim", a.removeFromLeft(aW).removeFromTop(12.0f * px));
    label("Level / Target", a.removeFromLeft(aW).removeFromTop(12.0f * px));

    auto s = safety.reduced(10.0f * px).withTrimmedTop(24.0f * px);
    const float sW = s.getWidth() / 4.0f;
    label("Limiter", s.removeFromLeft(sW).removeFromTop(12.0f * px));
    label("Dither", s.removeFromLeft(sW).removeFromTop(12.0f * px));
    label("Routing", s.removeFromLeft(sW).removeFromTop(12.0f * px));
    label("GR", s.removeFromLeft(sW).removeFromTop(12.0f * px));

    auto lufsArea = layout.lufsMeterBounds;
    auto calArea = layout.calibrationMeterBounds;
    auto tpArea = layout.tpMeterBounds;
    auto historyArea = layout.grHistoryBounds;

    auto drawLufs = [&](juce::Rectangle<float> slot, const char* name, float value)
    {
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.fillRoundedRectangle(slot, 3.0f * px);
        const float t = juce::jlimit(0.0f, 1.0f, (value + 60.0f) / 60.0f);
        auto fill = slot.reduced(5.0f * px);
        fill.removeFromTop(fill.getHeight() * (1.0f - t));
        g.setColour(juce::Colour::fromRGB(255, 140, 26).withAlpha(0.85f));
        g.fillRoundedRectangle(fill, 2.0f * px);
        g.setColour(juce::Colours::white.withAlpha(0.72f));
        g.setFont(juce::FontOptions(9.0f * px).withStyle("Bold"));
        g.drawText(name, slot.reduced(4.0f * px).removeFromTop(13.0f * px),
                   juce::Justification::centred, false);
        g.setFont(juce::FontOptions(10.0f * px));
        g.drawText(juce::String::formatted("%.1f", static_cast<double>(value)),
                   slot.reduced(4.0f * px).removeFromBottom(16.0f * px),
                   juce::Justification::centred, false);
    };

    const float barGap = 8.0f * px;
    const float barW = (lufsArea.getWidth() - 2.0f * barGap) / 3.0f;
    drawLufs(lufsArea.removeFromLeft(barW), "M", ms.momentaryLufs);
    lufsArea.removeFromLeft(barGap);
    drawLufs(lufsArea.removeFromLeft(barW), "S", ms.shortTermLufs);
    lufsArea.removeFromLeft(barGap);
    drawLufs(lufsArea.removeFromLeft(barW), "I", ms.integratedLufs);

    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.fillRoundedRectangle(calArea, 3.0f * px);
    auto calText = calArea.reduced(8.0f * px, 6.0f * px).toNearestInt();
    auto calTitle = calText.removeFromTop(static_cast<int>(std::round(14.0f * px)));
    calText.removeFromTop(static_cast<int>(std::round(5.0f * px)));
    auto calLine1 = calText.removeFromTop(static_cast<int>(std::round(18.0f * px)));
    auto calLine2 = calText.removeFromTop(static_cast<int>(std::round(18.0f * px)));
    auto calLine3 = calText.removeFromTop(static_cast<int>(std::round(18.0f * px)));
    const char* rangeText = (ms.targetMatchState == 0) ? "Underdriven"
                         : (ms.targetMatchState == 2) ? "Overdriven"
                         : "In Range";
    const juce::Colour rangeColour =
        (ms.targetMatchState == 1)
            ? juce::Colour::fromRGB(105, 210, 130)
            : juce::Colour::fromRGB(255, 140, 26);

    g.setColour(juce::Colours::white.withAlpha(0.58f));
    g.setFont(juce::FontOptions(9.0f * px).withStyle("Bold"));
    g.drawFittedText("Calibration / Target", calTitle,
                     juce::Justification::centredLeft, 1, 0.90f);
    g.setFont(juce::FontOptions(10.0f * px));
    g.setColour(juce::Colours::white.withAlpha(0.70f));
    g.drawFittedText(juce::String::formatted("Input %.1f RMS / %.1f peak",
                                             static_cast<double>(ms.inputRmsDbfs),
                                             static_cast<double>(ms.inputPeakDbfs)),
                     calLine1, juce::Justification::centredLeft, 1, 0.85f);
    g.drawFittedText(juce::String::formatted("Trim needed %+.1f dB",
                                             static_cast<double>(ms.inputTrimNeededDb)),
                     calLine2, juce::Justification::centredLeft, 1, 0.85f);
    g.setColour(rangeColour);
    g.drawFittedText(juce::String::formatted("%s %.0f%% | match %+.1f dB",
                                             rangeText,
                                             static_cast<double>(ms.targetMatchScore),
                                             static_cast<double>(ms.levelMatchAppliedDb)),
                     calLine3, juce::Justification::centredLeft, 1, 0.85f);

    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.fillRoundedRectangle(tpArea, 3.0f * px);
    auto tpText = tpArea.reduced(8.0f * px, 6.0f * px).toNearestInt();
    auto tpTitle = tpText.removeFromTop(static_cast<int>(std::round(14.0f * px)));
    tpText.removeFromTop(static_cast<int>(std::round(4.0f * px)));
    auto tpFooter = tpText.removeFromBottom(static_cast<int>(std::round(16.0f * px)));
    tpText.removeFromBottom(static_cast<int>(std::round(3.0f * px)));

    g.setColour(juce::Colours::white.withAlpha(0.58f));
    g.setFont(juce::FontOptions(9.0f * px).withStyle("Bold"));
    g.drawFittedText("True Peak / Sample Peak", tpTitle,
                     juce::Justification::centredLeft, 1, 0.90f);

    const float tpValuePt = juce::jlimit(12.0f * px, 18.0f * px,
                                         static_cast<float>(tpText.getHeight()) * 0.65f);
    g.setFont(juce::FontOptions(tpValuePt).withStyle("Bold"));
    g.setColour(ms.truePeakDbtp > -0.3f
                    ? juce::Colour::fromRGB(255, 90, 70)
                    : juce::Colour::fromRGB(255, 140, 26));
    g.drawFittedText(juce::String::formatted("%.1f dBTP", static_cast<double>(ms.truePeakDbtp)),
                     tpText,
                     juce::Justification::centredLeft, 1, 0.85f);

    g.setColour(juce::Colours::white.withAlpha(0.65f));
    g.setFont(juce::FontOptions(10.0f * px));
    g.drawFittedText(juce::String::formatted("Sample %.1f dBFS | Corr %.2f",
                                             static_cast<double>(ms.peakDbfs),
                                             static_cast<double>(ms.correlation)),
                     tpFooter,
                     juce::Justification::centredLeft, 1, 0.85f);

    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.fillRoundedRectangle(historyArea, 3.0f * px);
    g.setColour(juce::Colours::white.withAlpha(0.58f));
    g.setFont(juce::FontOptions(9.0f * px).withStyle("Bold"));
    g.drawText("GR history", historyArea.reduced(8.0f * px).removeFromTop(15.0f * px),
               juce::Justification::centredLeft, false);
    auto graph = historyArea.reduced(8.0f * px).withTrimmedTop(20.0f * px).withTrimmedBottom(8.0f * px);
    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.drawRect(graph, 1.0f);
    juce::Path p;
    for (std::size_t i = 0; i < grHistory_.size(); ++i)
    {
        const std::size_t idx = (grWrite_ + i) % grHistory_.size();
        const float gr = juce::jlimit(0.0f, 12.0f, -grHistory_[idx]);
        const float x = graph.getX() + graph.getWidth() * static_cast<float>(i)
                                  / static_cast<float>(grHistory_.size() - 1);
        const float y = graph.getBottom() - graph.getHeight() * (gr / 12.0f);
        if (i == 0) p.startNewSubPath(x, y);
        else        p.lineTo(x, y);
    }
    g.setColour(juce::Colour::fromRGB(255, 140, 26).withAlpha(0.9f));
    g.strokePath(p, juce::PathStrokeType(1.5f * px));
}

void MasteringPanel::resized()
{
    const auto layout = makeMasteringPanelLayout(getLocalBounds(), uiScale_);
    analogModeBox_.setBounds(layout.analogModeBounds);
    analogAmountKnob_.setBounds(layout.analogAmountBounds);
    analogMixKnob_.setBounds(layout.analogMixBounds);
    realismKnob_.setBounds(layout.realismBounds);
    inputTrimKnob_.setBounds(layout.inputTrimBounds);
    levelMatchModeBox_.setBounds(layout.levelMatchBounds);
    targetProfileBox_.setBounds(layout.targetProfileBounds);
    calibrateButton_.setBounds(layout.calibrateBounds);
    analyzeMatchButton_.setBounds(layout.analyzeMatchBounds);
    tpToggle_.setBounds(layout.tpToggleBounds);
    tpModeBox_.setBounds(layout.tpModeBounds);
    ceilingKnob_.setBounds(layout.ceilingBounds);
    lookaheadKnob_.setBounds(layout.lookaheadBounds);
    ditherToggle_.setBounds(layout.ditherToggleBounds);
    depthBox_.setBounds(layout.depthBounds);
    msModeBox_.setBounds(layout.routingBounds);
    grMeter_.setBounds(layout.grMeterBounds);
}

void MasteringPanel::timerCallback()
{
    syncAnalogModeFromProcessor();
    grHistory_[grWrite_] = processor_.gainReductionDb();
    grWrite_ = (grWrite_ + 1) % grHistory_.size();
    repaint();
}

#if defined(VALVRA_ENABLE_UI_TEST_HOOKS) && VALVRA_ENABLE_UI_TEST_HOOKS
MasteringPanel::DebugLayout MasteringPanel::debugLayout() const
{
    const auto layout = makeMasteringPanelLayout(getLocalBounds(), uiScale_);
    return {
        layout.analogSection.toNearestInt(),
        layout.safetySection.toNearestInt(),
        layout.meterSection.toNearestInt(),
        layout.lufsMeterBounds.toNearestInt(),
        layout.calibrationMeterBounds.toNearestInt(),
        layout.tpMeterBounds.toNearestInt(),
        layout.grHistoryBounds.toNearestInt(),
        layout.grMeterBounds
    };
}
#endif

void MasteringPanel::syncAnalogModeFromProcessor()
{
    const int mode = static_cast<int>(
        processor_.parameters().getRawParameterValue("expansionMode")->load());
    const int id = (mode >= 0 && mode <= 3) ? mode + 1 : 0;
    if (analogModeBox_.getSelectedId() != id)
        analogModeBox_.setSelectedId(id, juce::dontSendNotification);
}

void MasteringPanel::writeAnalogMode(int modeIndex)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(
            processor_.parameters().getParameter("expansionMode")))
        *p = juce::jlimit(0, 3, modeIndex);
}

// ─────────────────────────────────────────────────────────────────────────────
// StageEditorPanel — per-stage tube/topology/drive/bias editor (docs/26 §6)
// ─────────────────────────────────────────────────────────────────────────────
StageEditorPanel::StageEditorPanel(ValvraProcessor& proc)
    : processor_ { proc }
{
    stageSelector_.addItemList(
        { "Stage 1", "Stage 2", "Stage 3", "Stage 4" }, 1);
    stageSelector_.setSelectedItemIndex(0, juce::dontSendNotification);
    stageSelector_.onChange = [this] { syncFromParams(); };

    tubeBox_.addItemList(
        { "Preset", "12AX7 RSD-1", "12AX7 RSD-2", "12AX7 EHX", "12AU7", "6SN7", "300B", "EF86 (triode)", "EL34", "6L6GC" }, 1);
    tubeBox_.onChange = [this]
    {
        const auto idx = static_cast<std::size_t>(selectedStage());
        writeChoice(ValvraProcessor::kStageParams[idx].tube,
                    tubeBox_.getSelectedItemIndex());
    };

    topologyBox_.addItemList(
        { "Preset", "Common Cathode", "Cathode Follower", "SRPP", "Long-Tailed Pair", "Cascode" }, 1);
    topologyBox_.onChange = [this]
    {
        const auto idx = static_cast<std::size_t>(selectedStage());
        writeChoice(ValvraProcessor::kStageParams[idx].topology,
                    topologyBox_.getSelectedItemIndex());
    };

    auto configureKnob = [](juce::Slider& s, double lo, double hi, double step,
                            const juce::String& suffix)
    {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.setRange(lo, hi, step);
        s.setTextValueSuffix(suffix);
    };
    configureKnob(driveSlider_, -12.0,  12.0, 0.1, " dB");
    configureKnob(biasSlider_,  -0.8,   0.8, 0.005, " V");
    driveSlider_.onValueChange = [this]
    {
        const auto idx = static_cast<std::size_t>(selectedStage());
        driveValueLabel_.setText(
            juce::String::formatted("%.1f dB", driveSlider_.getValue()),
            juce::dontSendNotification);
        writeFloat(ValvraProcessor::kStageParams[idx].drive,
                   driveSlider_.getValue());
    };
    biasSlider_.onValueChange = [this]
    {
        const auto idx = static_cast<std::size_t>(selectedStage());
        biasValueLabel_.setText(
            juce::String::formatted("%.3f V", biasSlider_.getValue()),
            juce::dontSendNotification);
        writeFloat(ValvraProcessor::kStageParams[idx].bias,
                   biasSlider_.getValue());
    };

    addAndMakeVisible(stageSelector_);
    addAndMakeVisible(tubeBox_);
    addAndMakeVisible(topologyBox_);
    addAndMakeVisible(driveSlider_);
    addAndMakeVisible(biasSlider_);
    addAndMakeVisible(driveValueLabel_);
    addAndMakeVisible(biasValueLabel_);
    driveValueLabel_.setJustificationType(juce::Justification::centred);
    biasValueLabel_.setJustificationType(juce::Justification::centred);
    stageSelector_.setName("Stage Editor - Selector");
    tubeBox_.setName("Stage Editor - Tube");
    topologyBox_.setName("Stage Editor - Topology");
    driveSlider_.setName("Stage Editor - Drive");
    biasSlider_.setName("Stage Editor - Bias");
    driveValueLabel_.setName("Stage Editor - Drive Value");
    biasValueLabel_.setName("Stage Editor - Bias Value");

    setUiScale(1.0f);
    syncFromParams();
    startTimerHz(8);  // pick up host automation / preset recalls
}

void StageEditorPanel::setUiScale(float scale) noexcept
{
    uiScale_ = juce::jlimit(1.0f, 2.0f, scale);
    driveValueLabel_.setFont(juce::FontOptions(9.0f * uiScale_));
    biasValueLabel_.setFont(juce::FontOptions(9.0f * uiScale_));

    resized();
    repaint();
}

void StageEditorPanel::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float px = uiScale_;
    g.setColour(juce::Colour::fromRGB(14, 14, 16));
    g.fillRoundedRectangle(r, 4.0f * px);

    g.setColour(juce::Colour::fromRGB(255, 140, 26));
    g.setFont(juce::FontOptions(11.0f * px).withStyle("Bold"));
    g.drawText("Stage Editor",
               r.reduced(8.0f * px).removeFromTop(14.0f * px),
               juce::Justification::topLeft, false);

    g.setColour(juce::Colours::white.withAlpha(0.45f));
    g.setFont(juce::FontOptions(9.0f * px));
    g.drawText("Per-stage tube | topology | drive | bias",
               r.reduced(8.0f * px).removeFromTop(14.0f * px),
               juce::Justification::topRight, false);

    g.setColour(juce::Colours::white.withAlpha(0.62f));
    g.setFont(juce::FontOptions(8.5f * px));
    const int captionH = static_cast<int>(std::round(12.0f * px));
    const int captionY = juce::jmax(
        static_cast<int>(std::round(24.0f * px)),
        juce::jmin(tubeBox_.getY(), driveSlider_.getY()) - captionH - static_cast<int>(std::round(2.0f * px)));
    auto drawCap = [&](const char* text, const juce::Rectangle<int>& target)
    {
        g.drawText(text,
                   juce::Rectangle<int>(target.getX() + static_cast<int>(std::round(2.0f * px)),
                                        captionY,
                                        target.getWidth() - static_cast<int>(std::round(4.0f * px)),
                                        captionH),
                   juce::Justification::centredLeft, false);
    };
    drawCap("Tube", tubeBox_.getBounds());
    drawCap("Topology", topologyBox_.getBounds());
    drawCap("Drive trim", driveSlider_.getBounds());
    drawCap("Bias offset", biasSlider_.getBounds());
}

void StageEditorPanel::resized()
{
    auto px = [this](int v) { return static_cast<int>(std::round(v * uiScale_)); };

    auto inner = getLocalBounds().reduced(px(8), px(8));
    const int titleH = px(22);
    const int selectorH = px(30);
    const int selectorGap = px(10);
    const int captionH = px(12);
    const int captionGap = px(4);
    const int comboH = px(30);
    const int knobSize = px(72);
    const int valueGap = px(2);
    const int valueH = px(16);
    const int colGap = px(8);

    auto content = inner;
    content.removeFromTop(titleH);

    const int colW = (content.getWidth() - colGap * 3) / 4;
    auto selectorRow = content.removeFromTop(selectorH);
    stageSelector_.setBounds(selectorRow.removeFromLeft(colW).reduced(px(2), px(2)));

    content.removeFromTop(selectorGap);
    const int gridTop = content.getY();

    auto makeCols = [&](juce::Rectangle<int> row)
    {
        std::array<juce::Rectangle<int>, 4> cols {};
        for (int i = 0; i < 4; ++i)
        {
            cols[static_cast<std::size_t>(i)] = row.removeFromLeft(colW);
            if (i != 3) row.removeFromLeft(colGap);
        }
        return cols;
    };
    const auto cols = makeCols(content);

    const int controlY = gridTop + captionH + captionGap;
    const int knobY = juce::jmax(gridTop + px(2), controlY - px(14));
    const int valueY = knobY + knobSize + valueGap;

    tubeBox_.setBounds(cols[0].getX() + px(2), controlY,
                       cols[0].getWidth() - px(4), comboH);
    topologyBox_.setBounds(cols[1].getX() + px(2), controlY,
                           cols[1].getWidth() - px(4), comboH);

    auto placeKnobAndValue = [&](juce::Slider& slider, juce::Label& valueLabel, const juce::Rectangle<int>& col)
    {
        const int knobX = col.getX() + (col.getWidth() - knobSize) / 2;
        slider.setBounds(knobX, knobY, knobSize, knobSize);
        valueLabel.setBounds(col.getX() + px(8), valueY, col.getWidth() - px(16), valueH);
    };
    placeKnobAndValue(driveSlider_, driveValueLabel_, cols[2]);
    placeKnobAndValue(biasSlider_, biasValueLabel_, cols[3]);
}

void StageEditorPanel::timerCallback()
{
    syncFromParams();
}

int StageEditorPanel::selectedStage() const noexcept
{
    return juce::jlimit(0, 3, stageSelector_.getSelectedItemIndex());
}

void StageEditorPanel::syncFromParams()
{
    const auto idx = static_cast<std::size_t>(selectedStage());
    const auto& ids = ValvraProcessor::kStageParams[idx];
    auto& p = processor_.parameters();

    const int   tubeV = static_cast<int>(*p.getRawParameterValue(ids.tube));
    const int   topoV = static_cast<int>(*p.getRawParameterValue(ids.topology));
    const float drvV  = *p.getRawParameterValue(ids.drive);
    const float biasV = *p.getRawParameterValue(ids.bias);

    tubeBox_    .setSelectedItemIndex(tubeV, juce::dontSendNotification);
    topologyBox_.setSelectedItemIndex(topoV, juce::dontSendNotification);
    driveSlider_.setValue(drvV,  juce::dontSendNotification);
    biasSlider_ .setValue(biasV, juce::dontSendNotification);
    driveValueLabel_.setText(juce::String::formatted("%.1f dB", static_cast<double>(drvV)),
                             juce::dontSendNotification);
    biasValueLabel_.setText(juce::String::formatted("%.3f V", static_cast<double>(biasV)),
                            juce::dontSendNotification);
}

#if defined(VALVRA_ENABLE_UI_TEST_HOOKS) && VALVRA_ENABLE_UI_TEST_HOOKS
StageEditorPanel::DebugLayout StageEditorPanel::debugLayout() const noexcept
{
    return {
        stageSelector_.getBounds(),
        tubeBox_.getBounds(),
        topologyBox_.getBounds(),
        driveSlider_.getBounds(),
        biasSlider_.getBounds(),
        driveValueLabel_.getBounds(),
        biasValueLabel_.getBounds()
    };
}
#endif

void StageEditorPanel::writeChoice(const char* paramId, int value)
{
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*>(
            processor_.parameters().getParameter(paramId)))
        *c = value;
}

void StageEditorPanel::writeFloat(const char* paramId, double value)
{
    if (auto* f = dynamic_cast<juce::AudioParameterFloat*>(
            processor_.parameters().getParameter(paramId)))
        *f = static_cast<float>(value);
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
    , chainBuilderView_(proc)
    , stageEditor_(proc)
    , masteringPanel_(proc)
    , driftView_(proc)
    , rerollTimeline_(proc)
{
    setLookAndFeel(&lnf_);
    setWantsKeyboardFocus(true);
    setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);

    presetBox_.addItemList(
        { "V72 Preamp", "Console Output", "Culture Vulture", "RNDI DI", "HiFi 300B" }, 1);
    addAndMakeVisible(presetBox_);
    addAndMakeVisible(presetLabel_);
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
    configureKnob(neuralBlendKnob_, neuralBlendLabel_);
    configureKnob(expansionAmountKnob_, expansionAmountLabel_);
    configureKnob(expansionMixKnob_, expansionMixLabel_);

    auto configureQuickKnob = [this](juce::Slider& s, juce::Label& label, const juce::String& tip)
    {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 16);
        s.setTooltip(tip);
        addAndMakeVisible(s);
        addAndMakeVisible(label);
        label.setJustificationType(juce::Justification::centred);
        label.attachToComponent(&s, false);
    };
    configureQuickKnob(quickWarmKnob_, quickWarmLabel_,
                       "Overall tube warmth and drive amount.");
    configureQuickKnob(quickToneKnob_, quickToneLabel_,
                       "Tone color amount (leans Tape Print as it rises).");
    configureQuickKnob(quickPunchKnob_, quickPunchLabel_,
                       "Transient punch amount (FET-style character).");
    configureQuickKnob(quickGlueKnob_, quickGlueLabel_,
                       "Bus glue amount (Opto-style character).");
    configureQuickKnob(quickWidthKnob_, quickWidthLabel_,
                       "Stereo width mode: lower=Stereo, higher=Mid/Side.");
    configureQuickKnob(quickOutputKnob_, quickOutputLabel_,
                       "Final output trim.");

    auto configurePercentQuickKnob = [](juce::Slider& s)
    {
        s.setRange(0.0, 100.0, 1.0);
        s.setTextValueSuffix(" %");
        s.textFromValueFunction = [](double v)
        {
            return juce::String(static_cast<int>(std::round(v)));
        };
    };
    configurePercentQuickKnob(quickWarmKnob_);
    configurePercentQuickKnob(quickToneKnob_);
    configurePercentQuickKnob(quickPunchKnob_);
    configurePercentQuickKnob(quickGlueKnob_);
    configurePercentQuickKnob(quickWidthKnob_);

    quickOutputKnob_.setRange(-12.0, 12.0, 0.1);
    quickOutputKnob_.setTextValueSuffix(" dB");

    auto wireQuickMacro = [this](juce::Slider& s)
    {
        s.onValueChange = [this]
        {
            if (! suppressQuickCallbacks_)
                applyQuickMacroTargets();
        };
    };
    wireQuickMacro(quickWarmKnob_);
    wireQuickMacro(quickToneKnob_);
    wireQuickMacro(quickPunchKnob_);
    wireQuickMacro(quickGlueKnob_);
    wireQuickMacro(quickWidthKnob_);
    quickOutputKnob_.onValueChange = [this]
    {
        if (suppressQuickCallbacks_)
            return;
        writeFloatParam("outputDb", static_cast<float>(quickOutputKnob_.getValue()));
    };
    neuralBlendKnob_.setTextValueSuffix(" %");
    neuralBlendKnob_.setTooltip(
        "Physics/neural residual blend. 0% = pure physics, 100% = full residual. "
        "Set VALVRA_NEURAL_MODEL=/path/to/model.json to load RTNeural weights.");
    neuralBlendKnob_.textFromValueFunction = [](double v)
    {
        return juce::String(static_cast<int>(std::round(v * 100.0)));
    };
    neuralBlendKnob_.valueFromTextFunction = [](const juce::String& t)
    {
        return t.getDoubleValue() / 100.0;
    };
    expansionAmountKnob_.setTextValueSuffix(" %");
    expansionAmountKnob_.setTooltip(
        "Tier 4+ expansion intensity. Off in Mode disables this block.");
    expansionAmountKnob_.textFromValueFunction = [](double v)
    {
        return juce::String(static_cast<int>(std::round(v * 100.0)));
    };
    expansionAmountKnob_.valueFromTextFunction = [](const juce::String& t)
    {
        return t.getDoubleValue() / 100.0;
    };
    expansionMixKnob_.setTextValueSuffix(" %");
    expansionMixKnob_.setTooltip("Tier 4+ expansion wet/dry mix.");
    expansionMixKnob_.textFromValueFunction = [](double v)
    {
        return juce::String(static_cast<int>(std::round(v * 100.0)));
    };
    expansionMixKnob_.valueFromTextFunction = [](const juce::String& t)
    {
        return t.getDoubleValue() / 100.0;
    };

    oversampleBox_.addItemList(
        { "Low (1x)", "Medium (2x)", "High (4x)", "Ultra (8x)", "Insane (16x)" }, 1);
    addAndMakeVisible(oversampleBox_);
    addAndMakeVisible(oversampleLabel_);
    oversampleLabel_.setJustificationType(juce::Justification::centredLeft);

    mcDistributionBox_.addItemList({ "Modern", "Vintage", "Warm", "Wild" }, 1);
    mcDistributionBox_.setTooltip(
        "Controls the Monte Carlo tolerance spread used by Reroll.");
    mcLockToggle_.setTooltip(
        "Locks the current Monte Carlo seed so Reroll, recall, and factory "
        "preset loads keep this unit's character.");
    addAndMakeVisible(mcDistributionBox_);
    addAndMakeVisible(mcDistributionLabel_);
    addAndMakeVisible(mcLockToggle_);
    mcDistributionLabel_.setJustificationType(juce::Justification::centredLeft);

    cvModeBox_.addItemList({ "T", "P1", "P2" }, 1);
    cvModeBox_.setTooltip(
        "Culture Vulture 6AS6 mode: T warm triode, P1 pentode, P2 extreme.");
    addAndMakeVisible(cvModeBox_);
    addAndMakeVisible(cvModeLabel_);
    cvModeLabel_.setJustificationType(juce::Justification::centredLeft);

    expansionModeBox_.addItemList(
        { "Off", "Opto Glue", "FET Punch", "Tape Print", "Synth FX" }, 1);
    expansionModeBox_.setTooltip(
        "Analog/creative engine. Output tab frames Opto/FET/Tape as analog bus colour.");
    addAndMakeVisible(expansionModeBox_);
    addAndMakeVisible(expansionModeLabel_);
    expansionModeLabel_.setJustificationType(juce::Justification::centredLeft);

    stageCountBox_.addItemList(
        { "Preset", "1 Stage", "2 Stages", "3 Stages", "4 Stages" }, 1);
    inputTrafoBox_.addItemList(
        { "Preset", "Off", "Marinair", "UTC A-12", "Jensen", "Lundahl" }, 1);
    outputTrafoBox_.addItemList(
        { "Preset", "Off", "Marinair", "UTC A-12", "Jensen", "Lundahl" }, 1);
    addAndMakeVisible(stageCountBox_);
    addAndMakeVisible(inputTrafoBox_);
    addAndMakeVisible(outputTrafoBox_);
    addAndMakeVisible(stageCountLabel_);
    addAndMakeVisible(inputTrafoLabel_);
    addAndMakeVisible(outputTrafoLabel_);
    stageCountLabel_.setJustificationType(juce::Justification::centredLeft);
    inputTrafoLabel_.setJustificationType(juce::Justification::centredLeft);
    outputTrafoLabel_.setJustificationType(juce::Justification::centredLeft);

    // Factory preset selector.  Item IDs are 1-based; IDs 1..N map to
    // factoryPresets()[0..N-1].  ID 0 is reserved as the "no selection" hint.
    const auto& presets = factoryPresets();
    factoryPresetBox_.addItem("Choose a Preset", -1);
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
    factoryPresetLabel_.setJustificationType(juce::Justification::centredLeft);

    // UI scale presets (docs/16): 100 / 125 / 150 / 200 %
    uiScaleBox_.addItem("100%", 1);
    uiScaleBox_.addItem("125%", 2);
    uiScaleBox_.addItem("150%", 3);
    uiScaleBox_.addItem("200%", 4);
    {
        const float saved = processor_.uiScale();
        int id = 1;
        if (saved >= 1.875f)      id = 4;
        else if (saved >= 1.375f) id = 3;
        else if (saved >= 1.125f) id = 2;
        uiScaleBox_.setSelectedId(id, juce::dontSendNotification);
    }
    uiScaleBox_.setTooltip("Editor scaling preset.");
    uiScaleBox_.onChange = [this]
    {
        switch (uiScaleBox_.getSelectedId())
        {
            case 2: applyUiScale(1.25f); break;
            case 3: applyUiScale(1.50f); break;
            case 4: applyUiScale(2.00f); break;
            case 1:
            default: applyUiScale(1.00f); break;
        }
        processor_.setUiScale(uiScale_);
    };
    addAndMakeVisible(uiScaleBox_);
    addAndMakeVisible(uiScaleLabel_);
    uiScaleLabel_.setJustificationType(juce::Justification::centredLeft);

    auto setupTabButton = [this](juce::TextButton& b, MainTab tab)
    {
        b.setClickingTogglesState(true);
        b.setRadioGroupId(0x56414C56); // "VALV"
        b.onClick = [this, tab] { selectTab(tab); };
        addAndMakeVisible(b);
    };
    setupTabButton(tabSoundButton_, MainTab::Sound);
    setupTabButton(tabAnalysisButton_, MainTab::Analysis);
    setupTabButton(tabMasterButton_, MainTab::Mastering);

    auto setupModeButton = [this](juce::TextButton& b, SoundControlMode mode)
    {
        b.setClickingTogglesState(true);
        b.setRadioGroupId(0x51554B31); // "QUK1"
        b.onClick = [this, mode] { setSoundControlMode(mode); };
        addAndMakeVisible(b);
    };
    setupModeButton(quickModeButton_, SoundControlMode::Quick);
    setupModeButton(advancedModeButton_, SoundControlMode::Advanced);

    // Signature views
    addAndMakeVisible(chainBuilderView_);
    addAndMakeVisible(stageEditor_);
    addAndMakeVisible(masteringPanel_);
    addAndMakeVisible(hysteresisView_);
    addAndMakeVisible(harmonicView_);
    addAndMakeVisible(driftView_);
    addAndMakeVisible(rerollTimeline_);
    addAndMakeVisible(nullTestToggle_);

    // Reroll
    rerollButton_.onClick = [this]
    {
        processor_.reroll();
        updateSeedLabel();
    };
    addAndMakeVisible(rerollButton_);

    // Warmup — re-arm gm envelope on every stage so the next ~30s of audio
    // settles back to unity.  The processor flag picks this up at the next
    // processBlock boundary; we don't need to wait for it here.
    warmupButton_.setTooltip(
        "Re-run the 30-second tube warmup envelope on every stage.");
    warmupButton_.onClick = [this] { processor_.triggerWarmup(); };
    addAndMakeVisible(warmupButton_);

    // A/B compare — first click populates the destination from current state.
    // Blind mode hides which slot is active and randomizes the compared slot.
    abButton_.setTooltip(
        "Switch between two parameter snapshots. "
        "Blind mode randomizes A/B selection.");
    abButton_.onClick = [this]
    {
        processor_.toggleABForCompare();
        refreshABControls();
        updateSeedLabel();
    };
    addAndMakeVisible(abButton_);

    copyAToBButton_.setTooltip("Copy slot A snapshot to slot B.");
    copyAToBButton_.onClick = [this]
    {
        processor_.copyAToB();
        refreshABControls();
        updateSeedLabel();
    };
    addAndMakeVisible(copyAToBButton_);

    copyBToAButton_.setTooltip("Copy slot B snapshot to slot A.");
    copyBToAButton_.onClick = [this]
    {
        processor_.copyBToA();
        refreshABControls();
        updateSeedLabel();
    };
    addAndMakeVisible(copyBToAButton_);

    resetABButton_.setTooltip("Reset A/B slots and set current state as slot A.");
    resetABButton_.onClick = [this]
    {
        processor_.resetAB();
        refreshABControls();
        updateSeedLabel();
    };
    addAndMakeVisible(resetABButton_);

    auto bindSnapshotButton = [this](juce::TextButton& b,
                                     ValvraProcessor::SnapshotSlot slot,
                                     const juce::String& label)
    {
        b.setTooltip(label + ": click=load, Shift+click=store.");
        b.onClick = [this, slot]
        {
            const bool forceStore =
                juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown();
            if (forceStore)
                processor_.storeSnapshot(slot);
            else if (! processor_.loadSnapshot(slot))
                processor_.storeSnapshot(slot);

            refreshABControls();
            updateSeedLabel();
        };
        addAndMakeVisible(b);
    };
    bindSnapshotButton(snapshotCButton_, ValvraProcessor::SnapshotSlot::C, "Slot C");
    bindSnapshotButton(snapshotDButton_, ValvraProcessor::SnapshotSlot::D, "Slot D");
    bindSnapshotButton(snapshotEButton_, ValvraProcessor::SnapshotSlot::E, "Slot E");

    undoABButton_.setTooltip("Undo A/B workflow action (up to 32 steps).");
    undoABButton_.onClick = [this]
    {
        processor_.undoAB();
        refreshABControls();
        updateSeedLabel();
    };
    addAndMakeVisible(undoABButton_);

    redoABButton_.setTooltip("Redo A/B workflow action.");
    redoABButton_.onClick = [this]
    {
        processor_.redoAB();
        refreshABControls();
        updateSeedLabel();
    };
    addAndMakeVisible(redoABButton_);

    blindABToggle_.setTooltip(
        "Blind A/B: hide active slot and randomize compare target.");
    blindABToggle_.setToggleState(processor_.abBlindMode(),
                                  juce::dontSendNotification);
    blindABToggle_.onClick = [this]
    {
        processor_.setABBlindModeWithHistory(blindABToggle_.getToggleState());
        refreshABControls();
    };
    addAndMakeVisible(blindABToggle_);

    addAndMakeVisible(seedLabel_);
    seedLabel_.setJustificationType(juce::Justification::centredLeft);

    loadNeuralButton_.setTooltip("Load RTNeural JSON model and crossfade in.");
    loadNeuralButton_.onClick = [this]
    {
        neuralModelChooser_ = std::make_unique<juce::FileChooser>(
            "Load RTNeural JSON",
            juce::File(),
            "*.json");
        auto flags = juce::FileBrowserComponent::openMode
                   | juce::FileBrowserComponent::canSelectFiles;
        neuralModelChooser_->launchAsync(flags, [this](const juce::FileChooser& chooser)
        {
            const auto f = chooser.getResult();
            if (! f.existsAsFile())
                return;
            processor_.loadNeuralModelFile(f.getFullPathName());
            refreshNeuralModelLabel();
            updateSeedLabel();
        });
    };
    addAndMakeVisible(loadNeuralButton_);

    unloadNeuralButton_.setTooltip("Unload neural model and run pure-physics path.");
    unloadNeuralButton_.onClick = [this]
    {
        processor_.unloadNeuralModel();
        refreshNeuralModelLabel();
        updateSeedLabel();
    };
    addAndMakeVisible(unloadNeuralButton_);

    addAndMakeVisible(neuralModelLabel_);
    neuralModelLabel_.setJustificationType(juce::Justification::centredLeft);
    neuralModelLabel_.setFont(juce::FontOptions(10.0f));

    rebuildVintageTexture();
    refreshNeuralModelLabel();
    updateSeedLabel();
    startTimerHz(4);

    auto& p = processor_.parameters();
    presetAttach_     = std::make_unique<ComboAttach>(p, "preset",     presetBox_);
    driveAttach_      = std::make_unique<SliderAttach>(p, "drive",     driveKnob_);
    outputAttach_     = std::make_unique<SliderAttach>(p, "outputDb",  outputKnob_);
    mixAttach_        = std::make_unique<SliderAttach>(p, "mix",       mixKnob_);
    neuralBlendAttach_ = std::make_unique<SliderAttach>(p, "neuralBlend", neuralBlendKnob_);
    oversampleAttach_ = std::make_unique<ComboAttach>(p, "oversample", oversampleBox_);
    mcDistributionAttach_ = std::make_unique<ComboAttach>(p, "mcDistribution", mcDistributionBox_);
    mcLockAttach_     = std::make_unique<ButtonAttach>(p, "mcLock",    mcLockToggle_);
    cvModeAttach_     = std::make_unique<ComboAttach>(p, "cvMode",     cvModeBox_);
    expansionModeAttach_ = std::make_unique<ComboAttach>(p, "expansionMode", expansionModeBox_);
    expansionAmountAttach_ = std::make_unique<SliderAttach>(p, "expansionAmount", expansionAmountKnob_);
    expansionMixAttach_ = std::make_unique<SliderAttach>(p, "expansionMix", expansionMixKnob_);
    stageCountAttach_ = std::make_unique<ComboAttach>(p, "stageCount", stageCountBox_);
    inputTrafoAttach_ = std::make_unique<ComboAttach>(p, "inputTrafo", inputTrafoBox_);
    outputTrafoAttach_ = std::make_unique<ComboAttach>(p, "outputTrafo", outputTrafoBox_);

    // Accessibility naming (docs/16 checklist): expose explicit control names
    // so host accessibility bridges can present meaningful labels.
    auto setControlName = [](juce::Component& c, const juce::String& name)
    {
        c.setName(name);
    };
    setControlName(presetBox_, "Mode Preset");
    setControlName(oversampleBox_, "Oversampling Quality");
    setControlName(mcDistributionBox_, "Monte Carlo Distribution");
    setControlName(mcLockToggle_, "Monte Carlo Lock");
    setControlName(cvModeBox_, "Culture Vulture Mode");
    setControlName(expansionModeBox_, "Analog Engine");
    setControlName(stageCountBox_, "Stage Count");
    setControlName(inputTrafoBox_, "Input Transformer");
    setControlName(outputTrafoBox_, "Output Transformer");
    setControlName(factoryPresetBox_, "Factory Preset");
    setControlName(uiScaleBox_, "UI Scale");
    setControlName(tabSoundButton_, "Tab Sound");
    setControlName(tabAnalysisButton_, "Tab Analysis");
    setControlName(tabMasterButton_, "Tab Output");
    setControlName(quickModeButton_, "Sound Quick Mode");
    setControlName(advancedModeButton_, "Sound Advanced Mode");

    setControlName(driveKnob_, "Drive");
    setControlName(outputKnob_, "Output Gain");
    setControlName(mixKnob_, "Wet Dry Mix");
    setControlName(neuralBlendKnob_, "Neural Blend");
    setControlName(expansionAmountKnob_, "Expansion Amount");
    setControlName(expansionMixKnob_, "Expansion Mix");
    setControlName(quickWarmKnob_, "Quick Warm");
    setControlName(quickToneKnob_, "Quick Tone");
    setControlName(quickPunchKnob_, "Quick Punch");
    setControlName(quickGlueKnob_, "Quick Glue");
    setControlName(quickWidthKnob_, "Quick Width");
    setControlName(quickOutputKnob_, "Quick Output");

    setControlName(rerollButton_, "Reroll Seed");
    setControlName(warmupButton_, "Warmup Trigger");
    setControlName(abButton_, "AB Compare Toggle");
    setControlName(blindABToggle_, "AB Blind Mode");
    setControlName(copyAToBButton_, "Copy A To B");
    setControlName(copyBToAButton_, "Copy B To A");
    setControlName(resetABButton_, "Reset AB");
    setControlName(snapshotCButton_, "Snapshot C");
    setControlName(snapshotDButton_, "Snapshot D");
    setControlName(snapshotEButton_, "Snapshot E");
    setControlName(undoABButton_, "AB Undo");
    setControlName(redoABButton_, "AB Redo");
    setControlName(loadNeuralButton_, "Load Neural Model");
    setControlName(unloadNeuralButton_, "Unload Neural Model");
    setControlName(hysteresisView_, "BH Hysteresis View");
    setControlName(harmonicView_, "Harmonic Meter View");
    setControlName(driftView_, "Drift Recorder View");
    setControlName(rerollTimeline_, "Reroll Timeline View");
    setControlName(nullTestToggle_, "Null Test Toggle");
    setControlName(chainBuilderView_, "Chain Builder View");
    setControlName(stageEditor_, "Stage Editor Panel");
    setControlName(masteringPanel_, "Output Panel");
    setControlName(seedLabel_, "Seed Status");
    setControlName(neuralModelLabel_, "Neural Model Status");

    refreshABControls();
    setSoundControlMode(SoundControlMode::Quick);
    syncQuickControlsFromParams();
    selectTab(MainTab::Sound);
    if (const char* forcedTab = std::getenv("VALVRA_TAB"))
    {
        juce::String tab = juce::String(forcedTab).trim().toLowerCase();
        if (tab == "analysis")
            selectTab(MainTab::Analysis);
        else if (tab == "output" || tab == "master" || tab == "mastering")
            selectTab(MainTab::Mastering);
        else if (tab == "sound")
            selectTab(MainTab::Sound);
    }

    // Tier-2 signature views acceleration path:
    // render the editor through JUCE's OpenGL backend (Metal-backed on macOS
    // via system OpenGL driver path where available).
#if !defined(VALVRA_DISABLE_EDITOR_OPENGL) || !VALVRA_DISABLE_EDITOR_OPENGL
    openGLContext_.setContinuousRepainting(false);
    openGLContext_.setSwapInterval(1);
    openGLContext_.attachTo(*this);
#endif

    // Host/user free-resize has caused severe layout breakage in practice.
    // Keep editor size deterministic; scaling is handled by UI Scale preset.
    setResizable(false, false);
    setResizeLimits(kBaseWidth, kBaseHeight, kBaseWidth, kBaseHeight);
    applyUiScale(processor_.uiScale());

    if (const char* forcedScale = std::getenv("VALVRA_UI_SCALE_OVERRIDE"))
    {
        const double parsed = std::atof(forcedScale);
        if (parsed > 0.0)
        {
            applyUiScale(static_cast<float>(parsed));
            processor_.setUiScale(uiScale_);
        }
    }
}

ValvraEditor::~ValvraEditor()
{
    neuralModelChooser_.reset();
#if !defined(VALVRA_DISABLE_EDITOR_OPENGL) || !VALVRA_DISABLE_EDITOR_OPENGL
    openGLContext_.detach();
#endif
    setLookAndFeel(nullptr);
}

void ValvraEditor::paint(juce::Graphics& g)
{
    auto px = [this](float v) { return v * uiScale_; };

    // Chassis background with subtle vertical gradient
    g.setGradientFill(juce::ColourGradient(
        juce::Colour::fromRGB(32, 32, 36), 0.0f, 0.0f,
        juce::Colour::fromRGB(16, 16, 18), 0.0f, static_cast<float>(getHeight()),
        false));
    g.fillAll();
    if (vintageTexture_.isValid())
    {
        g.setOpacity(0.075f);
        g.drawImageWithin(vintageTexture_, 0, 0, getWidth(), getHeight(),
                          juce::RectanglePlacement::stretchToFit, false);
        g.setOpacity(1.0f);
    }

    // Title
    g.setColour(juce::Colour::fromRGB(255, 140, 26));
    g.setFont(juce::FontOptions(px(26.0f)).withStyle("Bold"));
    g.drawText("VALVRA", getLocalBounds().removeFromTop(static_cast<int>(px(40.0f))),
               juce::Justification::centred, false);

    g.setColour(juce::Colours::white.withAlpha(0.55f));
    g.setFont(juce::FontOptions(px(10.0f)));
    g.drawText("LIVING TUBE AMP COLOUR",
               getLocalBounds().withTrimmedTop(static_cast<int>(px(42.0f)))
                              .removeFromTop(static_cast<int>(px(16.0f))),
               juce::Justification::centred, false);

    // Subtle "power glow" indicator bar under the title
    const auto ms = processor_.readMasteringState();
    const float peakT = juce::jlimit(0.0f, 1.0f,
        juce::jmap(ms.peakDbfs, -24.0f, -1.0f, 0.0f, 1.0f));
    const float pulse = 0.65f + 0.35f * std::sin(
        static_cast<float>(juce::Time::getMillisecondCounterHiRes() * 0.0026));
    const float glowAlpha = (0.16f + 0.34f * peakT) * pulse;
    auto glowArea = juce::Rectangle<float>(0.0f, px(60.0f),
                                           static_cast<float>(getWidth()), px(2.0f));
    g.setGradientFill(juce::ColourGradient(
        juce::Colours::transparentBlack, 0.0f, px(61.0f),
        juce::Colour::fromRGB(255, 140, 26).withAlpha(glowAlpha),
        glowArea.getCentreX(), px(61.0f), true));
    g.fillRect(glowArea);
}

void ValvraEditor::resized()
{
    auto px = [this](int v) { return static_cast<int>(std::round(v * uiScale_)); };

    const int kLabelH = px(12);
    const int kComboH = px(26);
    const int kComboRowH = kLabelH + kComboH + px(4);
    const int kTopGap = px(4);
    const int kStripH = px(30);
    auto placeLabeledCombo = [&](juce::Rectangle<int> cell,
                                 juce::Label& label,
                                 juce::ComboBox& box)
    {
        label.setBounds(cell.removeFromTop(kLabelH).reduced(px(2), 0));
        box.setBounds(cell.removeFromTop(kComboH).reduced(px(2), px(1)));
    };

    auto area = getLocalBounds().reduced(px(14));
    area.removeFromTop(px(52));  // title

    // Row 0 — Factory preset selector (full-width on its own row so the
    // category section headings have room to breathe in the popup menu)
    auto row0 = area.removeFromTop(kComboRowH);
    auto scaleCol = row0.removeFromRight(px(164));
    placeLabeledCombo(row0.reduced(px(6), 0), factoryPresetLabel_, factoryPresetBox_);
    placeLabeledCombo(scaleCol.reduced(px(6), 0), uiScaleLabel_, uiScaleBox_);

    // Row 1 — Mode + Quality + Monte Carlo + CV + Tier4+ engine mode
    area.removeFromTop(kTopGap);
    auto row1 = area.removeFromTop(kComboRowH);
    const int row1W = row1.getWidth() / 6;
    placeLabeledCombo(row1.removeFromLeft(row1W).reduced(px(6), 0), presetLabel_, presetBox_);
    placeLabeledCombo(row1.removeFromLeft(row1W).reduced(px(6), 0), oversampleLabel_, oversampleBox_);
    placeLabeledCombo(row1.removeFromLeft(row1W).reduced(px(6), 0), mcDistributionLabel_, mcDistributionBox_);
    placeLabeledCombo(row1.removeFromLeft(row1W).reduced(px(6), 0), cvModeLabel_, cvModeBox_);
    placeLabeledCombo(row1.removeFromLeft(row1W).reduced(px(6), 0), expansionModeLabel_, expansionModeBox_);
    auto lockCell = row1.reduced(px(8), 0);
    lockCell.removeFromTop(kLabelH);
    const auto lockBounds = lockCell.removeFromTop(kComboH).reduced(px(6), px(1));
    mcLockToggle_.setBounds(lockBounds);

    area.removeFromTop(kTopGap);
    auto compareRowA = area.removeFromTop(kStripH);
    abButton_.setBounds(compareRowA.removeFromLeft(px(58)).reduced(px(2)));
    blindABToggle_.setBounds(compareRowA.removeFromLeft(px(64)).reduced(px(2)));
    copyAToBButton_.setBounds(compareRowA.removeFromLeft(px(54)).reduced(px(2)));
    copyBToAButton_.setBounds(compareRowA.removeFromLeft(px(54)).reduced(px(2)));
    resetABButton_.setBounds(compareRowA.removeFromLeft(px(78)).reduced(px(2)));
    undoABButton_.setBounds(compareRowA.removeFromLeft(px(58)).reduced(px(2)));
    redoABButton_.setBounds(compareRowA.removeFromLeft(px(58)).reduced(px(2)));
    auto nullBounds = lockBounds;
    nullBounds.setY(compareRowA.getY() + (compareRowA.getHeight() - nullBounds.getHeight()) / 2);
    nullBounds.setHeight(juce::jmin(nullBounds.getHeight(), compareRowA.getHeight() - px(2)));
    nullTestToggle_.setBounds(nullBounds);
    auto seedBounds = compareRowA.reduced(px(6), px(3));
    seedBounds.setRight(juce::jmax(seedBounds.getX(), nullBounds.getX() - px(6)));
    seedLabel_.setBounds(seedBounds);

    area.removeFromTop(px(2));
    auto compareRowB = area.removeFromTop(kStripH);
    snapshotCButton_.setBounds(compareRowB.removeFromLeft(px(34)).reduced(px(2)));
    snapshotDButton_.setBounds(compareRowB.removeFromLeft(px(34)).reduced(px(2)));
    snapshotEButton_.setBounds(compareRowB.removeFromLeft(px(34)).reduced(px(2)));
    rerollButton_.setBounds(compareRowB.removeFromLeft(px(66)).reduced(px(2)));
    warmupButton_.setBounds(compareRowB.removeFromLeft(px(72)).reduced(px(2)));
    loadNeuralButton_.setBounds(compareRowB.removeFromLeft(px(74)).reduced(px(2)));
    unloadNeuralButton_.setBounds(compareRowB.removeFromLeft(px(84)).reduced(px(2)));

    area.removeFromTop(kTopGap);
    auto tabRow = area.removeFromTop(kStripH);
    const int tabW = tabRow.getWidth() / 3;
    tabSoundButton_.setBounds(tabRow.removeFromLeft(tabW).reduced(px(2)));
    tabAnalysisButton_.setBounds(tabRow.removeFromLeft(tabW).reduced(px(2)));
    tabMasterButton_.setBounds(tabRow.reduced(px(2)));

    area.removeFromTop(px(8));
    auto content = area;

    if (activeTab_ == MainTab::Sound)
    {
        auto row2 = content.removeFromTop(px(116));
        const int knobCol = row2.getWidth() / 6;
        auto kArea = row2.reduced(px(8), px(10));
        driveKnob_.setBounds(kArea.removeFromLeft(knobCol));
        outputKnob_.setBounds(kArea.removeFromLeft(knobCol));
        mixKnob_.setBounds(kArea.removeFromLeft(knobCol));
        neuralBlendKnob_.setBounds(kArea.removeFromLeft(knobCol));
        expansionAmountKnob_.setBounds(kArea.removeFromLeft(knobCol));
        expansionMixKnob_.setBounds(kArea);

        content.removeFromTop(px(6));
        auto chainControls = content.removeFromTop(kComboRowH);
        const int controlW = chainControls.getWidth() / 3;
        placeLabeledCombo(chainControls.removeFromLeft(controlW).reduced(px(6), 0), stageCountLabel_, stageCountBox_);
        placeLabeledCombo(chainControls.removeFromLeft(controlW).reduced(px(6), 0), inputTrafoLabel_, inputTrafoBox_);
        placeLabeledCombo(chainControls.reduced(px(6), 0), outputTrafoLabel_, outputTrafoBox_);
        content.removeFromTop(px(6));

        auto chainRow = content.removeFromTop(px(88));
        chainBuilderView_.setBounds(chainRow.reduced(px(4)));

        auto footer = content.removeFromBottom(px(20));
        content.removeFromTop(px(8));
        stageEditor_.setBounds(content.reduced(px(4)));
        neuralModelLabel_.setBounds(footer.reduced(px(4), 0));
    }
    else if (activeTab_ == MainTab::Analysis)
    {
        auto analysisArea = content.reduced(px(4));
        const int gap = px(8);
        const int visH = juce::jmax(px(170), static_cast<int>(analysisArea.getHeight() * 0.46f));
        auto rowVis = analysisArea.removeFromTop(visH);
        hysteresisView_.setBounds(rowVis.removeFromLeft(rowVis.getWidth() / 2).reduced(px(4)));
        harmonicView_.setBounds(rowVis.reduced(px(4)));

        analysisArea.removeFromTop(gap);
        const int driftH = juce::jmax(px(112), static_cast<int>(analysisArea.getHeight() * 0.55f));
        auto driftRow = analysisArea.removeFromTop(juce::jmin(driftH, analysisArea.getHeight() - px(44)));
        driftView_.setBounds(driftRow.reduced(px(4)));

        analysisArea.removeFromTop(gap);
        rerollTimeline_.setBounds(analysisArea.reduced(px(4)));
    }
    else if (activeTab_ == MainTab::Mastering)
    {
        masteringPanel_.setBounds(content.reduced(px(4)));
    }

    applyTabVisibility();
    refreshTabButtons();
}

void ValvraEditor::selectTab(MainTab tab)
{
    activeTab_ = tab;
    refreshTabButtons();
    applyTabVisibility();
    resized();
    repaint();
}

void ValvraEditor::refreshTabButtons()
{
    tabSoundButton_.setToggleState(activeTab_ == MainTab::Sound,
                                   juce::dontSendNotification);
    tabAnalysisButton_.setToggleState(activeTab_ == MainTab::Analysis,
                                      juce::dontSendNotification);
    tabMasterButton_.setToggleState(activeTab_ == MainTab::Mastering,
                                    juce::dontSendNotification);
}

void ValvraEditor::setSoundControlMode(SoundControlMode mode)
{
    soundControlMode_ = mode;
    refreshSoundControlButtons();
    syncQuickControlsFromParams();
    applyTabVisibility();
    resized();
    repaint();
}

void ValvraEditor::refreshSoundControlButtons()
{
    quickModeButton_.setToggleState(soundControlMode_ == SoundControlMode::Quick,
                                    juce::dontSendNotification);
    advancedModeButton_.setToggleState(soundControlMode_ == SoundControlMode::Advanced,
                                       juce::dontSendNotification);
}

float ValvraEditor::readParamValue(const char* paramId, float fallback) const
{
    if (auto* v = processor_.parameters().getRawParameterValue(paramId))
        return v->load();
    return fallback;
}

void ValvraEditor::writeFloatParam(const char* paramId, float value)
{
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(
            processor_.parameters().getParameter(paramId)))
    {
        const float n = p->convertTo0to1(value);
        p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, n));
    }
}

void ValvraEditor::writeChoiceParam(const char* paramId, int index)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(
            processor_.parameters().getParameter(paramId)))
    {
        const int clamped = juce::jlimit(0, p->choices.size() - 1, index);
        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(clamped)));
    }
}

void ValvraEditor::applyQuickMacroTargets()
{
    const float warm  = static_cast<float>(quickWarmKnob_.getValue() * 0.01);
    const float tone  = static_cast<float>(quickToneKnob_.getValue() * 0.01);
    const float punch = static_cast<float>(quickPunchKnob_.getValue() * 0.01);
    const float glue  = static_cast<float>(quickGlueKnob_.getValue() * 0.01);
    const float width = static_cast<float>(quickWidthKnob_.getValue() * 0.01);

    const float drive = juce::jmap(warm, 0.70f, 2.40f);
    writeFloatParam("drive", juce::jlimit(0.0f, 3.0f, drive));

    const float amount = juce::jlimit(0.0f, 1.0f, std::max({ tone, punch, glue }));
    int expansionMode = 0; // Off
    if (amount > 0.06f)
    {
        if (tone >= punch && tone >= glue)      expansionMode = 3; // Tape Print
        else if (punch >= glue)                 expansionMode = 2; // FET Punch
        else                                    expansionMode = 1; // Opto Glue
    }

    writeChoiceParam("expansionMode", expansionMode);
    writeFloatParam("expansionAmount", amount);
    writeFloatParam("expansionMix", juce::jlimit(0.0f, 1.0f, 0.25f + 0.70f * amount));

    const int currentMsMode = static_cast<int>(readParamValue("msMode", 0.0f));
    int targetMsMode = currentMsMode;
    if (width >= 0.58f) targetMsMode = 1;
    else if (width <= 0.42f) targetMsMode = 0;
    writeChoiceParam("msMode", targetMsMode);

    writeFloatParam("outputDb", static_cast<float>(quickOutputKnob_.getValue()));
}

void ValvraEditor::syncQuickControlsFromParams()
{
    if (quickWarmKnob_.isMouseButtonDown()
        || quickToneKnob_.isMouseButtonDown()
        || quickPunchKnob_.isMouseButtonDown()
        || quickGlueKnob_.isMouseButtonDown()
        || quickWidthKnob_.isMouseButtonDown()
        || quickOutputKnob_.isMouseButtonDown())
        return;

    const float drive = readParamValue("drive", 1.0f);
    const int expansionMode = static_cast<int>(readParamValue("expansionMode", 0.0f));
    const float expansionAmount = readParamValue("expansionAmount", 0.0f);
    const int msMode = static_cast<int>(readParamValue("msMode", 0.0f));
    const float outputDb = readParamValue("outputDb", 0.0f);

    suppressQuickCallbacks_ = true;
    quickWarmKnob_.setValue(
        juce::jlimit(0.0, 100.0,
            static_cast<double>(juce::jmap(
                juce::jlimit(0.70f, 2.40f, drive), 0.70f, 2.40f, 0.0f, 100.0f))),
        juce::dontSendNotification);

    quickToneKnob_.setValue(0.0, juce::dontSendNotification);
    quickPunchKnob_.setValue(0.0, juce::dontSendNotification);
    quickGlueKnob_.setValue(0.0, juce::dontSendNotification);
    if (expansionMode == 3)      quickToneKnob_.setValue(expansionAmount * 100.0f, juce::dontSendNotification);
    else if (expansionMode == 2) quickPunchKnob_.setValue(expansionAmount * 100.0f, juce::dontSendNotification);
    else if (expansionMode == 1) quickGlueKnob_.setValue(expansionAmount * 100.0f, juce::dontSendNotification);

    quickWidthKnob_.setValue(msMode == 1 ? 100.0 : 0.0, juce::dontSendNotification);
    quickOutputKnob_.setValue(juce::jlimit(-12.0, 12.0, static_cast<double>(outputDb)),
                              juce::dontSendNotification);
    suppressQuickCallbacks_ = false;
}

void ValvraEditor::applyTabVisibility()
{
    const bool sound = activeTab_ == MainTab::Sound;
    const bool analysis = activeTab_ == MainTab::Analysis;
    const bool mastering = activeTab_ == MainTab::Mastering;

    driveKnob_.setVisible(sound);
    outputKnob_.setVisible(sound);
    mixKnob_.setVisible(sound);
    neuralBlendKnob_.setVisible(sound);
    expansionAmountKnob_.setVisible(sound);
    expansionMixKnob_.setVisible(sound);
    driveLabel_.setVisible(sound);
    outputLabel_.setVisible(sound);
    mixLabel_.setVisible(sound);
    neuralBlendLabel_.setVisible(sound);
    expansionAmountLabel_.setVisible(sound);
    expansionMixLabel_.setVisible(sound);
    stageCountBox_.setVisible(sound);
    inputTrafoBox_.setVisible(sound);
    outputTrafoBox_.setVisible(sound);
    stageCountLabel_.setVisible(sound);
    inputTrafoLabel_.setVisible(sound);
    outputTrafoLabel_.setVisible(sound);

    quickWarmKnob_.setVisible(false);
    quickToneKnob_.setVisible(false);
    quickPunchKnob_.setVisible(false);
    quickGlueKnob_.setVisible(false);
    quickWidthKnob_.setVisible(false);
    quickOutputKnob_.setVisible(false);
    quickWarmLabel_.setVisible(false);
    quickToneLabel_.setVisible(false);
    quickPunchLabel_.setVisible(false);
    quickGlueLabel_.setVisible(false);
    quickWidthLabel_.setVisible(false);
    quickOutputLabel_.setVisible(false);

    chainBuilderView_.setVisible(sound);
    stageEditor_.setVisible(sound);
    loadNeuralButton_.setVisible(sound);
    unloadNeuralButton_.setVisible(sound);
    neuralModelLabel_.setVisible(sound);

    hysteresisView_.setVisible(analysis);
    harmonicView_.setVisible(analysis);
    driftView_.setVisible(analysis);

    masteringPanel_.setVisible(mastering);
    quickModeButton_.setVisible(false);
    advancedModeButton_.setVisible(false);

    abButton_.setVisible(true);
    blindABToggle_.setVisible(true);
    copyAToBButton_.setVisible(true);
    copyBToAButton_.setVisible(true);
    resetABButton_.setVisible(true);
    snapshotCButton_.setVisible(true);
    snapshotDButton_.setVisible(true);
    snapshotEButton_.setVisible(true);
    undoABButton_.setVisible(true);
    redoABButton_.setVisible(true);
    nullTestToggle_.setVisible(true);

    rerollTimeline_.setVisible(analysis);
    rerollButton_.setVisible(true);
    warmupButton_.setVisible(true);
    seedLabel_.setVisible(true);
}

bool ValvraEditor::keyPressed(const juce::KeyPress& key)
{
    // docs/16.9 keyboard workflow:
    //   A/B key: compare toggle
    //   Cmd/Ctrl+Z: undo
    //   Cmd/Ctrl+Shift+Z: redo
    const auto mods = key.getModifiers();
    if (auto* focused = juce::Component::getCurrentlyFocusedComponent())
    {
        // Don't steal shortcuts while editing text or interacting with
        // selector widgets that expect keyboard navigation.
        if (dynamic_cast<juce::TextEditor*>(focused) != nullptr
            || focused->findParentComponentOfClass<juce::TextEditor>() != nullptr
            || focused->findParentComponentOfClass<juce::ComboBox>() != nullptr)
            return juce::AudioProcessorEditor::keyPressed(key);
    }
    if (auto* modal = juce::ModalComponentManager::getInstance())
    {
        if (modal->getNumModalComponents() > 0)
            return juce::AudioProcessorEditor::keyPressed(key);
    }

    if (mods.isCommandDown()
        && key.getKeyCode() == 'z'
        && mods.isShiftDown())
    {
        processor_.redoAB();
        refreshABControls();
        updateSeedLabel();
        return true;
    }

    if (mods.isCommandDown()
        && key.getKeyCode() == 'z')
    {
        processor_.undoAB();
        refreshABControls();
        updateSeedLabel();
        return true;
    }

    // UI scale quick presets for fast layout validation and accessibility.
    if (mods.isCommandDown() && !mods.isShiftDown() && !mods.isAltDown())
    {
        switch (key.getKeyCode())
        {
            case '1': applyUiScale(1.00f); processor_.setUiScale(uiScale_); return true;
            case '2': applyUiScale(1.25f); processor_.setUiScale(uiScale_); return true;
            case '3': applyUiScale(1.50f); processor_.setUiScale(uiScale_); return true;
            case '4': applyUiScale(2.00f); processor_.setUiScale(uiScale_); return true;
            default: break;
        }
    }

    if (!mods.isAnyModifierKeyDown())
    {
        const auto kc = key.getKeyCode();
        switch (kc)
        {
            case '1': applyUiScale(1.00f); processor_.setUiScale(uiScale_); return true;
            case '2': applyUiScale(1.25f); processor_.setUiScale(uiScale_); return true;
            case '3': applyUiScale(1.50f); processor_.setUiScale(uiScale_); return true;
            case '4': applyUiScale(2.00f); processor_.setUiScale(uiScale_); return true;
            default: break;
        }
        if (kc == 'a' || kc == 'A' || kc == 'b' || kc == 'B')
        {
            processor_.toggleABForCompare();
            refreshABControls();
            updateSeedLabel();
            return true;
        }
    }

    return juce::AudioProcessorEditor::keyPressed(key);
}

void ValvraEditor::mouseDown(const juce::MouseEvent& e)
{
    juce::AudioProcessorEditor::mouseDown(e);
    if (!hasKeyboardFocus(false))
        grabKeyboardFocus();
}

void ValvraEditor::updateSeedLabel()
{
    const auto seedHex = juce::String::toHexString(
        static_cast<juce::int64>(processor_.currentSeed())).toUpperCase();
    const juce::String neuralState =
        processor_.neuralModelLoaded() ? "RT" : "Bootstrap";
    seedLabel_.setText("Seed: 0x" + seedHex + " | NN: " + neuralState,
                       juce::dontSendNotification);
}

void ValvraEditor::refreshNeuralModelLabel()
{
    const auto p = processor_.neuralModelPath();
    if (p.isEmpty())
    {
        neuralModelLabel_.setText("Model: (none)", juce::dontSendNotification);
        return;
    }
    juce::File f(p);
    neuralModelLabel_.setText("Model: " + f.getFileName(),
                              juce::dontSendNotification);
}

void ValvraEditor::applyUiScale(float scale)
{
    const float requestedScale = juce::jlimit(1.0f, 2.0f, scale);

    float fitScaleX = 2.0f;
    float fitScaleY = 2.0f;
    {
        const auto& displays = juce::Desktop::getInstance().getDisplays();
        const auto* d = displays.getDisplayForRect(getScreenBounds());
        if (d == nullptr)
            d = displays.getPrimaryDisplay();
        if (d != nullptr)
        {
            const int usableW = juce::jmax(kBaseWidth, d->userArea.getWidth() - 24);
            const int usableH = juce::jmax(kBaseHeight, d->userArea.getHeight() - 56);
            fitScaleX = static_cast<float>(usableW) / static_cast<float>(kBaseWidth);
            fitScaleY = static_cast<float>(usableH) / static_cast<float>(kBaseHeight);
        }
    }

    float fitScale = juce::jlimit(1.0f, 2.0f, std::min(fitScaleX, fitScaleY));
#if defined(VALVRA_ENABLE_UI_TEST_HOOKS) && VALVRA_ENABLE_UI_TEST_HOOKS
    // Tests can request exact scale application even when the host display
    // would normally clamp by available screen area.
    if (std::getenv("VALVRA_UI_TEST_NO_FIT") != nullptr)
        fitScale = 2.0f;
#endif
    scale = juce::jmin(requestedScale, fitScale);
    const bool changed = std::abs(scale - uiScale_) >= 1.0e-6f;
    uiScale_ = scale;
    lnf_.setUiScale(uiScale_);
    auto scaleKnobTextBox = [this](juce::Slider& s)
    {
        const int w = static_cast<int>(std::round(70.0f * uiScale_));
        const int h = static_cast<int>(std::round(16.0f * uiScale_));
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, w, h);
    };
    scaleKnobTextBox(driveKnob_);
    scaleKnobTextBox(outputKnob_);
    scaleKnobTextBox(mixKnob_);
    scaleKnobTextBox(neuralBlendKnob_);
    scaleKnobTextBox(expansionAmountKnob_);
    scaleKnobTextBox(expansionMixKnob_);
    scaleKnobTextBox(quickWarmKnob_);
    scaleKnobTextBox(quickToneKnob_);
    scaleKnobTextBox(quickPunchKnob_);
    scaleKnobTextBox(quickGlueKnob_);
    scaleKnobTextBox(quickWidthKnob_);
    scaleKnobTextBox(quickOutputKnob_);
    const auto comboLabelFont = juce::FontOptions(9.5f * uiScale_);
    factoryPresetLabel_.setFont(comboLabelFont);
    uiScaleLabel_.setFont(comboLabelFont);
    presetLabel_.setFont(comboLabelFont);
    oversampleLabel_.setFont(comboLabelFont);
    mcDistributionLabel_.setFont(comboLabelFont);
    cvModeLabel_.setFont(comboLabelFont);
    expansionModeLabel_.setFont(comboLabelFont);
    stageCountLabel_.setFont(comboLabelFont);
    inputTrafoLabel_.setFont(comboLabelFont);
    outputTrafoLabel_.setFont(comboLabelFont);
    quickWarmLabel_.setFont(juce::FontOptions(10.0f * uiScale_));
    quickToneLabel_.setFont(juce::FontOptions(10.0f * uiScale_));
    quickPunchLabel_.setFont(juce::FontOptions(10.0f * uiScale_));
    quickGlueLabel_.setFont(juce::FontOptions(10.0f * uiScale_));
    quickWidthLabel_.setFont(juce::FontOptions(10.0f * uiScale_));
    quickOutputLabel_.setFont(juce::FontOptions(10.0f * uiScale_));

    chainBuilderView_.setUiScale(uiScale_);
    stageEditor_.setUiScale(uiScale_);
    masteringPanel_.setUiScale(uiScale_);
    hysteresisView_.setUiScale(uiScale_);
    harmonicView_.setUiScale(uiScale_);
    driftView_.setUiScale(uiScale_);
    rerollTimeline_.setUiScale(uiScale_);
    neuralModelLabel_.setFont(juce::FontOptions(10.0f * uiScale_));

    const int minW = static_cast<int>(std::round(kBaseWidth * uiScale_));
    const int minH = static_cast<int>(std::round(kBaseHeight * uiScale_));
    setResizeLimits(minW, minH, minW, minH);

    int scaleId = 1;
    if (uiScale_ >= 1.875f)      scaleId = 4;
    else if (uiScale_ >= 1.375f) scaleId = 3;
    else if (uiScale_ >= 1.125f) scaleId = 2;
    uiScaleBox_.setSelectedId(scaleId, juce::dontSendNotification);

    // Ensure a deterministic initial editor size even when the requested
    // scale equals the current value (constructor/default state path).
    if (changed || getWidth() <= 0 || getHeight() <= 0)
    {
        setSize(minW, minH);
    }
    else
    {
        resized();
        repaint();
    }
}

void ValvraEditor::refreshABButtonLabel()
{
    if (processor_.abBlindMode())
        abButton_.setButtonText("X | Y");
    else
        abButton_.setButtonText(processor_.isOnSlotB() ? "B | A" : "A | B");
}

void ValvraEditor::refreshABControls()
{
    refreshABButtonLabel();
    blindABToggle_.setToggleState(processor_.abBlindMode(),
                                  juce::dontSendNotification);

    undoABButton_.setEnabled(processor_.canUndoAB());
    redoABButton_.setEnabled(processor_.canRedoAB());

    const bool hasC = processor_.hasSnapshot(ValvraProcessor::SnapshotSlot::C);
    const bool hasD = processor_.hasSnapshot(ValvraProcessor::SnapshotSlot::D);
    const bool hasE = processor_.hasSnapshot(ValvraProcessor::SnapshotSlot::E);
    snapshotCButton_.setButtonText(hasC ? "C*" : "C");
    snapshotDButton_.setButtonText(hasD ? "D*" : "D");
    snapshotEButton_.setButtonText(hasE ? "E*" : "E");
}

void ValvraEditor::timerCallback()
{
    syncQuickControlsFromParams();
    refreshNeuralModelLabel();
    updateSeedLabel();
}

void ValvraEditor::rebuildVintageTexture()
{
    vintageTexture_ = juce::Image(
        juce::Image::RGB, 512, 512, true);
    juce::Image::BitmapData pix(vintageTexture_, juce::Image::BitmapData::writeOnly);
    std::mt19937 rng(0x56A2B17u);
    std::uniform_int_distribution<int> dist(0, 255);

    for (int y = 0; y < pix.height; ++y)
    {
        for (int x = 0; x < pix.width; ++x)
        {
            const int n = dist(rng);
            const int v = 64 + (n / 8);
            pix.setPixelColour(x, y, juce::Colour((juce::uint8)v, (juce::uint8)v, (juce::uint8)v));
        }
    }
}

#if defined(VALVRA_ENABLE_UI_TEST_HOOKS) && VALVRA_ENABLE_UI_TEST_HOOKS
void ValvraEditor::debugSetUiScale(float scale)
{
    applyUiScale(scale);
    resized();
}

void ValvraEditor::debugSelectTab(DebugTab tab)
{
    switch (tab)
    {
        case DebugTab::Sound:    selectTab(MainTab::Sound); break;
        case DebugTab::Analysis: selectTab(MainTab::Analysis); break;
        case DebugTab::Output:   selectTab(MainTab::Mastering); break;
    }
}

static juce::Rectangle<int> findNamedBoundsRecursive(const juce::Component& root,
                                                     const juce::Component& c,
                                                     const juce::String& name)
{
    if (c.getName() == name)
        return root.getLocalArea(&c, c.getLocalBounds());

    for (int i = 0; i < c.getNumChildComponents(); ++i)
    {
        if (auto* child = c.getChildComponent(i))
        {
            if (child->getName() == name)
                return root.getLocalArea(child, child->getLocalBounds());
            const auto nested = findNamedBoundsRecursive(root, *child, name);
            if (! nested.isEmpty())
                return nested;
        }
    }
    return {};
}

juce::Rectangle<int> ValvraEditor::debugBoundsForNamedComponent(const juce::String& name) const
{
    if (name.isEmpty())
        return {};
    for (int i = 0; i < getNumChildComponents(); ++i)
    {
        if (auto* child = getChildComponent(i))
        {
            if (child->getName() == name)
                return getLocalArea(child, child->getLocalBounds());
            const auto nested = findNamedBoundsRecursive(*this, *child, name);
            if (! nested.isEmpty())
                return nested;
        }
    }
    return {};
}

StageEditorPanel::DebugLayout ValvraEditor::debugStageEditorLayout() const noexcept
{
    return stageEditor_.debugLayout();
}

MasteringPanel::DebugLayout ValvraEditor::debugMasteringLayout() const
{
    return masteringPanel_.debugLayout();
}

std::vector<ChainBuilderView::DebugNodeBands> ValvraEditor::debugChainBuilderNodeBands() const
{
    return chainBuilderView_.debugNodeBands();
}
#endif

} // namespace valvra
