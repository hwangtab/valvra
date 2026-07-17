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
#include "KorenPentode.h"
#include "JilesAtherton.h"
#include <algorithm>

namespace valvra::dsp {

enum class VariationDistribution : int
{
    Modern = 0,
    Vintage = 1,
    Worn = 2,
    Wild = 3
};

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
    double Rp_scale       {1.0};  // Plate load resistor — independent draw
                                  // (legacy reused Rk_scale, which made the
                                  // two resistors track each other: real
                                  // units never correlate that way)
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

    // Expansion-engine spreads (tape oxide pinning, transport wow depth,
    // T4 photocell trap-level τ) — real units differ here as much as in
    // the tube section.
    double tapeCoreK_scale        {1.0};
    double tapeWow_scale          {1.0};
    double optoMemoryTau_scale    {1.0};

    // Heater thermal mass spread: warm-up τ varies tube-to-tube within
    // the documented 15–30 s class.
    double warmupTau_scale        {1.0};

    // ─── Era-dependent component pathologies (docs/34 §3.6, §3.7) ──────
    // Coupling-cap leakage: an aged paper/wax cap leaks the PREVIOUS
    // stage's plate DC onto this grid through the grid-leak divider
    // Rg/(R_leak + Rg), warming the bias by up to volts — the classic
    // reason two channels of one vintage console never bias alike.
    // Stored as the divider RATIO (0 = modern film, effectively ∞ R_leak).
    double couplingLeak_ratio     {0.0};
    // Carbon-composition resistors add DC-bias-proportional 1/f excess
    // noise (10–30 dB above metal film) — an ADDITIVE contribution to the
    // stage's flicker ratio on vintage/worn/wild populations.
    double excessNoise_offset     {0.0};

    // Matched-pair residual mismatch spread (push-pull power tubes, LTP
    // halves).  "Matched" means binned, not identical — the bin width
    // itself varies unit to unit.
    double pairMismatch_scale     {1.0};
};

