// ─────────────────────────────────────────────────────────────────────────────
// ExpansionRack — Tier 4+ expansion engines (product incubation core)
//
// Purpose:
//   - Opto-style compressor core (T4 photocell with slow trap-level memory)
//   - FET-style compressor core
//   - Tape-style saturation core (Jiles-Atherton oxide hysteresis, flux-driven)
//   - Synth/FX feed core
//
// This module is intentionally framework-agnostic and real-time safe:
// no allocation, no locks, no exceptions in process paths.
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "JilesAtherton.h"
#include "PolyphaseOversampler.h"   // kaiserLowpass() for the tape 2× wrap

namespace valvra::dsp {

enum class ExpansionMode : int
{
    Off = 0,
    OptoComp,
    FetComp,
    TapeSat,
    SynthFx
};

class ExpansionRack
{
public:
    void prepare(double sampleRate) noexcept
    {
        sampleRate_ = (sampleRate > 1000.0) ? sampleRate : 48000.0;
        fadeSamples_ = std::max(1, static_cast<int>(0.010 * sampleRate_));

        // Tape flux-drive coefficients (see processTape()).  The leak
        // realises the record-head LF corner; tapeFluxToH_ calibrates the
        // integrated drive so the oxide is hit at the reference level at
        // kTapeFluxRefHz; tapeEmfGain_ is the matching replay-head (dB/dt)
        // gain for a unity passband.  Same scheme as TransformerStage.
        // The nonlinear core runs 2× oversampled (docs/34 §4.2), so its
        // coefficients are derived at 2·fs.
        constexpr double kTwoPi = 6.28318530717958647692;
        const double fs2 = 2.0 * sampleRate_;
        tapeFluxLeak_ = std::exp(-kTwoPi * kTapeFcLowHz / fs2);
        tapeFluxToH_  = kTapeHScale * (kTwoPi * kTapeFluxRefHz / fs2);
        tapeEmfGain_  = fs2 / (kTwoPi * kTapeFluxRefHz);
        configureTapeCore();

        // Tape 2× anti-alias / anti-image FIR (docs/34 §4.2): the JA core
        // generates high-order harmonics that folded at base Nyquist when
        // it ran at base rate.  41-tap Kaiser (β = 6 → ≈ −50 dB stop,
        // passband to ≈ 0.36·fs) — round-trip group delay (kOsTaps−1)/2
        // base samples, matched by the other modes' delay ring so the
        // rack's latency is mode-invariant (crossfades stay aligned and
        // PDC never flips on mode changes).
        {
            const auto h = kaiserLowpass(kOsTaps, 0.214, 6.0);
            for (int i = 0; i < kOsTaps; ++i)
                osTaps_[static_cast<std::size_t>(i)] =
                    h[static_cast<std::size_t>(i)];
        }

        reset();
    }

    /// Mode-invariant processing latency of the rack in base-rate samples
    /// (the tape wrap's FIR round trip; other modes ride a matching delay
    /// ring).  The processor adds this to its PDC and dry-alignment.
    static constexpr int latencyInBaseSamples() noexcept
    {
        return (kOsTaps - 1) / 2;
    }

    void reset() noexcept
    {
        activeMode_ = ExpansionMode::Off;
        targetMode_ = ExpansionMode::Off;
        crossfadeActive_ = false;
        fadePos_ = 0;

        opto_ = {};
        fet_ = {};
        tape_ = {};
        synth_ = {};
        tapeJaL_.reset();
        tapeJaR_.reset();
        osL_ = {};
        osR_ = {};
        delayL_ = {};
        delayR_ = {};
    }

    // Per-instance Monte Carlo variation (ComponentVariation scales).
    // Click-free: only coefficient scales change; all engine state is
    // preserved (JA setParams does not reset magnetization).
    void setVariation(double tapeCoreKScale,
                      double tapeWowScale,
                      double optoMemoryTauScale) noexcept
    {
        tapeCoreKScale_     = std::clamp(tapeCoreKScale,     0.25, 4.0);
        tapeWowScale_       = std::clamp(tapeWowScale,       0.25, 4.0);
        optoMemoryTauScale_ = std::clamp(optoMemoryTauScale, 0.25, 4.0);
        configureTapeCore();
    }

