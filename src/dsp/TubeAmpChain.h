// ─────────────────────────────────────────────────────────────────────────────
// TubeAmpChain — multi-stage tube amplifier chain builder
//
// Assembles 1–4 TubeStage instances with:
//   - Shared PowerSupplySag (one B+ rail driving all stages)
//   - Interstage RC high-pass (DC blocking coupling capacitors)
//   - ComponentVariation applied globally (per-instance Monte Carlo)
//   - Optional static input/output transformer coloration
//
// This is the runtime engine behind the signature mode presets:
//   Preamp (V72)  →  2 stages (12AX7 pentode-strap + 12AU7 driver)
//   Output Stage  →  ideal phase split + power-tube push-pull
//   Line Color    →  EF86 → 6AS6 → 12AU7 cathode follower (Tier 2)
//   DI (RNDI)     →  12AX7 follower + 12AU7 stage + output trafo
//   HiFi 300B     →  6SN7 driver + 300B single-ended colour
//
// References:
//   docs/20 §4.4  (Chain builder concept)
//   docs/24 §A–D  (per-mode circuit details)
//   docs/04 §4    (interstage coupling capacitor)
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include "TubeStage.h"
#include "PushPullStage.h"
#include "PowerSupplySag.h"
#include "ComponentVariation.h"
#include "TransformerStage.h"

#include <algorithm>
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
    void prepare(double Cc,
                 double Rg,
                 double sampleRate,
                 double daAmount = 0.0,
                 double daTauSeconds = 0.35) noexcept
    {
        const double fc = 1.0 / (2.0 * M_PI * Rg * Cc);
        const double rc = 1.0 / (2.0 * M_PI * fc);
        alpha_ = rc / (rc + 1.0 / sampleRate);
        daAmount_ = std::clamp(daAmount, 0.0, 0.20);
        const double tau = std::max(daTauSeconds, 1.0e-3);
        daFastAlpha_ = std::exp(-1.0 / (std::max(0.018, tau * 0.08) * sampleRate));
        daMidAlpha_  = std::exp(-1.0 / (std::max(0.090, tau * 0.35) * sampleRate));
        daSlowAlpha_ = std::exp(-1.0 / (tau * sampleRate));
        daEnvAlpha_ = std::exp(-1.0 / (0.012 * sampleRate));
        state_x_ = state_y_ = daFastState_ = daMidState_ = daSlowState_ = 0.0;
        daLevelEnv_ = 0.0;
    }

    void reset() noexcept
    {
        state_x_ = state_y_ = daFastState_ = daMidState_ = daSlowState_ = 0.0;
        daLevelEnv_ = 0.0;
    }

    // One-pole HPF: y[n] = α · (y[n-1] + x[n] − x[n-1]).
    // NaN-recovery: a single non-finite input would otherwise latch the
    // delay line into NaN forever.
    double process(double x,
                   double interactionDrive = 0.0,
                   double blockingDrive = 0.0) noexcept
    {
        if (! std::isfinite(x) || ! std::isfinite(state_x_)
                               || ! std::isfinite(state_y_)
                               || ! std::isfinite(daFastState_)
                               || ! std::isfinite(daMidState_)
                               || ! std::isfinite(daSlowState_))
        {
            state_x_ = 0.0;
            state_y_ = 0.0;
            daFastState_ = daMidState_ = daSlowState_ = 0.0;
            daLevelEnv_ = 0.0;
            return 0.0;
        }
        const double y = alpha_ * (state_y_ + x - state_x_);
        state_x_ = x;
        state_y_ = y;
        if (daAmount_ <= 1.0e-9)
            return y;

        daFastState_ = daFastAlpha_ * daFastState_ + (1.0 - daFastAlpha_) * y;
        daMidState_  = daMidAlpha_  * daMidState_  + (1.0 - daMidAlpha_)  * y;
        daSlowState_ = daSlowAlpha_ * daSlowState_ + (1.0 - daSlowAlpha_) * y;
        daLevelEnv_ = daEnvAlpha_ * daLevelEnv_
                    + (1.0 - daEnvAlpha_) * std::abs(y);
        const double levelDrive = std::clamp(
            std::max(daLevelEnv_ * 3.0, interactionDrive), 0.0, 1.0);
        const double memoryDrive = std::clamp(
            0.72 * levelDrive + 0.28 * blockingDrive, 0.0, 1.0);
        const double cleanGate = memoryDrive * memoryDrive;
        const double daBlend = daAmount_ * (0.06 + 0.94 * cleanGate);
        const double memory = 0.28 * daFastState_
                            + 0.46 * daMidState_
                            + 0.26 * daSlowState_;
        return y + daBlend * memory;
    }

