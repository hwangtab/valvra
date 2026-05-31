// ─────────────────────────────────────────────────────────────────────────────
// ExpansionRack — Tier 4+ expansion engines (product incubation core)
//
// Purpose:
//   - Opto-style compressor core
//   - FET-style compressor core
//   - Tape-style saturation core
//   - Synth/FX feed core
//
// This module is intentionally framework-agnostic and real-time safe:
// no allocation, no locks, no exceptions in process paths.
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

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
        reset();
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
        double aL = x, aR = x;
        processActivePath(aL, aR);
        const double wet = 0.5 * (aL + aR);
        y = lerp(x, wet, mix_);
    }

    void processStereo(double inL, double inR, double& outL, double& outR) noexcept
    {
        double aL = inL, aR = inR;
        processActivePath(aL, aR);
        outL = lerp(inL, aL, mix_);
        outR = lerp(inR, aR, mix_);
    }

private:
    struct OptoState
    {
        double envFast { 0.0 };
        double envSlow { 0.0 };
        double gain { 1.0 };
    };

    struct FetState
    {
        double env { 0.0 };
        double gain { 1.0 };
    };

    struct TapeState
    {
        double magL { 0.0 }, magR { 0.0 };
        double lpL { 0.0 }, lpR { 0.0 };
        double bumpL { 0.0 }, bumpR { 0.0 };
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
            case ExpansionMode::TapeSat:  tape_ = {}; break;
            case ExpansionMode::SynthFx:  synth_ = {}; break;
            case ExpansionMode::Off:
            default: break;
        }
    }

    void processByMode(ExpansionMode mode, double& l, double& r) noexcept
    {
        switch (mode)
        {
            case ExpansionMode::OptoComp: processOpto(l, r); break;
            case ExpansionMode::FetComp:  processFet(l, r);  break;
            case ExpansionMode::TapeSat:  processTape(l, r); break;
            case ExpansionMode::SynthFx:  processSynthFx(l, r); break;
            case ExpansionMode::Off:
            default: break;
        }
    }

    void processActivePath(double& l, double& r) noexcept
    {
        double aL = l, aR = r;
        processByMode(activeMode_, aL, aR);

        if (! crossfadeActive_)
        {
            l = aL;
            r = aR;
            return;
        }

        double bL = l, bR = r;
        processByMode(targetMode_, bL, bR);

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
        const double det = std::max(std::abs(l), std::abs(r));
        const double attackMs = 4.0 + 14.0 * (1.0 - amount_);
        const double releaseMs = 90.0 + 540.0 * (1.0 - amount_);
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

        const double gainAttack = envCoeff(sampleRate_, 2.0);
        const double gainRelease = envCoeff(sampleRate_, 70.0 + 260.0 * (1.0 - amount_));
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

    void processTape(double& l, double& r) noexcept
    {
        constexpr double kTwoPi = 6.28318530717958647692;
        const double drive = 1.0 + 5.8 * amount_;

        tape_.wowPhase += kTwoPi * (0.25 + 0.35 * amount_) / sampleRate_;
        tape_.flutterPhase += kTwoPi * (5.0 + 4.0 * amount_) / sampleRate_;
        if (tape_.wowPhase > kTwoPi) tape_.wowPhase -= kTwoPi;
        if (tape_.flutterPhase > kTwoPi) tape_.flutterPhase -= kTwoPi;

        const double speedMod =
            1.0
            + 0.010 * amount_ * std::sin(tape_.wowPhase)
            + 0.003 * amount_ * std::sin(tape_.flutterPhase);

        const double lpCoeff = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * 1900.0 / sampleRate_);
        const double magCoeff = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * 1200.0 / sampleRate_);
        const double bumpCoeff = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * 85.0 / sampleRate_);

        auto processOne = [&](double x, double& lp, double& mag, double& bump) noexcept
        {
            lp += lpCoeff * (x - lp);
            const double hp = x - lp;
            const double pre = x + 0.45 * hp;
            mag += magCoeff * ((pre * speedMod) - mag);

            const double core = pre + 0.33 * mag;
            double y = std::tanh(core * drive) / std::tanh(std::max(1.0, drive));

            bump += bumpCoeff * (y - bump);
            y += (0.09 + 0.14 * amount_) * bump;
            y *= 1.0 / (1.0 + 0.25 * amount_ * std::abs(y));
            return y;
        };

        l = processOne(l, tape_.lpL, tape_.magL, tape_.bumpL);
        r = processOne(r, tape_.lpR, tape_.magR, tape_.bumpR);
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
};

} // namespace valvra::dsp