    void setMode(ExpansionMode mode) noexcept
    {
        if (mode == targetMode_ && (crossfadeActive_ || mode == activeMode_))
            return;

        targetMode_ = mode;
        resetModeState(targetMode_);

        if (targetMode_ == activeMode_)
        {
            crossfadeActive_ = false;
            fadePos_ = 0;
            return;
        }

        crossfadeActive_ = true;
        fadePos_ = 0;
    }

    void setAmount(double amount) noexcept
    {
        amount_ = clamp01(amount);
    }

    void setMix(double mix) noexcept
    {
        mix_ = clamp01(mix);
    }

    void processMono(double x, double& y) noexcept
    {
        // The rack has a fixed, mode-invariant latency (the tape wrap's
        // FIR round trip); its dry side must ride the matching delay tap
        // so the internal mix never combs (docs/34 §4.2).
        const double delL = delayL_.process(x);
        const double delR = delayR_.process(x);
        double aL = x, aR = x;
        processActivePath(aL, aR, delL, delR);
        const double wet = 0.5 * (aL + aR);
        y = lerp(0.5 * (delL + delR), wet, mix_);
    }

    void processStereo(double inL, double inR, double& outL, double& outR) noexcept
    {
        const double delL = delayL_.process(inL);
        const double delR = delayR_.process(inR);
        double aL = inL, aR = inR;
        processActivePath(aL, aR, delL, delR);
        outL = lerp(delL, aL, mix_);
        outR = lerp(delR, aR, mix_);
    }

private:
    struct OptoState
    {
        double envFast { 0.0 };
        double envSlow { 0.0 };
        double gain { 1.0 };
        double history { 0.0 };  ///< T4 trap-level memory (slow GR-depth env)
    };

    struct FetState
    {
        double env { 0.0 };
        double gain { 1.0 };
    };

    struct TapeState
    {
        double preLpL { 0.0 }, preLpR { 0.0 };   ///< record pre-emphasis LP
        double fluxL { 0.0 },  fluxR { 0.0 };    ///< record-head flux (leaky ∫)
        double bPrevL { 0.0 }, bPrevR { 0.0 };   ///< previous normalized B
        double bumpL { 0.0 },  bumpR { 0.0 };    ///< head-bump emphasis
        double wowPhase { 0.0 };
        double flutterPhase { 0.0 };
    };

    struct SynthState
    {
        double phaseSlow { 0.0 };
        double phaseFast { 0.0 };
        double hpL { 0.0 }, hpR { 0.0 };
        double prevL { 0.0 }, prevR { 0.0 };
    };

    static double clamp01(double x) noexcept
    {
        return std::clamp(x, 0.0, 1.0);
    }

    static double lerp(double a, double b, double t) noexcept
    {
        return a + (b - a) * t;
    }

    static double envCoeff(double sampleRate, double ms) noexcept
    {
        const double tau = std::max(ms * 0.001, 1.0e-6);
        return std::exp(-1.0 / (tau * sampleRate));
    }

    static double envCoeffSeconds(double sampleRate, double sec) noexcept
    {
        const double tau = std::max(sec, 1.0e-6);
        return std::exp(-1.0 / (tau * sampleRate));
    }

    static double dBToGain(double db) noexcept
    {
        return std::pow(10.0, db / 20.0);
    }

    static double gainToDb(double g) noexcept
    {
        return 20.0 * std::log10(std::max(g, 1.0e-12));
    }

    void resetModeState(ExpansionMode mode) noexcept
    {
        switch (mode)
        {
            case ExpansionMode::OptoComp: opto_ = {}; break;
            case ExpansionMode::FetComp:  fet_ = {}; break;
            case ExpansionMode::TapeSat:
                tape_ = {};
                tapeJaL_.reset();
                tapeJaR_.reset();
                osL_ = {};
                osR_ = {};
                break;
            case ExpansionMode::SynthFx:  synth_ = {}; break;
            case ExpansionMode::Off:
            default: break;
        }
    }

