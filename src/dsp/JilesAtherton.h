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
};

// ─────────────────────────────────────────────────────────────────────────────
// Material-specific parameter presets (literature-derived estimates).
// ─────────────────────────────────────────────────────────────────────────────
namespace ja_params
{
    // 80% Ni-Permalloy (Peerless S-217-D estimated core)
    // Source: Roubal 2013 Measurement / ESPI Metals Permalloy 80 datasheet
    constexpr JAParams kNiPermalloy_Peerless = {
        .Ms = 420.0e3, .a = 25.0, .alpha = 3.0e-5, .k = 15.0, .c = 0.15
    };

    // Mu-metal (Triad HS-52 interstage, very similar to permalloy)
    constexpr JAParams kMuMetal_Triad = {
        .Ms = 500.0e3, .a = 30.0, .alpha = 2.0e-5, .k = 30.0, .c = 0.12
    };

    // Grain-oriented silicon steel M6 (Sowter 9530 modern clone reference)
    // Source: Springer 2015 Unisil M130-27s fits
    constexpr JAParams kSiSteel_M6 = {
        .Ms = 1590.0e3, .a = 60.0, .alpha = 1.0e-4, .k = 50.0, .c = 0.15
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

    // Compute magnetization M given the applied field H (sample-by-sample).
    // This is the per-sample driver: call once per input sample with the
    // instantaneous H derived from the transformer primary current.
    double process(double H) noexcept
    {
        step(H, state_);
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
    void step(double H_new, JAState& s) noexcept
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

        const double dH_total = H_new - s.H_prev;

        // Adaptive sub-stepping target: |dH_sub| ≤ a/4.  a is the Langevin
        // slope parameter and sets the natural field scale of the model.
        const double dH_max   = 0.25 * p_.a;
        int          numSteps = 1;
        if (std::abs(dH_total) > dH_max)
            numSteps = static_cast<int>(std::ceil(std::abs(dH_total) / dH_max));
        // No upper cap: at audio rates ΔH per sample is small and numSteps
        // stays in the single digits.  Only offline / test scenarios ever
        // reach large ramps, and there correctness outweighs CPU.

        const double dH = dH_total / static_cast<double>(numSteps);

        // δ selection: preserve last valid sign when dH == 0 (DC / silence)
        if      (dH > 0.0) s.delta = +1.0;
        else if (dH < 0.0) s.delta = -1.0;
        else if (!s.primed) s.delta = +1.0;
        s.primed = true;

        for (int i = 0; i < numSteps; ++i)
        {
            const double H0 = s.H_prev;
            const double k1 = dMirrDH(H0,            s.Mirr,                    s.delta);
            const double k2 = dMirrDH(H0 + 0.5 * dH, s.Mirr + 0.5 * dH * k1,    s.delta);
            const double k3 = dMirrDH(H0 + 0.5 * dH, s.Mirr + 0.5 * dH * k2,    s.delta);
            const double k4 = dMirrDH(H0 + dH,       s.Mirr +       dH * k3,    s.delta);

            s.Mirr  += (dH / 6.0) * (k1 + 2.0 * k2 + 2.0 * k3 + k4);

            // Physical clamp — |Mirr| cannot exceed Ms.
            if (s.Mirr >  p_.Ms) s.Mirr =  p_.Ms;
            if (s.Mirr < -p_.Ms) s.Mirr = -p_.Ms;

            s.H_prev += dH;
        }

        // Pin to exact value to avoid float drift over many sub-steps.
        s.H_prev = H_new;
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
    // with wiping-out condition enforcement.
    double dMirrDH(double H, double Mirr, double delta) const noexcept
    {
        const double He  = H + p_.alpha * Mirr;
        const double Man = p_.Ms * langevin(He / p_.a);
        const double diff = Man - Mirr;

        // Wiping-out: domain walls cannot move opposite to δ.
        if (diff * delta < 0.0) return 0.0;

        double denom = p_.k * delta - p_.alpha * diff;

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
