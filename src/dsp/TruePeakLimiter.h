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
//   1) push input into a delay ring sized lookahead + detector group delay
//   2) feed input into a dedicated PolyphaseOversampler<4> for *detection*
//      (we do not use the upsampled stream for output — only its peaks).
//      The detector FIR lags the input by kDetectorDelaySamples, so the
//      peak seen now belongs to the sample written that many steps ago.
//   3) compute the raw gain needed to pin that (past) sample's TP estimate
//      at the ceiling
//   4) sliding minimum over the last lookahead+1 raw gains (monotonic
//      deque, O(1) amortized) — the minimum therefore covers the sample
//      about to leave the delay ring AND every sample behind it, which is
//      what turns the lookahead delay into a real pre-attack window
//   5) smooth the minimum with a 0.1 ms attack / 50 ms release one-pole
//      (docs/13 §13.3.3); by the time a hot sample emerges from the delay,
//      the envelope has had lookahead+1 samples to settle onto its gain
//   6) output = delayed input × smoothed envelope
//
// Bypass behaviour: latency is reported even when bypassed so the host's
// PDC compensator stays consistent — only the gain multiplier is forced
// to 1.0.  This lets null-test mode work cleanly: dry and wet paths see
// the same fixed delay regardless of limiter state.
//
// Reference: docs/13 §13.3.3, docs/20 §4.8, ITU-R BS.1770-5 (2023).
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include "PolyphaseOversampler.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace valvra::dsp {

class TruePeakLimiter
{
public:
    /// Default lookahead window in base-rate samples (≈ 1.33 ms at 48 kHz)
    /// used until the host pushes the TP Lookahead parameter (1–10 ms).
    /// Must stay ≥ ~10× the 0.1 ms attack constant so the envelope settles
    /// within the window — see the convergence note in process().
    static constexpr int kLookaheadSamples = 64;
    static constexpr int kMaxLookaheadSamples = 1024;
    static constexpr int kBufSize          = 2048;
    static constexpr int kBufMask          = kBufSize - 1;

    /// Detection-path group delay in base-rate samples.  The upsample FIR is
    /// linear-phase with group delay (kTaps−1)/2 at the upsampled rate;
    /// derived from the oversampler constants (NOT hardcoded) so a filter
    /// redesign cannot silently break the gain/audio alignment below.
    static constexpr int kDetectorDelaySamples =
        ((PolyphaseOversampler<4>::kTaps - 1) / 2
         + PolyphaseOversampler<4>::factor() / 2)
        / PolyphaseOversampler<4>::factor();

    static_assert(PolyphaseOversampler<4>::kTaps % 2 == 1,
                  "detector group delay assumes an odd-tap linear-phase FIR");
    static_assert(kMaxLookaheadSamples + kDetectorDelaySamples < kBufSize,
                  "delay ring must hold lookahead + detector group delay");

    void prepare(double sampleRate) noexcept
    {
        sampleRate_ = sampleRate;
        const double releaseTauSamp = 0.050  * sampleRate;
        const double attackTauSamp  = 0.0001 * sampleRate;
        releaseCoeff_ = std::exp(-1.0 / std::max(releaseTauSamp, 1.0));
        attackCoeff_  = std::exp(-1.0 / std::max(attackTauSamp, 1.0e-3));
        // Brickwall convergence floor: the window minimum must sit in the
        // envelope for ≥ ~12 attack time constants before the hot sample
        // emerges, or the residual breaches the ceiling.  The attack tau
        // scales with fs while the 64-sample default does not — at 192 kHz
        // the unguarded default would leave an e^-3.4 ≈ 3.3 % residual.
        minLookaheadSamples_ = std::clamp(
            static_cast<int>(std::ceil(12.0 * attackTauSamp)),
            kLookaheadSamples, kMaxLookaheadSamples);
        lookaheadSamples_ = std::max(lookaheadSamples_, minLookaheadSamples_);
        reset();
    }

    void reset() noexcept
    {
        osL_.reset();
        osR_.reset();
        currentGain_ = 1.0;
        writeIdx_    = 0;
        step_        = 0;
        dqHead_      = 0;
        dqCount_     = 0;
        laL_.fill(0.0f);
        laR_.fill(0.0f);
    }

    void setBypass(bool b) noexcept { bypass_ = b; }
    bool bypass() const noexcept    { return bypass_; }

    void setLookaheadMs(float ms) noexcept
    {
        const float clampedMs = std::clamp(ms, 1.0f, 10.0f);
        const int samples = std::clamp(
            static_cast<int>(std::round(
                static_cast<double>(clampedMs) * 0.001 * sampleRate_)),
            minLookaheadSamples_,
            kMaxLookaheadSamples);
        lookaheadSamples_ = samples;
    }

    int currentLatencyInSamples() const noexcept
    {
        return lookaheadSamples_ + kDetectorDelaySamples;
    }

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

    static constexpr int latencyInSamples() noexcept
    {
        return kLookaheadSamples + kDetectorDelaySamples;
    }

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

            // 1) Read the sample written (lookahead + detector delay) ago
            //    FIRST, before overwriting that slot with the new input.
            const int delaySamples = lookaheadSamples_ + kDetectorDelaySamples;
            const int readIdx = (writeIdx_ + kBufSize - delaySamples)
                              & kBufMask;
            const float yL = laL_[static_cast<std::size_t>(readIdx)];
            const float yR = laR_[static_cast<std::size_t>(readIdx)];