private:
    double alpha_ {0.99};
    double daFastAlpha_ {0.99};
    double daMidAlpha_ {0.999};
    double daSlowAlpha_ {0.999};
    double daEnvAlpha_ {0.99};
    double daAmount_ {0.0};
    double state_x_ {0.0};
    double state_y_ {0.0};
    double daFastState_ {0.0};
    double daMidState_ {0.0};
    double daSlowState_ {0.0};
    double daLevelEnv_ {0.0};
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
enum class FeedbackVoicing
{
    Neutral = 0,
    Controlled,
    LowFeedback,
    IronDamping
};

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
    double interstageDAAmount { 0.0 };  // dielectric absorption memory
    double interstageDATau { 0.35 };    // slow recovery tail [s]

    // Analog Realism layer: weak surrounding-circuit interactions.  These
    // are profile-scaled by the processor, not user-facing per-stage knobs.
    double realismAmount { 0.0 };
    double feedbackAmount { 0.0 };
    double transformerLoading { 0.0 };
    FeedbackVoicing feedbackVoicing { FeedbackVoicing::Neutral };

    // Input/output transformers — full Jiles-Atherton model
    bool                    useInputTransformer  { true };
    TransformerStageConfig  inputTrafoConfig     { transformer_presets::Marinair() };
    bool                    useOutputTransformer { true };
    TransformerStageConfig  outputTrafoConfig    { transformer_presets::Marinair() };

    // Optional push-pull power output stage.  When enabled, sits AFTER the
    // regular tube stages and BEFORE the output transformer — i.e. it
    // drives the OPT primary directly, the way a real guitar-amp output
    // section does.  Used by the Marshall preset (12AX7 cascade → ideal
    // phase split + PP EL34).  Off by default so existing presets keep their
    // behaviour.
    bool                  usePushPullOutputStage { false };
    PushPullStageConfig   pushPullConfig         {};

    // Culture Vulture T/P1/P2 mode switch.  Only used by the Culture Vulture
    // preset; kept in the chain config so automation rebuilds the 6AS6 stage
    // itself rather than layering gain after the fact.
    CultureVultureVoicing cultureVoicing { CultureVultureVoicing::PentodeLow };

    // Monte Carlo per-instance seed. Same seed → same character forever.
    std::uint64_t variationSeed { 0 };
    VariationDistribution variationDistribution { VariationDistribution::Modern };
};

struct AnalogInteractionState
{
    double inputEnv { 0.0 };
    double currentEnv { 0.0 };
    double sagEnv { 0.0 };
    double coreEnv { 0.0 };
    double coreMemoryEnv { 0.0 };
    double lfEnv { 0.0 };
    double lfSignal { 0.0 };
    double stageMemoryEnv { 0.0 };
    double headroomEnv { 1.0 };
    double feedbackLow { 0.0 };
    double feedbackHigh { 0.0 };
    double feedbackActivity { 0.0 };
    double phaseLagX { 0.0 };
    double phaseLagY { 0.0 };
    double interactionDrive { 0.0 };

