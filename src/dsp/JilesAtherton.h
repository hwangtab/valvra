// ─────────────────────────────────────────────────────────────────────────────
// JilesAtherton — ferromagnetic hysteresis model for audio transformer cores
//
// Implements the Jiles-Atherton (1986) model with the coupled reversible +
// irreversible magnetization formulation, integrated via RK4.
//
// References:
//   - Jiles, D.C., Atherton, D.L. (1986). "Theory of ferromagnetic
//     hysteresis." J. Magn. Magn. Mater. 61, 48–60.
//   - Holters, M., Zölzer, U. (2016). "Circuit Simulation with Inductors and
//     Transformers Based on the Jiles-Atherton Model of Magnetization."
//     Proc. DAFx-16.
//   - docs/02-transformer-physics-and-distortion.md §2 (coupled dM/dH)
//   - docs/22-academic-quantitative-data.md §C (parameter estimates)
//
// Numerical fixes applied (vs. naive implementations):
//   - Langevin Taylor expansion for |He/a| < 1e-4 (no coth(0) blowup)
//   - Wiping-out condition: (Man − Mirr) and δ opposite → dMirr/dH = 0
//   - Denominator clamping at loop reversal points
//   - State encapsulated (no global lastH)
//
// Pultec context:
//   Peerless S-217-D (output):   Ni-permalloy core, estimated JA parameters
//   Triad HS-52 (interstage):    Mu-metal core, similar permeability class
//
// Direct JA fits for Pultec transformers are NOT in the literature — the
// parameters below are estimates from Roubal (2013) and material datasheets.
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <algorithm>
#include <cmath>

namespace valvra::dsp {

// ─────────────────────────────────────────────────────────────────────────────
// Five Jiles-Atherton parameters.
// Units match the standard A/m (magnetic field) and kA/m (magnetization).
// ─────────────────────────────────────────────────────────────────────────────
struct JAParams
{
    double Ms    {420.0e3}; ///< Saturation magnetization [A/m]
    double a     {25.0};    ///< Langevin slope parameter [A/m]
    double alpha {3.0e-5};  ///< Inter-domain coupling (dimensionless)
    double k     {15.0};    ///< Pinning (hysteresis loss) [A/m]
    double c     {0.15};    ///< Reversibility ratio (0..1)

    // ─── Bertotti excess (anomalous) loss (docs/34 §3.2) ────────────────
    // Real laminated cores show DYNAMIC hysteresis: the measured coercivity
    // (loop width) grows ≈ √(dB/dt) beyond the static loop — micro-scale
    // eddy drag on the moving domain walls.  Modelled as a rate-dependent
    // pinning multiplier  k_dyn = k·(1 + kExcess·√(|dH/dt| / (a·ω_ref)))
    // with ω_ref = 2π·1 kHz, applied only when the caller supplies the
    // sample period (dt = 0 keeps the classic rate-independent model —
    // offline ramps, tests and the setup-time χ probe are unchanged).
    // Thin-lamination / low-loss materials get small values; grain-
    // oriented Si-steel the largest.
    double kExcess {0.0};   ///< Excess-loss coefficient (0 = quasi-static)
};

// ─────────────────────────────────────────────────────────────────────────────
// Material-specific parameter presets (literature-derived estimates).
// ─────────────────────────────────────────────────────────────────────────────
namespace ja_params
{
    // 80% Ni-Permalloy (Peerless S-217-D estimated core)
    // Source: Roubal 2013 Measurement / ESPI Metals Permalloy 80 datasheet
    constexpr JAParams kNiPermalloy_Peerless = {
        .Ms = 420.0e3, .a = 25.0, .alpha = 3.0e-5, .k = 15.0, .c = 0.15,
        .kExcess = 0.03    // thin 80%-Ni laminations: mild dynamic widening
    };

    // Mu-metal (Triad HS-52 interstage, very similar to permalloy)
    constexpr JAParams kMuMetal_Triad = {
        .Ms = 500.0e3, .a = 30.0, .alpha = 2.0e-5, .k = 30.0, .c = 0.12,
        .kExcess = 0.03
    };

