// ─────────────────────────────────────────────────────────────────────────────
// PushPullStage — ideal phase splitter + class-AB push-pull pair
//
// What it models (the "Output Stage" of a Marshall / Vox / Fender amp):
//
//             ┌──── Vb+ rail (shared with chain via PSU sag) ────┐
//             │                                                  │
//             ▼                                                  ▼
//           Rp_a                                               Rp_b
//             │                                                  │
//        ┌────┴────┐                                        ┌────┴────┐
//        │ Power+  │                                        │ Power−  │
//        │ Triode  │ ←─ +Vg_drive  (ideal phase split)   ─→ │ Triode  │
//        └────┬────┘                                        └────┬────┘
//             │                                                  │
//             └─────────── shared cathode ───────────────────────┘
//                                  │
//                                Rk_tail  ─→  Vk floats with summed Ip
//                                  │
//                                 GND
//
// Output: the *difference* current flowing through the two halves of the
// OPT primary.  In class-A region this is a clean linear difference where
// the two tubes' even-order non-linearities cancel (signature push-pull
// cleanliness).  In class-AB cutoff one tube switches off entirely while
// the other conducts hard — that's where the characteristic odd-harmonic
// "British crunch" sound comes from.
//
// Two physics features competitors miss:
//   1) Cathode-tail coupling — Vk is solved per sample so heavy conduction
//      on one side raises Vk, which biases BOTH grids more negative.  This
//      gives the output pair its self-limiting, "soft compressed" overdrive
//      feel versus naive two-independent-triodes models.
//   2) Tube asymmetry — even a "matched pair" tube manufacturer hand-picks
//      to ±5% mismatch.  That residual mismatch leaks small-amplitude
//      even harmonics into the otherwise odd-only spectrum, giving real
//      hardware its breath.  We expose this via cfg.tubeAsymmetry.
//
// References:
//   docs/14 §14.4 (LTP topology), §14.7 (push-pull power)
//   docs/24 §B.2 (Marshall power section)
//   docs/22 §C.4 (academic data on EL34 triode-strapped)
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include "KorenTriode.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace valvra::dsp {

struct PushPullStageConfig
{
    // Power tube model — defaults to EL34 triode-strapped.  Use the
    // 6L6GC params for an American voicing.  The same model is used for
    // both halves of the pair; tubeAsymmetry below introduces the small
    // real-world mismatch.
    DempwolfParams powerTube { params::kEL34_TriodeStrapped };

    // Operating point.  EL34 typical class-AB1 is Vg_bias ≈ −36 V at
    // Vp ≈ 450 V; that puts Ip ≈ 50 mA per side at idle.  We pick numbers
    // that produce reasonable Ip swing under signal load while matching
    // typical Marshall JCM800 schematic values.
    double Vg_bias    { -36.0 };
    double Vp_nominal { 450.0 };
    double Rp_primary { 3400.0 };  ///< OPT primary impedance per side [Ω]
                                   ///  (AC reflected load — see dcrPrimary)
    double dcrPrimary { 80.0 };    ///< OPT primary winding DCR per side [Ω].
                                   ///  The transformer is an inductor: at DC
                                   ///  the plate sits only DCR·Ip below the
                                   ///  rail, while signal excursions ride
                                   ///  the Rp_primary AC load line through
                                   ///  that idle point.
    double Rk_tail    { 470.0 };   ///< Common-cathode tail resistor [Ω]

    // A real LTP phase splitter ahead of the power tubes typically delivers
    // ~30–60 V peak swing on each side at full drive (driven hard by a
    // 12AX7 stage with cathode follower).  driveScale maps the upstream
    // signal (normalized ≈ ±1.5 at full preamp drive) into volts at each
    // power-tube grid.  Larger value = harder push into class-AB cutoff.
    double driveScale { 28.0 };

    // ─── LTP phase splitter (optional, docs/20 & docs/24) ───────────────
    // When enabled, the push-pull grids are driven by a simulated
    // long-tailed-pair stage instead of an ideal anti-phase split.
    // This adds finite tail impedance, branch mismatch, and common-mode
    // leakage before the power pair.
    bool   useLtpPhaseSplitter { false };
    DempwolfParams ltpTube { params::kRSD_1 };   ///< typically 12AX7
    double ltpVgBias        { -1.6 };
    double ltpVpNominal     { 300.0 };
    double ltpRp            { 100.0e3 };
    double ltpTailR         { 47.0e3 };
    double ltpDriveVolts    { 2.2 };             ///< grid drive per +1 input
    double ltpPlateRRatio   { 1.0 };             ///< branch load mismatch
    double ltpTubeMismatch  { 0.02 };            ///< ±2% triode mismatch
    double ltpCommonModeLeak { 0.03 };           ///< finite CMRR imprint
    int    ltpSolverIters   { 2 };
    double ltpToPowerGridGain { 0.22 };          ///< plate-swing -> power-grid

    // Output normalization — the difference current Ip_pos − Ip_neg is in
    // amperes; we convert to a normalized audio sample using this gain.
    // setup() recalculates it from the resting-current envelope so the
    // limit at full drive lands near ±1.0 sample.
    double outputGainLinear { 1.0 };

    // Manufacturing mismatch between the two tubes — even a "matched pair"
    // is only ±2–5% on real benches.  ±0 means dead-matched (perfect even-
    // harmonic null), 0.05 means ±5% spread on G/mu/gamma between halves.
    double tubeAsymmetry { 0.025 };

    // ─── Cathode-tail Newton-Raphson solver ───────────────────────────────
    // Set false to use the simplified "fixed Vk" model (faster, less
    // authentic).  When true (default), Vk is solved per-sample so the
    // tail resistor genuinely couples the two halves the way the real
    // hardware does.  Convergence is 1–2 iterations from the warm-started
    // last-sample value, so the per-sample cost is bounded.
    bool   solveTailCoupling { true };
    int    tailSolverIters   { 2 };

    // ─── Coupled OPT differential load (docs/34 §2.3, docs/04 §7) ─────────
    // A push-pull OPT's two half-primaries share one flux, so each plate's
    // AC voltage is set by the DIFFERENTIAL current through the quarter
    // impedance:  v_half = (Raa/4)·(i₁ − i₂).  With both tubes conducting
    // (class A) this reduces EXACTLY to the legacy per-side Raa/2 load
    // line; when one side cuts off, the survivor's load line kinks to
    // Raa/4 (double current slope — the class-AB crunch signature) and the
    // cut-off plate is driven ABOVE the rail by the partner's swing (the
    // real 2×B+ flyback), all emergent from the one constraint.
    // Rp_primary keeps its historical meaning (class-A per-side load =
    // Raa/2), so small-signal calibration is bit-compatible.
    bool   coupleOPTDifferential { true };

    // ─── Ultralinear screen tap (docs/14, docs/34 v2) ────────────────────
    // The screen rides an OPT tap at this fraction of the plate swing:
    // 1.0 ≡ triode-strap (bit-compatible default), 0.43 = the classic UL
    // tap, → 0 approaches pentode stiffness.  The (triode-strap-fitted)
    // device law is evaluated at the TAP voltage, so ∂Ip/∂Vp picks up the
    // tap factor by the chain rule and the effective plate resistance
    // lands between triode and pentode — the defining UL property —
    // with no separate pentode model needed.
    double ulTapRatio { 1.0 };

