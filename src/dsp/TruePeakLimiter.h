// ─────────────────────────────────────────────────────────────────────────────
// TruePeakLimiter — ITU-R BS.1770-5 4× oversampled TP-aware brickwall limiter
//
// Final-stage safety: holds the output below a user-set ceiling (typically
// −1 dBTP) by detecting peaks in a 4× upsampled domain so inter-sample peaks
// that would clip a host's playback DAC are also caught.  Operates in-place
// on a stereo pair with a single shared gain envelope (stereo image stays
// intact even when one channel triggers harder than the other).
//
// Algorithm (per output sample):
//   1) push input into a kLookaheadSamples-deep ring buffer
//   2) feed input into a dedicated PolyphaseOversampler<4> for *detection*
//      (we do not use the upsampled stream for output — only its peaks)
//   3) compute the exact gain needed for the current sample's TP estimate
//   4) store that gain beside the delayed audio sample
//   5) output = lookahead-delayed input × its matching delayed gain
//
// Bypass behaviour: latency is reported even when bypassed so the host's
// PDC compensator stays consistent — only the gain multiplier is forced
// to 1.0.  This lets null-test mode work cleanly: dry and wet paths see
// the same fixed delay regardless of limiter state.
//
// Reference: docs/20 §4.8 (mastering features), ITU-R BS.1770-5 (2023).
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include "PolyphaseOversampler.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace valvra::dsp {

class TruePeakLimiter
{
public:
    /// Lookahead window in base-rate samples.  Power-of-2 minus headroom so
    /// the buffer below is exactly 2× this for cheap modulo via mask.  At
    /// 48 kHz, 64 samples ≈ 1.33 ms — long enough for transient pre-attack,
    /// short enough that the introduced latency is inaudible musically.
    static constexpr int kLookaheadSamples = 64;
    static constexpr int kMaxLookaheadSamples = 1024;
    static constexpr int kBufSize          = 2048;
    static constexpr int kBufMask          = kBufSize - 1;

    void prepare(double sampleRate) noexcept
    {
        sampleRate_ = sampleRate;
        const double releaseTauSamp = 0.050  * sampleRate;
        releaseCoeff_ = std::exp(-1.0 / std::max(releaseTauSamp, 1.0));
        reset();
    }

    void reset() noexcept
    {
        osL_.reset();
        osR_.reset();
        currentGain_ = 1.0;
        writeIdx_    = 0;
        laL_.fill(0.0f);
        laR_.fill(0.0f);
        gainDelay_.fill(1.0f);
    }

    void setBypass(bool b) noexcept { bypass_ = b; }
    bool bypass() const noexcept    { return bypass_; }

    void setLookaheadMs(float ms) noexcept
    {
        const float clampedMs = std::clamp(ms, 1.0f, 10.0f);
        const int samples = std::clamp(
            static_cast<int>(std::round(
                static_cast<double>(clampedMs) * 0.001 * sampleRate_)),
            1,
            kMaxLookaheadSamples);
        lookaheadSamples_ = samples;
    }

    int currentLatencyInSamples() const noexcept { return lookaheadSamples_; }

    /// Set the brickwall ceiling.  Typical mastering values: −1 dBTP for
    /// streaming-safe masters, −0.3 dBTP for CD-only masters.
    void setCeilingDb(float db) noexcept
    {
        ceiling_ = std::pow(10.0f, db / 20.0f);
        if (ceiling_ < 1.0e-6f) ceiling_ = 1.0e-6f;  // sanity floor
    }
    float ceiling() const noexcept { return ceiling_; }

    /// Last computed gain reduction in dB (0 = no reduction, negative when
    /// limiting).  UI gain-reduction meter reads from here.
    float gainReductionDb() const noexcept
    {
        if (bypass_) return 0.0f;
        const double g = std::max(currentGain_, 1.0e-6);
        return static_cast<float>(20.0 * std::log10(g));
    }

    static constexpr int latencyInSamples() noexcept { return kLookaheadSamples; }