// ─────────────────────────────────────────────────────────────────────────────
// Generate a per-instance variation from a seed. Same seed → same variation.
// Uses truncated-Gaussian perturbations with academically justified σ values.
// ─────────────────────────────────────────────────────────────────────────────
inline ComponentVariation makeVariation(
    uint64_t seed,
    VariationDistribution distribution = VariationDistribution::Modern) noexcept
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

    double tubeSigma = 0.08;
    double tubeGSigma = 0.12;      // perveance: Dempwolf's three real
                                   // tubes span ±24% — wider than μ
    double passiveSigma = 1.0;
    double resistorSigma = 0.01;   // modern 1% metal film
    double transformerSigma = 1.0;
    double hiddenSigma = 1.0;
    double ageTilt = 1.0;

    switch (distribution)
    {
        case VariationDistribution::Vintage:
            // Vintage-era passives were 5–10% carbon composition — far
            // LOOSER than modern film (docs/06 §3); only the tubes were
            // hand-selected tighter at the factory.
            tubeSigma = 0.05;
            tubeGSigma = 0.08;
            passiveSigma = 0.9;
            resistorSigma = 0.05;
            transformerSigma = 0.80;
            hiddenSigma = 0.80;
            ageTilt = 0.97;
            break;
        case VariationDistribution::Worn:
            tubeSigma = 0.12;
            tubeGSigma = 0.16;
            passiveSigma = 1.45;
            resistorSigma = 0.08;  // drifted carbon comp
            transformerSigma = 1.20;
            hiddenSigma = 1.55;
            ageTilt = 0.90;
            break;
        case VariationDistribution::Wild:
            tubeSigma = 0.18;
            tubeGSigma = 0.24;
            passiveSigma = 2.00;
            resistorSigma = 0.12;
            transformerSigma = 1.70;
            hiddenSigma = 2.20;
            ageTilt = 0.86;
            break;
        case VariationDistribution::Modern:
        default:
            break;
    }

    // Tube: μ σ = 8% baseline (Dempwolf spread), perveance wider.
    // Distribution presets scale the spread; ageTilt models broad gm
    // loss on worn/wild populations.
    //
    // G and μ co-vary weakly (ρ ≈ 0.4): both ride the same cathode
    // geometry / emission lottery in a real bottle, so a hot-perveance
    // tube tends to sit slightly high in μ too.  The construction below
    // keeps each MARGINAL σ exactly as before — only the joint shape
    // changes (docs/34 §1.6 residual).
    {
        constexpr double kRho = 0.4;
        auto clip3 = [&]() {
            double x = n(rng);
            return std::clamp(x, -3.0, 3.0);
        };
        const double zc = clip3();
        const double z1 = clip3();
        const double z2 = clip3();
        const double gG  = std::sqrt(kRho) * zc + std::sqrt(1.0 - kRho) * z1;
        const double gMu = std::sqrt(kRho) * zc + std::sqrt(1.0 - kRho) * z2;
        v.tube_G_scale  = (1.0 + tubeGSigma * gG) * ageTilt;
        v.tube_mu_scale = 1.0 + tubeSigma * gMu;
    }
    v.tube_gamma_scale = clampedGauss(0.02 * hiddenSigma);   // γ is stable

    // Transformer: σ = 6–10%
    v.trafo_Ms_scale = clampedGauss(0.06 * transformerSigma);
    v.trafo_a_scale  = clampedGauss(0.10 * transformerSigma);
    v.trafo_k_scale  = clampedGauss(0.12 * transformerSigma);

    // Passives.  Electrolytics use a LOGNORMAL draw: the real tolerance
    // window is asymmetric (−20% / +80% — etching variance only ever
    // ADDS capacitance), so a symmetric Gaussian mis-shapes the
    // population that sets every bypass/bounce time constant.
    v.Rk_scale       = clampedGauss(resistorSigma);
    v.Rp_scale       = clampedGauss(resistorSigma);
    v.Ck_scale       = std::exp(clampedGaussAdditive(0.20 * passiveSigma)
                                + 0.08 * passiveSigma);
    v.coupling_scale = clampedGauss(0.05 * passiveSigma);     // film cap

    // PSU: reservoir caps drift, transformer DCR varies
    v.Vb_scale      = clampedGauss(0.02 * passiveSigma);
    v.Zsupply_scale = clampedGauss(0.08 * passiveSigma);

    // ─── Hidden-physics perturbations ──────────────────────────────────
    // Tolerances chosen so two random instances sound *different* in the
    // same mechanism, but no single one blows up.  σ values are loosely
    // calibrated to docs/06 §3 (passive tolerances) and docs/03 §2/3/5/7
    // (time-varying behaviour variability).
    // Strictly-positive quantities draw LOGNORMAL so wide (Worn/Wild)
    // tails can never flip a hum amplitude or soakage coefficient
    // negative — a sign flip there is not "more variation", it is a
    // different (nonexistent) circuit.
    auto logGauss = [&](double sigma) {
        return std::exp(clampedGaussAdditive(sigma));
    };
    v.gridLeakR_scale        = clampedGauss(0.05 * passiveSigma);
    v.gridCouplingC_scale    = clampedGauss(0.10 * passiveSigma);   // film/electrolytic
    v.gridVon_offset         = std::clamp(
        clampedGaussAdditive(0.08 * hiddenSigma), -0.35, 0.35);     // contact potential
    v.heaterHum_scale        = logGauss(0.30 * hiddenSigma);   // heater-to-cathode
                                                     // coupling is loose
    v.soakageAmt_scale       = logGauss(0.25 * hiddenSigma);   // aged caps drift
    v.soakageTau_scale       = logGauss(0.15 * hiddenSigma);
    v.ripple_scale           = logGauss(0.20 * hiddenSigma);   // line-voltage +
                                                     // reservoir-cap skew
    v.thermalSens_scale      = logGauss(0.15 * hiddenSigma);   // emission spread
    v.slewPos_scale          = clampedGauss(0.08 * hiddenSigma);   // tied to gm
    v.slewNeg_scale          = clampedGauss(0.08 * hiddenSigma);
    v.presenceFreq_scale     = clampedGauss(0.04 * transformerSigma);   // ~640 Hz @ 3σ on
                                                     // a 16 kHz peak
    v.presenceGain_offset_dB = clampedGaussAdditive(0.4 * transformerSigma);   // ±1.2 dB 3σ

    v.tapeCoreK_scale     = clampedGauss(0.12 * hiddenSigma);
    v.tapeWow_scale       = clampedGauss(0.15 * hiddenSigma);
    v.optoMemoryTau_scale = logGauss(0.25 * hiddenSigma);
    v.warmupTau_scale     = logGauss(0.18 * hiddenSigma);
    v.pairMismatch_scale  = logGauss(0.40 * hiddenSigma);

    // ─── Era-dependent pathologies (docs/34 §3.6, §3.7) ─────────────────
    // Hard zero on Modern: film caps effectively don't leak and metal-film
    // resistors have negligible excess noise — these are properties the
    // component ERA either has or hasn't, not a continuum around modern.
    // Lognormal draws elsewhere: leakage resistance spans decades unit to
    // unit (paper/wax ~1 GΩ fresh → single-digit MΩ badly aged), and the
    // divider ratio Rg/(R_leak+Rg) inherits that spread.
    {
        double leakCenter = 0.0, leakCap = 0.0, noiseCenter = 0.0;
        switch (distribution)
        {
            case VariationDistribution::Vintage:
                leakCenter = 1.0e-3;  leakCap = 0.02;  noiseCenter = 0.10;
                break;
            case VariationDistribution::Worn:
                leakCenter = 4.0e-3;  leakCap = 0.03;  noiseCenter = 0.22;
                break;
            case VariationDistribution::Wild:
                leakCenter = 1.0e-2;  leakCap = 0.05;  noiseCenter = 0.40;
                break;
            case VariationDistribution::Modern:
            default:
                break;
        }
        if (leakCenter > 0.0)
        {
            v.couplingLeak_ratio = std::min(
                leakCenter * std::exp(clampedGaussAdditive(0.8)), leakCap);
            v.excessNoise_offset = std::min(
                noiseCenter * std::exp(clampedGaussAdditive(0.5)), 0.45);
        }
    }

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

// Pentodes spread at least as much as triodes — emission sets the space
// current (G, G2 jointly: both come off the same cathode) and grid
// geometry sets the screen leverage (muG2, the pentode's μ analogue).
inline PentodeParams applyVariation(const PentodeParams& base,
                                    const ComponentVariation& v) noexcept
{
    PentodeParams p = base;
    p.G    *= v.tube_G_scale;
    p.G2   *= v.tube_G_scale;
    p.muG2 *= v.tube_mu_scale;
    return p;
}

} // namespace valvra::dsp
