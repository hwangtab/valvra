// ─────────────────────────────────────────────────────────────────────────────
// SignatureViews.h — the three visualizations that differentiate Valvra
// from every other tube emulator on the market:
//
//   1. HysteresisLoopView — live B-H curve of the output transformer
//   2. HarmonicMeterView  — H2..H7 vertical bars, refreshed 20 Hz
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
        // Keep B-H loop visually "live" at monitor cadence.
        startTimerHz(60);
        history_.resize(kMaxPoints);
    }

    void setUiScale(float scale) noexcept
    {
        uiScale_ = juce::jlimit(1.0f, 2.0f, scale);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();

        // Background
        g.setColour(juce::Colour::fromRGB(14, 14, 16));
        const float px = uiScale_;
        g.fillRoundedRectangle(r, 4.0f * px);

        // Title
        g.setColour(juce::Colour::fromRGB(255, 140, 26));
        g.setFont(juce::FontOptions(11.0f * px).withStyle("Bold"));
        g.drawText("B-H Hysteresis (output trafo)",
                   r.reduced(6.0f * px).removeFromTop(16.0f * px),
                   juce::Justification::topLeft, false);

        if (history_.empty() || msSat_ <= 0.0f) return;

        // Axes
        const float w   = r.getWidth()  - 20.0f * px;
        const float h   = r.getHeight() - 40.0f * px;
        const float cx  = r.getX() + 10.0f * px + w * 0.5f;
        const float cy  = r.getY() + 30.0f * px + h * 0.5f;

        // Guide lines
        g.setColour(juce::Colours::white.withAlpha(0.12f));
        g.drawHorizontalLine(static_cast<int>(cy),
                             r.getX() + 10.0f * px, r.getRight() - 10.0f * px);
        g.drawVerticalLine(static_cast<int>(cx),
                           r.getY() + 30.0f * px, r.getBottom() - 10.0f * px);

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
        g.strokePath(path, juce::PathStrokeType(1.2f * px));

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
            g.fillEllipse(nx - 2.5f * px, ny - 2.5f * px, 5.0f * px, 5.0f * px);
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
    float uiScale_ { 1.0f };
};