    /// Process a (mono or stereo) buffer in-place.  Pass R = nullptr for mono.
    void process(float* L, float* R, int numSamples) noexcept
    {
        const bool stereo = (R != nullptr);
        for (int n = 0; n < numSamples; ++n)
        {
            const float inL = L[n];
            const float inR = stereo ? R[n] : 0.0f;
            const float xL = std::isfinite(inL) ? inL : 0.0f;
            const float xR = std::isfinite(inR) ? inR : 0.0f;

            // 1) Read the sample written kLookaheadSamples-ago FIRST, before
            //    overwriting that slot with the new input.  read = write - L
            //    modulo buffer size (with +size guard for safety).
            const int readIdx = (writeIdx_ + kBufSize - lookaheadSamples_)
                              & kBufMask;
            const float yL = laL_[static_cast<std::size_t>(readIdx)];
            const float yR = laR_[static_cast<std::size_t>(readIdx)];
            const float delayedGain = bypass_
                ? 1.0f
                : gainDelay_[static_cast<std::size_t>(readIdx)];

            // 2) Push current input into the ring buffer for future readout.
            laL_[static_cast<std::size_t>(writeIdx_)] = xL;
            laR_[static_cast<std::size_t>(writeIdx_)] = xR;

            // 3) 4× upsample BOTH channels (separate oversamplers preserve
            //    independent FIR state) and take the max absolute value as
            //    this sample's TP estimate.  Per ITU-R BS.1770-5, 4× is
            //    sufficient for ±0.5 dB worst-case TP error on speech /
            //    music material.
            const auto upL = osL_.upsample(static_cast<double>(xL));
            std::array<double, 4> upR {};
            if (stereo) upR = osR_.upsample(static_cast<double>(xR));

            double sampPeak = std::abs(static_cast<double>(xL));
            if (stereo)
                sampPeak = std::max(sampPeak, std::abs(static_cast<double>(xR)));
            for (int k = 0; k < 4; ++k)
            {
                sampPeak = std::max(sampPeak, std::abs(upL[static_cast<std::size_t>(k)]));
                if (stereo)
                    sampPeak = std::max(sampPeak,
                                        std::abs(upR[static_cast<std::size_t>(k)]));
            }

            double gainForCurrent = 1.0;
            if (! bypass_)
            {
                // 4) Target gain for the CURRENT input sample.  Store it with
                //    that sample so, after the lookahead delay, the exact
                //    transient that triggered limiting receives the limiting
                //    gain.  Attack is intentionally instantaneous for
                //    brickwall safety; release is smoothed so gain recovers
                //    without pumping after isolated peaks.
                const double target =
                    (sampPeak > static_cast<double>(ceiling_))
                    ? (static_cast<double>(ceiling_) / sampPeak)
                    : 1.0;
                currentGain_ = (target < currentGain_)
                    ? target
                    : releaseCoeff_ * currentGain_ + (1.0 - releaseCoeff_) * target;
                gainForCurrent = currentGain_;
            }
            else
            {
                currentGain_ = 1.0;
            }

            gainDelay_[static_cast<std::size_t>(writeIdx_)] =
                static_cast<float>(gainForCurrent);
            writeIdx_ = (writeIdx_ + 1) & kBufMask;

            // 5) Apply the delayed gain to the matching delayed audio.  When
            //    bypassed, force unity gain immediately; this prevents stale
            //    pre-bypass gain-reduction values from attenuating the first
            //    lookahead window after the user toggles TP off.
            L[n] = yL * delayedGain;
            if (stereo) R[n] = yR * delayedGain;
        }
    }

private:
    PolyphaseOversampler<4> osL_ {};
    PolyphaseOversampler<4> osR_ {};
    std::array<float, kBufSize> laL_ {};
    std::array<float, kBufSize> laR_ {};
    std::array<float, kBufSize> gainDelay_ {};
    int writeIdx_ { 0 };
    int lookaheadSamples_ { kLookaheadSamples };

    double sampleRate_   { 48000.0 };
    double releaseCoeff_ { 0.0 };
    double currentGain_  { 1.0 };

    float  ceiling_      { 0.891f };  // 10^(−1/20) ≈ −1 dBFS
    bool   bypass_       { true };
};

} // namespace valvra::dsp
