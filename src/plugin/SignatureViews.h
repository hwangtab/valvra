// ─────────────────────────────────────────────────────────────────────────────
// SignatureViews.h — the three visualizations that differentiate Valvra
// from every other tube emulator on the market:
//
//   1. HysteresisLoopView — live B-H curve of the output transformer
//   2. HarmonicMeterView  — H1..H7 vertical bars, refreshed 30 Hz
//   3. NullTestToggle     — one-click "play only the change" button
//
// References: docs/20 §4.7 (sensational UI)
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

#include <array>
#include <deque>

namespace valvra {

// ─────────────────────────────────────────────────────────────────────────────
// HysteresisLoopView — plots the traced (H, M) path as it evolves in time
// ─────────────────────────────────────────────────────────────────────────────
class HysteresisLoopView final
    : public juce::Component,
      private juce::Timer
{
public:
    explicit HysteresisLoopView(ValvraProcessor& proc)
        : processor_ { proc }
    {
        startTimerHz(30);
        history_.resize(kMaxPoints);
    }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();

        // Background
        g.setColour(juce::Colour::fromRGB(14, 14, 16));
        g.fillRoundedRectangle(r, 4.0f);

        // Title
        g.setColour(juce::Colour::fromRGB(255, 140, 26));
        g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
        g.drawText("B-H Hysteresis (output trafo)",
                   r.reduced(6.0f).removeFromTop(16.0f),
                   juce::Justification::topLeft, false);

        if (history_.empty() || msSat_ <= 0.0f) return;

        // Axes
        const float w   = r.getWidth()  - 20.0f;
        const float h   = r.getHeight() - 40.0f;
        const float cx  = r.getX() + 10.0f + w * 0.5f;
        const float cy  = r.getY() + 30.0f + h * 0.5f;

        // Guide lines
        g.setColour(juce::Colours::white.withAlpha(0.12f));
        g.drawHorizontalLine(static_cast<int>(cy),
                             r.getX() + 10.0f, r.getRight() - 10.0f);
        g.drawVerticalLine(static_cast<int>(cx),
                           r.getY() + 30.0f, r.getBottom() - 10.0f);

        // Trace the loop
        juce::Path path;
        bool first = true;
        for (const auto& pt : history_)
        {
            const float nx = cx + juce::jlimit(-1.0f, 1.0f,
                                               pt.first  / (msSat_ * 0.15f))
                                   * (w * 0.45f);
            const float ny = cy - juce::jlimit(-1.0f, 1.0f,
                                               pt.second / msSat_)
                                   * (h * 0.45f);
            if (first) { path.startNewSubPath(nx, ny); first = false; }
            else        path.lineTo(nx, ny);
        }

        g.setColour(juce::Colour::fromRGB(255, 140, 26).withAlpha(0.85f));
        g.strokePath(path, juce::PathStrokeType(1.2f));

        // Mark the current point
        if (! history_.empty())
        {
            const auto& pt = history_.back();
            const float nx = cx + juce::jlimit(-1.0f, 1.0f,
                                               pt.first / (msSat_ * 0.15f))
                                   * (w * 0.45f);
            const float ny = cy - juce::jlimit(-1.0f, 1.0f,
                                               pt.second / msSat_)
                                   * (h * 0.45f);
            g.setColour(juce::Colours::white);
            g.fillEllipse(nx - 2.5f, ny - 2.5f, 5.0f, 5.0f);
        }
    }

private:
    void timerCallback() override
    {
        const auto s = processor_.readBHState();
        msSat_ = s.Ms;
        if (history_.size() >= kMaxPoints) history_.pop_front();
        history_.push_back({ s.H, s.M });
        repaint();
    }

    static constexpr std::size_t kMaxPoints = 256;
    ValvraProcessor& processor_;
    std::deque<std::pair<float, float>> history_;
    float msSat_ { 1.0f };
};