    // ─── Power-tube grid conduction / blocking (docs/34 §2.4) ─────────────
    // The splitter drives each power grid through a coupling cap + grid
    // stopper, with a grid-leak (bias-feed) resistor to the bias supply.
    // When overdrive swings a grid to conduction, the grid current charges
    // the coupling cap, pushing that side's bias negative for
    // τ ≈ R_leak·C — the crossover shift / duty-cycle walk / "blocking
    // fart" of a cranked push-pull amp, and the dominant recovery physics
    // above the class-AB knee.  Values follow JCM800 practice.
    bool   enablePowerGridConduction { true };
    double gridCouplingC     { 22.0e-9 };   ///< splitter→grid coupling [F]
    double gridLeakR         { 220.0e3 };   ///< grid-leak / bias feed [Ω]
    double gridStopperR      { 5.6e3 };     ///< power-grid stopper [Ω]
    double gridTurnOnVoltage { 0.5 };       ///< g-k contact-potential shift
    double gridDriveSourceZ  { 40.0e3 };    ///< splitter Zout fallback when
                                            ///  the LTP splitter is off

    // Thermal warmup (the power section warms up over ≈ 30 s).  Used the
    // same way as TubeStage: gm linearly scales from 0.85 → 1.0.
    bool   enableWarmup     { true };
    double warmupTauSeconds { 30.0 };

    // Plate-dissipation thermal drift — the power section is where "the
    // amp sits down" actually happens in real hardware (25 W anodes get
    // HOT).  A slow envelope of relative plate dissipation drags the
    // bias more negative over tens of seconds.
    bool   enableThermalDrift     { true };
    double thermalTauSeconds      { 15.0 };
    double thermalBiasVoltsPerRel { 0.8 };  ///< ΔVg per +100% dissipation
};

// ─────────────────────────────────────────────────────────────────────────────
// PushPullStage — drop-in chain stage that takes one input and returns one
// output, internally simulating an ideal phase split + PP pair.  Mirrors TubeStage's
// public interface (process / reset / lastPlateCurrent / etc.) so the
// chain can plug it in as the final stage before the OPT.
// ─────────────────────────────────────────────────────────────────────────────
class PushPullStage
{
public:
    PushPullStage() = default;

    void setup(const PushPullStageConfig& cfg, double sampleRate)
    {
        config_     = cfg;
        sampleRate_ = sampleRate;

        // Apply small mismatch to the two halves of the pair.  Pos side
        // gets +asymmetry on G and −asymmetry/2 on mu; neg side gets the
        // opposite.  This produces the characteristic small even-harmonic
        // bleed in real PP amps without being a full Monte Carlo path.
        DempwolfParams pPos = cfg.powerTube;
        DempwolfParams pNeg = cfg.powerTube;
        const double a = cfg.tubeAsymmetry;
        pPos.G  *= (1.0 + a);
        pNeg.G  *= (1.0 - a);
        pPos.mu *= (1.0 - a * 0.5);
        pNeg.mu *= (1.0 + a * 0.5);
        tubePos_.setParams(pPos);
        tubeNeg_.setParams(pNeg);

        // Resting plate currents WITH the OPT-primary load line: each
        // plate sits Rp_primary·Ip below the rail (the legacy code never
        // used Rp_primary at all — the plates were pinned to B+ and the
        // reflected OPT load had zero effect on the transfer).
        Ip_rest_pos_ = solveRestSide(tubePos_, cfg, VpPosLast_);
        Ip_rest_neg_ = solveRestSide(tubeNeg_, cfg, VpNegLast_);

        // Small-signal calibration makeup: the load line divides the pair
        // gain by (1 + Rp_primary·rpInv) and the loaded rest point has a
        // different gm, so restore the legacy diff-current gain ratio to
        // keep the chain's empirically-tuned levels intact.
        {
            const auto dLeg = tubePos_.evalWithDerivatives(
                cfg.Vp_nominal, cfg.Vg_bias);
            const auto dNew = tubePos_.evalWithDerivatives(
                VpPosLast_, cfg.Vg_bias);
            const double rTot = cfg.Rp_primary + std::max(cfg.dcrPrimary, 0.0);
            const double gNew = dNew.gm
                / (1.0 + rTot * std::max(0.0, dNew.rpInv));
            loadLineMakeup_ = (gNew > 1.0e-9)
                ? std::clamp(dLeg.gm / gNew, 1.0, 100.0)
                : 1.0;
        }

        setupLtpPhaseSplitter();

        // Resting tail voltage.  Project convention: cfg.Vg_bias is the
        // RESTING Vgk, so the grids are referenced to VkRest_ + Vg_bias —
        // without this the 470 Ω tail drop (tens of volts at power-tube
        // currents) silently shifted the pair toward cutoff and the
        // documented operating class was never reached.
        VkRest_  = (Ip_rest_pos_ + Ip_rest_neg_) * cfg.Rk_tail;
        Vk_last_ = VkRest_;

        // Thermal-drift envelope (relative plate dissipation).
        thermalAlpha_ = std::exp(
            -1.0 / (std::max(cfg.thermalTauSeconds, 0.1) * sampleRate));
        dissRest_ = std::max(1.0e-6,
            VpPosLast_ * Ip_rest_pos_ + VpNegLast_ * Ip_rest_neg_);
        dissAvg_ = dissRest_;

        // Output normalization — at moderate drive each tube swings ±50 mA
        // worst case; the difference therefore swings ±100 mA.  Scale so
        // that ≈ 100 mA difference current → ±1.0 sample output.  Caller
        // can override via cfg.outputGainLinear.
        const double idleSum = std::max(Ip_rest_pos_ + Ip_rest_neg_, 1.0e-6);
        normalizer_ = idleSum * 2.0;  // ≈ peak diff current at full drive

        // Warmup envelope (matches TubeStage convention)
        warmupAlpha_ = cfg.enableWarmup
            ? std::exp(-1.0 / (cfg.warmupTauSeconds * sampleRate))
            : 0.0;
        warmupCurrent_ = cfg.enableWarmup ? 0.85 : 1.0;

        // 0.5 Hz output-DC tracker, sample-rate independent.
        dcLeakAlpha_ = std::exp(
            -2.0 * 3.14159265358979323846 * 0.5 / sampleRate);

        // Prime the DC tracker to the RESTING normalized output.  A
        // mismatched pair (tubeAsymmetry, per-half bias trim) idles with
        // a non-zero plate-current difference, so the resting yNorm is
        // non-zero.  Starting outputDC_ at 0 would emit that full offset
        // as a step the 0.5 Hz tracker needs ~2 s to remove — and the
        // downstream flux transformer (whose DC rejection has its own
        // ~10 ms corner) would pass it as a large startup level.  Priming
        // here makes the first sample sit at AC-zero.
        outputDC_ = (config_.outputGainLinear * loadLineMakeup_
                     * (Ip_rest_pos_ - Ip_rest_neg_)) / normalizer_;

        lastIp_ = Ip_rest_pos_ + Ip_rest_neg_;

        // Coupled-OPT / magnetizing / power-grid state (docs/34 W3).
        setupPowerCouplingState();
    }

    void reset(bool coldStart = true) noexcept
    {
        Vk_last_       = VkRest_;
        dissAvg_       = dissRest_;
        ltpVkLast_     = ltpVkRest_;
        ltpVpPosLast_  = ltpOutPosRest_;
        ltpVpNegLast_  = ltpOutNegRest_;
        ipAcPosLast_   = 0.0;
        ipAcNegLast_   = 0.0;
        iMagA_         = 0.0;
        gridChargeFastPos_ = gridChargeRestV_ * (25.0 / 43.0);
        gridChargeSlowPos_ = gridChargeRestV_ * (18.0 / 43.0);
        gridChargeFastNeg_ = gridChargeRestV_ * (25.0 / 43.0);
        gridChargeSlowNeg_ = gridChargeRestV_ * (18.0 / 43.0);
        warmupCurrent_ = (coldStart && config_.enableWarmup) ? 0.85 : 1.0;
        lastIp_        = Ip_rest_pos_ + Ip_rest_neg_;
        // Prime to the resting normalized output (see setup()) so a
        // mismatched pair's idle offset is not emitted as a startup step.
        outputDC_      = (config_.outputGainLinear * loadLineMakeup_
                          * (Ip_rest_pos_ - Ip_rest_neg_)) / normalizer_;
    }