    // Grain-oriented silicon steel M6 (Sowter 9530 modern clone reference)
    // Source: Springer 2015 Unisil M130-27s fits
    constexpr JAParams kSiSteel_M6 = {
        .Ms = 1590.0e3, .a = 60.0, .alpha = 1.0e-4, .k = 50.0, .c = 0.15,
        .kExcess = 0.08    // GO Si-steel: largest excess-loss share
    };
}

// ─────────────────────────────────────────────────────────────────────────────
// State encapsulation: per-transformer-instance mutable state.
// ─────────────────────────────────────────────────────────────────────────────
struct JAState
{
    double Mirr   {0.0};    ///< Irreversible magnetization (integration state)
    double H_prev {0.0};    ///< Previous H for δ sign detection
    double delta  {+1.0};   ///< Last valid sign(dH/dt)
    bool   primed {false};  ///< First-call guard
};

// ─────────────────────────────────────────────────────────────────────────────
// Langevin function L(x) = coth(x) − 1/x and its derivative.
// Taylor expansion for |x| < 1e-4 avoids the 0/0 singularity.
// ─────────────────────────────────────────────────────────────────────────────
inline double langevin(double x) noexcept
{
    if (std::abs(x) < 1.0e-4)
        return x / 3.0 - (x * x * x) / 45.0;        // O(x^5) Taylor
    return 1.0 / std::tanh(x) - 1.0 / x;
}

inline double langevinDeriv(double x) noexcept
{
    if (std::abs(x) < 1.0e-4)
        return 1.0 / 3.0 - (x * x) / 15.0;          // O(x^4) Taylor
    const double sh = std::sinh(x);
    return 1.0 / (x * x) - 1.0 / (sh * sh);
}

// ─────────────────────────────────────────────────────────────────────────────
// Jiles-Atherton hysteresis integrator.
// ─────────────────────────────────────────────────────────────────────────────
class JilesAtherton
{
public:
    explicit JilesAtherton(JAParams p = ja_params::kNiPermalloy_Peerless) noexcept
        : p_ { p } {}

    void setParams(const JAParams& p) noexcept { p_ = p; }
    const JAParams& params() const noexcept { return p_; }

    void reset() noexcept
    {
        state_ = {};
    }

    // State accessors for visualization (B-H loop plot, diagnostics)
    double currentH() const noexcept { return state_.H_prev; }
    double currentMirr() const noexcept { return state_.Mirr; }
    const JAParams& currentParams() const noexcept { return p_; }

    /// Mutable handle to the integrator state.  TransformerStage's
    /// flux-imposed mode needs trial steps on COPIES of the state (the
    /// JA inverse H(B) is found by secant before committing), which the
    /// public step(H, state) API supports once the state is reachable.
    JAState& stateRef() noexcept { return state_; }
    const JAState& stateRef() const noexcept { return state_; }

    // Compute magnetization M given the applied field H (sample-by-sample).
    // This is the per-sample driver: call once per input sample with the
    // instantaneous H derived from the transformer primary current.
    // Pass the sample period as dtSeconds to enable the Bertotti excess-
    // loss (rate-dependent loop widening); 0 keeps the quasi-static model.
    double process(double H, double dtSeconds = 0.0) noexcept
    {
        step(H, state_, dtSeconds);
        return observedM(H, state_);
    }