    void reset() noexcept
    {
        inputEnv = currentEnv = sagEnv = coreEnv = coreMemoryEnv = lfEnv = lfSignal = 0.0;
        stageMemoryEnv = 0.0;
        headroomEnv = 1.0;
        feedbackLow = feedbackHigh = 0.0;
        feedbackActivity = 0.0;
        phaseLagX = phaseLagY = 0.0;
        interactionDrive = 0.0;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Mode presets (docs/24 signatures plus the HiFi 300B extension)
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

/// Console Output — power-tube saturation tuned for mix/master use.
/// Same physical DSP as the Marshall guitar-amp output stage (real
/// push-pull EL34 pair through a UTC OPT, with the tail-coupling Newton-
/// Raphson solver), but the *operating point* is rolled into the heart
/// of class-A1 instead of poised at the class-AB cutoff knee.  At the
/// default Drive=1.0 the user gets gentle even-harmonic warming and the
/// transformer's mid-rich saturation — the kind of "console power-stage
/// drive" that lifts a drum bus or a master without sounding like a
/// guitar amp.  Push Drive past ~2.0 and the operating point eventually
/// crosses into class-AB cutoff, where the JCM800-style British crunch
/// emerges (so guitar coloration is still available — it's just no
/// longer the *default* voice).
///
///   Stage 1: 12AX7 input/gain (warm bias, was cold)
///   Stage 2: 12AX7 driver
///   PushPull: ideal phase split + EL34 push-pull → OPT primary
///
/// docs/24 §B.2 (Marshall power section physical reference).
inline TubeAmpChainConfig MarshallMode()
{
    TubeAmpChainConfig c;
    c.numStages      = 2;
    c.stages[0]      = presets::marshallStage1();
    c.stages[1]      = presets::marshallStage2();
    c.psu            = psu_presets::kSolidState;   // SS rectifier, very low sag
    c.enablePSUSag   = true;
    c.interstageCc   = 22.0e-9;
    c.interstageRg   = 470.0e3;

    c.useInputTransformer      = false;            // line-level direct
    c.useOutputTransformer     = true;
    // UTC A-12 OPT — saturation is moderate at default drive, lifting
    // the mids audibly without crunching.  Pushing Drive opens the
    // transformer's nonlinear region the same way a real console's
    // power-stage transformer does.
    c.outputTrafoConfig        = transformer_presets::UTC_A12();
    c.outputTrafoConfig.drive  = 0.55;             // gentler than guitar-amp fit
    c.outputTrafoConfig.H_scale = 90.0;
    c.outputTrafoConfig.fc_high = 14000.0;         // wider than a guitar amp
    // Output-level calibration for mix/master workflows.  Keep the PP and
    // transformer nonlinear behavior unchanged; raise only post-trafo level.
    c.outputTrafoConfig.outputGain = 2.2;

    // ─── Push-pull EL34 power section, biased for mix/master use ────────
    // Vg_bias = −25 V puts the pair squarely in class-A1: both tubes
    // conduct through the entire signal swing at the default Drive=1.0.
    // The audible result is even-harmonic-dominant gentle saturation
    // and the transformer's program-dependent mid colour, with NO
    // asymmetric cutoff distortion.
    //
    // driveScale = 15 V/unit means a peak-1.0 sample at the PP input
    // produces ±15 V at the grid (vs the previous ±32 V that pushed
    // straight into cutoff).  Drive=2.0 brings ±30 V — approaching the
    // class-AB knee.  Drive=2.5+ enters the British-crunch territory
    // for users who want guitar coloration on tap.
    //
    // tubeAsymmetry stays at 3 % so the matched-pair "even-harmonic
    // breath" is preserved — that's part of what gives the mode its
    // analog-rack feel rather than perfect digital symmetry.
    c.usePushPullOutputStage = true;
    PushPullStageConfig pp;
    pp.powerTube   = params::kEL34_TriodeStrapped;
    pp.Vg_bias     = -25.0;          // class-A1 (was −36 V class-AB1)
    pp.Vp_nominal  = 450.0;
    pp.Rp_primary  = 3400.0;
    pp.Rk_tail     = 470.0;
    pp.driveScale  = 15.0;           // gentler default (was 32)
    pp.tubeAsymmetry  = 0.03;        // matched-pair feel preserved
    pp.solveTailCoupling = true;
    pp.tailSolverIters   = 2;
    pp.useLtpPhaseSplitter = true;
    pp.ltpTube          = params::kRSD_1;   // 12AX7-ish phase inverter
    pp.ltpVgBias        = -1.6;
    pp.ltpVpNominal     = 300.0;
    pp.ltpRp            = 100.0e3;
    pp.ltpTailR         = 47.0e3;
    pp.ltpDriveVolts    = 2.2;
    pp.ltpPlateRRatio   = 0.92;             // classic 82k/100k imbalance
    pp.ltpTubeMismatch  = 0.02;
    pp.ltpCommonModeLeak = 0.03;
    pp.ltpSolverIters   = 2;
    pp.ltpToPowerGridGain = 0.20;
    pp.enableWarmup    = true;
    pp.warmupTauSeconds = 30.0;
    // Output makeup so the chain output level matches V72/CV/RNDI in
    // normalised audio space.  The PP's natural diff-current
    // normalisation lands ~−47 dB below V72 unless boosted; 48× linear
    // empirically lines up at Drive=1.0.  Class-AB clipping still caps
    // at ±1 sample at extreme drive thanks to diff-current saturation,
    // so this boost only fills out the linear region.
    pp.outputGainLinear = 48.0;
    c.pushPullConfig = pp;

    c.variationSeed = 0;
    return c;
}

/// Culture Vulture–style distortion unit.
/// 3-stage chain: EF86-like pentode input → 6AS6-like distortion core →
/// 12AU7 cathode follower output. Designed for extreme drive territory
/// (boutique "analog destruction" box).
inline TubeAmpChainConfig CultureVultureMode(
    CultureVultureVoicing voicing = CultureVultureVoicing::PentodeLow)
{
    TubeAmpChainConfig c;
    c.numStages      = 3;
    c.stages[0]      = presets::cultureVultureInput();
    c.stages[1]      = presets::cvDistortionCore(voicing);
    c.stages[2]      = presets::cvOutputBuffer();
    c.cultureVoicing = voicing;
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
    // CV mode is intentionally extreme, but default level was too low for
    // practical A/B in mix sessions.
    c.outputTrafoConfig.outputGain = 4.0;

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
    c.stages[0].enableShotNoise = true;
    c.stages[0].shotNoiseScale = 2.0e-6;
    c.stages[1].enableHeaterHum = false;          // modern DI is hum-managed
    c.stages[1].shotNoiseScale = 2.5e-6;
    c.psu            = psu_presets::kSolidState;  // modern DI has clean rails
    c.enablePSUSag   = false;
    c.interstageCc   = 100.0e-9;  // extended low-end for bass DI
    c.interstageRg   = 1.0e6;

    c.useInputTransformer  = false;  // Hi-Z direct input
    c.useOutputTransformer = true;
    c.outputTrafoConfig    = transformer_presets::JensenJT11();  // wide BW, clean
    // Practical DI workflow level alignment.
    c.outputTrafoConfig.outputGain = 3.0;

    c.variationSeed = 0;
    return c;
}

/// HiFi 300B SE — single-ended class-A1 audiophile mastering voice.
/// 6SN7 input → 6SN7 cathode-follower buffer → 300B SE power triode →
/// Lundahl LL output transformer.  Designed for low THD, even-harmonic
/// dominant warming on master-bus material — the "Audio Note / Cary"
/// flavour, distinct from V72 (broadcast-vintage), Marshall (guitar
/// push-pull), Culture Vulture (extreme distortion), and RNDI (DI).
inline TubeAmpChainConfig HiFi300BMode()
{
    TubeAmpChainConfig c;
    c.numStages      = 3;
    c.stages[0]      = presets::hifi6SN7Input();
    c.stages[1]      = presets::hifi6SN7Buffer();
    c.stages[2]      = presets::hifi300BPower();
    c.psu            = psu_presets::kSolidState;  // HiFi: clean, regulated
    c.enablePSUSag   = false;                     // SS-rectified, no sag
    c.interstageCc   = 220.0e-9;                  // generous LF response
    c.interstageRg   = 470.0e3;

    c.useInputTransformer = false;                // line input, no input trafo
    c.useOutputTransformer = true;
    // Lundahl LL1582 amorphous-core OPT — near-linear, very wide BW.
    // Drive set conservatively so the OPT stays in linear region — a
    // HiFi engineer wants the tube colour, not the trafo distortion.
    c.outputTrafoConfig         = transformer_presets::Lundahl();
    c.outputTrafoConfig.drive   = 0.4;
    c.outputTrafoConfig.H_scale = 30.0;
    c.outputTrafoConfig.fc_high = 30000.0;        // wider than guitar amps
    // Gentle level lift for mastering-chain usability at moderate drive.
    c.outputTrafoConfig.outputGain = 1.5;

    c.usePushPullOutputStage = false;             // single-ended, not PP
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
            const auto idx = static_cast<std::size_t>(i);
            stages_[idx].setup(configuredStage(i), sampleRate);

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
            stages_[idx].setHeaterPhase(phase);

            // Seed the shot-noise RNG from a different hash of the chain
            // seed + stage index so every stage (and L/R chains) roll
            // their own noise stream.  Same chain seed → same streams
            // across project reloads.
            const std::uint64_t noiseBits =
                config_.variationSeed
                ^ (0xC2B2AE3D27D4EB4FULL
                   * static_cast<std::uint64_t>(i + 1));
            stages_[idx].setShotNoiseSeed(noiseBits);
        }

        // Interstage couplings (between stage i and i+1)
        for (int i = 0; i < config_.numStages - 1; ++i)
        {
            interstages_[static_cast<std::size_t>(i)]
                .prepare(config_.interstageCc,
                         config_.interstageRg,
                         sampleRate,
                         config_.interstageDAAmount,
                         config_.interstageDATau);
        }

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
        {
            auto cfg = perturbTrafo(config_.inputTrafoConfig);
            const double loading = std::clamp(config_.transformerLoading, 0.0, 1.0);
            cfg.drive *= 1.0 + 0.16 * loading;
            cfg.H_scale *= 1.0 + 0.10 * loading;
            cfg.fc_low *= 1.0 + 0.45 * loading;
            cfg.lfSatDepth = std::clamp(cfg.lfSatDepth + 0.18 * loading, 0.0, 1.0);
            inputTrafo_.setup(cfg, sampleRate);
        }
        if (config_.useOutputTransformer)
        {
            auto cfg = perturbTrafo(config_.outputTrafoConfig);
            const double loading = std::clamp(config_.transformerLoading, 0.0, 1.0);
            cfg.drive *= 1.0 + 0.20 * loading;
            cfg.H_scale *= 1.0 + 0.12 * loading;
            cfg.fc_low *= 1.0 + 0.70 * loading;
            cfg.presence_gain_dB += 0.7 * loading;
            cfg.lfSatDepth = std::clamp(cfg.lfSatDepth + 0.22 * loading, 0.0, 1.0);
            outputTrafo_.setup(cfg, sampleRate);
        }

        // Push-pull output stage — opt-in.  Uses Monte Carlo perturbation
        // from the LAST variation entry (different hash from the preamp
        // stages) so the PP and the preamp don't co-vary identically on
        // every reroll.  Real racks have a power section with its own
        // tube spread independent of the preamp valves.
        if (config_.usePushPullOutputStage)
        {
            auto ppCfg = config_.pushPullConfig;
            if (! variationCache_.empty())
            {
                const auto& vp = variationCache_.back();
                ppCfg.powerTube =
                    ::valvra::dsp::applyVariation(ppCfg.powerTube, vp);
            }
            pushPull_.setup(ppCfg, sampleRate);
        }
        feedbackState_ = 0.0;
        interaction_.reset();
    }

