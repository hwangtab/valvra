// ─────────────────────────────────────────────────────────────────────────────
// TubeAmpChain — multi-stage tube amplifier chain builder
//
// Assembles 1–4 TubeStage instances with:
//   - Shared PowerSupplySag (one B+ rail driving all stages)
//   - Interstage RC high-pass (DC blocking coupling capacitors)
//   - ComponentVariation applied globally (per-instance Monte Carlo)
//   - Optional static input/output transformer coloration
//
// This is the runtime engine behind the 4 mode presets:
//   Preamp (V72)  →  2 stages (12AX7 pentode-strap + 12AU7 driver)
//   Output Stage  →  phase-inverter LTP + power-tube push-pull (Tier 2)
//   Line Color    →  EF86 → 6AS6 → 12AU7 cathode follower (Tier 2)
//   DI (RNDI)     →  12AX7 follower + 12AU7 stage + output trafo
//
// References:
//   docs/20 §4.4  (Chain builder concept)
//   docs/24 §A–D  (per-mode circuit details)
//   docs/04 §4    (interstage coupling capacitor)
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include "TubeStage.h"
#include "PowerSupplySag.h"
#include "ComponentVariation.h"
#include "TransformerStage.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace valvra::dsp {

// ─────────────────────────────────────────────────────────────────────────────
// Interstage coupling capacitor + grid-leak: one-pole high-pass.
//   fc = 1 / (2π · Rg · Cc)
// Typical: Cc = 22 nF, Rg = 1 MΩ → fc ≈ 7 Hz
// ─────────────────────────────────────────────────────────────────────────────
class InterstageCoupling
{
public:
    void prepare(double Cc, double Rg, double sampleRate) noexcept
    {
        const double fc = 1.0 / (2.0 * M_PI * Rg * Cc);
        const double rc = 1.0 / (2.0 * M_PI * fc);
        alpha_ = rc / (rc + 1.0 / sampleRate);
        state_x_ = state_y_ = 0.0;
    }

    void reset() noexcept { state_x_ = state_y_ = 0.0; }

    // One-pole HPF: y[n] = α · (y[n-1] + x[n] − x[n-1]).
    // NaN-recovery: a single non-finite input would otherwise latch the
    // delay line into NaN forever.
    double process(double x) noexcept
    {
        if (! std::isfinite(x) || ! std::isfinite(state_x_)
                               || ! std::isfinite(state_y_))
        {
            state_x_ = 0.0;
            state_y_ = 0.0;
            return 0.0;
        }
        const double y = alpha_ * (state_y_ + x - state_x_);
        state_x_ = x;
        state_y_ = y;
        return y;
    }

private:
    double alpha_ {0.99};
    double state_x_ {0.0};
    double state_y_ {0.0};
};

// ─────────────────────────────────────────────────────────────────────────────
// Static transformer coloration — a linear high-frequency rolloff plus a
// mild low-frequency rolloff, imitating leakage inductance + primary Lm.
// Jiles-Atherton nonlinearity is wired separately (full TransformerStage
// will be in Tier 2 / next step).
//
// fc_low  : low-frequency corner (primary inductance limit)
// fc_high : high-frequency corner (leakage inductance limit)
// ─────────────────────────────────────────────────────────────────────────────
class LinearTransformerColor
{
public:
    void prepare(double fc_low, double fc_high, double sampleRate) noexcept
    {
        // High-pass (primary Lm rolloff)
        const double rc_low = 1.0 / (2.0 * M_PI * fc_low);
        alpha_hp_ = rc_low / (rc_low + 1.0 / sampleRate);

        // Low-pass (leakage + stray cap rolloff)
        alpha_lp_ =
            1.0 - std::exp(-2.0 * M_PI * fc_high / sampleRate);

        reset();
    }

    void reset() noexcept
    {
        xhp_ = yhp_ = ylp_ = 0.0;
    }

    double process(double x) noexcept
    {
        // HPF
        const double yhp = alpha_hp_ * (yhp_ + x - xhp_);
        xhp_ = x;
        yhp_ = yhp;

        // LPF
        ylp_ += alpha_lp_ * (yhp - ylp_);
        return ylp_;
    }

private:
    double alpha_hp_ {0.99};
    double alpha_lp_ {0.1};
    double xhp_ {0.0};
    double yhp_ {0.0};
    double ylp_ {0.0};
};

