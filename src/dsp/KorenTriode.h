// ─────────────────────────────────────────────────────────────────────────────
// KorenTriode — physically-motivated vacuum tube triode model
//
// Implements the Dempwolf-Holters-Zölzer (2011) physically-motivated triode
// model, which improves on Koren (1996) by using smoothing functions that
// are continuously differentiable. This matters for real-time solvers.
//
// References:
//   - Dempwolf, Holters, Zölzer (2011). "A Physically-Motivated Triode Model
//     for Circuit Simulations." Proc. DAFx-11, Paris, pp. 257–262.
//   - docs/01-vacuum-tube-physics.md §3 (Koren base model)
//   - docs/22-academic-quantitative-data.md §A (measured parameters)
//
// Measured parameters for three real 12AX7 samples (Dempwolf 2011 Table 1)
// are provided. Use RSD_1 as the canonical 12AX7. Per-instance variation
// (ComponentVariation.h) perturbs these to emulate unit-to-unit differences.
//
// Pultec EQP-1A context:
//   - V1 (ECC83/12AX7): Vp ≈ 290V, Vk ≈ 2V (output makeup amp)
//   - V2 (ECC82/12AU7): Vp ≈ 140V, Vk ≈ 1V (cathode follower)
//   Academic 12AU7 fit is absent; use Koren fallback for 12AU7.
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <cmath>

namespace valvra::dsp {

// ─────────────────────────────────────────────────────────────────────────────
// Parameter set for Dempwolf physical model.
// Cathode current: Ik = G * softplus_γ(Veff)
// Grid current:    Ig = Gg * softplus_ξ(Veff_g) + Ig0
// where softplus_p(x) = log(1 + exp(C*x))/C ^ p  (smoothed power)
// ─────────────────────────────────────────────────────────────────────────────
struct DempwolfParams
{
    // Cathode current law
    double G   {2.242e-3}; ///< Perveance [A/V^γ]
    double mu  {103.2};    ///< Amplification factor
    double gamma {1.26};   ///< Langmuir exponent (≈ 3/2 ideal, measured ~1.3)
    double C   {3.40};     ///< Smoothing: transition sharpness

    // Grid current law (positive grid conduction)
    double Gg  {6.177e-4}; ///< Grid-current coefficient
    double xi  {1.314};    ///< Grid-current exponent
    double Cg  {9.901};    ///< Grid-current smoothing
    double Ig0 {8.025e-8}; ///< Grid leakage offset [A]

    // Parasitic capacitances (for Miller effect modeling in upstream stages)
    double Cak {0.9e-12};  ///< Plate–cathode capacitance [F]
    double Cgk {2.3e-12};  ///< Grid–cathode capacitance [F]
    double Cag {2.4e-12};  ///< Grid–plate (Miller source) [F]
};

// ─────────────────────────────────────────────────────────────────────────────
// Canonical 12AX7 parameter sets (Dempwolf 2011 Table 1, real measured tubes).
// The spread μ ∈ [86.9, 103.2] provides the academic basis for Monte Carlo
// per-instance variation (docs/06 §2.3).
// ─────────────────────────────────────────────────────────────────────────────
namespace params
{
    // Raytheon / Sylvania / Dempwolf sample 1 (standard-brightness tube)
    constexpr DempwolfParams kRSD_1 = {
        .G = 2.242e-3, .mu = 103.2, .gamma = 1.26, .C = 3.40,
        .Gg = 6.177e-4, .xi = 1.314, .Cg = 9.901, .Ig0 = 8.025e-8,
        .Cak = 0.9e-12, .Cgk = 2.3e-12, .Cag = 2.4e-12
    };

    // Raytheon / Sylvania / Dempwolf sample 2 (very similar to RSD_1)
    constexpr DempwolfParams kRSD_2 = {
        .G = 2.173e-3, .mu = 100.2, .gamma = 1.28, .C = 3.19,
        .Gg = 5.911e-4, .xi = 1.358, .Cg = 11.76, .Ig0 = 4.527e-8,
        .Cak = 0.9e-12, .Cgk = 2.3e-12, .Cag = 2.4e-12
    };

    // Electro-Harmonix modern production (lower μ, softer knee — "warmer")
    constexpr DempwolfParams kEHX_1 = {
        .G = 1.371e-3, .mu = 86.9, .gamma = 1.349, .C = 4.56,
        .Gg = 3.263e-4, .xi = 1.156, .Cg = 11.99, .Ig0 = 3.917e-8,
        .Cak = 0.9e-12, .Cgk = 2.3e-12, .Cag = 2.4e-12
    };