            // 2) Push current input into the ring buffer for future readout.
            laL_[static_cast<std::size_t>(writeIdx_)] = xL;
            laR_[static_cast<std::size_t>(writeIdx_)] = xR;

            // 3) 4× upsample BOTH channels (separate oversamplers preserve
            //    independent FIR state) and take the max absolute value as
            //    the TP estimate.  Per ITU-R BS.1770-5, 4× is sufficient for
            //    ±0.5 dB worst-case TP error on speech / music material.
            //    The upsampled stream lags the input by kDetectorDelaySamples,
            //    so the base-rate sample folded into the same estimate must be
            //    read back from the ring at that offset — never the current
            //    input, which is what mis-timed the gain pre-fix.
            const auto upL = osL_.upsample(static_cast<double>(xL));
            std::array<double, 4> upR {};
            if (stereo) upR = osR_.upsample(static_cast<double>(xR));

            const int detIdx = (writeIdx_ + kBufSize - kDetectorDelaySamples)
                             & kBufMask;
            double sampPeak = std::abs(
                static_cast<double>(laL_[static_cast<std::size_t>(detIdx)]));
            if (stereo)
                sampPeak = std::max(sampPeak,
                    std::abs(static_cast<double>(
                        laR_[static_cast<std::size_t>(detIdx)])));
            for (int k = 0; k < 4; ++k)
            {
                sampPeak = std::max(sampPeak, std::abs(upL[static_cast<std::size_t>(k)]));
                if (stereo)
                    sampPeak = std::max(sampPeak,
                                        std::abs(upR[static_cast<std::size_t>(k)]));
            }

            // 4) Raw gain pinning this (detector-delayed) sample's TP at
            //    the ceiling, then sliding minimum over the most recent
            //    lookahead+1 raw gains via monotonic deque.  The sample
            //    leaving the ring this step corresponds to the OLDEST
            //    entry still inside the window, so its raw gain is always
            //    covered and every later (future, from its perspective)
            //    gain can pre-duck it.  The deque is maintained even while
            //    BYPASSED: only the gain application is suspended, so the
            //    samples already inside the delay ring stay covered the
            //    instant the limiter is re-engaged (clearing it left the
            //    first lookahead window after a bypass toggle unlimited).
            const float raw =
                (sampPeak > static_cast<double>(ceiling_))
                ? static_cast<float>(static_cast<double>(ceiling_) / sampPeak)
                : 1.0f;

            while (dqCount_ > 0
                   && dqVal_[static_cast<std::size_t>(
                          (dqHead_ + dqCount_ - 1) & kBufMask)] >= raw)
                --dqCount_;
            const int slot = (dqHead_ + dqCount_) & kBufMask;
            dqVal_[static_cast<std::size_t>(slot)]  = raw;
            dqStep_[static_cast<std::size_t>(slot)] = step_;
            ++dqCount_;
            while (dqCount_ > 0
                   && dqStep_[static_cast<std::size_t>(dqHead_)]
                          + static_cast<std::int64_t>(lookaheadSamples_)
                      < step_)
            {
                dqHead_ = (dqHead_ + 1) & kBufMask;
                --dqCount_;
            }
            const double target = static_cast<double>(
                dqVal_[static_cast<std::size_t>(dqHead_)]);

            double gain = 1.0;
            if (! bypass_)
            {
                // 5) 0.1 ms attack / 50 ms release one-pole on the window
                //    minimum.  A hot sample's raw gain sits in the window for
                //    lookahead+1 steps before that sample emerges from the
                //    delay, and prepare() floors the lookahead at 12 attack
                //    time constants, so the attack residual at emergence is
                //    ≤ e^−12 at every sample rate — far below the ±0.1 dB
                //    ceiling tolerance.
                currentGain_ = (target < currentGain_)
                    ? attackCoeff_  * currentGain_ + (1.0 - attackCoeff_)  * target
                    : releaseCoeff_ * currentGain_ + (1.0 - releaseCoeff_) * target;
                gain = currentGain_;
            }
            else
            {
                // Envelope parks at unity while bypassed (no stale
                // reduction on re-engage); the window minimum above keeps
                // tracking so re-engage attacks from live data.
                currentGain_ = 1.0;
            }

            ++step_;
            writeIdx_ = (writeIdx_ + 1) & kBufMask;

            // 6) Apply the smoothed envelope to the matching delayed audio.
            const float g = static_cast<float>(gain);
            L[n] = yL * g;
            if (stereo) R[n] = yR * g;
        }
    }

private:
    PolyphaseOversampler<4> osL_ {};
    PolyphaseOversampler<4> osR_ {};
    std::array<float, kBufSize> laL_ {};
    std::array<float, kBufSize> laR_ {};

    // Monotonic min-deque over the raw gain stream.  Capacity kBufSize is
    // sufficient: live entries never exceed lookahead+1 ≤ kMaxLookahead+1.
    std::array<float, kBufSize>        dqVal_ {};
    std::array<std::int64_t, kBufSize> dqStep_ {};
    int dqHead_  { 0 };
    int dqCount_ { 0 };

    int writeIdx_ { 0 };
    std::int64_t step_ { 0 };
    int lookaheadSamples_ { kLookaheadSamples };
    int minLookaheadSamples_ { kLookaheadSamples };

    double sampleRate_   { 48000.0 };
    double releaseCoeff_ { 0.0 };
    double attackCoeff_  { 0.0 };
    double currentGain_  { 1.0 };

    float  ceiling_      { 0.891f };  // 10^(−1/20) ≈ −1 dBFS
    bool   bypass_       { true };
};

} // namespace valvra::dsp