// ─────────────────────────────────────────────────────────────────────────────
// Chain configuration: up to 4 stages + shared PSU + interstage + transformer
// ─────────────────────────────────────────────────────────────────────────────
struct TubeAmpChainConfig
{
    static constexpr int kMaxStages = 4;

    int numStages { 2 };
    std::array<TubeStageConfig, kMaxStages> stages {};

    // Shared PSU
    PSUSagParams psu { psu_presets::k6X4_Pultec };
    bool  enablePSUSag { true };

    // Interstage coupling between stages i and i+1
    double interstageCc { 22.0e-9 };   // 22 nF
    double interstageRg { 1.0e6 };     // 1 MΩ → fc ≈ 7 Hz

    // Input/output transformers — full Jiles-Atherton model
    bool                    useInputTransformer  { true };
    TransformerStageConfig  inputTrafoConfig     { transformer_presets::Marinair() };
    bool                    useOutputTransformer { true };
    TransformerStageConfig  outputTrafoConfig    { transformer_presets::Marinair() };

    // Monte Carlo per-instance seed. Same seed → same character forever.
    std::uint64_t variationSeed { 0 };
};

// ─────────────────────────────────────────────────────────────────────────────
// Mode presets (4 signature modes from docs/24)
// ─────────────────────────────────────────────────────────────────────────────
namespace chain_presets {

/// V72-style Preamp: 12AX7 pentode-strap → 12AU7 driver with 6X4 PSU
inline TubeAmpChainConfig V72Preamp()
{
    TubeAmpChainConfig c;
    c.numStages      = 2;
    c.stages[0]      = presets::v72Stage1();
    c.stages[1]      = presets::v72Stage2();
    c.psu            = psu_presets::k6X4_Pultec;
    c.enablePSUSag   = true;
    c.interstageCc   = 22.0e-9;
    c.interstageRg   = 1.0e6;

    // Neve/Marinair-style input transformer — near-linear region.
    // H_scale / drive chosen so that a +4 dBu (≈1 V) signal stays comfortably
    // in the linear portion of the Langevin curve while still providing the
    // characteristic HF rolloff and subtle saturation on peaks.
    c.useInputTransformer      = true;
    c.inputTrafoConfig         = transformer_presets::Marinair();
    c.inputTrafoConfig.drive   = 0.5;
    c.inputTrafoConfig.H_scale = 50.0;

    // Output transformer: mild coloration for classic warmth
    c.useOutputTransformer      = true;
    c.outputTrafoConfig         = transformer_presets::Marinair();
    c.outputTrafoConfig.drive   = 0.6;
    c.outputTrafoConfig.H_scale = 60.0;
    c.outputTrafoConfig.fc_high = 16000.0;

    c.variationSeed = 0;  // default seed; user re-rolls via setVariationSeed()
    return c;
}

/// Marshall-style output stage (cascade approximation of push-pull).
/// Characterized by hot drive, bright partial-bypass cathode caps, UTC-style
/// output transformer with earlier saturation.  The classic British crunch.
inline TubeAmpChainConfig MarshallMode()
{
    TubeAmpChainConfig c;
    c.numStages      = 2;
    c.stages[0]      = presets::marshallStage1();
    c.stages[1]      = presets::marshallStage2();
    c.psu            = psu_presets::kSolidState;   // Marshall JCM800 era uses SS rect
    c.enablePSUSag   = true;                       // weak but non-zero SS-rectifier sag
    c.interstageCc   = 22.0e-9;
    c.interstageRg   = 470.0e3;                    // classic Marshall value

    c.useInputTransformer      = false;            // line-level direct
    c.useOutputTransformer     = true;
    c.outputTrafoConfig        = transformer_presets::UTC_A12();  // earlier saturation
    c.outputTrafoConfig.drive  = 0.8;
    c.outputTrafoConfig.H_scale = 120.0;
    c.outputTrafoConfig.fc_high = 10000.0;         // guitar-amp-like top rolloff

    c.variationSeed = 0;
    return c;
}

/// Culture Vulture–style distortion unit.
/// 3-stage chain: EF86-like pentode input → 6AS6-like distortion core →
/// 12AU7 cathode follower output. Designed for extreme drive territory
/// (boutique "analog destruction" box).
inline TubeAmpChainConfig CultureVultureMode()
{
    TubeAmpChainConfig c;
    c.numStages      = 3;
    c.stages[0]      = presets::cultureVultureInput();
    c.stages[1]      = presets::cvDistortionCore();
    c.stages[2]      = presets::cvOutputBuffer();
    c.psu            = psu_presets::kGZ34;
    c.enablePSUSag   = true;
    c.interstageCc   = 100.0e-9;   // extended low end
    c.interstageRg   = 1.0e6;

    c.useInputTransformer      = true;
    c.inputTrafoConfig         = transformer_presets::UTC_A12();
    c.inputTrafoConfig.drive   = 0.8;
    c.inputTrafoConfig.H_scale = 90.0;

    c.useOutputTransformer     = true;
    c.outputTrafoConfig        = transformer_presets::UTC_A12();
    c.outputTrafoConfig.drive  = 1.0;
    c.outputTrafoConfig.H_scale = 100.0;
    c.outputTrafoConfig.fc_high = 14000.0;

    c.variationSeed = 0;
    return c;
}

/// RNDI-style DI: 12AX7 follower + 12AU7 + output transformer dominated
inline TubeAmpChainConfig RNDIMode()
{
    TubeAmpChainConfig c;
    c.numStages      = 2;
    c.stages[0]      = presets::rndiStage();
    c.stages[1]      = presets::v72Stage2();
    c.stages[1].enableCathodeBounce = false;      // DI mode: no cathode-bounce memory
    c.psu            = psu_presets::kSolidState;  // modern DI has clean rails
    c.enablePSUSag   = false;
    c.interstageCc   = 100.0e-9;  // extended low-end for bass DI
    c.interstageRg   = 1.0e6;

    c.useInputTransformer  = false;  // Hi-Z direct input
    c.useOutputTransformer = true;
    c.outputTrafoConfig    = transformer_presets::JensenJT11();  // wide BW, clean

    c.variationSeed = 0;
    return c;
}

} // namespace chain_presets