    void reset(bool coldStart = true)
    {
        for (int i = 0; i < config_.numStages; ++i)
            stages_[static_cast<std::size_t>(i)].reset(coldStart);
        for (int i = 0; i < config_.numStages - 1; ++i)
            interstages_[static_cast<std::size_t>(i)].reset();
        psu_.reset();
        inputTrafo_.reset();
        outputTrafo_.reset();
        if (config_.usePushPullOutputStage)
            pushPull_.reset(coldStart);
        feedbackState_ = 0.0;
        interaction_.reset();
    }

    // Re-seed Monte Carlo variation (user "Reroll" button).
    // Rebuilds the chain so tube curves, passive tolerances, hidden-physics
    // parameters, transformer perturbations, PSU spread, heater phase, and
    // noise seeds all move together as a real new hardware instance would.
    //
    // clickFree=true keeps the currently-running graph alive and crossfades
    // to the rebuilt graph over ~10 ms.  This avoids the hard state reset
    // click (warmup/PSU/transformer memories snapping) during live reroll.
    // clickFree=false preserves the legacy immediate swap, which is useful
    // for deterministic tests that compare against a freshly built chain.
    void setVariationSeed(std::uint64_t seed, bool clickFree = true)
    {
        if (config_.variationSeed == seed)
            return;

        if (clickFree)
        {
            rerollFromConfig_ = config_;
            rerollFromStages_ = stages_;
            rerollFromInterstages_ = interstages_;
            rerollFromPsu_ = psu_;
            rerollFromInputTrafo_ = inputTrafo_;
            rerollFromOutputTrafo_ = outputTrafo_;
            rerollFromPushPull_ = pushPull_;
            rerollFromFeedbackState_ = feedbackState_;
            rerollFromInteraction_ = interaction_;
            rerollFromExternalPSUMode_ = externalPSUMode_;
            rerollFromExternalVb_ = externalVb_;
            rerollFromLastTotalIp_ = lastTotalIp_;
            rerollCrossfadeSamples_ =
                std::max(1, static_cast<int>(0.010 * sampleRate_));
            rerollCrossfadePos_ = 0;
            rerollCrossfadeActive_ = true;
        }
        else
        {
            rerollCrossfadeActive_ = false;
            rerollCrossfadePos_ = 0;
        }

        const bool keepExternalPSU = externalPSUMode_;
        const double keepExternalVb = externalVb_;

        config_.variationSeed = seed;
        const auto cfg = config_;
        setup(cfg, sampleRate_);
        externalPSUMode_ = keepExternalPSU;
        externalVb_ = keepExternalVb;
    }