    /// Carry the SLOW state from a previous incarnation after a parameter-
    /// edit rebuild (docs/34 §4.3) — mirrors TubeStage::carrySlowStateFrom.
    void carrySlowStateFrom(const PushPullStage& o) noexcept
    {
        auto fin = [](double v, double fb)
        { return std::isfinite(v) ? v : fb; };

        warmupCurrent_ = std::clamp(fin(o.warmupCurrent_, 1.0), 0.5, 1.0);
        dissAvg_ = dissRest_ * std::clamp(
            fin(o.dissAvg_ / std::max(o.dissRest_, 1.0e-9), 1.0), 0.2, 5.0);
        Vk_last_ = VkRest_ + fin(o.Vk_last_ - o.VkRest_, 0.0);
        ltpVkLast_ = ltpVkRest_ + fin(o.ltpVkLast_ - o.ltpVkRest_, 0.0);

        const double oF = o.gridChargeRestV_ * (25.0 / 43.0);
        const double oS = o.gridChargeRestV_ * (18.0 / 43.0);
        const double nF = gridChargeRestV_ * (25.0 / 43.0);
        const double nS = gridChargeRestV_ * (18.0 / 43.0);
        gridChargeFastPos_ = std::max(0.0,
            nF + fin(o.gridChargeFastPos_ - oF, 0.0));
        gridChargeSlowPos_ = std::max(0.0,
            nS + fin(o.gridChargeSlowPos_ - oS, 0.0));
        gridChargeFastNeg_ = std::max(0.0,
            nF + fin(o.gridChargeFastNeg_ - oF, 0.0));
        gridChargeSlowNeg_ = std::max(0.0,
            nS + fin(o.gridChargeSlowNeg_ - oS, 0.0));

        const double oPrime = (o.config_.outputGainLinear * o.loadLineMakeup_
            * (o.Ip_rest_pos_ - o.Ip_rest_neg_))
            / std::max(o.normalizer_, 1.0e-12);
        const double nPrime = (config_.outputGainLinear * loadLineMakeup_
            * (Ip_rest_pos_ - Ip_rest_neg_))
            / std::max(normalizer_, 1.0e-12);
        outputDC_ = nPrime + fin(o.outputDC_ - oPrime, 0.0);
        iMagA_ = fin(o.iMagA_, 0.0);
    }

    /// Re-arm the warmup envelope (called by chain when user clicks Warmup).
    void simulateWarmup() noexcept
    {
        if (config_.enableWarmup) warmupCurrent_ = 0.85;
    }

    /// Hot-update the power-tube model without touching state variables.
    /// Used by the chain on Monte Carlo reroll so the PP stage's character
    /// shifts in lock-step with the preamp's reroll instead of waiting for
    /// the next full chain rebuild — keeps the user feedback snappy.
    void setTubeParams(const DempwolfParams& base) noexcept
    {
        DempwolfParams pPos = base;
        DempwolfParams pNeg = base;
        const double a = config_.tubeAsymmetry;
        pPos.G  *= (1.0 + a);
        pNeg.G  *= (1.0 - a);
        pPos.mu *= (1.0 - a * 0.5);
        pNeg.mu *= (1.0 + a * 0.5);
        tubePos_.setParams(pPos);
        tubeNeg_.setParams(pNeg);

        // Resting currents must be recomputed for the new tube curve
        // (load-line solve, matching setup()).
        Ip_rest_pos_ = solveRestSide(tubePos_, config_, VpPosLast_);
        Ip_rest_neg_ = solveRestSide(tubeNeg_, config_, VpNegLast_);
        const double idleSum = std::max(Ip_rest_pos_ + Ip_rest_neg_, 1.0e-6);
        normalizer_ = idleSum * 2.0;

        // The coupling constants track the rest point (de-embed share,
        // grid-leak equilibrium) — refresh them for the new curve, same
        // as setup() does.  Re-priming the grid charges on a tube swap is
        // physical: fresh tubes re-settle their coupling caps.
        setupPowerCouplingState();
    }