// ─────────────────────────────────────────────────────────────────────────────
// TubeAmpChain — real-time multi-stage tube amplifier
// ─────────────────────────────────────────────────────────────────────────────
class TubeAmpChain
{
public:
    TubeAmpChain() = default;

    void setup(const TubeAmpChainConfig& cfg, double sampleRate)
    {
        config_     = cfg;
        sampleRate_ = sampleRate;

        // Apply component variation (same seed drives all stages)
        applyVariation();

        // Prepare stages
        for (int i = 0; i < config_.numStages; ++i)
        {
            stages_[i].setup(configuredStage(i), sampleRate);

            // Scatter heater-hum phases from the Monte Carlo seed so no two
            // stages (and no two L/R chain instances) run in lock-step with
            // the 60 Hz cycle.  Real racks never wire every heater to the
            // same AC phase; this reproduces the uncorrelated flavour.
            const std::uint64_t phaseBits =
                config_.variationSeed
                ^ (0xD1B54A32D192ED03ULL
                   * static_cast<std::uint64_t>(i + 1));
            const double phase =
                (static_cast<double>(phaseBits & 0xFFFFFFFFULL)
                 / static_cast<double>(0xFFFFFFFFULL))
                * 2.0 * M_PI;
            stages_[i].setHeaterPhase(phase);

            // Seed the shot-noise RNG from a different hash of the chain
            // seed + stage index so every stage (and L/R chains) roll
            // their own noise stream.  Same chain seed → same streams
            // across project reloads.
            const std::uint64_t noiseBits =
                config_.variationSeed
                ^ (0xC2B2AE3D27D4EB4FULL
                   * static_cast<std::uint64_t>(i + 1));
            stages_[i].setShotNoiseSeed(noiseBits);
        }

        // Interstage couplings (between stage i and i+1)
        for (int i = 0; i < config_.numStages - 1; ++i)
            interstages_[i].prepare(config_.interstageCc, config_.interstageRg, sampleRate);

        // PSU — apply chain-wide Monte Carlo to rail level, impedance,
        // and 120 Hz ripple amplitude.
        auto psu = config_.psu;
        psu.sampleRate = sampleRate;
        if (! variationCache_.empty())
        {
            const auto& v0 = variationCache_.front();
            psu.Vb_nominal *= v0.Vb_scale;
            psu.Z_internal *= v0.Zsupply_scale;
            psu.ripple_amp *= v0.ripple_scale;
        }
        psu_.setParams(psu);
        psu_.reset();

        // Scatter the ripple (120 Hz) phase from the chain's variation
        // seed, for the same reason we scatter heater-hum phases.
        const std::uint64_t rippleBits =
            config_.variationSeed ^ 0xF39E91E1A7E7A8B1ULL;
        const double ripplePhase =
            (static_cast<double>(rippleBits & 0xFFFFFFFFULL)
             / static_cast<double>(0xFFFFFFFFULL))
            * 2.0 * M_PI;
        psu_.setRipplePhase(ripplePhase);

        // Transformers — full Jiles-Atherton + physical rolloffs + per-
        // instance perturbations to JA core + leakage-resonance peak.
        auto perturbTrafo = [this](TransformerStageConfig c) {
            if (variationCache_.empty()) return c;
            const auto& v0 = variationCache_.front();
            c.ja = ::valvra::dsp::applyVariation(c.ja, v0);
            c.presence_freq    *= v0.presenceFreq_scale;
            c.presence_gain_dB += v0.presenceGain_offset_dB;
            return c;
        };
        if (config_.useInputTransformer)
            inputTrafo_.setup(perturbTrafo(config_.inputTrafoConfig), sampleRate);
        if (config_.useOutputTransformer)
            outputTrafo_.setup(perturbTrafo(config_.outputTrafoConfig), sampleRate);
    }