    /// Re-arm the warmup envelope on every stage of the chain — preamp
    /// triodes plus the optional push-pull power section.  The processor
    /// calls this from the audio thread when the user clicks Warmup, so
    /// it must never allocate or block.
    void warmupAllStages() noexcept
    {
        for (int i = 0; i < config_.numStages; ++i)
            stages_[static_cast<std::size_t>(i)].simulateWarmup();
        if (config_.usePushPullOutputStage)
            pushPull_.simulateWarmup();
    }

    std::uint64_t variationSeed() const noexcept { return config_.variationSeed; }

    // One audio sample through the full chain.
    double process(double in) noexcept
    {
        auto runPath = [](double x,
                          const TubeAmpChainConfig& cfg,
                          std::array<TubeStage, TubeAmpChainConfig::kMaxStages>& stages,
                          std::array<InterstageCoupling, TubeAmpChainConfig::kMaxStages - 1>& interstages,
                          PowerSupplySag& psu,
                          TransformerStage& inputTrafo,
                          TransformerStage& outputTrafo,
                          PushPullStage& pushPull,
                          double& feedbackState,
                          AnalogInteractionState& interaction,
                          bool externalPSUMode,
                          double externalVb,
                          double& lastTotalIp) noexcept
        {
            if (!std::isfinite(feedbackState))
                feedbackState = 0.0;
            if (!std::isfinite(interaction.interactionDrive))
                interaction.reset();

            const double feedback = std::clamp(cfg.feedbackAmount, 0.0, 0.35);
            double feedbackLowWeight = 0.70;
            switch (cfg.feedbackVoicing)
            {
                case FeedbackVoicing::Controlled:  feedbackLowWeight = 0.82; break;
                case FeedbackVoicing::LowFeedback: feedbackLowWeight = 0.55; break;
                case FeedbackVoicing::IronDamping: feedbackLowWeight = 0.90; break;
                case FeedbackVoicing::Neutral:
                default: break;
            }
            const double feedbackComposite =
                feedbackLowWeight * interaction.feedbackLow
              + (1.0 - feedbackLowWeight) * interaction.feedbackHigh;
            x -= feedback * feedbackComposite;

            constexpr double envAlpha = 0.0065;
            constexpr double slowAlpha = 0.0015;
            constexpr double memoryAlpha = 0.0025;
            interaction.inputEnv += envAlpha * (std::abs(x) - interaction.inputEnv);

            // 1) Input transformer linear color
            if (cfg.useInputTransformer)
                x = inputTrafo.process(x);

            // 2) Get current B+ (before stages for this sample).
            double Vb;
            if (externalPSUMode)
                Vb = externalVb;
            else if (cfg.enablePSUSag)
                Vb = psu.currentVb();
            else
                Vb = cfg.psu.Vb_nominal;

            // 3) Cascade stages with interstage coupling.
            double stageOutput = x;
            double totalIp     = 0.0;
            double stageMemoryDrive = 0.0;
            double stageCurrentDrive = 0.0;
            for (int i = 0; i < cfg.numStages; ++i)
            {
                const auto idx = static_cast<std::size_t>(i);
                stageOutput = stages[idx].process(stageOutput, Vb);
                totalIp += stages[idx].lastPlateCurrent();
                const double blockingMemory = stages[idx].blockingMemoryDrive();
                const double thermalMemory = stages[idx].thermalMemoryDrive();
                const double cathodeStress = stages[idx].cathodeStressDrive();
                const double currentStress = stages[idx].stageCurrentDrive();
                stageMemoryDrive = std::max(stageMemoryDrive,
                    std::clamp(0.46 * blockingMemory
                             + 0.24 * thermalMemory
                             + 0.18 * cathodeStress
                             + 0.12 * currentStress,
                        0.0, 1.0));
                stageCurrentDrive = std::max(stageCurrentDrive, currentStress);

                if (i < cfg.numStages - 1)
                {
                    const double envelopeBlocking = std::clamp(
                        std::max(0.0, interaction.inputEnv - interaction.currentEnv),
                        0.0, 1.0);
                    const double blockingDrive = std::max(
                        blockingMemory,
                        0.45 * envelopeBlocking + 0.55 * stageMemoryDrive);
                    stageOutput = interstages[idx].process(
                        stageOutput,
                        interaction.interactionDrive,
                        blockingDrive);
                }
            }

            // 4) Push-pull output stage (optional).
            if (cfg.usePushPullOutputStage)
            {
                stageOutput = pushPull.process(stageOutput, Vb);
                totalIp    += pushPull.lastPlateCurrent();
            }

            // 5) Publish current draw / advance PSU.
            lastTotalIp = totalIp;
            if (! externalPSUMode && cfg.enablePSUSag)
                psu.process(totalIp);

            const double currentDrive = std::clamp(std::abs(totalIp) * 180.0, 0.0, 1.0);
            interaction.currentEnv += envAlpha * (
                std::max(currentDrive, stageCurrentDrive) - interaction.currentEnv);
            const double sagDrive = cfg.enablePSUSag
                ? std::clamp((cfg.psu.Vb_nominal - Vb)
                    / std::max(cfg.psu.Vb_nominal, 1.0) * 18.0, 0.0, 1.0)
                : 0.0;
            interaction.sagEnv += slowAlpha * (sagDrive - interaction.sagEnv);
            interaction.stageMemoryEnv += memoryAlpha
                * (stageMemoryDrive - interaction.stageMemoryEnv);

            // 6) Output transformer linear color.
            if (cfg.useOutputTransformer)
                stageOutput = outputTrafo.process(stageOutput);

            const double absOut = std::abs(stageOutput);
            double coreDrive = absOut;
            if (cfg.useOutputTransformer)
            {
                const double ms = std::max(std::abs(outputTrafo.currentMsat()), 1.0);
                const double magRatio = std::clamp(std::abs(outputTrafo.currentM()) / ms,
                                                   0.0, 1.0);
                const double hScale = std::max(
                    std::abs(cfg.outputTrafoConfig.H_scale
                        * cfg.outputTrafoConfig.drive) * 1.6,
                    1.0);
                const double hRatio = std::clamp(
                    std::abs(outputTrafo.currentH()) / hScale, 0.0, 1.0);
                coreDrive = std::max(coreDrive, 0.72 * magRatio + 0.28 * hRatio);
            }
            interaction.coreEnv += envAlpha * (coreDrive - interaction.coreEnv);
            interaction.coreMemoryEnv += slowAlpha
                * (coreDrive - interaction.coreMemoryEnv);
            interaction.lfSignal = 0.9975 * interaction.lfSignal
                                 + 0.0025 * stageOutput;
            const double lfDrive = std::clamp(std::abs(interaction.lfSignal) * 2.4,
                                              0.0, 1.0);
            interaction.lfEnv = 0.997 * interaction.lfEnv + 0.003 * lfDrive;
            const double headroom = std::clamp(1.0 - absOut, 0.0, 1.0);
            interaction.headroomEnv += envAlpha * (headroom - interaction.headroomEnv);
            interaction.interactionDrive = std::clamp(
                0.28 * interaction.inputEnv
              + 0.22 * interaction.currentEnv
              + 0.18 * interaction.sagEnv
              + 0.14 * interaction.coreEnv
              + 0.10 * interaction.stageMemoryEnv
              + 0.04 * interaction.coreMemoryEnv
              + 0.06 * (1.0 - interaction.headroomEnv),
                0.0,
                1.0);

            const double phaseStrength = std::clamp(
                cfg.realismAmount * cfg.transformerLoading
                    * (0.15 + 0.62 * interaction.lfEnv
                            + 0.23 * interaction.coreMemoryEnv),
                0.0, 1.0);
            if (phaseStrength > 1.0e-6)
            {
                const double a = std::clamp(0.58 + 0.32 * phaseStrength, 0.35, 0.95);
                const double ap = -a * stageOutput
                                + interaction.phaseLagX
                                + a * interaction.phaseLagY;
                interaction.phaseLagX = stageOutput;
                interaction.phaseLagY = ap;
                const double blend = 0.035 + 0.075 * phaseStrength;
                stageOutput = stageOutput * (1.0 - blend) + ap * blend;
            }
            else
            {
                interaction.phaseLagX = stageOutput;
                interaction.phaseLagY = stageOutput;
            }

            if (feedback > 1.0e-9)
            {
                double fbLowAlpha = 0.9985;
                double fbHighAlpha = 0.935;
                switch (cfg.feedbackVoicing)
                {
                    case FeedbackVoicing::Controlled:
                        fbLowAlpha = 0.9990;
                        fbHighAlpha = 0.955;
                        break;
                    case FeedbackVoicing::LowFeedback:
                        fbLowAlpha = 0.9978;
                        fbHighAlpha = 0.900;
                        break;
                    case FeedbackVoicing::IronDamping:
                        fbLowAlpha = 0.9993;
                        fbHighAlpha = 0.945;
                        break;
                    case FeedbackVoicing::Neutral:
                    default:
                        break;
                }
                interaction.feedbackLow =
                    fbLowAlpha * interaction.feedbackLow
                    + (1.0 - fbLowAlpha) * stageOutput;
                const double highTarget = stageOutput - interaction.feedbackLow;
                interaction.feedbackHigh =
                    fbHighAlpha * interaction.feedbackHigh
                    + (1.0 - fbHighAlpha) * highTarget;
                feedbackState = feedbackLowWeight * interaction.feedbackLow
                              + (1.0 - feedbackLowWeight) * interaction.feedbackHigh;
                interaction.feedbackActivity += envAlpha
                    * (std::abs(feedbackState) - interaction.feedbackActivity);
            }
            else
            {
                feedbackState = 0.0;
                interaction.feedbackLow = 0.0;
                interaction.feedbackHigh = 0.0;
                interaction.feedbackActivity = 0.0;
            }

            return stageOutput;
        };

        const double yNew = runPath(
            in,
            config_,
            stages_,
            interstages_,
            psu_,
            inputTrafo_,
            outputTrafo_,
            pushPull_,
            feedbackState_,
            interaction_,
            externalPSUMode_,
            externalVb_,
            lastTotalIp_);

        if (! rerollCrossfadeActive_)
            return yNew;

        const double yOld = runPath(
            in,
            rerollFromConfig_,
            rerollFromStages_,
            rerollFromInterstages_,
            rerollFromPsu_,
            rerollFromInputTrafo_,
            rerollFromOutputTrafo_,
            rerollFromPushPull_,
            rerollFromFeedbackState_,
            rerollFromInteraction_,
            rerollFromExternalPSUMode_,
            rerollFromExternalVb_,
            rerollFromLastTotalIp_);

        const float t = static_cast<float>(rerollCrossfadePos_ + 1)
                      / static_cast<float>(std::max(1, rerollCrossfadeSamples_));
        const double y = (1.0 - t) * yOld + t * yNew;
        lastTotalIp_ = (1.0 - t) * rerollFromLastTotalIp_ + t * lastTotalIp_;

        ++rerollCrossfadePos_;
        if (rerollCrossfadePos_ >= rerollCrossfadeSamples_)
            rerollCrossfadeActive_ = false;

        return y;
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
    double currentInteractionDrive() const noexcept
    {
        return interaction_.interactionDrive;
    }

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
    TubeStage& stage(int i) noexcept { return stages_[static_cast<std::size_t>(i)]; }
    const TubeStage& stage(int i) const noexcept { return stages_[static_cast<std::size_t>(i)]; }
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
            variationCache_.push_back(
                makeVariation(stageSeed, config_.variationDistribution));
        }
    }

    TubeStageConfig configuredStage(int i) const
    {
        auto cfg = config_.stages[static_cast<std::size_t>(i)];
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
    PushPullStage    pushPull_ {};

    // Click-free reroll transition: old graph state kept for a short
    // crossfade window while the rebuilt graph ramps in.
    TubeAmpChainConfig rerollFromConfig_ {};
    std::array<TubeStage, TubeAmpChainConfig::kMaxStages> rerollFromStages_ {};
    std::array<InterstageCoupling, TubeAmpChainConfig::kMaxStages - 1> rerollFromInterstages_ {};
    PowerSupplySag   rerollFromPsu_ {};
    TransformerStage rerollFromInputTrafo_ {};
    TransformerStage rerollFromOutputTrafo_ {};
    PushPullStage    rerollFromPushPull_ {};
    double rerollFromFeedbackState_      { 0.0 };
    AnalogInteractionState rerollFromInteraction_ {};
    bool   rerollFromExternalPSUMode_ { false };
    double rerollFromExternalVb_      { 325.0 };
    double rerollFromLastTotalIp_     { 0.0 };
    bool   rerollCrossfadeActive_     { false };
    int    rerollCrossfadePos_        { 0 };
    int    rerollCrossfadeSamples_    { 1 };

    std::vector<ComponentVariation> variationCache_ {};

    // External PSU (shared-rail stereo coupling) state.  When on, the
    // chain ignores its own psu_ and takes Vb from the injected value.
    bool   externalPSUMode_ { false };
    double externalVb_      { 325.0 };
    double lastTotalIp_     { 0.0 };
    double feedbackState_   { 0.0 };
    AnalogInteractionState interaction_ {};
};

} // namespace valvra::dsp