    // Koren (1996) 12AU7 fallback — no academic measured Dempwolf fit exists.
    // Approximated via Koren shape: Ik = (E1^Ex)/Kg * (1+sgn(E1)),
    // mapped to Dempwolf by matching small-signal gm and plate curves at
    // Vp=250V, Vg=-8.5V (datasheet operating point).
    // Tuning validated against Pocnet ECC82 datasheet.
    constexpr DempwolfParams kECC82_Koren = {
        .G = 2.2e-3, .mu = 17.0, .gamma = 1.30, .C = 4.5,
        .Gg = 3.0e-4, .xi = 1.25, .Cg = 10.0, .Ig0 = 1.0e-7,
        .Cak = 1.5e-12, .Cgk = 1.6e-12, .Cag = 1.5e-12
    };
}

// ─────────────────────────────────────────────────────────────────────────────
// Numerically-stable softplus: log(1 + exp(x))
// - For x > 20: returns x (exp overflow avoided, error < 2e-9 @ double)
// - For x < -20: returns exp(x) directly (log1p precision loss avoided)
// - Inline + branchless-friendly for the hot audio path
// ─────────────────────────────────────────────────────────────────────────────
inline double softplus(double x) noexcept
{
    if (x > 20.0)  return x;
    if (x < -20.0) return std::exp(x);
    return std::log1p(std::exp(x));
}

// ─────────────────────────────────────────────────────────────────────────────
// Triode stage: computes plate current (cathode current minus grid current,
// since for triodes Ik = Ip + Ig) given plate and grid voltages.
//
// Usage (per-sample):
//     KorenTriode t { params::kRSD_1 };
//     const double ip = t.plateCurrent(Vp, Vg);   // [Amperes]
// ─────────────────────────────────────────────────────────────────────────────
class KorenTriode
{
public:
    explicit KorenTriode(DempwolfParams p = params::kRSD_1) noexcept
        : p_ { p } {}

    // Set / replace the tube parameter set (e.g., for Monte Carlo variation)
    void setParams(const DempwolfParams& p) noexcept { p_ = p; }
    const DempwolfParams& params() const noexcept { return p_; }

    // Cathode current: Ik = G * [softplus(C * (μ·Vg + Vp)/C) / C]^γ
    //
    // Rewritten for numerical stability:
    //   Veff = (μ·Vg + Vp) / μ      (effective control voltage)
    //   s    = softplus(C * Veff) / C
    //   Ik   = G * s^γ
    //
    // This form matches Dempwolf 2011 Eq. (3–5).
    double cathodeCurrent(double Vp, double Vg) const noexcept
    {
        const double Veff = (p_.mu * Vg + Vp) / p_.mu;
        const double s    = softplus(p_.C * Veff) / p_.C;
        if (s <= 0.0) return 0.0;
        return p_.G * std::pow(s, p_.gamma);
    }

    // Grid current: Ig = Gg * [softplus(Cg·Vg) / Cg]^ξ + Ig0
    // Only significant when Vg approaches or exceeds 0 (positive grid drive).
    double gridCurrent(double Vg) const noexcept
    {
        const double s = softplus(p_.Cg * Vg) / p_.Cg;
        if (s <= 0.0) return p_.Ig0;
        return p_.Gg * std::pow(s, p_.xi) + p_.Ig0;
    }

    // Plate current: Ip = Ik - Ig (KCL at plate)
    double plateCurrent(double Vp, double Vg) const noexcept
    {
        return cathodeCurrent(Vp, Vg) - gridCurrent(Vg);
    }

    // Small-signal transconductance gm = ∂Ip/∂Vg at operating point.
    // Useful for Monte Carlo validation and dynamic Miller-cap calculation.
    // Uses central difference (∓1 mV) — fast enough for setup-phase queries.
    double transconductance(double Vp, double Vg) const noexcept
    {
        constexpr double h = 1.0e-3;  // 1 mV probe
        const double ip1 = plateCurrent(Vp, Vg + h);
        const double ip0 = plateCurrent(Vp, Vg - h);
        return (ip1 - ip0) / (2.0 * h);
    }

    // Plate resistance rp = ∂Vp/∂Ip at constant Vg, inverted from ∂Ip/∂Vp.
    double plateResistance(double Vp, double Vg) const noexcept
    {
        constexpr double h = 1.0;  // 1 V probe
        const double ip1 = plateCurrent(Vp + h, Vg);
        const double ip0 = plateCurrent(Vp - h, Vg);
        const double dIp_dVp = (ip1 - ip0) / (2.0 * h);
        if (std::abs(dIp_dVp) < 1.0e-12) return 1.0e12;  // saturation region
        return 1.0 / dIp_dVp;
    }

private:
    DempwolfParams p_;
};

} // namespace valvra::dsp
