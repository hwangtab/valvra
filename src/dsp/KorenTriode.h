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

#include <algorithm>
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

    // ─── Grid-current division region (docs/34 §3.1, Rutt 1984) ─────────
    // The Dempwolf Ig law is fitted with the plate well above the grid.
    // When a hard-driven plate swings DOWN toward/below the grid voltage,
    // the grid competes directly for the space current and collects a
    // several-fold larger share — the "division region" that makes deep
    // overdrive blocking bite harder than the baseline law predicts.
    // Ig_eff = Ig·(1 + divK·σ((Vg−Va)/divV)); σ→0 for Va ≫ Vg keeps the
    // fitted law intact everywhere the fit was made.
    double divK {4.0};     ///< extra Ig multiple deep in division
    double divV {8.0};     ///< Vg−Va logistic transition width [V]
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
    // Recalibrated against the Pocnet ECC82 datasheet point now that the
    // plate node is actually solved: Ip(250, −8.5) = 10.5 mA, gm = 2.2 mA/V
    // (the previous G = 2.2e-3 hit 23.6 mA / 4.95 mA/V — a hand
    // compensation for the missing load line).
    constexpr DempwolfParams kECC82_Koren = {
        .G = 9.785e-4, .mu = 17.0, .gamma = 1.30, .C = 4.5,
        .Gg = 3.0e-4, .xi = 1.25, .Cg = 10.0, .Ig0 = 1.0e-7,
        .Cak = 1.5e-12, .Cgk = 1.6e-12, .Cag = 1.5e-12
    };

    // EL34 in triode-strapped configuration — characteristic British power
    // tube.  Mu drops to ~10 (vs 100 for 12AX7) — signal swing on the grid
    // is therefore an order of magnitude larger to drive comparable plate
    // current.  These parameters are derived from the Mullard EL34 data
    // sheet plate curves in triode-strapped mode (g2 tied to plate),
    // Koren-fitted then mapped onto Dempwolf perveance form by matching
    // small-signal gm (≈ 11 mA/V at idle) and plate dissipation curves.
    //
    // Note: EL34 in *pentode* mode requires a different model (4-element
    // current law).  We deliberately use the triode-strapped variant
    // because it's the most common voicing in modern guitar-amp output
    // stages (post-1970 Marshalls and clones) and falls within the
    // Dempwolf triode framework without modification.
    // Recalibrated at the working anchor Ip(450 V, −36 V) = 55 mA,
    // gm ≈ 11 mA/V (JCM800-class idle at the documented fixed bias).
    constexpr DempwolfParams kEL34_TriodeStrapped = {
        .G = 3.713e-3, .mu = 10.5, .gamma = 1.40, .C = 3.0,
        .Gg = 2.0e-4, .xi = 1.30, .Cg = 8.0, .Ig0 = 1.0e-7,
        .Cak = 8.5e-12, .Cgk = 15.2e-12, .Cag = 1.1e-12
    };

    // 6L6GC in triode-strapped configuration — the American counterpart.
    // Slightly higher mu than EL34, harder transition to cutoff (faster
    // softplus knee), more "bite" at the top end of overdrive.  Same
    // calibration approach: data-sheet curves → Koren → Dempwolf form.
    // Recalibrated at the RCA triode-connection point Ip(250 V, −20 V)
    // = 40 mA (datasheet gm 4.7 mA/V; model lands 5.7 — curve-shape
    // limited, within the unit spread Monte Carlo applies anyway).
    constexpr DempwolfParams k6L6GC_TriodeStrapped = {
        .G = 1.939e-3, .mu = 8.5, .gamma = 1.35, .C = 3.4,
        .Gg = 1.8e-4, .xi = 1.28, .Cg = 9.0, .Ig0 = 1.0e-7,
        .Cak = 6.5e-12, .Cgk = 11.8e-12, .Cag = 1.2e-12
    };

    // 6SN7GT — octal twin-triode, μ ≈ 20.  The "smooth and warm" preamp
    // tube favoured by HiFi designers and vintage Hammond organs.  Its
    // lower μ means it generates less harmonic distortion than 12AX7 at
    // the same drive — quieter and more linear.  Used in the Aiken
    // High-Voltage Stage style preamps and in modern Manley products.
    //
    // Calibration procedure: Koren parameters (μ=20, Ex=1.4, Kg ≈ 8200,
    // Kp ≈ 50) → Dempwolf G ≈ 1/Kg fitted at the canonical RCA
    // datasheet operating point (Vp=250 V, Vg=−8 V → Ip ≈ 9 mA per tube
    // section, ≈ 18 mA effective when both halves of the bottle are
    // tied in parallel — the more common HiFi usage).
    // Recalibrated per-section: Ip(250 V, −8 V) = 9 mA, gm 2.8 mA/V
    // (RCA datasheet 9 mA / 2.6 mA/V; the previous G modelled both
    // bottle halves in parallel while the stage models one section).
    constexpr DempwolfParams k6SN7 = {
        .G = 1.096e-3, .mu = 20.0, .gamma = 1.40, .C = 3.50,
        .Gg = 4.0e-4, .xi = 1.32, .Cg = 11.0, .Ig0 = 1.0e-7,
        .Cak = 1.4e-12, .Cgk = 3.0e-12, .Cag = 4.0e-12
    };

    // 300B — directly-heated power triode (Western Electric, 1933).  The
    // legendary single-ended HiFi tube: μ ≈ 3.85 means it needs a large
    // grid swing (~80 V peak for full output) but produces extremely
    // linear, low-IMD output thanks to that low μ.  The "liquid sound"
    // of high-end HiFi amps (Audio Note, Cary, vintage Western Electric
    // 91A) traces almost entirely to this tube's low-distortion physics.
    //
    // Operating point: Vp ≈ 350 V, Vg ≈ −76 V → Ip ≈ 60–80 mA on a
    // typical SE 300B amp.  Use in a Common-Cathode stage with very
    // heavy grid drive (inputVoltageSwing and outputGainLinear scaled
    // accordingly).
    //
    // Calibration procedure: Koren parameters (μ=3.85, Ex=1.5, Kg ≈
    // 1500, Kp=50) → Dempwolf G ≈ 1/Kg.  Verified at the WE 300B-
    // canonical operating point against published plate-curve sheets.
    // Recalibrated to the WE 300B canonical point Ip(350 V, −76 V)
    // = 70 mA (datasheet 60–80 mA; the previous "2× idle convention"
    // had no basis and put the tube at 121 mA).
    constexpr DempwolfParams k300B = {
        .G = 1.216e-3, .mu = 3.85, .gamma = 1.50, .C = 4.0,
        .Gg = 8.0e-4, .xi = 1.40, .Cg = 6.0, .Ig0 = 5.0e-7,
        .Cak = 4.3e-12, .Cgk = 9.0e-12, .Cag = 12.0e-12
    };

    // EF86 in triode-strapped configuration — small-signal pentode (g2
    // tied to plate).  Effective μ ≈ 38, low noise, high sensitivity:
    // the canonical "vintage British input stage" tube — Vox AC30
    // (pre-Top Boost), 1959 Selmer Treble-N-Bass, Selmer's Truvoice.
    // Tonal character: lots of midrange, slightly soft top, lower
    // headroom than 12AX7 with a "creamy" overdrive at the limit.
    //
    // We use the triode-strapped variant deliberately: the full
    // pentode model needs a separate g2-current law that the Dempwolf
    // triode framework doesn't accommodate, while triode-strapped EF86
    // is simply a triode and falls in the framework's scope.
    //
    // Calibration: Mullard EF86 datasheet triode-strap operating point
    // (Vp = 250 V, Vg = −2 V → Ip ≈ 3 mA).  Koren fit Kg ≈ 13000,
    // mapped to Dempwolf G = 1/Kg = 0.77e-3.
    // Recalibrated to the Mullard triode-strap point Ip(250 V, −2 V)
    // = 3 mA (was 6.9 mA — the same ~2.3× load-line hand compensation).
    constexpr DempwolfParams kEF86_TriodeStrapped = {
        .G = 3.565e-4, .mu = 38.0, .gamma = 1.40, .C = 3.60,
        .Gg = 5.0e-4, .xi = 1.30, .Cg = 10.0, .Ig0 = 8.0e-8,
        .Cak = 1.0e-12, .Cgk = 3.0e-12, .Cag = 0.06e-12
    };

    // 6AS6 / 5725 variable-mu pentode, represented in the triode solver as
    // the triode-equivalent control path used by Culture Vulture-style
    // distortion stages.  The extra variable-mu behaviour is applied by
    // TubeStageConfig because the real tube's suppressor-grid control is a
    // signal-dependent effect rather than a static plate-curve fit.
    constexpr DempwolfParams k6AS6_VariableMu = {
        .G = 1.15e-3, .mu = 28.0, .gamma = 1.62, .C = 4.10,
        .Gg = 5.5e-4, .xi = 1.38, .Cg = 12.0, .Ig0 = 1.2e-7,
        .Cak = 2.2e-12, .Cgk = 4.6e-12, .Cag = 0.18e-12
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

    // Division-region multiplier (docs/34 §3.1): smooth logistic in
    // (Vg − Va) that leaves the fitted law untouched for Va ≫ Vg and
    // multiplies Ig by up to (1 + divK) when the plate dips below the
    // grid (the grid then competes directly for the space current).
    double gridDivisionFactor(double Vg, double Va) const noexcept
    {
        if (p_.divK <= 0.0) return 1.0;
        const double x = (Vg - Va) / std::max(p_.divV, 1.0e-3);
        if (x < -20.0) return 1.0;
        return 1.0 + p_.divK / (1.0 + std::exp(-x));
    }

    /// Grid current including the plate-competition division region —
    /// pass the (one-sample-stale) plate-cathode voltage from the solver.
    double gridCurrent(double Vg, double Va) const noexcept
    {
        return gridCurrent(Vg) * gridDivisionFactor(Vg, Va);
    }

    // Grid current with its analytic slope dIg/dVg in one pass — the
    // grid-conduction Newton in TubeStage needs both, and the closed
    // form (same chain rule as evalWithDerivatives' grid branch) saves
    // the two finite-difference evaluations per iteration.
    struct IgDeriv { double Ig; double dIg; };
    IgDeriv gridCurrentWithDeriv(double Vg) const noexcept
    {
        const double cgv = p_.Cg * Vg;
        double sp_g, sig_g;
        if (cgv > 20.0)       { sp_g = cgv;          sig_g = 1.0;  }
        else if (cgv < -20.0) { sp_g = std::exp(cgv); sig_g = sp_g; }
        else
        {
            const double e = std::exp(cgv);
            sp_g  = std::log1p(e);
            sig_g = e / (1.0 + e);
        }
        const double sg = sp_g / p_.Cg;
        IgDeriv r { p_.Ig0, 0.0 };
        if (sg > 0.0)
        {
            const double sgPowXi = std::pow(sg, p_.xi);
            r.Ig  += p_.Gg * sgPowXi;
            r.dIg  = p_.xi * p_.Gg * (sgPowXi / sg) * sig_g;
        }
        return r;
    }

    /// Grid current + slope including the division region (product rule:
    /// the logistic also moves with Vg at fixed plate).  Used by the
    /// grid-conduction Newton so its step stays exact deep in overdrive.
    IgDeriv gridCurrentWithDeriv(double Vg, double Va) const noexcept
    {
        IgDeriv r = gridCurrentWithDeriv(Vg);
        if (p_.divK <= 0.0) return r;
        const double w = std::max(p_.divV, 1.0e-3);
        const double x = (Vg - Va) / w;
        if (x < -20.0) return r;
        const double sig = 1.0 / (1.0 + std::exp(-x));
        const double f   = 1.0 + p_.divK * sig;
        const double df  = p_.divK * sig * (1.0 - sig) / w;
        r.dIg = r.dIg * f + r.Ig * df;
        r.Ig *= f;
        return r;
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

    // ─────────────────────────────────────────────────────────────────────
    // Analytical evaluation: Ip and its partial derivatives in one pass.
    //
    // The Newton-Raphson solvers in PushPullStage and TubeStage's compound
    // topologies (SRPP / Cascode) need both Ip and its slope at the current
    // operating point.  Computing the slope via finite differences burns 2
    // extra plateCurrent calls per probe — at audio rate that's 4–6 extra
    // softplus evaluations per sample, which dominates the per-sample cost
    // for those topologies (Marshall preset measured at ~52% CPU @ 4× OS
    // before this optimisation).
    //
    // Closed-form derivation:
    //   Veff   = (μ·Vg + Vp) / μ        = Vg + Vp/μ
    //   s      = softplus(C·Veff) / C
    //   sig    = sigmoid(C·Veff) = ds/dVeff   (chain-rule into softplus)
    //   Ik     = G · s^γ
    //   ∂Ik/∂Vg = G·γ·s^(γ−1)·sig·(∂Veff/∂Vg)  with ∂Veff/∂Vg = 1
    //   ∂Ik/∂Vp = G·γ·s^(γ−1)·sig·(1/μ)
    //   sg     = softplus(Cg·Vg) / Cg
    //   sig_g  = sigmoid(Cg·Vg)
    //   Ig     = Gg·sg^ξ + Ig0
    //   ∂Ig/∂Vg = Gg·ξ·sg^(ξ−1)·sig_g
    //   ∂Ig/∂Vp = 0                     (grid current ignores plate)
    //   Ip     = Ik − Ig
    //   gm    = ∂Ip/∂Vg = ∂Ik/∂Vg − ∂Ig/∂Vg
    //   rpInv = ∂Ip/∂Vp = ∂Ik/∂Vp        (= gm_k/μ where gm_k is the cathode-
    //                                       current gm before grid-current
    //                                       correction)
    // ─────────────────────────────────────────────────────────────────────
    struct IpDerivatives
    {
        double Ip;       ///< plate current [A]
        double gm;       ///< ∂Ip/∂Vg     [A/V]
        double rpInv;    ///< ∂Ip/∂Vp     [A/V]  (1/rp)
    };

    IpDerivatives evalWithDerivatives(double Vp, double Vg) const noexcept
    {
        IpDerivatives r {};

        // Cathode-current branch.
        const double Veff = Vg + Vp / p_.mu;     // = (μ·Vg + Vp) / μ
        const double cv   = p_.C * Veff;

        double sp;     // softplus(cv)
        double sig;    // sigmoid(cv) = ds/dVeff·(1/...) — the chain-rule factor
        if (cv > 20.0)
        {
            sp  = cv;
            sig = 1.0;
        }
        else if (cv < -20.0)
        {
            sp  = std::exp(cv);
            sig = sp;        // sigmoid(x) ≈ exp(x) for very negative x
        }
        else
        {
            const double e = std::exp(cv);
            sp  = std::log1p(e);
            sig = e / (1.0 + e);
        }
        const double s = sp / p_.C;

        double Ik       = 0.0;
        double dIk_dVg  = 0.0;
        if (s > 0.0)
        {
            // One pow instead of two: s^(γ−1) = s^γ / s exactly (s > 0
            // in this branch) — the second std::pow was ~15% of the
            // whole evaluate() on the profile (docs/35 D2).
            const double sPowG = std::pow(s, p_.gamma);
            Ik = p_.G * sPowG;
            // pow_term = G·γ·s^(γ−1).  Multiplied by sig gives the
            // chain-rule completion — ds/dVeff at Vg-derivative.
            const double pow_term = p_.gamma * p_.G * (sPowG / s);
            dIk_dVg = pow_term * sig;
        }

        // Grid-current branch.
        const double cgv = p_.Cg * Vg;
        double sp_g, sig_g;
        if (cgv > 20.0)
        {
            sp_g  = cgv;
            sig_g = 1.0;
        }
        else if (cgv < -20.0)
        {
            sp_g  = std::exp(cgv);
            sig_g = sp_g;
        }
        else
        {
            const double e = std::exp(cgv);
            sp_g  = std::log1p(e);
            sig_g = e / (1.0 + e);
        }
        const double sg = sp_g / p_.Cg;

        double Ig      = p_.Ig0;
        double dIg_dVg = 0.0;
        if (sg > 0.0)
        {
            const double sgPowXi = std::pow(sg, p_.xi);
            Ig += p_.Gg * sgPowXi;
            const double pow_g = p_.xi * p_.Gg * (sgPowXi / sg);
            dIg_dVg = pow_g * sig_g;
        }

        r.Ip    = Ik - Ig;
        r.gm    = dIk_dVg - dIg_dVg;
        r.rpInv = dIk_dVg / p_.mu;       // ∂Ig/∂Vp = 0 by construction
        return r;
    }

private:
    DempwolfParams p_;
};

} // namespace valvra::dsp
