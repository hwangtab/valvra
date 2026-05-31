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

    // Thermal warmup (the power section warms up over ≈ 30 s).  Used the
    // same way as TubeStage: gm linearly scales from 0.85 → 1.0.
    bool   enableWarmup     { true };
    double warmupTauSeconds { 30.0 };
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

        // Resting plate currents (used for normalization + initial Vk seed).
        Ip_rest_pos_ = tubePos_.plateCurrent(cfg.Vp_nominal, cfg.Vg_bias);
        Ip_rest_neg_ = tubeNeg_.plateCurrent(cfg.Vp_nominal, cfg.Vg_bias);

        setupLtpPhaseSplitter();

        // Initial Vk: sum of resting currents through tail resistor.  The
        // solver warm-starts from here.
        Vk_last_ = (Ip_rest_pos_ + Ip_rest_neg_) * cfg.Rk_tail;

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

        lastIp_ = Ip_rest_pos_ + Ip_rest_neg_;
    }

    void reset(bool coldStart = true) noexcept
    {
        Vk_last_       = (Ip_rest_pos_ + Ip_rest_neg_) * config_.Rk_tail;
        ltpVkLast_     = ltpVkRest_;
        warmupCurrent_ = (coldStart && config_.enableWarmup) ? 0.85 : 1.0;
        lastIp_        = Ip_rest_pos_ + Ip_rest_neg_;
        outputDC_      = 0.0;
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

        // Resting currents must be recomputed for the new tube curve.
        Ip_rest_pos_ = tubePos_.plateCurrent(config_.Vp_nominal, config_.Vg_bias);
        Ip_rest_neg_ = tubeNeg_.plateCurrent(config_.Vp_nominal, config_.Vg_bias);
        const double idleSum = std::max(Ip_rest_pos_ + Ip_rest_neg_, 1.0e-6);
        normalizer_ = idleSum * 2.0;
    }

    // Process one audio sample.
    //   inputSample : normalized audio (≈ ±1.5 at full preamp drive)
    //   Vb_plus     : current B+ rail voltage (from PSU sag)
    double process(double inputSample, double Vb_plus) noexcept
    {
        if (! std::isfinite(inputSample) || ! std::isfinite(Vb_plus)
                                         || ! std::isfinite(Vk_last_)
                                         || ! std::isfinite(warmupCurrent_))
        {
            Vk_last_       = (Ip_rest_pos_ + Ip_rest_neg_) * config_.Rk_tail;
            warmupCurrent_ = 1.0;
            outputDC_      = 0.0;
            lastIp_        = Ip_rest_pos_ + Ip_rest_neg_;
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
        double Vg_pos_external = config_.Vg_bias;
        double Vg_neg_external = config_.Vg_bias;
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

        if (config_.solveTailCoupling)
        {
            constexpr double kTinyR  = 1.0e-6;
            const double oneOverRk =
                1.0 / std::max(config_.Rk_tail, kTinyR);

            for (int iter = 0; iter < config_.tailSolverIters; ++iter)
            {
                const auto pos = tubePos_.evalWithDerivatives(
                    Vb_plus, Vg_pos_external - Vk);
                const auto neg = tubeNeg_.evalWithDerivatives(
                    Vb_plus, Vg_neg_external - Vk);
                Ip_pos = pos.Ip;
                Ip_neg = neg.Ip;
                const double f = Ip_pos + Ip_neg - Vk * oneOverRk;

                // f' wrt Vk = −gm_pos − gm_neg − 1/Rk
                // (raising Vk lowers Vgk on both sides, lowering both
                // Ips by gm; the Vk/Rk side rises by 1/Rk).
                const double fprime = -pos.gm - neg.gm - oneOverRk;

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
            Ip_pos = tubePos_.plateCurrent(Vb_plus, Vg_pos_external - Vk);
            Ip_neg = tubeNeg_.plateCurrent(Vb_plus, Vg_neg_external - Vk);
        }

        // 4) Apply warmup gm scaling (cold tubes deliver less current).
        Ip_pos *= warmupCurrent_;
        Ip_neg *= warmupCurrent_;

        // 5) The OPT sees the difference of the two plate currents.  In
        //    class-A region this is approximately linear (each tube's
        //    even-harmonic contribution cancels in the subtraction); in
        //    class-AB cutoff one current goes to zero while the other
        //    keeps growing, producing the asymmetric clipping that
        //    creates the odd-harmonic-rich British crunch.
        const double diff = Ip_pos - Ip_neg;
        lastIp_ = Ip_pos + Ip_neg;  // sum used by PSU sag

        // 6) Normalize and AC-couple (slow DC tracker mirrors TubeStage).
        double yNorm = (config_.outputGainLinear * diff) / normalizer_;
        constexpr double dcLeakAlpha = 0.9999;
        outputDC_ = dcLeakAlpha * outputDC_ + (1.0 - dcLeakAlpha) * yNorm;
        return yNorm - outputDC_;
    }

    // Diagnostics
    double lastPlateCurrent() const noexcept { return lastIp_; }
    double restingPlateCurrent() const noexcept
    {
        return Ip_rest_pos_ + Ip_rest_neg_;
    }
    double warmupProgress() const noexcept { return warmupCurrent_; }
    double currentVk() const noexcept { return Vk_last_; }

private:
    void setupLtpPhaseSplitter() noexcept
    {
        ltpVkRest_      = 0.0;
        ltpVkLast_      = 0.0;
        ltpOutPosRest_  = config_.ltpVpNominal;
        ltpOutNegRest_  = config_.ltpVpNominal;

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

        const double tailR = std::max(config_.ltpTailR, 1.0);
        double Vk = 0.0;
        for (int it = 0; it < 24; ++it)
        {
            const auto p = ltpPos_.evalWithDerivatives(
                config_.ltpVpNominal, config_.ltpVgBias - Vk);
            const auto n = ltpNeg_.evalWithDerivatives(
                config_.ltpVpNominal, config_.ltpVgBias - Vk);
            const double f = p.Ip + n.Ip - Vk / tailR;
            const double fp = -(p.gm + n.gm) - 1.0 / tailR;
            if (std::abs(fp) < 1.0e-15) break;
            Vk -= f / fp;
            if (! std::isfinite(Vk)) { Vk = 0.0; break; }
            Vk = std::max(0.0, Vk);
        }
        ltpVkRest_ = Vk;
        ltpVkLast_ = Vk;

        const double rpRatio = std::max(0.2, config_.ltpPlateRRatio);
        const auto p = ltpPos_.evalWithDerivatives(
            config_.ltpVpNominal, config_.ltpVgBias - Vk);
        const auto n = ltpNeg_.evalWithDerivatives(
            config_.ltpVpNominal, config_.ltpVgBias - Vk);
        const double VpPos = config_.ltpVpNominal - p.Ip * config_.ltpRp;
        const double VpNeg = config_.ltpVpNominal - n.Ip * config_.ltpRp * rpRatio;
        ltpOutPosRest_ = VpPos;
        ltpOutNegRest_ = VpNeg;
    }

    std::pair<double, double> processLtpPhaseSplitter(double inputSample) noexcept
    {
        const double drive = inputSample * config_.ltpDriveVolts;
        const double VgPos = config_.ltpVgBias + drive;
        const double VgNeg = config_.ltpVgBias - drive;

        double Vk = std::max(0.0, ltpVkLast_);
        const double tailR = std::max(config_.ltpTailR, 1.0);
        for (int it = 0; it < std::max(1, config_.ltpSolverIters); ++it)
        {
            const auto p = ltpPos_.evalWithDerivatives(
                config_.ltpVpNominal, VgPos - Vk);
            const auto n = ltpNeg_.evalWithDerivatives(
                config_.ltpVpNominal, VgNeg - Vk);
            const double f = p.Ip + n.Ip - Vk / tailR;
            const double fp = -(p.gm + n.gm) - 1.0 / tailR;
            if (std::abs(fp) < 1.0e-15) break;
            Vk -= f / fp;
            if (! std::isfinite(Vk)) { Vk = ltpVkLast_; break; }
            Vk = std::max(0.0, Vk);
        }
        ltpVkLast_ = Vk;

        const double rpRatio = std::max(0.2, config_.ltpPlateRRatio);
        const auto p = ltpPos_.evalWithDerivatives(
            config_.ltpVpNominal, VgPos - Vk);
        const auto n = ltpNeg_.evalWithDerivatives(
            config_.ltpVpNominal, VgNeg - Vk);

        // Plate outputs are naturally anti-phase in an LTP.
        const double VpPos = config_.ltpVpNominal - p.Ip * config_.ltpRp;
        const double VpNeg = config_.ltpVpNominal - n.Ip * config_.ltpRp * rpRatio;
        const double outPos = VpPos - ltpOutPosRest_;
        const double outNeg = VpNeg - ltpOutNegRest_;

        // Finite CMRR lets some common-mode plate movement leak into both
        // branches (shared imprint).
        const double cm = 0.5 * (outPos + outNeg);
        const double cmLeak = std::clamp(config_.ltpCommonModeLeak, 0.0, 1.0) * cm;

        // Convert plate-voltage swing [V] into power-grid drive [V].
        const double g = std::max(0.0, config_.ltpToPowerGridGain);
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
    double ltpVkRest_     { 0.0 };
    double ltpVkLast_     { 0.0 };
    double ltpOutPosRest_ { 0.0 };
    double ltpOutNegRest_ { 0.0 };
    double warmupAlpha_   { 0.0 };
    double warmupCurrent_ { 1.0 };
    double outputDC_      { 0.0 };
    double lastIp_        { 0.0 };
};

} // namespace valvra::dsp
