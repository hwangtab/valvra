// ─────────────────────────────────────────────────────────────────────────────
// ComponentVariation — per-instance Monte Carlo parameter perturbation
//
// Every instance of the plugin is a "different unit" — tubes, capacitors,
// resistors all drift within manufacturer tolerance. This is what makes
// a rack of Pultecs sound like a rack of Pultecs rather than N copies of
// the same function.
//
// Academic basis: Dempwolf 2011 measured three real 12AX7 tubes with
// μ = 86.9 / 100.2 / 103.2 — a 17% spread. This justifies σ ≈ 8% for a
// population-representative Gaussian perturbation.
//
// Tolerance ranges (docs/06, docs/22 §A):
//   Tubes    : μ σ = 8–10 %, gm σ = 10–15 %, rp σ = 10–15 %
//   Resistors: 1% film → σ = 0.3 %, 5% carbon → bimodal (selection truncation)
//   Caps     : Film ±2–5 %, Electrolytic −20% / +80%
//
// Reproducibility: state_seed is saved/loaded with the plugin, so the
// same project file always produces identical sonic character.
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <cstdint>
#include <random>

#include "KorenTriode.h"
#include "JilesAtherton.h"
#include <algorithm>

namespace valvra::dsp {

struct ComponentVariation
{
    uint64_t seed {0};

    // Tube parameter scales (multiplicative)
    double tube_G_scale     {1.0};
    double tube_mu_scale    {1.0};
    double tube_gamma_scale {1.0};

    // Transformer JA scales
    double trafo_Ms_scale {1.0};
    double trafo_a_scale  {1.0};
    double trafo_k_scale  {1.0};

    // Passive component scales
    double Rk_scale       {1.0};  // Cathode resistor (±1% film)
    double Ck_scale       {1.0};  // Cathode cap (±20% electrolytic)
    double coupling_scale {1.0};  // Interstage coupling cap

    // Power supply scales
    double Vb_scale       {1.0};
    double Zsupply_scale  {1.0};

    // ─── Hidden-physics per-instance variation ──────────────────────────
    // Every new mechanism that gives Valvra its differentiated vintage
    // feel also varies unit-to-unit in real hardware.  Without Monte
    // Carlo on these, multiple instances of the plugin would all have
    // IDENTICAL "vintage mojo" — which defeats half the point.
    double gridLeakR_scale        {1.0};  ///< grid-leak R
    double gridCouplingC_scale    {1.0};  ///< input coupling cap
    double gridVon_offset         {0.0};  ///< g-k diode threshold (V)
    double heaterHum_scale        {1.0};  ///< hum amplitude
    double soakageAmt_scale       {1.0};  ///< cap DA coefficient
    double soakageTau_scale       {1.0};  ///< DA relaxation τ
    double ripple_scale           {1.0};  ///< B+ ripple level
    double thermalSens_scale      {1.0};  ///< plate thermal sensitivity
    double slewPos_scale          {1.0};  ///< rising-edge rate
    double slewNeg_scale          {1.0};  ///< falling-edge rate
    double presenceFreq_scale     {1.0};  ///< transformer peak centre
    double presenceGain_offset_dB {0.0};  ///< transformer peak boost
};

// ─────────────────────────────────────────────────────────────────────────────
// Generate a per-instance variation from a seed. Same seed → same variation.
// Uses truncated-Gaussian perturbations with academically justified σ values.
// ─────────────────────────────────────────────────────────────────────────────
inline ComponentVariation makeVariation(uint64_t seed) noexcept
{
    ComponentVariation v;
    v.seed = seed;

    std::mt19937_64 rng { seed };
    std::normal_distribution<double> n { 0.0, 1.0 };

    auto clampedGauss = [&](double sigma, double clip = 3.0) {
        double x = n(rng);
        if      (x >  clip) x =  clip;
        else if (x < -clip) x = -clip;
        return 1.0 + sigma * x;
    };
    auto clampedGaussAdditive = [&](double sigma, double clip = 3.0) {
        double x = n(rng);
        if      (x >  clip) x =  clip;
        else if (x < -clip) x = -clip;
        return sigma * x;
    };

    // Tube: σ = 8% (Dempwolf spread)
    v.tube_G_scale     = clampedGauss(0.08);
    v.tube_mu_scale    = clampedGauss(0.08);
    v.tube_gamma_scale = clampedGauss(0.02);   // γ is stable

    // Transformer: σ = 6–10%
    v.trafo_Ms_scale = clampedGauss(0.06);
    v.trafo_a_scale  = clampedGauss(0.10);
    v.trafo_k_scale  = clampedGauss(0.12);

    // Passives
    v.Rk_scale       = clampedGauss(0.01);     // 1% resistor
    v.Ck_scale       = clampedGauss(0.15);     // electrolytic skew high
    v.coupling_scale = clampedGauss(0.05);     // film cap

    // PSU: reservoir caps drift, transformer DCR varies
    v.Vb_scale      = clampedGauss(0.02);
    v.Zsupply_scale = clampedGauss(0.08);

    // ─── Hidden-physics perturbations ──────────────────────────────────
    // Tolerances chosen so two random instances sound *different* in the
    // same mechanism, but no single one blows up.  σ values are loosely
    // calibrated to docs/06 §3 (passive tolerances) and docs/03 §2/3/5/7
    // (time-varying behaviour variability).
    v.gridLeakR_scale        = clampedGauss(0.05);
    v.gridCouplingC_scale    = clampedGauss(0.10);   // film/electrolytic
    v.gridVon_offset         = clampedGaussAdditive(0.08);   // ±0.24 V @3σ
    v.heaterHum_scale        = clampedGauss(0.30);   // heater-to-cathode
                                                     // coupling is loose
    v.soakageAmt_scale       = clampedGauss(0.25);   // aged caps drift
    v.soakageTau_scale       = clampedGauss(0.15);
    v.ripple_scale           = clampedGauss(0.20);   // line-voltage +
                                                     // reservoir-cap skew
    v.thermalSens_scale      = clampedGauss(0.15);   // emission spread
    v.slewPos_scale          = clampedGauss(0.08);   // tied to gm
    v.slewNeg_scale          = clampedGauss(0.08);
    v.presenceFreq_scale     = clampedGauss(0.04);   // ~640 Hz @ 3σ on
                                                     // a 16 kHz peak
    v.presenceGain_offset_dB = clampedGaussAdditive(0.4);   // ±1.2 dB 3σ

    return v;
}

// ─────────────────────────────────────────────────────────────────────────────
// Apply variation to base Dempwolf parameters → return a varied copy.
// Pure function: no side effects, no allocations.
// ─────────────────────────────────────────────────────────────────────────────
inline DempwolfParams applyVariation(const DempwolfParams& base,
                                     const ComponentVariation& v) noexcept
{
    DempwolfParams p = base;
    p.G     *= v.tube_G_scale;
    p.mu    *= v.tube_mu_scale;
    p.gamma *= v.tube_gamma_scale;
    return p;
}

inline JAParams applyVariation(const JAParams& base,
                               const ComponentVariation& v) noexcept
{
    JAParams p = base;
    p.Ms *= v.trafo_Ms_scale;
    p.a  *= v.trafo_a_scale;
    p.k  *= v.trafo_k_scale;
    return p;
}

} // namespace valvra::dsp