    // Run one mode.  The tape path consumes the RAW input (its 2× FIR
    // round trip supplies the rack's fixed latency inherently); every
    // other mode — including Off — consumes the matching DELAY TAP, so
    // the two legs of a mode crossfade are always time-aligned.
    void runMode(ExpansionMode mode,
                 double rawL, double rawR,
                 double delL, double delR,
                 double& outL, double& outR) noexcept
    {
        switch (mode)
        {
            case ExpansionMode::TapeSat:
                outL = rawL; outR = rawR;
                processTape(outL, outR);
                break;
            case ExpansionMode::OptoComp:
                outL = delL; outR = delR;
                processOpto(outL, outR);
                break;
            case ExpansionMode::FetComp:
                outL = delL; outR = delR;
                processFet(outL, outR);
                break;
            case ExpansionMode::SynthFx:
                outL = delL; outR = delR;
                processSynthFx(outL, outR);
                break;
            case ExpansionMode::Off:
            default:
                outL = delL; outR = delR;
                break;
        }
    }

    void processActivePath(double& l, double& r,
                           double delL, double delR) noexcept
    {
        double aL, aR;
        runMode(activeMode_, l, r, delL, delR, aL, aR);

        if (! crossfadeActive_)
        {
            l = aL;
            r = aR;
            return;
        }

        double bL, bR;
        runMode(targetMode_, l, r, delL, delR, bL, bR);

        const double t = static_cast<double>(fadePos_ + 1)
                       / static_cast<double>(std::max(1, fadeSamples_));
        l = lerp(aL, bL, t);
        r = lerp(aR, bR, t);

        ++fadePos_;
        if (fadePos_ >= fadeSamples_)
        {
            activeMode_ = targetMode_;
            crossfadeActive_ = false;
            fadePos_ = 0;
        }
    }

    void processOpto(double& l, double& r) noexcept
    {
        // T4 photocell "memory effect" (docs/12-analog-beyond-tubes.md):
        // CdS trap levels charge up under sustained drive, so resistance
        // recovery — i.e. the release — slows after the cell has been
        // working hard.  Modelled as a third, very slow time constant
        // (τ₃ ≈ 25 s nominal, 10–60 s class) tracking gain-reduction
        // depth; it stretches the release time constants up to ~3×.
        const double memoryStretch =
            1.0 + 2.0 * clamp01(opto_.history);

        const double det = std::max(std::abs(l), std::abs(r));
        const double attackMs = 4.0 + 14.0 * (1.0 - amount_);
        const double releaseMs =
            (90.0 + 540.0 * (1.0 - amount_)) * memoryStretch;
        const double aA = envCoeff(sampleRate_, attackMs);
        const double aR = envCoeff(sampleRate_, releaseMs);
        const double aSlow = envCoeffSeconds(sampleRate_, 8.0 + 18.0 * amount_);

        if (det > opto_.envFast)
            opto_.envFast = aA * opto_.envFast + (1.0 - aA) * det;
        else
            opto_.envFast = aR * opto_.envFast + (1.0 - aR) * det;

        opto_.envSlow = aSlow * opto_.envSlow + (1.0 - aSlow) * opto_.envFast;

        const double eff = 0.72 * opto_.envFast + 0.28 * opto_.envSlow;
        const double threshold = 0.20;
        const double ratio = 2.0 + 4.5 * amount_;
        const double overDb = gainToDb(std::max(eff / threshold, 1.0));
        const double grDb = overDb * (1.0 - 1.0 / ratio);
        const double targetGain = dBToGain(-grDb);

        // Accumulate trap-level history from normalized GR depth
        // (~12 dB of gain reduction ≡ "working hard").
        const double aHist = envCoeffSeconds(
            sampleRate_, 25.0 * optoMemoryTauScale_);
        opto_.history = aHist * opto_.history
                      + (1.0 - aHist) * clamp01(grDb / 12.0);

        const double gainAttack = envCoeff(sampleRate_, 2.0);
        const double gainRelease = envCoeff(
            sampleRate_,
            (70.0 + 260.0 * (1.0 - amount_)) * memoryStretch);
        const double gCoeff = (targetGain < opto_.gain) ? gainAttack : gainRelease;
        opto_.gain = gCoeff * opto_.gain + (1.0 - gCoeff) * targetGain;

        const double makeup = 1.0 + 0.55 * amount_;
        l *= opto_.gain * makeup;
        r *= opto_.gain * makeup;
    }