    // Process one audio sample.
    //   inputSample : normalized audio (≈ ±1.5 at full preamp drive)
    //   Vb_plus     : current B+ rail voltage (from PSU sag)
    double process(double inputSample, double Vb_plus) noexcept
    {
        if (! std::isfinite(inputSample) || ! std::isfinite(Vb_plus)
                                         || ! std::isfinite(Vk_last_)
                                         || ! std::isfinite(VpPosLast_)
                                         || ! std::isfinite(VpNegLast_)
                                         || ! std::isfinite(warmupCurrent_))
        {
            Vk_last_       = VkRest_;
            VpPosLast_     = config_.Vp_nominal * 0.9;
            VpNegLast_     = config_.Vp_nominal * 0.9;
            warmupCurrent_ = 1.0;
            // Recover to the PRIMED resting offset (mirrors setup()/reset()),
            // not 0 — a mismatched pair idles with a non-zero normalized
            // output, so re-zeroing here would re-emit that idle step (and
            // the downstream flux OPT would pass it) after a NaN event.
            outputDC_      = (config_.outputGainLinear * loadLineMakeup_
                              * (Ip_rest_pos_ - Ip_rest_neg_)) / normalizer_;
            // Restore the thermal-drift envelope too; leaving it NaN would
            // silently disable power-stage drift until the next reset().
            dissAvg_       = dissRest_;
            lastIp_        = Ip_rest_pos_ + Ip_rest_neg_;
            // Coupled-OPT / magnetizing / blocking state back to rest.
            ipAcPosLast_   = 0.0;
            ipAcNegLast_   = 0.0;
            iMagA_         = 0.0;
            gridChargeFastPos_ = gridChargeRestV_ * (25.0 / 43.0);
            gridChargeSlowPos_ = gridChargeRestV_ * (18.0 / 43.0);
            gridChargeFastNeg_ = gridChargeRestV_ * (25.0 / 43.0);
            gridChargeSlowNeg_ = gridChargeRestV_ * (18.0 / 43.0);
            return 0.0;
        }

        // 1) Slow warmup envelope
        if (config_.enableWarmup)
            warmupCurrent_ =
                warmupAlpha_ * warmupCurrent_ + (1.0 - warmupAlpha_) * 1.0;

        // 2) Phase-split the input into two anti-phase grid drive voltages.
        //    Each side's bias = Vg_bias + phase-split drive.  In v1 this
        //    can be either ideal anti-phase or an explicit long-tailed-pair
        //    solver (docs/24).
        double Vg_pos_external = VkRest_ + config_.Vg_bias;
        double Vg_neg_external = VkRest_ + config_.Vg_bias;

        // Slow thermal compression: sustained dissipation above idle
        // drags both grids more negative (the power section "sits down").
        if (config_.enableThermalDrift)
        {
            const double rel = std::max(0.0, dissAvg_ / dissRest_ - 1.0);
            const double drift = config_.thermalBiasVoltsPerRel * rel;
            Vg_pos_external -= drift;
            Vg_neg_external -= drift;
        }
        if (config_.useLtpPhaseSplitter)
        {
            const auto split = processLtpPhaseSplitter(inputSample);
            Vg_pos_external += split.first;
            Vg_neg_external += split.second;
        }
        else
        {
            const double drive = inputSample * config_.driveScale;
            Vg_pos_external += drive;
            Vg_neg_external -= drive;
        }

        // 2b) Power-grid conduction / blocking (docs/34 §2.4).  Each grid
        //     sees the splitter through its coupling cap: conduction on
        //     overdriven positive peaks charges the cap (via the stopper +
        //     splitter source impedance), imposing a negative bias shift
        //     that recovers over R_leak·C — per side, so the crossover
        //     point WALKS with the program (duty-cycle shift).  Uses the
        //     one-sample-stale tail voltage for Vgk (same convention as
        //     TubeStage's cathode-follower conduction; error is a few mV).
        if (config_.enablePowerGridConduction)
        {
            // Per-side stale plate voltages feed the Ig division region
            // (docs/34 §3.1): a power plate bottoming out under extreme
            // drive multiplies that side's grid conduction — the blocking
            // bites hardest exactly where the real amp "farts out".
            Vg_pos_external = applyPowerGridNetwork(
                Vg_pos_external, tubePos_,
                gridChargeFastPos_, gridChargeSlowPos_, VpPosLast_);
            Vg_neg_external = applyPowerGridNetwork(
                Vg_neg_external, tubeNeg_,
                gridChargeFastNeg_, gridChargeSlowNeg_, VpNegLast_);
        }

        // 3) Solve for the shared cathode voltage Vk so that
        //    Ip_pos(Vp, Vg_pos_ext − Vk) + Ip_neg(Vp, Vg_neg_ext − Vk)
        //      = Vk / Rk_tail
        //
        //    Newton-Raphson converges in 1–2 iterations from the warm
        //    start (Vk barely moves between samples).  We use analytical
        //    gm via KorenTriode::evalWithDerivatives — one call returns
        //    Ip and ∂Ip/∂Vg in a single pass, replacing what was 6
        //    plateCurrent calls per iteration with 2 (a ~3× speedup on
        //    the chain's dominant hotspot).
        double Vk = Vk_last_;
        double Ip_pos = 0.0, Ip_neg = 0.0;

        // Per-side plate load-line Newton.  The OPT's standing flux holds
        // the DC point at Vb − DCR·Ip_rest; signal excursions ride the
        // AC reflected load through that point.
        //
        // Coupled-OPT form (docs/34 §2.3): the two half-primaries share one
        // flux, so each plate's AC drop is set by the DIFFERENTIAL current
        // through the quarter impedance rQ = Raa/4 = Rp_primary/2:
        //   Vp₁ = Vb − DCR·i₁ − rQ·(i₁_ac − i₂_ac)
        //       = [Vb + rQ·(i₁_rest + i₂_ac)] − (rQ + DCR)·i₁
        // With symmetric class-A drive (i₂_ac = −i₁_ac) this is EXACTLY the
        // legacy per-side Raa/2 load line (calibration preserved); with the
        // partner cut off (i₂_ac pinned at −i₂_rest) the slope kinks to
        // Raa/4 and the partner's plate rides ABOVE the rail on the
        // survivor's swing — the physical class-AB transition + flyback.
        // The partner current enters via Gauss–Seidel (fresh within the
        // tail iteration, warm-started across samples); the partner-
        // coupling contraction factor rQ·rpInv/(1 + rpInv·(rQ+DCR)) is
        // ≈ 0.6 at the EL34/3.4k rest point — convergent, and the warm
        // start leaves only a residual step per sample at audio rate.
        //
        // The magnetizing-current injection iMagSide (docs/34 §2.2) enters
        // the node KCL as an extra current the tube must source:
        //   f(Vp) = Ip(Vp,Vgk) − iMagSide + (Vp − vOpen)/rTot = 0.
        const bool coupled = config_.coupleOPTDifferential;
        const double rQ = coupled
            ? std::max(0.5 * config_.Rp_primary, 1.0)
            : std::max(config_.Rp_primary, 1.0);
        auto solveSide = [this, Vb_plus, rQ](const KorenTriode& tube,
                                             double Vgk, double ipRestSide,
                                             double iPartnerAc,
                                             double iMagSide,
                                             double& VpWarm)
        {
            const double rDc = std::max(config_.dcrPrimary, 0.0);
            // Effective Thevenin source for this side's plate node.
            const double vOpen = Vb_plus + rQ * (ipRestSide + iPartnerAc);
            const double rTot  = rQ + rDc;
            const double gLoad = 1.0 / rTot;
            const double vMax  = std::max(1.0, 2.0 * Vb_plus);
            double Vp = std::isfinite(VpWarm)
                ? std::clamp(VpWarm, 0.0, vMax)
                : Vb_plus;
            // ONE Newton step per call: the tail loop calls this twice
            // per side, so the joint (Vk, Vp_pos, Vp_neg) system still
            // gets two combined relaxation passes per sample from warm
            // starts.  The converged current is read from the circuit
            // side (exact KCL at the solved node, no extra device eval).
            const double ul = std::clamp(config_.ulTapRatio, 0.05, 1.0);
            const double vTap = Vb_plus - ul * (Vb_plus - Vp);
            KorenTriode::IpDerivatives d
                = tube.evalWithDerivatives(vTap, Vgk);
            d.rpInv *= ul;   // chain rule: ∂Ip/∂Vp through the tap
            {
                const double f  = d.Ip - iMagSide + (Vp - vOpen) * gLoad;
                const double fp = d.rpInv + gLoad;
                Vp -= f / fp;
                if (! std::isfinite(Vp)) Vp = Vb_plus;
                // The inductive load lets the plate swing ABOVE the rail
                // on the cutoff half — clamp only against runaway.
                Vp = std::clamp(Vp, 0.0, vMax);
            }
            VpWarm = Vp;
            d.Ip = std::max(0.0, (vOpen - Vp) * gLoad + iMagSide);
            return d;
        };

        const double iMagPos = coupled ?  0.5 * iMagA_ : 0.0;
        const double iMagNeg = coupled ? -0.5 * iMagA_ : 0.0;
        double iAcPos = std::isfinite(ipAcPosLast_) ? ipAcPosLast_ : 0.0;
        double iAcNeg = std::isfinite(ipAcNegLast_) ? ipAcNegLast_ : 0.0;
        if (! coupled) { iAcPos = 0.0; iAcNeg = 0.0; }
        double rpInvPosOut = 0.0, rpInvNegOut = 0.0;

        if (config_.solveTailCoupling)
        {
            constexpr double kTinyR  = 1.0e-6;
            const double oneOverRk =
                1.0 / std::max(config_.Rk_tail, kTinyR);
            const double rDc = std::max(config_.dcrPrimary, 0.0);

            for (int iter = 0; iter < config_.tailSolverIters; ++iter)
            {
                const auto pos = solveSide(tubePos_, Vg_pos_external - Vk,
                                           Ip_rest_pos_, iAcNeg, iMagPos,
                                           VpPosLast_);
                if (coupled) iAcPos = pos.Ip - Ip_rest_pos_;
                const auto neg = solveSide(tubeNeg_, Vg_neg_external - Vk,
                                           Ip_rest_neg_, iAcPos, iMagNeg,
                                           VpNegLast_);
                if (coupled) iAcNeg = neg.Ip - Ip_rest_neg_;
                Ip_pos = pos.Ip;
                Ip_neg = neg.Ip;
                rpInvPosOut = std::max(0.0, pos.rpInv);
                rpInvNegOut = std::max(0.0, neg.rpInv);
                const double f = Ip_pos + Ip_neg - Vk * oneOverRk;

                // f' wrt Vk.  Coupled OPT: a common-mode current change
                // leaves the differential term untouched, so the pair sees
                // only the winding DCR — the tail responds with nearly the
                // full gm (physically why PP supply current follows the
                // envelope).  Legacy: per-side degenerated slope.
                double gmP, gmN;
                if (coupled)
                {
                    gmP = pos.gm / (1.0 + rDc * std::max(0.0, pos.rpInv));
                    gmN = neg.gm / (1.0 + rDc * std::max(0.0, neg.rpInv));
                }
                else
                {
                    gmP = pos.gm
                        / (1.0 + config_.Rp_primary * std::max(0.0, pos.rpInv));
                    gmN = neg.gm
                        / (1.0 + config_.Rp_primary * std::max(0.0, neg.rpInv));
                }
                const double fprime = -gmP - gmN - oneOverRk;

                if (std::abs(fprime) > 1.0e-12)
                    Vk -= f / fprime;
                if (! std::isfinite(Vk)) { Vk = Vk_last_; break; }
            }
            Vk_last_ = Vk;
        }
        else
        {
            // Fixed-Vk simplification — useful for unit tests where we
            // want to isolate the PP nonlinearity from the tail coupling.
            Vk = Vk_last_;
            for (int iter = 0; iter < 2; ++iter)
            {
                const auto pos = solveSide(tubePos_, Vg_pos_external - Vk,
                                           Ip_rest_pos_, iAcNeg, iMagPos,
                                           VpPosLast_);
                if (coupled) iAcPos = pos.Ip - Ip_rest_pos_;
                const auto neg = solveSide(tubeNeg_, Vg_neg_external - Vk,
                                           Ip_rest_neg_, iAcPos, iMagNeg,
                                           VpNegLast_);
                if (coupled) iAcNeg = neg.Ip - Ip_rest_neg_;
                Ip_pos = pos.Ip;
                Ip_neg = neg.Ip;
                rpInvPosOut = std::max(0.0, pos.rpInv);
                rpInvNegOut = std::max(0.0, neg.rpInv);
                if (! coupled) break;   // legacy: sides independent, 1 pass
            }
        }
        ipAcPosLast_ = iAcPos;
        ipAcNegLast_ = iAcNeg;
        lastRpInvAvg_ = 0.5 * (rpInvPosOut + rpInvNegOut);

        // 4) Apply warmup gm scaling (cold tubes deliver less current).
        Ip_pos *= warmupCurrent_;
        Ip_neg *= warmupCurrent_;

        // 5) The OPT sees the difference of the two plate currents.  In
        //    class-A region this is approximately linear (each tube's
        //    even-harmonic contribution cancels in the subtraction); in
        //    class-AB cutoff one current goes to zero while the other
        //    keeps growing, producing the asymmetric clipping that
        //    creates the odd-harmonic-rich British crunch.
        //
        //    Magnetizing de-embed (docs/34 §2.2): the OPT already models
        //    the LINEAR share of its magnetizing drop internally, so the
        //    linear response the injection adds to the diff is subtracted
        //    back out here (alphaMagRest_ = the rest-point current-divider
        //    share the tubes supply).  Net effect in the linear regime is
        //    zero — OPT calibration bit-preserved — while the NONLINEAR
        //    deviation (tubes failing to supply the iron under clip / at
        //    the load-line extremes) passes through as new, physically
        //    placed compression.  The tubes' internal stress (current,
        //    dissipation, PSU draw, tail shift) always carries the full
        //    magnetizing demand.
        const double diff = (Ip_pos - Ip_neg)
                          - warmupCurrent_ * alphaMagRest_ * iMagA_;
        lastIp_ = Ip_pos + Ip_neg;  // sum used by PSU sag

        // Advance the dissipation envelope (uses the solved per-side
        // plate voltages, so it is true V·I anode power, not |I| alone).
        if (config_.enableThermalDrift)
        {
            const double diss = std::max(0.0,
                VpPosLast_ * Ip_pos + VpNegLast_ * Ip_neg);
            dissAvg_ = thermalAlpha_ * dissAvg_
                     + (1.0 - thermalAlpha_) * diss;
        }

        // 6) Normalize and AC-couple (slow DC tracker mirrors TubeStage,
        //    0.5 Hz corner independent of the oversampled rate).
        //    loadLineMakeup_ restores the legacy small-signal calibration
        //    (see setup()) so the load line changes curvature, not level.
        double yNorm = (config_.outputGainLinear * loadLineMakeup_ * diff)
                     / normalizer_;
        outputDC_ = dcLeakAlpha_ * outputDC_ + (1.0 - dcLeakAlpha_) * yNorm;
        return yNorm - outputDC_;
    }