// ─────────────────────────────────────────────────────────────────────────────
// HarmonicMeterView — vertical bars for H2..H7 in dBc
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

    void setUiScale(float scale) noexcept
    {
        uiScale_ = juce::jlimit(1.0f, 2.0f, scale);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();

        g.setColour(juce::Colour::fromRGB(14, 14, 16));
        const float px = uiScale_;
        g.fillRoundedRectangle(r, 4.0f * px);

        g.setColour(juce::Colour::fromRGB(255, 140, 26));
        g.setFont(juce::FontOptions(11.0f * px).withStyle("Bold"));
        g.drawText("Harmonics (dBc)",
                   r.reduced(6.0f * px).removeFromTop(16.0f * px),
                   juce::Justification::topLeft, false);

        const auto snap = latest_;
        if (! snap.valid)
        {
            g.setColour(juce::Colours::white.withAlpha(0.3f));
            g.drawText("(accumulating...)", r, juce::Justification::centred,
                       false);
            return;
        }

        // Info line
        g.setColour(juce::Colours::white.withAlpha(0.55f));
        g.setFont(juce::FontOptions(9.0f * px));
        g.drawText(juce::String::formatted("fund = %.0f Hz",
                                           snap.fundamentalHz),
                   r.reduced(6.0f * px).removeFromTop(28.0f * px)
                     .withTrimmedTop(16.0f * px),
                   juce::Justification::topRight, false);

        // Bars: harmonics H2..H7, x-axis left→right
        auto chart = r.reduced(10.0f * px, 30.0f * px);
        const int numBars = static_cast<int>(snap.harmonicsDbc.size());
        const float gap = 4.0f * px;
        const float barW  = chart.getWidth() / static_cast<float>(numBars) - gap;
        const float floorDb = -80.0f;
        const float ceilDb  = 0.0f;
        for (int i = 0; i < numBars; ++i)
        {
            const float v = snap.harmonicsDbc[static_cast<std::size_t>(i)];
            const float t = juce::jlimit(0.0f, 1.0f,
                                         (v - floorDb) / (ceilDb - floorDb));
            const float x = chart.getX() + i * (barW + gap);
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
            g.fillRoundedRectangle(barRect, 2.0f * px);

            // Label
            g.setColour(juce::Colours::white.withAlpha(0.5f));
            g.setFont(juce::FontOptions(9.0f * px));
            g.drawText("H" + juce::String(i + 2),
                       juce::Rectangle<int>(
                           static_cast<int>(x),
                           static_cast<int>(chart.getBottom()),
                           static_cast<int>(barW), static_cast<int>(14.0f * px)),
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
    float uiScale_ { 1.0f };
};

// ─────────────────────────────────────────────────────────────────────────────
// DriftRecorderView — 60-second timeline of slow time-varying state
//
// Records and plots three quantities the rest of the industry's plugins do
// not even compute: PSU sag percentage, gm warmup fraction, and slow
// thermal grid-bias drift.  Together they make the "why this is alive"
// visible — a still picture of a still tube amp shows nothing, while this
// view traces the rack settling into character over the first half-minute
// and breathing under sustained load.
//
// Reference: docs/20 §4.7.5 (Drift Recorder).
// ─────────────────────────────────────────────────────────────────────────────
class DriftRecorderView final
    : public juce::Component,
      private juce::Timer
{
public:
    explicit DriftRecorderView(ValvraProcessor& proc)
        : processor_ { proc }
    {
        startTimerHz(kSampleRateHz);
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
        const float px = uiScale_;
        g.fillRoundedRectangle(r, 4.0f * px);

        // Title
        g.setColour(juce::Colour::fromRGB(255, 140, 26));
        g.setFont(juce::FontOptions(11.0f * px).withStyle("Bold"));
        g.drawText("Drift Recorder | 60 s",
                   r.reduced(6.0f * px).removeFromTop(16.0f * px),
                   juce::Justification::topLeft, false);

        // Legend (top-right corner)
        const std::array<std::pair<juce::Colour, const char*>, 3> legend {{
            { juce::Colour::fromRGB(255, 140, 26), "B+ sag %" },
            { juce::Colour::fromRGB(120, 200, 120), "Warmup %" },
            { juce::Colour::fromRGB(120, 160, 240), "Bias drift V" }
        }};
        const float legendW = 86.0f * px;
        const float legendTotalW = legendW * static_cast<float>(legend.size());
        float lx = r.getRight() - legendTotalW;
        for (const auto& [colour, name] : legend)
        {
            g.setColour(colour);
            g.fillRect(juce::Rectangle<float>(lx + 6.0f * px,
                                              r.getY() + 7.0f * px,
                                              8.0f * px, 4.0f * px));
            g.setColour(juce::Colours::white.withAlpha(0.65f));
            g.setFont(juce::FontOptions(9.0f * px));
            g.drawText(name, juce::Rectangle<int>(
                static_cast<int>(lx + 18.0f * px), static_cast<int>(r.getY() + 4.0f * px),
                static_cast<int>(legendW - 18.0f * px), static_cast<int>(12.0f * px)),
                juce::Justification::centredLeft, false);
            lx += legendW;
        }

        // Plot area
        auto plot = r.reduced(8.0f * px, 22.0f * px);
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.drawRoundedRectangle(plot, 3.0f * px, 1.0f);

        if (sagHistory_.empty()) return;

        auto drawTrace = [&](const std::deque<float>& history,
                             juce::Colour colour,
                             float minV, float maxV)
        {
            if (history.empty() || maxV <= minV) return;

            const float w = plot.getWidth();
            const float h = plot.getHeight();
            const float dx = w / static_cast<float>(kMaxPoints);

            juce::Path path;
            const std::size_t startIdx =
                kMaxPoints - history.size();
            bool first = true;
            std::size_t i = 0;
            for (float v : history)
            {
                const float x = plot.getX()
                              + (static_cast<float>(startIdx + i)) * dx;
                const float t = juce::jlimit(0.0f, 1.0f,
                                             (v - minV) / (maxV - minV));
                const float y = plot.getBottom() - t * h;
                if (first) { path.startNewSubPath(x, y); first = false; }
                else        path.lineTo(x, y);
                ++i;
            }
            g.setColour(colour.withAlpha(0.85f));
            g.strokePath(path, juce::PathStrokeType(1.4f * px));
        };

        // Each trace gets its own scale so the line uses the full vertical
        // extent of the plot.  Without independent scales the warmup line
        // (0.85–1.0) and thermal drift line (0–~0.3 V) would compress into
        // a sliver and the user wouldn't see them move.
        drawTrace(sagHistory_,    juce::Colour::fromRGB(255, 140, 26),
                  0.0f, 15.0f);
        drawTrace(warmupHistory_, juce::Colour::fromRGB(120, 200, 120),
                  0.80f, 1.02f);
        drawTrace(thermalHistory_, juce::Colour::fromRGB(120, 160, 240),
                  0.0f, 0.30f);

        // "now" marker on the right
        g.setColour(juce::Colours::white.withAlpha(0.18f));
        g.drawVerticalLine(static_cast<int>(plot.getRight() - 1),
                           plot.getY(), plot.getBottom());
    }

private:
    void timerCallback() override
    {
        const auto s = processor_.readDriftState();
        if (sagHistory_.size()    >= kMaxPoints) sagHistory_.pop_front();
        if (warmupHistory_.size() >= kMaxPoints) warmupHistory_.pop_front();
        if (thermalHistory_.size() >= kMaxPoints) thermalHistory_.pop_front();
        sagHistory_.push_back(s.sagPercent);
        warmupHistory_.push_back(s.warmup);
        thermalHistory_.push_back(s.thermalBiasV);
        repaint();
    }

    static constexpr int kSampleRateHz = 20;            // ticks per second
    static constexpr std::size_t kMaxPoints = 60 * kSampleRateHz;

    ValvraProcessor& processor_;
    std::deque<float> sagHistory_;
    std::deque<float> warmupHistory_;
    std::deque<float> thermalHistory_;
    float uiScale_ { 1.0f };
};

// ─────────────────────────────────────────────────────────────────────────────
// RerollTimelinePanel — last-N seed history with click-to-recall.
//
// Each previous Monte Carlo seed is displayed as a small, clickable cell
// showing the high 16 bits as hex.  Clicking restores that seed via the
// processor's recallSeed() — same code path as Reroll, just with a fixed
// value, so the user can A/B "the unit I had a moment ago" against the
// current one without losing it.
//
// Reference: docs/20 §4.7.6 (Reroll Timeline).
// ─────────────────────────────────────────────────────────────────────────────
class RerollTimelinePanel final
    : public juce::Component,
      private juce::Timer
{
public:
    explicit RerollTimelinePanel(ValvraProcessor& proc)
        : processor_ { proc }
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        startTimerHz(6);
        refresh();
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
        const float px = uiScale_;
        g.fillRoundedRectangle(r, 4.0f * px);

        g.setColour(juce::Colour::fromRGB(255, 140, 26));
        g.setFont(juce::FontOptions(10.0f * px).withStyle("Bold"));
        g.drawText("Reroll Timeline",
                   r.reduced(6.0f * px).removeFromTop(13.0f * px),
                   juce::Justification::topLeft, false);

        if (count_ <= 0) return;

        const auto cells = cellRects();
        const std::uint64_t current = processor_.currentSeed();
        for (std::size_t i = 0; i < cells.size() && i < static_cast<std::size_t>(count_); ++i)
        {
            const auto& cell = cells[i];
            const auto seed = history_[i];

            // Highlight the active seed and the most-recent entry
            const bool isActive = seed == current;
            const bool isLatest = static_cast<int>(i) == count_ - 1;

            g.setColour(isActive
                ? juce::Colour::fromRGB(255, 140, 26).withAlpha(0.40f)
                : juce::Colour::fromRGB(38, 38, 44));
            g.fillRoundedRectangle(cell, 3.0f * px);
            g.setColour(isActive
                ? juce::Colour::fromRGB(255, 140, 26).withAlpha(0.95f)
                : juce::Colours::white.withAlpha(0.18f));
            g.drawRoundedRectangle(cell, 3.0f * px, (isLatest ? 1.5f : 1.0f) * px);

            g.setColour(juce::Colours::white.withAlpha(isActive ? 0.95f : 0.7f));
            g.setFont(juce::FontOptions(9.0f * px).withStyle("Bold"));
            const auto label = juce::String::toHexString(
                static_cast<juce::int64>(seed >> 48)).paddedLeft('0', 4)
                .toUpperCase();
            g.drawText(label, cell, juce::Justification::centred, false);
        }
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        const auto cells = cellRects();
        for (std::size_t i = 0; i < cells.size() && i < static_cast<std::size_t>(count_); ++i)
        {
            if (cells[i].contains(e.position))
            {
                processor_.recallSeed(history_[i]);
                repaint();
                return;
            }
        }
    }

private:
    void timerCallback() override { refresh(); }

    void refresh()
    {
        auto fresh = processor_.seedHistory();
        const int n = processor_.seedHistoryCount();
        bool changed = (n != count_);
        if (! changed)
        {
            for (int i = 0; i < n; ++i)
                if (fresh[static_cast<std::size_t>(i)] != history_[static_cast<std::size_t>(i)])
                { changed = true; break; }
        }
        if (changed)
        {
            history_ = fresh;
            count_   = n;
            repaint();
        }
        else
        {
            // Active highlight may move even when the history list does not.
            const auto cur = processor_.currentSeed();
            if (cur != lastDrawnCurrent_)
            {
                lastDrawnCurrent_ = cur;
                repaint();
            }
        }
    }

    std::array<juce::Rectangle<float>, ValvraProcessor::kSeedHistorySize>
    cellRects() const
    {
        std::array<juce::Rectangle<float>, ValvraProcessor::kSeedHistorySize> rects {};
        auto r = getLocalBounds().toFloat()
                    .reduced(6.0f * uiScale_, 4.0f * uiScale_).withTrimmedTop(14.0f * uiScale_);
        const float gap = 4.0f * uiScale_;
        const float w =
            (r.getWidth() - gap * (ValvraProcessor::kSeedHistorySize - 1))
            / static_cast<float>(ValvraProcessor::kSeedHistorySize);
        for (std::size_t i = 0; i < rects.size(); ++i)
        {
            rects[i] = juce::Rectangle<float>(
                r.getX() + static_cast<float>(i) * (w + gap),
                r.getY(), w, r.getHeight());
        }
        return rects;
    }

    ValvraProcessor& processor_;
    std::array<std::uint64_t, ValvraProcessor::kSeedHistorySize> history_ {};
    int count_ { 0 };
    std::uint64_t lastDrawnCurrent_ { 0 };
    float uiScale_ { 1.0f };
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
        setToggleState(processor_.nullTestMode(), juce::dontSendNotification);
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