    void processFet(double& l, double& r) noexcept
    {
        const double det = std::max(std::abs(l), std::abs(r));
        const double attackMs = 0.06 + 0.34 * (1.0 - amount_);
        const double releaseMs = 22.0 + 110.0 * (1.0 - amount_);
        const double aA = envCoeff(sampleRate_, attackMs);
        const double aR = envCoeff(sampleRate_, releaseMs);
        if (det > fet_.env)
            fet_.env = aA * fet_.env + (1.0 - aA) * det;
        else
            fet_.env = aR * fet_.env + (1.0 - aR) * det;

        const double threshold = 0.13;
        const double ratio = 4.0 + 16.0 * amount_;
        const double overDb = gainToDb(std::max(fet_.env / threshold, 1.0));
        const double knee = 4.0;
        const double kneeT = clamp01(overDb / knee);
        const double grDb = overDb * (1.0 - 1.0 / ratio) * kneeT;
        const double targetGain = dBToGain(-grDb);

        const double gCoeff = (targetGain < fet_.gain)
            ? envCoeff(sampleRate_, 0.18)
            : envCoeff(sampleRate_, 25.0);
        fet_.gain = gCoeff * fet_.gain + (1.0 - gCoeff) * targetGain;

        const double drive = 1.0 + 2.2 * amount_;
        const double norm = 1.0 / std::tanh(std::max(1.0, drive));
        l = std::tanh(l * fet_.gain * drive) * norm;
        r = std::tanh(r * fet_.gain * drive) * norm;
    }