    void reset(bool coldStart = true)
    {
        for (int i = 0; i < config_.numStages; ++i)
            stages_[i].reset(coldStart);
        for (int i = 0; i < config_.numStages - 1; ++i)
            interstages_[i].reset();
        psu_.reset();
        inputTrafo_.reset();
        outputTrafo_.reset();
    }

    // Re-seed Monte Carlo variation (user "Reroll" button).
    // Hot-updates tube parameters without resetting state (no clicks).
    void setVariationSeed(std::uint64_t seed)
    {
        config_.variationSeed = seed;
        applyVariation();
        for (int i = 0; i < config_.numStages; ++i)
        {
            auto cfg = configuredStage(i);
            stages_[i].triodeRef().setParams(cfg.tube);
        }
    }

    std::uint64_t variationSeed() const noexcept { return config_.variationSeed; }

    // One audio sample through the full chain.
    double process(double in) noexcept
    {
        double x = in;

        // 1) Input transformer linear color
        if (config_.useInputTransformer)
            x = inputTrafo_.process(x);

        // 2) Get current B+ (before stages for this sample).
        //    Three modes:
        //      externalPSUMode_ == true  → Vb supplied by caller (shared-PSU
        //                                  stereo coupling; the processor
        //                                  feeds the same sagged rail into
        //                                  both L and R chains every sample)
        //      enablePSUSag      == true → use our own PowerSupplySag
        //      otherwise                 → ideal fixed rail
        double Vb;
        if (externalPSUMode_)
            Vb = externalVb_;
        else if (config_.enablePSUSag)
            Vb = psu_.currentVb();
        else
            Vb = config_.psu.Vb_nominal;

        // 3) Cascade stages with interstage coupling.
        //    Track the sum of instantaneous plate currents across all stages
        //    as the actual load on the shared B+ rail — this drives PSU sag.
        double stageOutput = x;
        double totalIp     = 0.0;
        for (int i = 0; i < config_.numStages; ++i)
        {
            stageOutput = stages_[i].process(stageOutput, Vb);
            totalIp += stages_[i].lastPlateCurrent();

            if (i < config_.numStages - 1)
                stageOutput = interstages_[i].process(stageOutput);
        }

        // Publish the instantaneous current draw for the caller.  In
        // externalPSUMode_ the processor sums L+R and feeds the shared
        // sag envelope; otherwise we feed our internal sag here.
        lastTotalIp_ = totalIp;
        if (! externalPSUMode_ && config_.enablePSUSag)
            psu_.process(totalIp);

        // 5) Output transformer linear color
        if (config_.useOutputTransformer)
            stageOutput = outputTrafo_.process(stageOutput);

        return stageOutput;
    }