    // Low-level step: explicit state reference for branch prediction / inlining.
    //
    // Implements **adaptive sub-stepping** for numerical stability:
    // when an external call supplies a large |ΔH| (e.g. test ramps or
    // signal transients exceeding roughly a/4 per sample), the RK4 slope
    // can overshoot and push M_irr outside the physical range [-Ms, +Ms].
    // We split the step so that each internal sub-step satisfies
    // |dH_sub| ≤ (a / 4), which is the empirically stable bound for
    // trapezoidal/RK4 JA integration (Holters-Zölzer 2016).
    //
    // Finally we clamp M_irr to [-Ms, +Ms] — this is a hard physical
    // constraint (saturation magnetization) and guards against residual
    // numerical error.
    void step(double H_new, JAState& s, double dtSeconds = 0.0) noexcept
    {
        // Defensive sanity guard: if an upstream stage produces a NaN or Inf
        // (e.g. a pathological signal or uninitialised memory on reset) we
        // silently recover by collapsing the state to zero rather than
        // propagating the corruption forever.
        if (! std::isfinite(H_new) || ! std::isfinite(s.Mirr)
                                    || ! std::isfinite(s.H_prev))
        {
            s = {};
            return;
        }

        // Saturation clamp on the APPLIED field.  Beyond |He/a| ≈ 20 the
        // Langevin anhysteretic is fully saturated (L(20) > 0.95) and Mirr
        // is pinned at ±Ms, so any larger |H| produces no further change
        // in M — clamping it is physically lossless for the output.  It
        // exists to bound the sub-step count below: a runaway upstream
        // (e.g. a mis-scaled or DC-drifting flux integrator) could
        // otherwise drive H to millions, demanding an unbounded RK4
        // sub-step loop that would stall the AUDIO THREAD for minutes.
        // The bound is generous (64·a, well past saturation) so it never
        // touches a legitimate signal or an offline hysteresis-loop ramp.
        const double Hclamp = 64.0 * p_.a;
        const double Hc = (H_new >  Hclamp) ?  Hclamp
                        : (H_new < -Hclamp) ? -Hclamp : H_new;

        const double dH_total = Hc - s.H_prev;

        // Adaptive sub-stepping target: |dH_sub| ≤ a/4.  a is the Langevin
        // slope parameter and sets the natural field scale of the model.
        const double dH_max   = 0.25 * p_.a;
        int          numSteps = 1;
        if (std::abs(dH_total) > dH_max)
            numSteps = static_cast<int>(std::ceil(std::abs(dH_total) / dH_max));
        // Backstop cap.  With the saturation clamp above, |dH_total| ≤
        // 128·a so this never binds for real input (max ≈ 512 sub-steps
        // from a full ±64a swing in one call — only an offline loop test
        // approaches it); it is a pure guarantee that a future change
        // can't reintroduce an unbounded loop on the audio thread.
        constexpr int kMaxSubSteps = 600;
        if (numSteps > kMaxSubSteps) numSteps = kMaxSubSteps;

        const double dH = dH_total / static_cast<double>(numSteps);

        // Bertotti excess-loss pinning multiplier (docs/34 §3.2): domain
        // walls moving faster see more micro-eddy drag — the loop's
        // coercivity widens ≈ √(field rate).  Normalised so a unit-a swing
        // at the 1 kHz reference gives factor ≈ 1 + kExcess·√(2π).  Only
        // active when the caller supplies real time (dt > 0); clamped so a
        // pathological transient cannot freeze the wall motion entirely.
        double kDynFactor = 1.0;
        if (dtSeconds > 0.0 && p_.kExcess > 0.0)
        {
            constexpr double kOmegaRef = 2.0 * 3.14159265358979323846 * 1000.0;
            const double rate = std::abs(dH_total) / dtSeconds;
            kDynFactor = 1.0 + p_.kExcess
                * std::sqrt(rate / std::max(p_.a * kOmegaRef, 1.0e-12));
            if (kDynFactor > 4.0) kDynFactor = 4.0;
        }

        // δ selection: preserve last valid sign when dH == 0 (DC / silence)
        if      (dH > 0.0) s.delta = +1.0;
        else if (dH < 0.0) s.delta = -1.0;
        else if (!s.primed) s.delta = +1.0;
        s.primed = true;

        for (int i = 0; i < numSteps; ++i)
        {
            const double H0 = s.H_prev;
            const double k1 = dMirrDH(H0,            s.Mirr,                    s.delta, kDynFactor);
            const double k2 = dMirrDH(H0 + 0.5 * dH, s.Mirr + 0.5 * dH * k1,    s.delta, kDynFactor);
            const double k3 = dMirrDH(H0 + 0.5 * dH, s.Mirr + 0.5 * dH * k2,    s.delta, kDynFactor);
            const double k4 = dMirrDH(H0 + dH,       s.Mirr +       dH * k3,    s.delta, kDynFactor);

            s.Mirr  += (dH / 6.0) * (k1 + 2.0 * k2 + 2.0 * k3 + k4);

            // Physical clamp — |Mirr| cannot exceed Ms.
            if (s.Mirr >  p_.Ms) s.Mirr =  p_.Ms;
            if (s.Mirr < -p_.Ms) s.Mirr = -p_.Ms;

            s.H_prev += dH;
        }

        // Pin to the (clamped) field to avoid float drift over many
        // sub-steps.  Storing the clamped value is what makes the clamp
        // lossless across calls: a later H returning from saturation
        // computes its delta from the saturated branch, not from an
        // un-physical multi-million field.
        s.H_prev = Hc;
    }

    // Observed M = (1-c)·Mirr + c·Man(He).  Called after step().
    double observedM(double H, const JAState& s) const noexcept
    {
        const double He  = H + p_.alpha * s.Mirr;
        const double Man = p_.Ms * langevin(He / p_.a);
        return (1.0 - p_.c) * s.Mirr + p_.c * Man;
    }

private:
    // Irreversible-magnetization slope: dMirr/dH = (Man - Mirr) / (k·δ - α·(Man-Mirr))
    // with wiping-out condition enforcement.  kFactor scales the pinning
    // for the Bertotti excess-loss (rate-dependent) regime; 1 = static.
    double dMirrDH(double H, double Mirr, double delta,
                   double kFactor = 1.0) const noexcept
    {
        const double He  = H + p_.alpha * Mirr;
        const double Man = p_.Ms * langevin(He / p_.a);
        const double diff = Man - Mirr;

        // Wiping-out: domain walls cannot move opposite to δ.
        if (diff * delta < 0.0) return 0.0;

        double denom = p_.k * kFactor * delta - p_.alpha * diff;

        // Denominator clamp near reversal points
        constexpr double kEps = 1.0e-12;
        if (std::abs(denom) < kEps)
            denom = (denom < 0.0 ? -kEps : kEps);

        return diff / denom;
    }

    JAParams p_;
    JAState  state_;
};

} // namespace valvra::dsp