    // Tape print — real Jiles-Atherton oxide hysteresis, flux-driven.
    //
    // Record head: the oxide flux is the leaky time-integral of the drive
    // signal (Faraday), so the core sees ~1/f-weighted drive — at constant
    // tape speed, low frequencies magnetize the oxide far harder than highs
    // at equal level.  Replay head: EMF = dB/dt restores a flat passband
    // while the JA hysteresis (remanence, coercivity, dynamic loop memory)
    // sits in between.  Same integrate→saturate→differentiate scheme as
    // TransformerStage; the JA parameters here are voiced for tape oxide
    // (γ-Fe₂O₃ class): lower Ms, much higher pinning k → wide loop.
    void processTape(double& l, double& r) noexcept
    {
        constexpr double kTwoPi = 6.28318530717958647692;
        const double drive = 1.0 + 5.8 * amount_;

        tape_.wowPhase += kTwoPi * (0.25 + 0.35 * amount_) / sampleRate_;
        tape_.flutterPhase += kTwoPi * (5.0 + 4.0 * amount_) / sampleRate_;
        if (tape_.wowPhase > kTwoPi) tape_.wowPhase -= kTwoPi;
        if (tape_.flutterPhase > kTwoPi) tape_.flutterPhase -= kTwoPi;

        // NOTE (acknowledged simplification): wow/flutter here modulates
        // the record-flux AMPLITUDE (head-contact level wobble), not the
        // time base.  True wow/flutter is a tape-speed variation, i.e. an
        // interpolated delay-line modulation; that upgrade is deliberately
        // out of scope for this allocation-free engine.
        const double speedMod =
            1.0
            + 0.010 * amount_ * tapeWowScale_ * std::sin(tape_.wowPhase)
            + 0.003 * amount_ * tapeWowScale_ * std::sin(tape_.flutterPhase);

        const double preCoeff = 1.0 - std::exp(-kTwoPi * 1900.0 / sampleRate_);
        const double bumpCoeff = 1.0 - std::exp(-kTwoPi * 85.0 / sampleRate_);

        // Nonlinear core 2× oversampled (docs/34 §4.2): the JA oxide
        // generates high-order harmonics that used to fold at base
        // Nyquist.  Linear voicing (pre-emphasis, head bump) stays at
        // base rate; the flux→JA→EMF loop runs at 2·fs between the
        // shared 41-tap polyphase halves of osTaps_.  Coefficients
        // (tapeFluxLeak_/ToH_/EmfGain_) are already derived at 2·fs in
        // prepare(), so the calibration is rate-invariant by the same
        // construction as TransformerStage.
        const double dt2x = 0.5 / sampleRate_;

        auto processOne = [&](double x, JilesAtherton& ja, double& preLp,
                              double& flux, double& bPrev, double& bump,
                              TapeOsState& os) noexcept
        {
            // Record EQ: mild HF pre-emphasis (legacy voicing) — partially
            // restores the top end that the flux integral de-emphasises.
            preLp += preCoeff * (x - preLp);
            const double pre = x + 0.45 * (x - preLp);
            const double u = pre * drive * speedMod;

            // 2× upsample: zero-stuffed → polyphase halves of osTaps_
            // (gain ×2 compensates the stuffing loss).
            os.up[static_cast<std::size_t>(os.upIdx)] = u;
            double y2[2] = { 0.0, 0.0 };
            for (int phase = 0; phase < 2; ++phase)
            {
                double sum = 0.0;
                int idx = os.upIdx;
                for (int j = phase; j < kOsTaps; j += 2)
                {
                    sum += osTaps_[static_cast<std::size_t>(j)]
                         * os.up[static_cast<std::size_t>(idx)];
                    idx = (idx - 1) & kOsUpMask;
                }
                y2[phase] = 2.0 * sum;
            }
            os.upIdx = (os.upIdx + 1) & kOsUpMask;

            // Nonlinear core at 2·fs, decimation FIR fed per sub-sample.
            double out = 0.0;
            for (int s = 0; s < 2; ++s)
            {
                // 1) Record-head flux: leaky ∫ (LF corner ≈ 30 Hz)
                flux = tapeFluxLeak_ * flux + y2[s];

                // 2) Oxide magnetization through the JA hysteresis core —
                //    authentic nonlinear memory, rate-aware (Bertotti).
                const double H = tapeFluxToH_ * flux;
                const double M = ja.process(H, dt2x);

                // 3) Replay EMF = dB/dt: the f / (1/f) pair cancels in the
                //    passband while saturation stays 1/f-weighted.
                const double bNorm = (M + H) / tapeMs_;
                const double dB = bNorm - bPrev;
                bPrev = bNorm;
                const double e = dB * tapeEmfGain_ * kTapeMakeup;

                os.down[static_cast<std::size_t>(os.dnIdx)] = e;
                if (s == 1)
                {
                    double sum = 0.0;
                    int idx = os.dnIdx;
                    for (int j = 0; j < kOsTaps; ++j)
                    {
                        sum += osTaps_[static_cast<std::size_t>(j)]
                             * os.down[static_cast<std::size_t>(idx)];
                        idx = (idx - 1) & kOsDnMask;
                    }
                    out = sum;
                }
                os.dnIdx = (os.dnIdx + 1) & kOsDnMask;
            }

            // 4) Head-bump emphasis + gentle rounding (legacy voicing).
            double y = out;
            bump += bumpCoeff * (y - bump);
            y += (0.09 + 0.14 * amount_) * bump;
            y *= 1.0 / (1.0 + 0.25 * amount_ * std::abs(y));
            return y;
        };

        l = processOne(l, tapeJaL_, tape_.preLpL, tape_.fluxL, tape_.bPrevL,
                       tape_.bumpL, osL_);
        r = processOne(r, tapeJaR_, tape_.preLpR, tape_.fluxR, tape_.bPrevR,
                       tape_.bumpR, osR_);
    }

    void processSynthFx(double& l, double& r) noexcept
    {
        constexpr double kTwoPi = 6.28318530717958647692;
        synth_.phaseSlow += kTwoPi * (0.45 + 1.8 * amount_) / sampleRate_;
        synth_.phaseFast += kTwoPi * (28.0 + 130.0 * amount_) / sampleRate_;
        if (synth_.phaseSlow > kTwoPi) synth_.phaseSlow -= kTwoPi;
        if (synth_.phaseFast > kTwoPi) synth_.phaseFast -= kTwoPi;

        const double modSlow = 0.5 + 0.5 * std::sin(synth_.phaseSlow);
        const double modFast = std::sin(synth_.phaseFast);
        const double hpCoeff = envCoeff(sampleRate_, 2.0);

        auto processOne = [&](double x, double& hp, double& prev) noexcept
        {
            const double hpNow = x - prev + hpCoeff * hp;
            hp = hpNow;
            prev = x;
            const double ring = (0.35 + 0.35 * modSlow) * modFast * x;
            const double excite = 0.24 * hpNow * modSlow;
            return x + amount_ * (ring + excite);
        };

        l = processOne(l, synth_.hpL, synth_.prevL);
        r = processOne(r, synth_.hpR, synth_.prevR);

        // Width lift for stereo feed use-case; mono remains unaffected.
        const double m = 0.5 * (l + r);
        const double s = 0.5 * (l - r) * (1.0 + 0.35 * amount_);
        l = m + s;
        r = m - s;
    }