    /// Chain hook (docs/34 §2.2): normalized magnetizing-current drop
    /// reported by the output transformer for the LAST sample.  Converted
    /// internally to primary differential amperes; each side's plate KCL
    /// then sources ±half of it next sample.  Clamped to the pair's idle
    /// sum — a saturating core can demand at most order-of-idle current
    /// before the tubes' own load lines run out.
    void setMagnetizingDropNorm(double dropNorm) noexcept
    {
        if (! std::isfinite(dropNorm)) { iMagA_ = 0.0; return; }
        const double k = config_.outputGainLinear * loadLineMakeup_;
        const double iA = (k > 1.0e-9)
            ? dropNorm * normalizer_ / k : 0.0;
        const double cap = Ip_rest_pos_ + Ip_rest_neg_;
        iMagA_ = std::clamp(iA, -cap, cap);
    }

    /// Normalized small-signal in→out gain at rest, assembled from the
    /// same setup-time quantities the level calibration uses.  Used by the
    /// chain to derive the global-NFB β analytically — an audio-thread-
    /// safe replacement for the sample probe (docs/34 §2.1): a rebuild
    /// happens inside processBlock, so setup() must never render audio.
    double smallSignalGainNorm() const noexcept
    {
        // Antisymmetric grid drive ±δVg → per-side δi = gm·δVg/(1+rpInv·Z)
        // where Z is the per-side differential load (identical for the
        // coupled and legacy forms: 2rQ+DCR = Rp_primary+DCR).
        const double zDiff = std::max(config_.Rp_primary, 1.0)
                           + std::max(config_.dcrPrimary, 0.0);
        const double ul = std::clamp(config_.ulTapRatio, 0.05, 1.0);
        const double vb = config_.Vp_nominal;
        const auto dP = tubePos_.evalWithDerivatives(
            vb - ul * (vb - VpPosLast_), config_.Vg_bias);
        const auto dN = tubeNeg_.evalWithDerivatives(
            vb - ul * (vb - VpNegLast_), config_.Vg_bias);
        const double gmEff =
            dP.gm / (1.0 + std::max(0.0, dP.rpInv) * ul * zDiff)
          + dN.gm / (1.0 + std::max(0.0, dN.rpInv) * ul * zDiff);

        // Grid volts per unit normalized input.
        const double gridVoltsPerUnit = config_.useLtpPhaseSplitter
            ? config_.ltpDriveVolts * ltpAvRest_
              * std::max(0.0, config_.ltpToPowerGridGain) * ltpMakeup_
            : config_.driveScale;

        return gridVoltsPerUnit * gmEff
             * config_.outputGainLinear * loadLineMakeup_ / normalizer_;
    }