    // ─── Shared-PSU stereo coupling (opt-in) ──────────────────────────────
    // When the ValvraProcessor runs L/R through a single shared rail, it
    // sets externalPSUMode=true and pushes the current sagged Vb before
    // each process() call.  After both chains have processed the upsampled
    // sample, it reads lastTotalIp() from each and pumps the combined
    // current into its shared PowerSupplySag instance.
    void setExternalPSUMode(bool on) noexcept { externalPSUMode_ = on; }
    void setExternalVb(double vb) noexcept    { externalVb_ = vb; }
    double lastTotalIp() const noexcept       { return lastTotalIp_; }

    // ─────────────────────────────────────────────────────────────────────
    // Diagnostics / UI accessors
    // ─────────────────────────────────────────────────────────────────────
    double currentVb() const noexcept { return psu_.currentVb(); }
    double currentSagPercent() const noexcept { return psu_.sagPercent(); }

    // Transformer state accessors (for B-H hysteresis loop visualization).
    // Return H / M / Ms of the output transformer (most characterful).
    double outputTrafoH() const noexcept
    {
        return config_.useOutputTransformer ? outputTrafo_.currentH() : 0.0;
    }
    double outputTrafoM() const noexcept
    {
        return config_.useOutputTransformer ? outputTrafo_.currentM() : 0.0;
    }
    double outputTrafoMs() const noexcept
    {
        return config_.useOutputTransformer ? outputTrafo_.currentMsat() : 1.0;
    }
    TubeStage& stage(int i) noexcept { return stages_[i]; }
    const TubeStage& stage(int i) const noexcept { return stages_[i]; }
    int numStages() const noexcept { return config_.numStages; }

private:
    // Generate one ComponentVariation per stage. We hash the chain seed with
    // the stage index so each stage gets its own independent perturbation,
    // while the overall chain remains reproducible from a single user seed.
    void applyVariation()
    {
        variationCache_.clear();
        variationCache_.reserve(static_cast<std::size_t>(config_.numStages));
        for (int i = 0; i < config_.numStages; ++i)
        {
            const std::uint64_t stageSeed =
                config_.variationSeed
                ^ (0x9E3779B97F4A7C15ULL * static_cast<std::uint64_t>(i + 1));
            variationCache_.push_back(makeVariation(stageSeed));
        }
    }

    TubeStageConfig configuredStage(int i) const
    {
        auto cfg = config_.stages[i];
        if (static_cast<std::size_t>(i) < variationCache_.size())
        {
            const auto& v = variationCache_[static_cast<std::size_t>(i)];
            // Apply tube + passive perturbations.
            // Use qualified name to disambiguate from member function
            // with identical base name.
            cfg.tube  = ::valvra::dsp::applyVariation(cfg.tube, v);
            cfg.Ck   *= v.Ck_scale;
            cfg.Rp   *= v.Rk_scale;   // shared resistor tolerance
            cfg.Rk   *= v.Rk_scale;

            // ─── Hidden-physics per-instance perturbations ──────────────
            // Each of the six new physical mechanisms picks up a small
            // random spread so two Valvra instances loaded on different
            // tracks / channels end up with genuinely different "mojo".
            cfg.gridLeakR         *= v.gridLeakR_scale;
            cfg.gridCouplingC     *= v.gridCouplingC_scale;
            cfg.gridTurnOnVoltage += v.gridVon_offset;
            cfg.heaterHumAmplitude *= v.heaterHum_scale;
            cfg.thermalBiasSensitivity *= v.thermalSens_scale;
            cfg.slewRatePositive  *= v.slewPos_scale;
            cfg.slewRateNegative  *= v.slewNeg_scale;
            cfg.soakageAmount     *= v.soakageAmt_scale;
            cfg.soakageTau        *= v.soakageTau_scale;
        }
        return cfg;
    }

    TubeAmpChainConfig config_ {};
    double sampleRate_ { 48000.0 };

    std::array<TubeStage, TubeAmpChainConfig::kMaxStages> stages_ {};
    std::array<InterstageCoupling, TubeAmpChainConfig::kMaxStages - 1> interstages_ {};
    PowerSupplySag   psu_ {};
    TransformerStage inputTrafo_ {};
    TransformerStage outputTrafo_ {};

    std::vector<ComponentVariation> variationCache_ {};

    // External PSU (shared-rail stereo coupling) state.  When on, the
    // chain ignores its own psu_ and takes Vb from the injected value.
    bool   externalPSUMode_ { false };
    double externalVb_      { 325.0 };
    double lastTotalIp_     { 0.0 };
};

} // namespace valvra::dsp
