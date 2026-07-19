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

    /// Carry the coupling network's state across a parameter-edit rebuild
    /// (docs/34 §4.3) — the HPF delay line and DA memory keep flowing.
    void carryStateFrom(const InterstageCoupling& o) noexcept
    {
        auto fin = [](double v) { return std::isfinite(v) ? v : 0.0; };
        state_x_     = fin(o.state_x_);
        state_y_     = fin(o.state_y_);
        daFastState_ = fin(o.daFastState_);
        daMidState_  = fin(o.daMidState_);
        daSlowState_ = fin(o.daSlowState_);
        daLevelEnv_  = fin(o.daLevelEnv_);
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

    // ─── Mains frequency (single source of truth) ───────────────────────
    // Every mains-derived texture in one instance comes off the SAME power
    // transformer in reality: heater-hum (1× line), rectifier ripple
    // (2× line), and chassis leakage hum.  The engine used to carry three
    // independent frequency fields (stage heaterFrequency = 60, psu
    // ripple_freq = 120, and a hardcoded 60 in the processor), so a 50 Hz
    // (EU) region could not be selected consistently.  setup() now derives
    // the heater and ripple frequencies from this one field; the processor
    // reads it back for its output leakage hum (docs/34 §1.5).  Per-stage /
    // per-instance PHASE offsets stay scattered from the Monte Carlo seed —
    // wiring differences are physical, the line frequency is not.
    double mainsFrequencyHz { 60.0 };

    /// Culture-Vulture-style BIAS knob (docs/35 C10): DC offset [V] added
    /// to every free (non-cathode-tied) suppressor grid.  The real unit's
    /// front-panel BIAS pot shifts the 6AS6 g3 DC — sweeping the transfer
    /// from polite to broken — on top of the envelope-driven voicing.
    /// 0 = the preset's documented operating point.
    double suppressorBiasOffsetV { 0.0 };

    // Interstage coupling between stages i and i+1
    double interstageCc { 22.0e-9 };   // 22 nF
    double interstageRg { 1.0e6 };     // 1 MΩ → fc ≈ 7 Hz
    double interstageDAAmount { 0.0 };  // dielectric absorption memory
    double interstageDATau { 0.35 };    // slow recovery tail [s]

    // ─── Per-stage B+ decoupling ladder (docs/03 §5, docs/04 §8) ────────
    // Real amps never feed every stage from the raw reservoir: the rail
    // runs through an R-C node per stage, with the INPUT stage the most
    // filtered.  Two physical consequences the single-shared-Vb model
    // missed: (1) preamp nodes see millivolt ripple while the output
    // stage rides the sawtooth, (2) heavy output-stage draw sags the
    // upstream nodes through the ladder with a ~0.5 s time constant —
    // the stages genuinely talk to each other through the supply.
    bool   enableRailDecoupling { true };
    // Minimum effective dropper resistance: the per-stage droppers are
    // auto-sized in setup() from the documented node voltages, and this
    // floors the node time constant when a stage's calibrated drop is
    // ~zero (direct tap — only short wiring between nodes).
    double decouplingR { 470.0 };
    double decouplingC { 47.0e-6 };

    // Analog Realism layer: weak surrounding-circuit interactions.  These
    // are profile-scaled by the processor, not user-facing per-stage knobs.
    double realismAmount { 0.0 };
    double feedbackAmount { 0.0 };
    double transformerLoading { 0.0 };
    FeedbackVoicing feedbackVoicing { FeedbackVoicing::Neutral };

    // ─── Global negative feedback loop (docs/34 §2.1) ───────────────────
    // Target loop gain T = β·A for a REAL per-sample feedback loop from the
    // output-transformer node back to the chain input (1-sample loop delay
    // = the physical propagation delay at the oversampled rate).  Unlike
    // feedbackAmount above (a slow envelope voicing), this loop's gain
    // collapses when the output stage clips — so the clipping knee hardens
    // and the linear-region THD / output impedance drop by (1+T), the
    // defining behaviour of a global-NFB power amp.  β is derived at setup
    // from a measured forward gain and a (1+T) makeup preserves the linear
    // level (docs/34 §5.2).  0 = no global feedback (single-ended / no-NFB
    // voicings such as HiFi 300B, V72, RNDI, Culture Vulture).
    double nfbLoopGain { 0.0 };

    /// docs/35 C9 (option C): profiles whose REFERENCE HARDWARE runs a
    /// global negative-feedback loop (console-class output amps) route
    /// their realism feedback budget into the REAL per-sample loop above
    /// instead of the envelope heuristic.  The envelope path stays for
    /// no-NFB references (V72/300B/RNDI/CV), whose voicing was
    /// deliberately built on it.  Mapping: an envelope amount of 0.10 at
    /// full realism adds T = 2.0 (with the Marshall preset's baseline
    /// 0.6 → total 2.6 ≈ 11 dB of feedback, console-grade).
    static constexpr double kEnvelopeToLoopGainC9 = 20.0;

    // Input/output transformers — full Jiles-Atherton model
    bool                    useInputTransformer  { true };
    TransformerStageConfig  inputTrafoConfig     { transformer_presets::Marinair() };
    bool                    useOutputTransformer { true };
    TransformerStageConfig  outputTrafoConfig    { transformer_presets::Marinair() };

    // ─── Voltage-native inter-stage interface (docs/34 §4.1) ────────────
    // Carries the signal between stages in PLATE VOLTS: each RC-coupled
    // boundary becomes  volts → coupling HPF → ×pad → next grid volts,
    // where the pad is synthesised at setup from the SAME legacy factors
    // (outputGainLinear·makeup·swing/normalizer) — the linear hand-off is
    // bit-identical, but the interior stages' 0.5 Hz output trackers no
    // longer eat the 0.5–7 Hz operating-point wobble: sag and thermal
    // motion genuinely pump the next stage's bias through the coupling
    // cap, as the physical circuit does.  Interstage-transformer
    // boundaries keep the legacy normalized hand-off (the iron's drive
    // calibration is normalized-domain).


    // ─── Interstage transformer (docs/14, docs/34 v2) ───────────────────
    // Replaces the RC coupling at ONE stage boundary with a full JA
    // transformer (classic interstage-iron topologies: Triad HS-52 class).
    // The trafo's flux path provides the DC blocking; its core adds the
    // interstage iron's own hysteresis colour.  −1 = off (default).
    int interstageTrafoAfterStage { -1 };
    TransformerStageConfig interstageTrafoConfig
        { transformer_presets::JensenJT11() };

    // OPT magnetizing-current → plate feedback (docs/34 §2.2).  The output
    // transformer reports its nonlinear magnetizing-current drop each
    // sample; the chain hands it (one sample delayed) to the driving power
    // stage — push-pull pair or a transformer-loaded SE stage — whose plate
    // KCL then genuinely sources the iron's demand.  Linear-regime response
    // is de-embedded inside the stages so the OPT's calibrated insertion
    // behaviour is preserved; what remains is the tubes' extra current
    // draw / dissipation / sag interaction, strongest on loud LF.
    bool enableOPTMagCoupling { true };

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

    // Output transformer: mild coloration for classic warmth.  Drive
    // calibrated against the measured OPT-node level (peak ≈ 3.9 at a
    // full-scale 1 kHz input) so the Ni-permalloy core peaks at ≈0.7·a —
    // audible iron grip without crushing the fundamental.  The legacy
    // value parked the core at 3.3·a, where the M/Ms-normalised output
    // degenerated into a unity limiter.
    c.useOutputTransformer      = true;
    c.outputTrafoConfig         = transformer_presets::Marinair();
    c.outputTrafoConfig.drive   = 0.13;
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
    // JCM800-class rail: the EL34 pair idles at 450 V (docs/24 §B.2); the
    // 12AX7 nodes sit at 300 V behind their droppers via the decoupling
    // ladder.  (Legacy fed the PP stage 325 V against a 450 V rest-point
    // assumption — the pair never operated where the preset documented.)
    c.psu.Vb_nominal = 450.0;
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
    // Si-steel core at ≈0.7·a for the measured PP-node level (pk ≈ 3.5
    // at full-scale input) — program-dependent mid grip, fundamental
    // intact.  Drive ≥ 2 pushes it past 1.4·a into the crunch zone.
    c.outputTrafoConfig.drive  = 0.23;
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
    // Resting Vgk = −36 V → 55 mA/side at the 450 V rail (the EL34
    // datasheet anchor, ≈25 W anode — true class-A idle at half the
    // load-line maximum).  The old −25 V figure was tuned against the
    // unsolved-plate engine; with real load-line physics it would idle
    // the pair at 210 mA/side (≈94 W — far beyond the EL34's rating).
    pp.Vg_bias     = -36.0;
    pp.Vp_nominal  = 450.0;
    pp.Rp_primary  = 3400.0;
    pp.Rk_tail     = 470.0;
    // ±7 V grid at peak-1.0 input = exactly the class-A/AB boundary
    // (one side reaches cutoff ≈ 7 V above idle): even a full-scale
    // sine at Drive 1 stays even-harmonic-dominant, music sits inside
    // class-A, and Drive ≥ 2 walks into the British AB crunch.
    pp.driveScale  = 7.0;
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

    // Global negative feedback around the power amp (docs/34 §2.1): a
    // console/guitar power stage wraps a loop from the OPT back to the
    // phase-splitter input.  T ≈ 0.6 (≈ 4 dB of feedback) tightens the
    // linear region and hardens the class-AB knee — the "controlled" power
    // amp feel — while the loop-gain collapse under clip keeps the crunch
    // available above Drive ~2.  β and level makeup are derived at setup.
    c.nfbLoopGain = 0.6;

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
    // CV's 3-stage cascade lands much quieter at the OPT node (pk ≈ 0.45
    // at full-scale): 1.6 puts the core at ≈0.7·a so the iron actually
    // participates at the default Drive, and walks into hard saturation
    // across the recommended 1.4–2.6 Drive range — the "analog
    // destruction" envelope this mode is for.
    c.outputTrafoConfig.drive  = 1.6;
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
    // Jensen voicing is "clean but not sterile": core at ≈0.5·a for the
    // measured DI-node level (pk ≈ 1.5 at full-scale input).
    c.outputTrafoConfig.drive = 0.25;
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
    // Rail headroom for the 300B node (350 V) plus its dropper.
    c.psu.Vb_nominal = 360.0;
    c.enablePSUSag   = false;                     // SS-rectified, no sag
    c.interstageCc   = 220.0e-9;                  // generous LF response
    c.interstageRg   = 470.0e3;

    c.useInputTransformer = false;                // line input, no input trafo
    c.useOutputTransformer = true;
    // Lundahl LL1582 amorphous-core OPT — near-linear, very wide BW.
    // Drive set conservatively so the OPT stays in linear region — a
    // HiFi engineer wants the tube colour, not the trafo distortion.
    c.outputTrafoConfig         = transformer_presets::Lundahl();
    // Amorphous core at ≈0.5·a for the measured 300B-node level (pk ≈ 7.3
    // at full-scale input) — HiFi engineers want the tube colour, not the
    // OPT's; the gapped-core asymmetry (H_dc below) is the audible part.
    c.outputTrafoConfig.drive   = 0.12;
    c.outputTrafoConfig.H_scale = 30.0;
    c.outputTrafoConfig.fc_high = 30000.0;        // wider than guitar amps
    // Single-ended OPT: the 300B's full idle current magnetizes the
    // (gapped) core, parking it part-way up the B-H curve.  The shifted
    // loop saturates asymmetrically — the physical source of the SE
    // even-harmonic dominance (docs/02 §6), not a tuning trick.
    c.outputTrafoConfig.H_dc    = 10.0;
    // Level alignment: the OPT is now insertion-unity (the legacy deep-
    // saturated core acted as a limiter that this gain fought against),
    // so the trim drops below 1 to keep the chain's normalized output
    // in mastering range at moderate drive.
    c.outputTrafoConfig.outputGain = 0.22;

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
            auto sCfg = configuredStage(i);

            // Coupling-cap leakage bias drift (docs/34 §3.6): an aged
            // paper/wax coupling cap leaks the PREVIOUS stage's plate DC
            // onto this grid through the Rg divider, warming the bias —
            // per-unit, era-dependent (zero on Modern populations).  The
            // previous stage is already set up, so its solved rest plate
            // voltage is the physically correct source.  Clamped to 40%
            // of the bias magnitude so a wild draw shifts character, not
            // the documented operating class.
            if (i >= 1 && static_cast<int>(idx) < variationCount_)
            {
                const double ratio =
                    variationCache_[idx].couplingLeak_ratio;
                if (ratio > 0.0)
                {
                    const double vPrev = stages_[idx - 1]
                        .restingPlateVoltage();
                    const double cap = 0.4 * std::abs(sCfg.Vg_bias);
                    sCfg.Vg_bias += std::clamp(vPrev * ratio, 0.0, cap);
                }
            }

            // Voltage-native boundary flags (docs/34 §4.1): the trafo
            // boundary (if any) stays legacy-normalized.
            const int trafoAt = config_.interstageTrafoAfterStage;
            // Volt-native is the ONLY inter-stage interface since docs/34
            // W8-④'s one-cycle legacy grace expired (docs/35 D3); the
            // trafo boundary stays normalized as before.
            sCfg.voltageNativeOutput = i < config_.numStages - 1
                && i != trafoAt;
            sCfg.voltageNativeInput = i > 0 && (i - 1) != trafoAt;
            stageSwingCache_[idx] = sCfg.inputVoltageSwing;
            stageOutGainCache_[idx] = sCfg.outputGainLinear;

            stages_[idx].setup(sCfg, sampleRate);

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

        // Voltage-native pads (docs/34 §4.1): one explicit attenuator per
        // RC boundary, synthesised from the legacy calibration product so
        // the linear hand-off is bit-identical to the normalized path.
        for (int i = 0; i < config_.numStages - 1; ++i)
        {
            const auto idx = static_cast<std::size_t>(i);
            interstagePads_[idx] = stageOutGainCache_[idx]
                * stages_[idx].outputMakeupFactor()
                * stageSwingCache_[idx + 1]
                / stages_[idx].outputNormalizer();
            if (! std::isfinite(interstagePads_[idx]))
                interstagePads_[idx] = 1.0;
        }

        // Interstage couplings (between stage i and i+1).  The coupling
        // capacitor takes its Monte Carlo film-cap tolerance — previously
        // generated but never applied.
        for (int i = 0; i < config_.numStages - 1; ++i)
        {
            const auto idx = static_cast<std::size_t>(i);
            double ccScale = 1.0;
            if (static_cast<int>(idx) < variationCount_)
                ccScale = variationCache_[idx].coupling_scale;
            interstages_[idx]
                .prepare(config_.interstageCc * ccScale,
                         config_.interstageRg,
                         sampleRate,
                         config_.interstageDAAmount,
                         config_.interstageDATau);
        }

        // ─── Rail-decoupling ladder calibration ─────────────────────────
        // Moved below: the ladder anchor needs the PSU's LOADED rest
        // output, which is only known after the PSU (and the push-pull
        // stage, whose draw loads it) is set up — see the block after
        // pushPull_.setup() (docs/35 §S2 D-A).

        // PSU — apply chain-wide Monte Carlo to rail level, impedance,
        // and 120 Hz ripple amplitude.
        auto psu = config_.psu;
        psu.sampleRate = sampleRate;
        // Full-wave rectifier ripple is at 2× line frequency — derive it
        // from the single mains source so an EU (50 Hz → 100 Hz ripple) or
        // US (60 Hz → 120 Hz) region stays coherent with the heater hum.
        psu.ripple_freq = 2.0 * config_.mainsFrequencyHz;
        if (variationCount_ > 0)
        {
            const auto& v0 = variationCache_[0];
            psu.Vb_nominal *= v0.Vb_scale;
            psu.Z_internal *= v0.Zsupply_scale;
            psu.ripple_amp *= v0.ripple_scale;
            // Reservoir model: the ripple spread comes from the cap's
            // tolerance — ripple p-p goes as I/(f·C), so divide C by the
            // same draw the legacy sine path multiplied into amplitude.
            psu.reservoirFarads /= std::max(v0.ripple_scale, 0.2);
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

        // Push-pull output stage — opt-in.  Uses Monte Carlo perturbation
        // from the LAST variation entry (different hash from the preamp
        // stages) so the PP and the preamp don't co-vary identically on
        // every reroll.  Real racks have a power section with its own
        // tube spread independent of the preamp valves.
        //
        // Set up BEFORE the output transformer: the pair's resting
        // current imbalance leaves a standing DC magnetization on the
        // OPT core (docs/02 §6) which the transformer needs at setup.
        if (config_.usePushPullOutputStage)
        {
            auto ppCfg = config_.pushPullConfig;
            // Independent draw for the power section — real racks have a
            // power-tube spread uncorrelated with the preamp valves (the
            // legacy code cloned the LAST preamp stage's draw).  The raw
            // spread is then DAMPENED ×0.4: power tubes ship factory-
            // graded and every fixed-bias amp gets its idle re-set at
            // install, so unit-to-unit idle varies far less than raw
            // emission would suggest (the character spread survives in
            // mu/gamma and the pair mismatch).
            auto vp = makeVariation(
                config_.variationSeed ^ 0xB5297A4D3F84D5B5ULL,
                config_.variationDistribution);
            vp.tube_G_scale  = 1.0 + 0.4 * (vp.tube_G_scale - 1.0);
            vp.tube_mu_scale = 1.0 + 0.6 * (vp.tube_mu_scale - 1.0);
            ppCfg.powerTube =
                ::valvra::dsp::applyVariation(ppCfg.powerTube, vp);
            const double pmDamped =
                1.0 + 0.5 * (vp.pairMismatch_scale - 1.0);
            ppCfg.tubeAsymmetry   *= pmDamped;
            ppCfg.ltpTubeMismatch *= pmDamped;

            // Bias adjustment: every fixed-bias amp gets its idle set to
            // spec at install regardless of which tubes landed in the
            // sockets (idle is exponentially sensitive to μ spread, and
            // an untrimmed pair would swing the stage gain several dB
            // per "unit").  Bisect Vg so the VARIED tube idles at the
            // PRESET tube's design current; the character spread then
            // lives where it does in hardware — curvature, mismatch,
            // class-AB transition shape — not in raw level.
            {
                KorenTriode nominal { config_.pushPullConfig.powerTube };
                KorenTriode varied  { ppCfg.powerTube };
                auto idleAt = [&](const KorenTriode& t, double vg)
                {
                    const double rDc = std::max(ppCfg.dcrPrimary, 0.0);
                    const double ul = std::clamp(ppCfg.ulTapRatio, 0.05, 1.0);
                    const double vb = ppCfg.Vp_nominal;
                    auto tap = [&](double vp) { return vb - ul * (vb - vp); };
                    double vp = vb;
                    for (int i = 0; i < 8; ++i)
                        vp = vb
                           - rDc * std::max(0.0, t.plateCurrent(tap(vp), vg));
                    return std::max(0.0, t.plateCurrent(tap(vp), vg));
                };
                const double target =
                    idleAt(nominal, config_.pushPullConfig.Vg_bias);
                double lo = ppCfg.Vg_bias - 12.0;
                double hi = ppCfg.Vg_bias + 12.0;
                for (int i = 0; i < 40; ++i)
                {
                    const double mid = 0.5 * (lo + hi);
                    if (idleAt(varied, mid) > target) hi = mid; else lo = mid;
                }
                ppCfg.Vg_bias = 0.5 * (lo + hi);
            }
            pushPull_.setup(ppCfg, sampleRate);
        }

        // ─── Loaded-rail settle + rail-decoupling ladder calibration ─────
        // Each stage's B+ node is fed through a dropper R + reservoir C,
        // auto-sized so every node RESTS exactly at the stage's documented
        // Vp_nominal.  The anchor must be the PSU's output AT THE CHAIN'S
        // QUIESCENT DRAW: anchoring on the no-load Vb_nominal left every
        // node a few percent low at equilibrium, which the rail-tracking
        // screen supply amplified into a ~20 V screen-node offset on the
        // near-critical 6AS6 (1/(1+Rs·dIg2/dVg2) sensitivity) — the
        // rest-vs-runtime divergence of docs/35 §S2 D-A.
        {
            double restDraw = 0.0;
            for (int i = 0; i < config_.numStages; ++i)
            {
                const auto idx = static_cast<std::size_t>(i);
                restDraw += stages_[idx].restingPlateCurrent()
                          + stages_[idx].restingScreenCurrent();
            }
            if (config_.usePushPullOutputStage)
                restDraw += pushPull_.restingPlateCurrent();

            double vbRest = config_.psu.Vb_nominal;
            if (config_.enablePSUSag)
            {
                // Settle the reservoir/sag ODE at the quiescent draw and
                // anchor on the ripple-mean of the final 0.1 s.  The PSU
                // is deliberately left in this settled state so t=0
                // matches the anchors (no cold-start rail transient).
                const int settleN = static_cast<int>(sampleRate);
                const int meanN = std::max(1,
                    static_cast<int>(0.1 * sampleRate));
                double acc = 0.0;
                for (int n = 0; n < settleN; ++n)
                {
                    const double v = psu_.process(restDraw);
                    if (n >= settleN - meanN) acc += v;
                }
                if (std::isfinite(acc) && acc > 0.0)
                    vbRest = acc / static_cast<double>(meanN);
            }

            const double cDec = std::max(config_.decouplingC, 1.0e-9);
            // Plate AND screen current leave through the same node.
            const auto restingNodeCurrent = [this](std::size_t idx) noexcept
            {
                return stages_[idx].restingPlateCurrent()
                     + stages_[idx].restingScreenCurrent();
            };
            double cumI = 0.0;
            for (int i = 0; i < config_.numStages; ++i)
                cumI += restingNodeCurrent(static_cast<std::size_t>(i));
            double nodeAbove = vbRest;
            for (int i = config_.numStages - 1; i >= 0; --i)
            {
                const auto idx = static_cast<std::size_t>(i);
                const double want = config_.stages[idx].Vp_nominal;
                const double drop = std::max(0.0, nodeAbove - want);
                decouplingRCalib_[idx] = (cumI > 1.0e-9)
                    ? drop / cumI : 0.0;
                // Node time constant from the calibrated dropper (the
                // config floor keeps zero-drop nodes lightly filtered
                // rather than bit-exact copies of the node above).
                const double rEff = std::max(decouplingRCalib_[idx],
                                             std::max(config_.decouplingR,
                                                      10.0));
                decouplingAlphaCalib_[idx] =
                    1.0 - std::exp(-1.0 / (rEff * cDec * sampleRate));
                railNodes_[idx] = std::min(nodeAbove, want);
                nodeAbove = railNodes_[idx];
                cumI -= restingNodeCurrent(idx);
            }
        }

        // Transformers — full Jiles-Atherton + physical rolloffs + per-
        // instance perturbations to JA core + leakage-resonance peak.
        // Input and output iron get INDEPENDENT draws: two different
        // physical transformers never share their winding/core lottery.
        auto perturbTrafo = [this](TransformerStageConfig c,
                                   std::uint64_t saltedSeed) {
            const auto v = makeVariation(saltedSeed,
                                         config_.variationDistribution);
            c.ja = ::valvra::dsp::applyVariation(c.ja, v);
            c.presence_freq    *= v.presenceFreq_scale;
            c.presence_gain_dB += v.presenceGain_offset_dB;
            return c;
        };
        if (config_.useInputTransformer)
        {
            auto cfg = perturbTrafo(config_.inputTrafoConfig,
                                    config_.variationSeed
                                        ^ 0x94D049BB133111EBULL);
            const double loading = std::clamp(config_.transformerLoading, 0.0, 1.0);
            cfg.drive *= 1.0 + 0.16 * loading;
            cfg.H_scale *= 1.0 + 0.10 * loading;
            cfg.fc_low *= 1.0 + 0.45 * loading;
            cfg.lfSatDepth = std::clamp(cfg.lfSatDepth + 0.18 * loading, 0.0, 1.0);
            inputTrafo_.setup(cfg, sampleRate);
        }
        if (config_.useOutputTransformer)
        {
            auto cfg = perturbTrafo(config_.outputTrafoConfig,
                                    config_.variationSeed
                                        ^ 0xBF58476D1CE4E5B9ULL);
            const double loading = std::clamp(config_.transformerLoading, 0.0, 1.0);
            cfg.drive *= 1.0 + 0.20 * loading;
            cfg.H_scale *= 1.0 + 0.12 * loading;
            cfg.fc_low *= 1.0 + 0.70 * loading;
            cfg.presence_gain_dB += 0.7 * loading;
            cfg.lfSatDepth = std::clamp(cfg.lfSatDepth + 0.22 * loading, 0.0, 1.0);

            // Standing DC magnetization on the OPT core (docs/02 §6):
            // push-pull idle imbalance leaves the differential ampere-
            // turns of the mismatch on the primary.  Shifts the B-H loop
            // off-centre → asymmetric saturation → the even-harmonic
            // "punch" a real PP output section has.  (Single-ended OPTs
            // set cfg.H_dc directly in their chain preset.)
            if (config_.usePushPullOutputStage)
                cfg.H_dc += 0.8 * cfg.H_scale
                          * pushPull_.restingImbalanceRatio();
            outputTrafo_.setup(cfg, sampleRate);
        }

        // Interstage transformer (docs/14): perturbed like the other iron,
        // with its own independent Monte Carlo draw.
        if (config_.interstageTrafoAfterStage >= 0
            && config_.interstageTrafoAfterStage < config_.numStages - 1)
        {
            auto cfg = perturbTrafo(config_.interstageTrafoConfig,
                                    config_.variationSeed
                                        ^ 0xA24BAED4963EE407ULL);
            interTrafo_.setup(cfg, sampleRate);
        }

        // fs-normalized interaction coefficients.  The legacy constants
        // were per-sample values tuned at 48 kHz; running the chain
        // oversampled (×2…×16) silently shortened every interaction
        // time constant by the same factor.
        {
            const double r = 48000.0 / std::max(sampleRate, 1.0);
            auto k48 = [&](double k) { return 1.0 - std::pow(1.0 - k, r); };
            auto p48 = [&](double a) { return std::pow(a, r); };
            envAlpha_    = k48(0.0065);
            slowAlpha_   = k48(0.0015);
            memoryAlpha_ = k48(0.0025);
            lfSigPole_   = p48(0.9975);
            lfEnvPole_   = p48(0.997);
            double fbLow = 0.9985, fbHigh = 0.935;
            switch (config_.feedbackVoicing)
            {
                case FeedbackVoicing::Controlled:  fbLow = 0.9990; fbHigh = 0.955; break;
                case FeedbackVoicing::LowFeedback: fbLow = 0.9978; fbHigh = 0.900; break;
                case FeedbackVoicing::IronDamping: fbLow = 0.9993; fbHigh = 0.945; break;
                case FeedbackVoicing::Neutral:
                default: break;
            }
            fbLowPole_  = p48(fbLow);
            fbHighPole_ = p48(fbHigh);
            rateRatio_  = r;
        }

        feedbackState_ = 0.0;
        interaction_.reset();

        // ─── OPT dynamic drive impedance anchor (docs/34 §3.9) ──────────
        // Record the rest output impedance of whatever drives the output
        // transformer; per-sample the chain then feeds the instantaneous
        // value so the iron's magnetizing drop follows the tube's rp — a
        // transformer distorts more against a softer (cutoff-bound) source.
        if (config_.useOutputTransformer)
        {
            const double zRest = config_.usePushPullOutputStage
                ? pushPull_.lastOutputImpedance()
                : (config_.numStages > 0
                       ? stages_[static_cast<std::size_t>(
                             config_.numStages - 1)].lastOutputImpedance()
                       : 0.0);
            outputTrafo_.setRestSourceImpedance(zRest);
        }

        // ─── Global NFB calibration (docs/34 §2.1) ──────────────────────
        // Derive the open-loop forward gain A ANALYTICALLY from the built
        // stages' rest-point calibration quantities, then pick β so the
        // loop gain βA hits the target T and a (1+T) makeup restores the
        // linear level.  setup() runs on the AUDIO THREAD during rebuilds
        // (reroll / preset switch happen inside processBlock's graph-fade
        // window), so it must never render probe audio — the original
        // 5k-sample measurement probe was a guaranteed deadline overrun.
        // β precision is inherently non-critical: real amps' NFB depth
        // varies unit-to-unit by this much, and the guard test validates
        // the analytic estimate against the offline probe.
        nfbBeta_ = 0.0;
        nfbMakeup_ = 1.0;
        nfbState_ = 0.0;
        if (config_.nfbLoopGain > 1.0e-6)
        {
            const double A = estimateForwardGain();
            const double T = std::clamp(config_.nfbLoopGain, 0.0, 3.0);
            nfbBeta_   = (A > 1.0e-6) ? T / A : 0.0;
            nfbMakeup_ = 1.0 + T;
        }
    }

    // Analytic open-loop forward gain (chain input → output node) from the
    // stages' rest-point small-signal gains.  Interstage HPFs, Miller poles
    // and the OPT's flux-path insertion are all ≈ unity in the mid band, so
    // only the calibrated stage gains and the OPT trim enter.  Audio-thread
    // safe: no audio is rendered.
    double estimateForwardGain() const noexcept
    {
        double A = 1.0;
        for (int i = 0; i < config_.numStages; ++i)
            A *= stages_[static_cast<std::size_t>(i)].smallSignalGainNorm();
        if (config_.usePushPullOutputStage)
            A *= pushPull_.smallSignalGainNorm();
        if (config_.useInputTransformer)
            A *= std::abs(config_.inputTrafoConfig.outputGain);
        if (config_.useOutputTransformer)
            A *= std::abs(config_.outputTrafoConfig.outputGain);
        A = std::abs(A);
        return (std::isfinite(A) && A > 1.0e-6) ? A : 1.0;
    }

    // Measure the open-loop small-signal forward gain (chain input → output
    // node) with a low-level 1 kHz probe.  OFFLINE / TEST USE ONLY — this
    // renders ~5k chain samples and must never run inside an audio-thread
    // setup(); it exists to validate estimateForwardGain() and for offline
    // calibration checks.  The caller should reset() afterward.
    double measureForwardGain() noexcept
    {
        const bool savedReroll = rerollCrossfadeActive_;
        const bool savedExtPSU = externalPSUMode_;
        const double savedBeta = nfbBeta_;
        const double savedMakeup = nfbMakeup_;
        rerollCrossfadeActive_ = false;   // single path for the probe
        externalPSUMode_ = false;         // measure against the chain's own
                                          // rail, not a stale injected Vb
        nfbBeta_ = 0.0;                   // open loop
        nfbMakeup_ = 1.0;                 // unity makeup while measuring A

        const double w = 2.0 * M_PI * 1000.0 / std::max(sampleRate_, 1.0);
        // Settle must outlast the chain's SLOW physics — the 0.5 Hz DC
        // trackers, PSU reservoir charge-up, rail-ladder (~0.5 s) and the
        // OPT's flux/JA settling — or the startup transient contaminates
        // the RMS ratio (the original 1024-sample settle over-read the
        // gain by up to ~6× on some Monte Carlo draws).  Offline use only,
        // so the 3 s render cost is irrelevant.
        const int kSettle = static_cast<int>(2.5 * sampleRate_);
        const int kMeas   = static_cast<int>(0.5 * sampleRate_);
        double inSq = 0.0, outSq = 0.0;
        for (int n = 0; n < kSettle + kMeas; ++n)
        {
            const double x = 1.0e-3 * std::sin(w * static_cast<double>(n));
            const double y = process(x);
            if (n >= kSettle) { inSq += x * x; outSq += y * y; }
        }

        rerollCrossfadeActive_ = savedReroll;
        externalPSUMode_ = savedExtPSU;
        nfbBeta_ = savedBeta;
        nfbMakeup_ = savedMakeup;
        return (inSq > 1.0e-20) ? std::sqrt(outSq / inSq) : 1.0;
    }

    void reset(bool coldStart = true)
    {
        for (int i = 0; i < config_.numStages; ++i)
            stages_[static_cast<std::size_t>(i)].reset(coldStart);
        for (int i = 0; i < config_.numStages - 1; ++i)
            interstages_[static_cast<std::size_t>(i)].reset();
        for (int i = 0; i < config_.numStages; ++i)
            railNodes_[static_cast<std::size_t>(i)] =
                config_.stages[static_cast<std::size_t>(i)].Vp_nominal;
        psu_.reset();
        inputTrafo_.reset();
        outputTrafo_.reset();
        interTrafo_.reset();
        if (config_.usePushPullOutputStage)
            pushPull_.reset(coldStart);
        feedbackState_ = 0.0;
        interaction_.reset();
        nfbState_ = 0.0;
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
            rerollFromInterTrafo_ = interTrafo_;
            rerollFromPushPull_ = pushPull_;
            rerollFromFeedbackState_ = feedbackState_;
            rerollFromInteraction_ = interaction_;
            rerollFromNfbBeta_ = nfbBeta_;
            rerollFromNfbMakeup_ = nfbMakeup_;
            rerollFromNfbState_ = nfbState_;
            rerollFromExternalPSUMode_ = externalPSUMode_;
            rerollFromExternalVb_ = externalVb_;
            rerollFromLastTotalIp_ = lastTotalIp_;
            rerollFromRailNodes_ = railNodes_;
            // The ladder calibration is re-derived by setup() for the
            // NEW draw's resting currents; the crossfading old path must
            // keep stepping its rail nodes with ITS droppers.
            rerollFromDecouplingRCalib_ = decouplingRCalib_;
            rerollFromDecouplingAlphaCalib_ = decouplingAlphaCalib_;
            rerollFromInterstagePads_ = interstagePads_;
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

    /// Carry the whole chain's SLOW state from its previous incarnation
    /// after a parameter-edit rebuild (docs/34 §4.3): warmup, thermal and
    /// magnetic history, supply charge, blocking deltas and DC trackers
    /// survive an automated Bias/Drive move instead of cold-starting.
    /// Returns false (carrying nothing) if the graph SHAPE changed —
    /// different stage count / topology / power section — where old state
    /// has no physical meaning on the new circuit.
    bool carrySlowStateFrom(const TubeAmpChain& o) noexcept
    {
        
        if (o.config_.numStages != config_.numStages) return false;
        if (o.config_.usePushPullOutputStage
            != config_.usePushPullOutputStage) return false;
        for (int i = 0; i < config_.numStages; ++i)
        {
            const auto idx = static_cast<std::size_t>(i);
            if (o.config_.stages[idx].topology
                    != config_.stages[idx].topology
                || o.config_.stages[idx].enablePentodeModel
                    != config_.stages[idx].enablePentodeModel)
                return false;
        }

        for (int i = 0; i < config_.numStages; ++i)
        {
            const auto idx = static_cast<std::size_t>(i);
            stages_[idx].carrySlowStateFrom(o.stages_[idx]);
        }
        for (int i = 0; i < config_.numStages - 1; ++i)
        {
            const auto idx = static_cast<std::size_t>(i);
            interstages_[idx].carryStateFrom(o.interstages_[idx]);
        }
        if (config_.usePushPullOutputStage)
            pushPull_.carrySlowStateFrom(o.pushPull_);
        if (config_.useInputTransformer && o.config_.useInputTransformer)
            inputTrafo_.carryCoreStateFrom(o.inputTrafo_);
        if (config_.useOutputTransformer && o.config_.useOutputTransformer)
            outputTrafo_.carryCoreStateFrom(o.outputTrafo_);
        if (config_.interstageTrafoAfterStage >= 0
            && o.config_.interstageTrafoAfterStage
                   == config_.interstageTrafoAfterStage)
            interTrafo_.carryCoreStateFrom(o.interTrafo_);
        psu_.carryStateFrom(o.psu_);
        // Rail-ladder NODES are deliberately not carried: they are derived
        // state (PSU reservoir × recalibrated droppers × draw) and a
        // factory-load can re-target them far from the carried values —
        // observed as a hard step under factory+reroll stress.  The sag
        // MEMORY lives in the carried PSU reservoir; the nodes re-walk
        // from their documented rest within the ladder's own ~0.5 s.
        // The fast feedback / interaction states (feedbackState_, nfbState_,
        // interaction_) are deliberately NOT carried: they settle within
        // milliseconds anyway, and carrying them across a rebuild whose
        // β / realism coefficients were re-derived injects a step (observed
        // under A/B + TP-switch stress).  Slow state only.
        return true;
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
        auto runPath = [this](double x,
                          const TubeAmpChainConfig& cfg,
                          std::array<TubeStage, TubeAmpChainConfig::kMaxStages>& stages,
                          std::array<InterstageCoupling, TubeAmpChainConfig::kMaxStages - 1>& interstages,
                          PowerSupplySag& psu,
                          TransformerStage& inputTrafo,
                          TransformerStage& outputTrafo,
                          TransformerStage& interTrafo,
                          PushPullStage& pushPull,
                          double& feedbackState,
                          AnalogInteractionState& interaction,
                          bool externalPSUMode,
                          double externalVb,
                          double& lastTotalIp,
                          std::array<double, TubeAmpChainConfig::kMaxStages>& railNodes,
                          const std::array<double, TubeAmpChainConfig::kMaxStages>& dropperR,
                          const std::array<double, TubeAmpChainConfig::kMaxStages>& dropperAlpha,
                          const std::array<double, TubeAmpChainConfig::kMaxStages - 1>& pads,
                          double nfbBeta,
                          double nfbMakeup,
                          double& nfbState) noexcept
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

            // Real global NFB: subtract β × the PREVIOUS output-transformer
            // sample (1-sample loop delay = the physical wiring/propagation
            // delay at the oversampled rate).  Because the tap is the actual
            // output node, the loop gain follows the forward gain — it
            // collapses when the output stage clips, hardening the knee,
            // and suppresses linear-region THD / Zout by (1+T).  The (1+T)
            // makeup at the return restores the linear level (docs/34 §2.1).
            if (nfbBeta > 0.0 && std::isfinite(nfbState))
                x -= nfbBeta * nfbState;

            const double envAlpha = envAlpha_;
            const double slowAlpha = slowAlpha_;
            const double memoryAlpha = memoryAlpha_;
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

            // 2b) Rail-decoupling ladder: walk from the reservoir down to
            //     the input stage.  Each node low-passes (node above −
            //     cumulative draw × calibrated dropper R).  The input
            //     stage ends up the most ripple-filtered and the whole
            //     chain shares supply history — stages talk through the
            //     rail exactly as in the physical circuit.
            if (cfg.enableRailDecoupling && cfg.numStages > 0)
            {
                double prefixI[TubeAmpChainConfig::kMaxStages] = {};
                double acc = 0.0;
                for (int i = 0; i < cfg.numStages; ++i)
                {
                    // Plate AND screen current: both leave through the
                    // stage's B+ node.  Must match the rest-time dropper
                    // calibration, which budgets Ip + Ig2 (docs/35 D-A).
                    const auto idx2 = static_cast<std::size_t>(i);
                    acc += stages[idx2].lastPlateCurrent()
                         + stages[idx2].lastScreenCurrent();
                    prefixI[i] = acc;
                }
                double nodeAbove = Vb;
                for (int i = cfg.numStages - 1; i >= 0; --i)
                {
                    const auto idx = static_cast<std::size_t>(i);
                    if (! std::isfinite(railNodes[idx]))
                        railNodes[idx] = cfg.stages[idx].Vp_nominal;
                    const double target = nodeAbove
                        - prefixI[i] * dropperR[idx];
                    railNodes[idx] += dropperAlpha[idx]
                                    * (target - railNodes[idx]);
                    nodeAbove = railNodes[idx];
                }
            }

            // 3) Cascade stages with interstage coupling.
            double stageOutput = x;
            double totalIp     = 0.0;
            double stageMemoryDrive = 0.0;
            double stageCurrentDrive = 0.0;
            for (int i = 0; i < cfg.numStages; ++i)
            {
                const auto idx = static_cast<std::size_t>(i);
                const double VbStage =
                    (cfg.enableRailDecoupling && std::isfinite(railNodes[idx]))
                        ? std::max(0.0, railNodes[idx])
                        : Vb;
                // Hand the previous stage's instantaneous output
                // impedance to this stage's Miller input model — the
                // physical Z_source the grid actually sees (docs/04 §3.1).
                if (i > 0)
                    stages[idx].setDynamicSourceImpedance(
                        stages[static_cast<std::size_t>(i - 1)]
                            .lastOutputImpedance());
                stageOutput = stages[idx].process(stageOutput, VbStage);
                totalIp += stages[idx].lastPlateCurrent()
                         + stages[idx].lastScreenCurrent();
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
                    if (i == cfg.interstageTrafoAfterStage)
                    {
                        // Interstage IRON coupling (docs/14): the trafo's
                        // flux path does the DC blocking the RC did, and
                        // its core writes real hysteresis onto the
                        // hand-off.
                        stageOutput = interTrafo.process(stageOutput);
                    }
                    else
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
                        // Voltage-native boundary: volts → HPF → explicit
                        // pad → next grid volts (docs/34 §4.1).
                        stageOutput *= pads[idx];
                    }
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
            {
                // Hand the driving stage's instantaneous output impedance
                // to the OPT before it processes this sample (docs/34
                // §3.9): tube rp modulation modulates the iron's THD.
                outputTrafo.setSourceImpedance(
                    cfg.usePushPullOutputStage
                        ? pushPull.lastOutputImpedance()
                        : (cfg.numStages > 0
                               ? stages[static_cast<std::size_t>(
                                     cfg.numStages - 1)].lastOutputImpedance()
                               : 0.0));
                stageOutput = outputTrafo.process(stageOutput);

                // Feed the OPT's magnetizing-current demand back to the
                // stage that physically drives the primary (docs/34 §2.2).
                // One-sample delay = the same relaxation pattern as the
                // PSU rail ladder; bounded by the stages' internal clamps.
                if (cfg.enableOPTMagCoupling)
                {
                    const double dropNorm =
                        outputTrafo.lastMagnetizingDropNorm();
                    if (cfg.usePushPullOutputStage)
                        pushPull.setMagnetizingDropNorm(dropNorm);
                    else if (cfg.numStages > 0
                             && cfg.stages[static_cast<std::size_t>(
                                    cfg.numStages - 1)].plateLoadIsTransformer)
                        stages[static_cast<std::size_t>(cfg.numStages - 1)]
                            .setMagnetizingDropNorm(dropNorm);
                }
            }

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
            interaction.lfSignal = lfSigPole_ * interaction.lfSignal
                                 + (1.0 - lfSigPole_) * stageOutput;
            const double lfDrive = std::clamp(std::abs(interaction.lfSignal) * 2.4,
                                              0.0, 1.0);
            interaction.lfEnv = lfEnvPole_ * interaction.lfEnv
                              + (1.0 - lfEnvPole_) * lfDrive;
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
                // First-order fs re-map of the allpass pole (exact pow()
                // would cost a transcendental per sample).
                const double a48 = std::clamp(0.58 + 0.32 * phaseStrength,
                                              0.35, 0.95);
                const double a = std::clamp(
                    1.0 - (1.0 - a48) * rateRatio_, 0.35, 0.999);
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
                // Voicing-specific poles precomputed (fs-normalized) in
                // setup(); cfg.feedbackVoicing matches config_ for both
                // the live and the reroll-crossfade path.
                const double fbLowAlpha  = fbLowPole_;
                const double fbHighAlpha = fbHighPole_;
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

            // NFB tap: store the output node to feed back next sample
            // (pre-makeup — the makeup is the plugin's output normalisation,
            // not part of the feedback divider).  Apply the (1+T) makeup to
            // the returned value to restore the level the loop suppressed.
            nfbState = std::isfinite(stageOutput) ? stageOutput : 0.0;
            return stageOutput * nfbMakeup;
        };

        const double yNew = runPath(
            in,
            config_,
            stages_,
            interstages_,
            psu_,
            inputTrafo_,
            outputTrafo_,
            interTrafo_,
            pushPull_,
            feedbackState_,
            interaction_,
            externalPSUMode_,
            externalVb_,
            lastTotalIp_,
            railNodes_,
            decouplingRCalib_,
            decouplingAlphaCalib_,
            interstagePads_,
            nfbBeta_,
            nfbMakeup_,
            nfbState_);

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
            rerollFromInterTrafo_,
            rerollFromPushPull_,
            rerollFromFeedbackState_,
            rerollFromInteraction_,
            rerollFromExternalPSUMode_,
            rerollFromExternalVb_,
            rerollFromLastTotalIp_,
            rerollFromRailNodes_,
            rerollFromDecouplingRCalib_,
            rerollFromDecouplingAlphaCalib_,
            rerollFromInterstagePads_,
            rerollFromNfbBeta_,
            rerollFromNfbMakeup_,
            rerollFromNfbState_);

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
    /// Read-only view of the output transformer — for the OPT deep-
    /// saturation guards (docs/35 §S2 D-B).
    const TransformerStage& outputTransformer() const noexcept { return outputTrafo_; }
    int numStages() const noexcept { return config_.numStages; }

    /// Line frequency this instance's mains-derived textures (heater hum,
    /// rectifier ripple) run at.  The processor reads it so its output
    /// leakage hum matches the same 50/60 Hz region (docs/34 §1.5).
    double mainsFrequencyHz() const noexcept { return config_.mainsFrequencyHz; }

private:
    // Generate one ComponentVariation per stage. We hash the chain seed with
    // the stage index so each stage gets its own independent perturbation,
    // while the overall chain remains reproducible from a single user seed.
    void applyVariation()
    {
        variationCount_ = std::clamp(config_.numStages, 0,
                                     TubeAmpChainConfig::kMaxStages);
        for (int i = 0; i < variationCount_; ++i)
        {
            const std::uint64_t stageSeed =
                config_.variationSeed
                ^ (0x9E3779B97F4A7C15ULL * static_cast<std::uint64_t>(i + 1));
            variationCache_[static_cast<std::size_t>(i)] =
                makeVariation(stageSeed, config_.variationDistribution);
        }
    }

    TubeStageConfig configuredStage(int i) const
    {
        auto cfg = config_.stages[static_cast<std::size_t>(i)];
        // Heater hum runs at the line frequency — one field for the whole
        // instance so 50/60 Hz is consistent across heater, ripple and the
        // processor's leakage hum (docs/34 §1.5).
        cfg.heaterFrequency = config_.mainsFrequencyHz;
        // BIAS knob: direct DC shift on free suppressor grids (docs/35
        // C10).  Applied before setup so the rest solve tracks the knob.
        if (! cfg.suppressorTieToCathode && cfg.enablePentodeModel)
            cfg.suppressorBiasVolts += config_.suppressorBiasOffsetV;
        // Every stage that feeds another stage sees that stage's grid-leak
        // resistor as part of its AC plate load (docs/34 §3.8).
        cfg.nextStageLoadR = (i < config_.numStages - 1)
            ? config_.interstageRg : 0.0;
        if (i < variationCount_)
        {
            const auto& v = variationCache_[static_cast<std::size_t>(i)];
            // Apply tube + passive perturbations.
            // Use qualified name to disambiguate from member function
            // with identical base name.
            cfg.tube    = ::valvra::dsp::applyVariation(cfg.tube, v);
            cfg.pentode = ::valvra::dsp::applyVariation(cfg.pentode, v);
            cfg.Ck   *= v.Ck_scale;
            cfg.Rp   *= v.Rp_scale;   // independent draws — real units
            cfg.Rk   *= v.Rk_scale;   // never correlate plate & cathode R

            // ─── Hidden-physics per-instance perturbations ──────────────
            // Each of the six new physical mechanisms picks up a small
            // random spread so two Valvra instances loaded on different
            // tracks / channels end up with genuinely different "mojo".
            cfg.gridLeakR         *= v.gridLeakR_scale;
            // The coupling cap that FEEDS this stage IS the interstage cap
            // between stage (i-1) and i — one physical component.  Bind the
            // blocking-charge network to that same value AND the same Monte
            // Carlo tolerance draw the interstage HPF uses, so a stage no
            // longer has a different cap for its HF corner than for its
            // blocking memory (docs/34 §1.4).  Stage 0's grid is fed by the
            // source / input transformer, so it keeps its own input-coupling
            // value and independent draw.
            if (i >= 1
                && i - 1 < variationCount_)
                cfg.gridCouplingC = config_.interstageCc
                    * variationCache_[static_cast<std::size_t>(i - 1)]
                          .coupling_scale;
            else
                cfg.gridCouplingC *= v.gridCouplingC_scale;
            cfg.gridTurnOnVoltage += v.gridVon_offset;
            cfg.warmupTauSeconds  *= v.warmupTau_scale;
            cfg.heaterHumAmplitude *= v.heaterHum_scale;
            cfg.thermalBiasSensitivity *= v.thermalSens_scale;
            cfg.slewRatePositive  *= v.slewPos_scale;
            cfg.slewRateNegative  *= v.slewNeg_scale;
            cfg.soakageAmount     *= v.soakageAmt_scale;
            cfg.soakageTau        *= v.soakageTau_scale;

            // Carbon-composition excess noise (docs/34 §3.7): vintage-era
            // plate resistors add DC-bias-proportional 1/f noise on top of
            // the cathode-interface flicker — an additive contribution to
            // the stage's pink ratio, zero on Modern populations.  Capped
            // below the feel-verify micro-motion budget (docs: pumping
            // risk past ~0.9 total).
            cfg.flickerNoiseRatio = std::min(0.85,
                cfg.flickerNoiseRatio + v.excessNoise_offset);
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
    TransformerStage interTrafo_ {};
    PushPullStage    pushPull_ {};

    // Click-free reroll transition: old graph state kept for a short
    // crossfade window while the rebuilt graph ramps in.
    TubeAmpChainConfig rerollFromConfig_ {};
    std::array<TubeStage, TubeAmpChainConfig::kMaxStages> rerollFromStages_ {};
    std::array<InterstageCoupling, TubeAmpChainConfig::kMaxStages - 1> rerollFromInterstages_ {};
    PowerSupplySag   rerollFromPsu_ {};
    TransformerStage rerollFromInputTrafo_ {};
    TransformerStage rerollFromOutputTrafo_ {};
    TransformerStage rerollFromInterTrafo_ {};
    PushPullStage    rerollFromPushPull_ {};
    double rerollFromFeedbackState_      { 0.0 };
    AnalogInteractionState rerollFromInteraction_ {};
    double rerollFromNfbBeta_   { 0.0 };
    double rerollFromNfbMakeup_ { 1.0 };
    double rerollFromNfbState_  { 0.0 };
    bool   rerollFromExternalPSUMode_ { false };
    double rerollFromExternalVb_      { 325.0 };
    double rerollFromLastTotalIp_     { 0.0 };
    std::array<double, TubeAmpChainConfig::kMaxStages> rerollFromRailNodes_ {};
    std::array<double, TubeAmpChainConfig::kMaxStages> rerollFromDecouplingRCalib_ {};
    std::array<double, TubeAmpChainConfig::kMaxStages> rerollFromDecouplingAlphaCalib_ {};
    bool   rerollCrossfadeActive_     { false };
    int    rerollCrossfadePos_        { 0 };
    int    rerollCrossfadeSamples_    { 1 };

    // Fixed-size cache — the rebuild/carry/reroll paths run on the audio
    // thread, and a std::vector here was the last allocation they made
    // (docs/35 D1).  kMaxStages entries live in the object itself.
    std::array<ComponentVariation, TubeAmpChainConfig::kMaxStages>
        variationCache_ {};
    int variationCount_ { 0 };

    // External PSU (shared-rail stereo coupling) state.  When on, the
    // chain ignores its own psu_ and takes Vb from the injected value.
    bool   externalPSUMode_ { false };
    double externalVb_      { 325.0 };
    double lastTotalIp_     { 0.0 };
    double feedbackState_   { 0.0 };
    AnalogInteractionState interaction_ {};

    // Global NFB loop state (docs/34 §2.1).  nfbBeta_ = β (feedback
    // fraction, derived from the measured forward gain), nfbMakeup_ = 1+T
    // (level restoration), nfbState_ = previous output-node sample.
    double nfbBeta_   { 0.0 };
    double nfbMakeup_ { 1.0 };
    double nfbState_  { 0.0 };

    // Voltage-native inter-stage pads + setup caches (docs/34 §4.1)
    std::array<double, TubeAmpChainConfig::kMaxStages - 1> interstagePads_ {};
    std::array<double, TubeAmpChainConfig::kMaxStages> stageSwingCache_ {};
    std::array<double, TubeAmpChainConfig::kMaxStages> stageOutGainCache_ {};
    std::array<double, TubeAmpChainConfig::kMaxStages - 1>
        rerollFromInterstagePads_ {};

    // Rail-decoupling ladder: per-stage node states + setup-calibrated
    // dropper resistances / time constants (see setup()).
    std::array<double, TubeAmpChainConfig::kMaxStages> railNodes_ {};
    std::array<double, TubeAmpChainConfig::kMaxStages> decouplingRCalib_ {};
    std::array<double, TubeAmpChainConfig::kMaxStages> decouplingAlphaCalib_ {};

    // fs-normalized interaction coefficients (48 kHz reference values).
    double envAlpha_    { 0.0065 };
    double slowAlpha_   { 0.0015 };
    double memoryAlpha_ { 0.0025 };
    double lfSigPole_   { 0.9975 };
    double lfEnvPole_   { 0.997 };
    double fbLowPole_   { 0.9985 };
    double fbHighPole_  { 0.935 };
    double rateRatio_   { 1.0 };
};

} // namespace valvra::dsp