    // Diagnostics
    double lastPlateCurrent() const noexcept { return lastIp_; }
    double restingPlateCurrent() const noexcept
    {
        return Ip_rest_pos_ + Ip_rest_neg_;
    }
    double warmupProgress() const noexcept { return warmupCurrent_; }
    double currentVk() const noexcept { return Vk_last_; }
    double posPlateVoltage() const noexcept { return VpPosLast_; }
    double negPlateVoltage() const noexcept { return VpNegLast_; }
    /// Differential source impedance the OPT primary sees (≈ the pair's
    /// average plate resistance at the instantaneous operating point).
    /// The chain feeds this to the output transformer's dynamic
    /// magnetizing-drop scaling (docs/34 §3.9); rises toward cutoff.
    double lastOutputImpedance() const noexcept
    {
        return 1.0 / std::max(lastRpInvAvg_, 1.0e-6);
    }
    /// Total blocking charge (max of the two sides) [V] — UI / tests.
    double blockingChargeVolts() const noexcept
    {
        const double p = gridChargeFastPos_ + gridChargeSlowPos_;
        const double n = gridChargeFastNeg_ + gridChargeSlowNeg_;
        return std::max(0.0, std::max(p, n));
    }

    /// Resting current imbalance between the two halves, normalized by
    /// the idle sum.  A real PP pair's mismatch leaves a net DC
    /// magnetization on the OPT primary — the chain feeds this into the
    /// output transformer as a standing H offset so the core saturates
    /// asymmetrically (docs/02 §6: the origin of PP "punch").
    double restingImbalanceRatio() const noexcept
    {
        const double sum = std::max(Ip_rest_pos_ + Ip_rest_neg_, 1.0e-9);
        return (Ip_rest_pos_ - Ip_rest_neg_) / sum;
    }

private:
    // Per-side rest solve.  At DC the OPT primary is just its winding
    // resistance, so the idle point uses dcrPrimary — the AC reflected
    // load only shapes signal excursions around it (solveSide).
    double solveRestSide(const KorenTriode& tube,
                         const PushPullStageConfig& cfg,
                         double& VpRestOut) noexcept
    {
        const double rDc = std::max(cfg.dcrPrimary, 0.0);
        const double ul = std::clamp(cfg.ulTapRatio, 0.05, 1.0);
        double Vp = cfg.Vp_nominal * 0.95;
        KorenTriode::IpDerivatives d {};
        for (int it = 0; it < 24; ++it)
        {
            const double vTap = cfg.Vp_nominal - ul * (cfg.Vp_nominal - Vp);
            d = tube.evalWithDerivatives(vTap, cfg.Vg_bias);
            const double f  = Vp - cfg.Vp_nominal + rDc * d.Ip;
            const double fp = 1.0 + rDc * d.rpInv * ul;
            Vp -= f / fp;
            if (! std::isfinite(Vp)) { Vp = cfg.Vp_nominal * 0.95; break; }
            Vp = std::clamp(Vp, 1.0, cfg.Vp_nominal);
        }
        VpRestOut = Vp;
        return std::max(0.0, tube.plateCurrent(
            cfg.Vp_nominal - ul * (cfg.Vp_nominal - Vp), cfg.Vg_bias));
    }
    // Power-grid conduction + two-branch blocking memory for ONE side
    // (docs/34 §2.4; same continuous Dempwolf Ig law and charge topology as
    // TubeStage's preamp grid network).  Returns the loaded/blocked grid
    // drive the tube actually sees.  Uses the one-sample-stale tail voltage
    // for Vgk — the tail moves millivolts per sample at audio rate.
    double applyPowerGridNetwork(double VgExt, const KorenTriode& tube,
                                 double& fastV, double& slowV,
                                 double VpPrevSide) noexcept
    {
        if (! std::isfinite(fastV) || ! std::isfinite(slowV))
        {
            fastV = gridChargeRestV_ * (25.0 / 43.0);
            slowV = gridChargeRestV_ * (18.0 / 43.0);
        }
        const double chargeV = fastV + slowV - gridChargeRestV_;
        const double VgLoaded = VgExt - chargeV;
        const double von = config_.gridTurnOnVoltage;
        const double vkRef = std::isfinite(Vk_last_) ? Vk_last_ : VkRest_;
        // One-sample-stale plate-cathode voltage → Ig division region.
        const double VaPrev = std::max(1.0,
            (std::isfinite(VpPrevSide) ? VpPrevSide : config_.Vp_nominal)
            - vkRef);

        double VgEff = VgLoaded;
        double Ig;
        if (VgLoaded - vkRef + von < -0.5)
        {
            // Deep below conduction (the common case: idle Vgk ≈ −36 V).
            // One cheap leakage-dominated evaluation, no Newton.
            Ig = std::max(0.0, tube.gridCurrent(
                VgLoaded - vkRef + von, VaPrev));
        }
        else
        {
            // Conducting: the grid loads the splitter through the stopper
            // PLUS the splitter's output impedance (positive-peak
            // flattening), solved by a 2-step Newton on the monotone
            // residual — same scheme as TubeStage.
            const double rStop = gridRStopEff_;
            for (int it = 0; it < 2; ++it)
            {
                const auto gd = tube.gridCurrentWithDeriv(
                    VgEff - vkRef + von, VaPrev);
                const double g  = VgEff + gd.Ig * rStop - VgLoaded;
                const double gp = 1.0 + std::max(0.0, gd.dIg) * rStop;
                VgEff -= g / gp;
                if (! std::isfinite(VgEff)) { VgEff = VgLoaded; break; }
            }
            Ig = std::max(0.0, tube.gridCurrent(
                VgEff - vkRef + von, VaPrev));
        }

        // Two-branch blocking memory (fast attack squeeze + slow recovery
        // tail), advanced from the OLD branch values like TubeStage.
        const double dVfast = (Ig * gridInvC_
            - fastV * gridLeakGC_
            - (fastV - slowV) * gridF2SGC_) * gridDt_;
        const double dVslow = ((fastV - slowV) * gridF2SGC_
            - slowV * gridBleedGC_) * gridDt_;
        fastV += dVfast;
        slowV += dVslow;
        if (fastV < 0.0) fastV = 0.0;
        if (slowV < 0.0) slowV = 0.0;

        return VgEff;
    }