    // Tape-oxide JA parameters (γ-Fe₂O₃ class).  Compared to the
    // transformer presets in JilesAtherton.h: lower Ms (thin oxide layer),
    // much higher pinning k (high coercivity → wide hysteresis loop) and a
    // larger reversible fraction c.  Estimates after Jiles 1986 §V and
    // typical audio-tape coercivity data.
    void configureTapeCore() noexcept
    {
        JAParams p;
        p.Ms    = 350.0e3;
        p.a     = 45.0;
        p.alpha = 8.0e-5;
        p.k     = 90.0 * tapeCoreKScale_;
        p.c     = 0.35;
        p.kExcess = 0.05;   // oxide particle drag: mild dynamic widening
        tapeMs_ = p.Ms;
        tapeJaL_.setParams(p);
        tapeJaR_.setParams(p);
    }

    // Tape flux-drive calibration constants.
    //   kTapeHScale / kTapeMakeup were tuned so the JA engine matches the
    //   previous static-tanh voicing at the 1 kHz reference (amount 0.5,
    //   0.35 amp): RMS within 0.1 dB and H3/H1 within ~1 dB of the legacy
    //   numbers (RMS −3.05 dBFS, H3/H1 −15.7 dB).
    static constexpr double kTapeFcLowHz   = 30.0;    ///< record-head LF corner
    static constexpr double kTapeFluxRefHz = 1000.0;  ///< drive calibration freq
    static constexpr double kTapeHScale    = 38.0;    ///< A/m per unit drive @ref
    static constexpr double kTapeMakeup    = 3.55;    ///< output level trim

    double sampleRate_ { 48000.0 };
    double amount_ { 0.0 };
    double mix_ { 1.0 };

    ExpansionMode activeMode_ { ExpansionMode::Off };
    ExpansionMode targetMode_ { ExpansionMode::Off };
    bool crossfadeActive_ { false };
    int fadePos_ { 0 };
    int fadeSamples_ { 480 };

    OptoState opto_ {};
    FetState fet_ {};
    TapeState tape_ {};
    SynthState synth_ {};

    // Tape JA cores (one per channel) + flux-drive coefficients
    JilesAtherton tapeJaL_ {};
    JilesAtherton tapeJaR_ {};
    double tapeFluxLeak_ { 0.999 };
    double tapeFluxToH_  { 1.0 };
    double tapeEmfGain_  { 1.0 };
    double tapeMs_       { 350.0e3 };

    // Tape 2× oversampling wrap (docs/34 §4.2) + mode-invariant delay.
    static constexpr int kOsTaps  = 41;
    static constexpr int kOsUpMask = 31;   // ring ≥ ceil(41/2)
    static constexpr int kOsDnMask = 63;   // ring ≥ 41
    struct TapeOsState
    {
        std::array<double, 32> up {};
        std::array<double, 64> down {};
        int upIdx { 0 };
        int dnIdx { 0 };
    };
    std::array<double, kOsTaps> osTaps_ {};
    TapeOsState osL_ {};
    TapeOsState osR_ {};

    // Matching delay tap for the non-tape modes (and the rack's dry mix
    // leg) so the rack's latency never depends on the selected mode.
    struct DelayTap
    {
        std::array<double, 64> buf {};
        int idx { 0 };
        double process(double x) noexcept
        {
            buf[static_cast<std::size_t>(idx)] = x;
            const int rd = (idx - latencyInBaseSamples() + 64) & 63;
            idx = (idx + 1) & 63;
            return buf[static_cast<std::size_t>(rd)];
        }
    };
    DelayTap delayL_ {};
    DelayTap delayR_ {};

    // Per-instance Monte Carlo scales (ComponentVariation)
    double tapeCoreKScale_     { 1.0 };
    double tapeWowScale_       { 1.0 };
    double optoMemoryTauScale_ { 1.0 };
};

} // namespace valvra::dsp
