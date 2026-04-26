// ─────────────────────────────────────────────────────────────────────────────
// HarmonicAnalyzer — rolling FFT that extracts H1..H7 from the processed
// signal. Feeds the harmonic meter UI and the null-test diagnostics.
//
// Designed for low CPU: one 2048-point FFT every ~50 ms on a dedicated
// background task.  Audio thread just writes into a lock-free ring buffer.
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace valvra {

// Results snapshot — all UI reads go through this atomic snapshot.
struct HarmonicSnapshot
{
    float fundamentalDb { -100.0f };
    std::array<float, 7> harmonicsDbc {
        -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f
    };
    float fundamentalHz { 0.0f };
    bool  valid { false };
};

class HarmonicAnalyzer
{
public:
    HarmonicAnalyzer()
        : fft_ { kFftOrder }
        , window_ { kFftSize,
                    juce::dsp::WindowingFunction<float>::blackmanHarris }
    {
        fftBuffer_.resize(kFftSize * 2, 0.0f);
        ringBuffer_.resize(kFftSize, 0.0f);
    }

    void prepare(double sampleRate)
    {
        sampleRate_  = sampleRate;
        writeIdx_.store(0, std::memory_order_relaxed);
        samplesAccumulated_.store(0, std::memory_order_relaxed);
        std::fill(ringBuffer_.begin(), ringBuffer_.end(), 0.0f);
    }

    // Audio-thread hot path: push one sample. No allocations, no locks.
    // writeIdx_ and samplesAccumulated_ are atomic so TSAN / strict C++
    // readers (updateSnapshot on the UI thread) never observe a torn value.
    void push(float x) noexcept
    {
        const int w = writeIdx_.load(std::memory_order_relaxed);
        ringBuffer_[static_cast<std::size_t>(w)] = x;
        writeIdx_.store((w + 1) % kFftSize, std::memory_order_relaxed);
        samplesAccumulated_.fetch_add(1, std::memory_order_relaxed);
    }

    // Called periodically from the editor timer (30 Hz).  Runs the FFT
    // and writes a new snapshot. Returns true if a new frame was produced.
    bool updateSnapshot() noexcept
    {
        if (samplesAccumulated_.load(std::memory_order_relaxed)
                < static_cast<std::size_t>(kFftSize))
            return false;

        // Copy ring buffer into linear order (starting from oldest).
        // We snapshot writeIdx_ once — the ringBuffer_ float reads may see
        // an audio-thread write mid-flight, but individual float reads are
        // atomic on every supported platform, so the worst-case is a single
        // sample glitch in the displayed spectrum.
        {
            const int w = writeIdx_.load(std::memory_order_relaxed);
            for (int i = 0; i < kFftSize; ++i)
            {
                const int srcIdx = (w + i) % kFftSize;
                fftBuffer_[static_cast<std::size_t>(i)] =
                    ringBuffer_[static_cast<std::size_t>(srcIdx)];
            }
            std::fill(fftBuffer_.begin() + kFftSize, fftBuffer_.end(), 0.0f);
        }

        // Window + FFT
        window_.multiplyWithWindowingTable(fftBuffer_.data(), kFftSize);
        fft_.performFrequencyOnlyForwardTransform(fftBuffer_.data());

        // Find fundamental: peak in [50 Hz, sr/4] range
        const int loBin =
            std::max(1, static_cast<int>(50.0 * kFftSize / sampleRate_));
        const int hiBin =
            std::min(kFftSize / 2,
                     static_cast<int>(5000.0 * kFftSize / sampleRate_));

        int fundBin = loBin;
        float fundMag = fftBuffer_[static_cast<std::size_t>(loBin)];
        for (int b = loBin + 1; b < hiBin; ++b)
        {
            const float m = fftBuffer_[static_cast<std::size_t>(b)];
            if (m > fundMag)
            {
                fundMag = m;
                fundBin = b;
            }
        }

        HarmonicSnapshot snap;
        snap.fundamentalHz =
            static_cast<float>(fundBin * sampleRate_ / kFftSize);
        const float fundAmp = fundMag > 1e-12f ? fundMag : 1e-12f;
        snap.fundamentalDb = 20.0f * std::log10(fundAmp + 1e-12f);

        for (int h = 2; h <= 8; ++h)
        {
            const int hb = fundBin * h;
            if (hb >= kFftSize / 2) break;
            // Peak-pick a 3-bin window to tolerate small pitch drift
            const int lo = std::max(1, hb - 2);
            const int hi = std::min(kFftSize / 2 - 1, hb + 2);
            float peak = 0.0f;
            for (int b = lo; b <= hi; ++b)
                peak = std::max(peak, fftBuffer_[static_cast<std::size_t>(b)]);
            const float dbc = 20.0f * std::log10((peak + 1e-12f) / fundAmp);
            snap.harmonicsDbc[static_cast<std::size_t>(h - 2)] = dbc;
        }
        snap.valid = true;

        snapshot_.store(snap);
        return true;
    }

    HarmonicSnapshot readSnapshot() const noexcept { return snapshot_.load(); }

private:
    static constexpr int kFftOrder = 11;                // 2^11 = 2048
    static constexpr int kFftSize  = 1 << kFftOrder;

    juce::dsp::FFT fft_;
    juce::dsp::WindowingFunction<float> window_;

    std::vector<float> fftBuffer_;
    std::vector<float> ringBuffer_;
    std::atomic<int>        writeIdx_ { 0 };
    std::atomic<std::size_t> samplesAccumulated_ { 0 };
    double             sampleRate_ { 48000.0 };

    std::atomic<HarmonicSnapshot> snapshot_ {};
};

} // namespace valvra