    // Setup-time constants + rest priming for the power-grid network and
    // the magnetizing de-embed factor.  Called at the end of setup().
    void setupPowerCouplingState() noexcept
    {
        // Grid-network coefficients.
        const double c     = std::max(config_.gridCouplingC, 1.0e-12);
        const double rLeak = std::max(config_.gridLeakR, 1.0e3);
        gridDt_      = 1.0 / sampleRate_;
        gridInvC_    = 1.0 / c;
        gridLeakGC_  = 1.0 / (rLeak * c);
        gridF2SGC_   = 1.0 / (7.0 * rLeak * c);
        gridBleedGC_ = 1.0 / (18.0 * rLeak * c);
        gridRStopEff_ = std::max(config_.gridStopperR, 1.0)
            + (config_.useLtpPhaseSplitter ? ltpZoutAvg_
                                           : std::max(config_.gridDriveSourceZ, 0.0));

        // Standing grid-leak equilibrium (Ig at rest is leakage-dominated
        // at Vgk = Vg_bias ≈ −36 V, so this is microvolts — primed anyway
        // so enabling the network never injects a startup step).
        const double IgRest = std::max(0.0, tubePos_.gridCurrent(
            config_.Vg_bias + config_.gridTurnOnVoltage));
        const double slowShare = 18.0 / 25.0;
        const double fastRest = IgRest * rLeak
                              / (1.0 + (1.0 - slowShare) / 7.0);
        gridChargeFastPos_ = fastRest;
        gridChargeSlowPos_ = slowShare * fastRest;
        gridChargeFastNeg_ = fastRest;
        gridChargeSlowNeg_ = slowShare * fastRest;
        gridChargeRestV_   = fastRest * (1.0 + slowShare);

        // Magnetizing de-embed factor: converged linear response of the
        // diff current to the antisymmetric ±iMag/2 injection through the
        // COUPLED load lines.  Deriving from the node relations
        //   Vp₁ = vOpen₁ − rTot·(i₁ − iMag/2),  δi₂ = −δi₁ (antisym.)
        // gives  δdiff/δiMag = rpInv·rTot / (1 + rpInv·(2rQ + DCR)) —
        // the differential perturbation sees the FULL plate-to-plate
        // half (2rQ + DCR = Raa/2 + DCR), not the single-node divider
        // (which over-subtracted ~25% of the OPT's linear Lm drop and
        // added spurious LF attenuation beyond the calibrated corner).
        // Zero when the coupled OPT is off (no injection happens).
        alphaMagRest_ = 0.0;
        if (config_.coupleOPTDifferential)
        {
            const double rQ   = std::max(0.5 * config_.Rp_primary, 1.0);
            const double rDc  = std::max(config_.dcrPrimary, 0.0);
            const double rTot = rQ + rDc;
            const double zDiff = 2.0 * rQ + rDc;
            const double ul = std::clamp(config_.ulTapRatio, 0.05, 1.0);
            const double vbN = config_.Vp_nominal;
            const auto dP = tubePos_.evalWithDerivatives(
                vbN - ul * (vbN - VpPosLast_), config_.Vg_bias);
            const auto dN = tubeNeg_.evalWithDerivatives(
                vbN - ul * (vbN - VpNegLast_), config_.Vg_bias);
            const double aP = std::max(0.0, dP.rpInv) * ul * rTot
                            / (1.0 + std::max(0.0, dP.rpInv) * ul * zDiff);
            const double aN = std::max(0.0, dN.rpInv) * ul * rTot
                            / (1.0 + std::max(0.0, dN.rpInv) * ul * zDiff);
            alphaMagRest_ = std::clamp(0.5 * (aP + aN), 0.0, 1.0);
        }

        ipAcPosLast_ = 0.0;
        ipAcNegLast_ = 0.0;
        iMagA_       = 0.0;

        // Rest differential plate resistance for the OPT's dynamic
        // source-impedance anchor (dP/dN evaluated at the rest points
        // above when the coupled OPT is on; recompute plainly otherwise).
        {
            const auto rP = tubePos_.evalWithDerivatives(
                VpPosLast_, config_.Vg_bias);
            const auto rN = tubeNeg_.evalWithDerivatives(
                VpNegLast_, config_.Vg_bias);
            lastRpInvAvg_ = 0.5 * (std::max(0.0, rP.rpInv)
                                 + std::max(0.0, rN.rpInv));
        }
    }

    // Per-side plate load-line Newton for the LTP splitter (mirrors
    // TubeStage's LTP path).  Solves Vp = ltpVpNominal − Rp·Ip(Vp, Vgk)
    // with a warm start, so the splitter's plate swing carries the physical
    // rp∥Rp compression and saturation-side clipping instead of the legacy
    // static gm·Rp gain evaluated at a pinned plate voltage.
    KorenTriode::IpDerivatives solveLtpPlate(const KorenTriode& tube,
                                             double Vgk, double Rp,
                                             double& VpWarm) const noexcept
    {
        const double vbNom = std::max(1.0, config_.ltpVpNominal);
        const double gLoad = 1.0 / std::max(Rp, 1.0);
        double Vp = std::isfinite(VpWarm)
            ? std::clamp(VpWarm, 1.0, vbNom)
            : vbNom * 0.8;
        // ONE warm-started Newton step per call (mirrors
        // PushPullStage::solveSide): the tail loop calls this twice per side
        // per sample and the plate warm start barely moves between samples,
        // so a single device evaluation converges to solver noise.  Reading
        // Ip from the circuit side at the solved node keeps KCL exact with
        // no second device evaluation — 1 eval/call instead of the previous
        // 4, which had regressed the Console preset ~37 pp CPU.
        KorenTriode::IpDerivatives d = tube.evalWithDerivatives(Vp, Vgk);
        const double f  = d.Ip + (Vp - vbNom) * gLoad;
        const double fp = d.rpInv + gLoad;
        Vp -= f / fp;
        if (! std::isfinite(Vp)) Vp = vbNom * 0.8;
        Vp = std::clamp(Vp, 1.0, vbNom);
        VpWarm = Vp;
        d.Ip = std::max(0.0, (vbNom - Vp) * gLoad);
        return d;
    }

    void setupLtpPhaseSplitter() noexcept
    {
        ltpVkRest_      = 0.0;
        ltpVkLast_      = 0.0;
        ltpOutPosRest_  = config_.ltpVpNominal;
        ltpOutNegRest_  = config_.ltpVpNominal;
        ltpVpPosLast_   = config_.ltpVpNominal;
        ltpVpNegLast_   = config_.ltpVpNominal;
        ltpMakeup_      = 1.0;

        if (! config_.useLtpPhaseSplitter)
            return;

        const double mm = std::clamp(config_.ltpTubeMismatch, -0.45, 0.45);
        auto tubeP = config_.ltpTube;
        auto tubeN = config_.ltpTube;
        tubeP.G  *= (1.0 + mm);
        tubeP.mu *= (1.0 - 0.5 * mm);
        tubeN.G  *= (1.0 - mm);
        tubeN.mu *= (1.0 + 0.5 * mm);
        ltpPos_.setParams(tubeP);
        ltpNeg_.setParams(tubeN);

        const double rpRatio = std::max(0.2, config_.ltpPlateRRatio);
        const double RpP = config_.ltpRp;
        const double RpN = config_.ltpRp * rpRatio;
        const double tailR = std::max(config_.ltpTailR, 1.0);

        // Joint rest solve WITH per-side plate load lines: each side
        // satisfies Vp = ltpVpNominal − Rp·Ip(Vp, Vgk) while the tail
        // carries the summed current.  (The legacy solve evaluated Ip at
        // the fixed ltpVpNominal — no load line — so the splitter had the
        // same static-waveshaper gain error docs/33 §1 fixed everywhere
        // else, and its output stage was fed an unphysical drive.)
        double Vk = 0.0;
        double VpP = config_.ltpVpNominal * 0.8;
        double VpN = config_.ltpVpNominal * 0.8;
        KorenTriode::IpDerivatives p {}, n {};
        for (int it = 0; it < 24; ++it)
        {
            p = solveLtpPlate(ltpPos_, config_.ltpVgBias - Vk, RpP, VpP);
            n = solveLtpPlate(ltpNeg_, config_.ltpVgBias - Vk, RpN, VpN);
            const double f = p.Ip + n.Ip - Vk / tailR;
            const double gmP = p.gm / (1.0 + RpP * std::max(0.0, p.rpInv));
            const double gmN = n.gm / (1.0 + RpN * std::max(0.0, n.rpInv));
            const double fp = -(gmP + gmN) - 1.0 / tailR;
            if (std::abs(fp) < 1.0e-15) break;
            Vk -= f / fp;
            if (! std::isfinite(Vk)) { Vk = 0.0; break; }
            Vk = std::max(0.0, Vk);
        }
        ltpVkRest_    = Vk;
        ltpVkLast_    = Vk;
        ltpVpPosLast_ = VpP;
        ltpVpNegLast_ = VpN;
        ltpOutPosRest_ = VpP;
        ltpOutNegRest_ = VpN;

        // Rest output impedance the power grids see from the splitter
        // (rp∥Rp per side, averaged) — series element of the power-grid
        // conduction network.
        {
            const double zP = 1.0 / std::max(1.0 / RpP
                                             + std::max(0.0, p.rpInv), 1.0e-9);
            const double zN = 1.0 / std::max(1.0 / RpN
                                             + std::max(0.0, n.rpInv), 1.0e-9);
            ltpZoutAvg_ = 0.5 * (zP + zN);
        }

        // Rest small-signal per-side plate gain of the splitter (loaded),
        // for the analytic chain-gain estimate (docs/34 §2.1).
        {
            const double avP = p.gm * RpP
                / (1.0 + RpP * std::max(0.0, p.rpInv));
            const double avN = n.gm * RpN
                / (1.0 + RpN * std::max(0.0, n.rpInv));
            ltpAvRest_ = 0.5 * (avP + avN);
        }

        // Level-preserving makeup: the load line divides the plate swing by
        // (1 + Rp·rpInv) relative to the legacy unloaded gm·Rp gain the
        // ltpToPowerGridGain calibration was tuned against.  Restore that
        // ratio so the load line reshapes curvature/headroom without
        // re-leveling the power-tube drive (same convention as
        // loadLineMakeup_ / TubeStage::outputMakeup_).
        const double avLoadedP = 1.0 + RpP * std::max(0.0, p.rpInv);
        const double avLoadedN = 1.0 + RpN * std::max(0.0, n.rpInv);
        ltpMakeup_ = std::clamp(0.5 * (avLoadedP + avLoadedN), 1.0, 20.0);
    }