// ─────────────────────────────────────────────────────────────────────────────
// HarmonicMeterView — vertical bars for H1..H7 in dBc
// ─────────────────────────────────────────────────────────────────────────────
class HarmonicMeterView final
    : public juce::Component,
      private juce::Timer
{
public:
    explicit HarmonicMeterView(ValvraProcessor& proc)
        : processor_ { proc }
    {
        startTimerHz(20);
    }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();

        g.setColour(juce::Colour::fromRGB(14, 14, 16));
        g.fillRoundedRectangle(r, 4.0f);

        g.setColour(juce::Colour::fromRGB(255, 140, 26));
        g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
        g.drawText("Harmonics (dBc)",
                   r.reduced(6.0f).removeFromTop(16.0f),
                   juce::Justification::topLeft, false);

        const auto snap = latest_;
        if (! snap.valid)
        {
            g.setColour(juce::Colours::white.withAlpha(0.3f));
            g.drawText("(accumulating…)", r, juce::Justification::centred,
                       false);
            return;
        }

        // Info line
        g.setColour(juce::Colours::white.withAlpha(0.55f));
        g.setFont(juce::FontOptions(9.0f));
        g.drawText(juce::String::formatted("fund = %.0f Hz",
                                           snap.fundamentalHz),
                   r.reduced(6.0f).removeFromTop(28.0f)
                     .withTrimmedTop(16.0f),
                   juce::Justification::topRight, false);

        // Bars: 7 harmonics H2..H8, x-axis left→right
        auto chart = r.reduced(10.0f, 30.0f);
        const int numBars = static_cast<int>(snap.harmonicsDbc.size());
        const float barW  = chart.getWidth() / static_cast<float>(numBars) - 4.0f;
        const float floorDb = -80.0f;
        const float ceilDb  = 0.0f;
        for (int i = 0; i < numBars; ++i)
        {
            const float v = snap.harmonicsDbc[static_cast<std::size_t>(i)];
            const float t = juce::jlimit(0.0f, 1.0f,
                                         (v - floorDb) / (ceilDb - floorDb));
            const float x = chart.getX() + i * (barW + 4.0f);
            const float y = chart.getBottom() - t * chart.getHeight();
            const auto barRect =
                juce::Rectangle<float>(x, y, barW,
                                       chart.getBottom() - y);

            // Even harmonics in warm orange, odd in cyan
            const bool even = ((i + 2) % 2) == 0;
            auto colour = even
                ? juce::Colour::fromRGB(255, 140, 26)
                : juce::Colour::fromRGB(80, 200, 220);
            g.setColour(colour.withAlpha(0.75f));
            g.fillRoundedRectangle(barRect, 2.0f);

            // Label
            g.setColour(juce::Colours::white.withAlpha(0.5f));
            g.setFont(juce::FontOptions(9.0f));
            g.drawText("H" + juce::String(i + 2),
                       juce::Rectangle<int>(
                           static_cast<int>(x),
                           static_cast<int>(chart.getBottom()),
                           static_cast<int>(barW), 14),
                       juce::Justification::centredTop, false);
        }
    }

private:
    void timerCallback() override
    {
        processor_.refreshHarmonicSnapshot();
        latest_ = processor_.latestHarmonics();
        repaint();
    }

    ValvraProcessor& processor_;
    HarmonicSnapshot latest_ {};
};

// ─────────────────────────────────────────────────────────────────────────────
// NullTestToggle — one-click A/B for the plugin's actual contribution
// ─────────────────────────────────────────────────────────────────────────────
class NullTestToggle final : public juce::ToggleButton
{
public:
    explicit NullTestToggle(ValvraProcessor& proc)
        : juce::ToggleButton("Null Test")
        , processor_ { proc }
    {
        setClickingTogglesState(true);
        setTooltip("Hear only the difference Valvra adds to the signal.");
        onClick = [this]
        {
            processor_.setNullTestMode(getToggleState());
        };
    }

private:
    ValvraProcessor& processor_;
};

} // namespace valvra