    std::pair<double, double> processLtpPhaseSplitter(double inputSample) noexcept
    {
        const double drive = inputSample * config_.ltpDriveVolts;
        const double VgPos = config_.ltpVgBias + drive;
        const double VgNeg = config_.ltpVgBias - drive;

        const double rpRatio = std::max(0.2, config_.ltpPlateRRatio);
        const double RpP = config_.ltpRp;
        const double RpN = config_.ltpRp * rpRatio;
        const double tailR = std::max(config_.ltpTailR, 1.0);

        double Vk = std::max(0.0, ltpVkLast_);
        KorenTriode::IpDerivatives p {}, n {};
        for (int it = 0; it < std::max(1, config_.ltpSolverIters); ++it)
        {
            p = solveLtpPlate(ltpPos_, VgPos - Vk, RpP, ltpVpPosLast_);
            n = solveLtpPlate(ltpNeg_, VgNeg - Vk, RpN, ltpVpNegLast_);
            const double f = p.Ip + n.Ip - Vk / tailR;
            const double gmP = p.gm / (1.0 + RpP * std::max(0.0, p.rpInv));
            const double gmN = n.gm / (1.0 + RpN * std::max(0.0, n.rpInv));
            const double fp = -(gmP + gmN) - 1.0 / tailR;
            if (std::abs(fp) < 1.0e-15) break;
            Vk -= f / fp;
            if (! std::isfinite(Vk)) { Vk = ltpVkLast_; break; }
            Vk = std::max(0.0, Vk);
        }
        ltpVkLast_ = Vk;

        // Plate outputs are naturally anti-phase in an LTP; use the last
        // tail iteration's solved plate voltages directly (a re-solve would
        // just duplicate the Newton work below solver noise).
        const double VpPos = ltpVpPosLast_;
        const double VpNeg = ltpVpNegLast_;
        const double outPos = VpPos - ltpOutPosRest_;
        const double outNeg = VpNeg - ltpOutNegRest_;

        // Finite CMRR lets some common-mode plate movement leak into both
        // branches (shared imprint).
        const double cm = 0.5 * (outPos + outNeg);
        const double cmLeak = std::clamp(config_.ltpCommonModeLeak, 0.0, 1.0) * cm;

        // Convert plate-voltage swing [V] into power-grid drive [V].  The
        // makeup restores the legacy calibration level (see setup()).
        const double g = std::max(0.0, config_.ltpToPowerGridGain) * ltpMakeup_;
        return {
            std::clamp((-outPos + cmLeak) * g, -80.0, 80.0),
            std::clamp((-outNeg + cmLeak) * g, -80.0, 80.0)
        };
    }

    PushPullStageConfig config_ {};
    double sampleRate_ { 48000.0 };

    KorenTriode tubePos_ {};
    KorenTriode tubeNeg_ {};
    KorenTriode ltpPos_  {};
    KorenTriode ltpNeg_  {};

    double Ip_rest_pos_ { 0.0 };
    double Ip_rest_neg_ { 0.0 };
    double normalizer_  { 1.0 };

    double Vk_last_       { 0.0 };
    double VkRest_        { 0.0 };    ///< Resting tail voltage (bias ref)
    double VpPosLast_     { 450.0 };  ///< Per-side plate-node warm starts
    double VpNegLast_     { 450.0 };
    double loadLineMakeup_ { 1.0 };   ///< Legacy-gain calibration factor
    double thermalAlpha_  { 0.0 };    ///< Dissipation envelope coefficient
    double dissRest_      { 1.0 };    ///< Resting plate dissipation [W]
    double dissAvg_       { 1.0 };    ///< Slow dissipation average [W]
    double ltpVkRest_     { 0.0 };
    double ltpVkLast_     { 0.0 };
    double ltpOutPosRest_ { 0.0 };
    double ltpOutNegRest_ { 0.0 };
    double ltpVpPosLast_  { 300.0 };  ///< LTP per-side plate warm starts
    double ltpVpNegLast_  { 300.0 };
    double ltpMakeup_     { 1.0 };    ///< Legacy-gain calibration (load line)
    double ltpZoutAvg_    { 40.0e3 }; ///< Splitter rest Zout (rp∥Rp avg)
    double ltpAvRest_     { 40.0 };   ///< Splitter rest plate gain (loaded)

    // Coupled-OPT partner-current memories (AC, pre-warmup) — Gauss–Seidel
    // warm starts for the differential load line (docs/34 §2.3).
    double ipAcPosLast_ { 0.0 };
    double ipAcNegLast_ { 0.0 };

    // OPT magnetizing-current injection [A, primary differential] and the
    // rest-point de-embed share (docs/34 §2.2).
    double iMagA_        { 0.0 };
    double alphaMagRest_ { 0.0 };
    double lastRpInvAvg_ { 1.0e-3 };  ///< Pair avg ∂Ip/∂Vp (Zout, §3.9)

    // Power-grid blocking network state + setup constants (docs/34 §2.4).
    double gridChargeFastPos_ { 0.0 };
    double gridChargeSlowPos_ { 0.0 };
    double gridChargeFastNeg_ { 0.0 };
    double gridChargeSlowNeg_ { 0.0 };
    double gridChargeRestV_   { 0.0 };
    double gridDt_      { 1.0 / 48000.0 };
    double gridInvC_    { 1.0 / 22.0e-9 };
    double gridLeakGC_  { 0.0 };
    double gridF2SGC_   { 0.0 };
    double gridBleedGC_ { 0.0 };
    double gridRStopEff_ { 45.6e3 };
    double warmupAlpha_   { 0.0 };
    double warmupCurrent_ { 1.0 };
    double outputDC_      { 0.0 };
    double lastIp_        { 0.0 };
    double dcLeakAlpha_   { 0.9999 };
};

} // namespace valvra::dsp
