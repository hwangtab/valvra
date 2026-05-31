// ─────────────────────────────────────────────────────────────────────────────
// KorenPentode — pentode/tetrode model with explicit screen + suppressor terms
//
// Base law:
//   - Space current uses a Koren-style control term driven by Vg1 and Vg2.
//   - Plate current uses an arctangent knee versus Va (Koren pentode idea).
//   - Screen current uses an explicit current law plus secondary-emission
//     pickup when Va < Vg2.
//   - Suppressor grid (Vg3) modulates both space current and secondary pickup.
//
// This is intentionally solver-friendly for real-time audio use and exposes
// both Ip and Ig2 so stage-level screen-supply dynamics can be modeled.
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include "KorenTriode.h"

#include <algorithm>
#include <cmath>

namespace valvra::dsp {

struct PentodeParams
{
    // Space-current law (Koren-like control branch).
    double G        { 1.15e-3 };   // Current scale [A/V^gamma]
    double gamma    { 1.55 };      // Exponent (≈ 1.3–1.8)
    double C        { 4.2 };       // Softplus smoothing
    double muG2     { 7.5 };       // Screen-grid leverage to g1 equivalent

    // Suppressor-grid coupling (g3). Negative g3 reduces conduction,
    // positive g3 opens it up (6AS6-like variable-mu behaviour).
    double g3Gain   { 0.65 };      // Equivalent g1 volts contribution
    double g3Ref    { 0.0 };       // Reference bias (usually cathode)
    double g3Scale  { 2.0 };       // tanh shaping scale

    // Plate-knee branch (Koren arctan style).
    double kVb      { 16.0 };      // Knee voltage at nominal screen [V]
    double kVbG2    { 0.08 };      // Knee growth vs screen voltage [V/V]

    // Screen-current law and secondary-emission pickup.
    double G2       { 2.4e-4 };    // Screen current scale [A/V^gamma2]
    double gamma2   { 1.45 };      // Screen exponent
    double secondaryGain { 0.18 }; // Extra Ig2 when Vg2 > Va
    double secondaryKnee { 12.0 }; // Soft knee for secondary emission [V]

    // Positive g1 conduction.
    double Gg1      { 5.0e-4 };
    double xi       { 1.30 };
    double Cg1      { 10.0 };
    double g1TurnOn { 0.35 };
    double Ig1Leak  { 8.0e-8 };
};

namespace pentode_params {
    // 6AS6 / 5725 variable-mu pentode nominal set.
    constexpr PentodeParams k6AS6 = {
        .G = 1.55e-3, .gamma = 1.62, .C = 4.6, .muG2 = 7.0,
        .g3Gain = 0.95, .g3Ref = 0.0, .g3Scale = 1.4,
        .kVb = 14.0, .kVbG2 = 0.10,
        .G2 = 3.1e-4, .gamma2 = 1.50,
        .secondaryGain = 0.26, .secondaryKnee = 10.0,
        .Gg1 = 5.8e-4, .xi = 1.34, .Cg1 = 11.5, .g1TurnOn = 0.30,
        .Ig1Leak = 1.1e-7
    };

    // EF86 pentode nominal set (full pentode, not triode-strapped).
    constexpr PentodeParams kEF86 = {
        .G = 8.6e-4, .gamma = 1.48, .C = 4.0, .muG2 = 8.0,
        .g3Gain = 0.30, .g3Ref = 0.0, .g3Scale = 2.4,
        .kVb = 18.0, .kVbG2 = 0.06,
        .G2 = 1.2e-4, .gamma2 = 1.42,
        .secondaryGain = 0.10, .secondaryKnee = 14.0,
        .Gg1 = 3.2e-4, .xi = 1.26, .Cg1 = 10.5, .g1TurnOn = 0.45,
        .Ig1Leak = 7.0e-8
    };
} // namespace pentode_params

class KorenPentode
{
public:
    explicit KorenPentode(PentodeParams p = pentode_params::k6AS6) noexcept
        : p_ { p } {}

    void setParams(const PentodeParams& p) noexcept { p_ = p; }
    const PentodeParams& params() const noexcept { return p_; }

    struct Currents
    {
        double Ip;         // plate current [A]
        double Ig2;        // screen current [A]
        double Ig1;        // control-grid current [A]
        double Ispace;     // total space-charge branch [A]
        double plateShare; // [0..1] arctan branch ratio
    };

    Currents evaluate(double Va, double Vg1, double Vg2, double Vg3) const noexcept
    {
        Currents c {};

        const double g3Scale = std::max(p_.g3Scale, 1.0e-3);
        const double g3Norm  = (Vg3 - p_.g3Ref) / g3Scale;
        const double g3Term  = p_.g3Gain * std::tanh(g3Norm);

        // Koren-like effective control using g1 and g2 plus explicit g3 term.
        const double muG2 = std::max(p_.muG2, 1.0e-6);
        const double Vctrl = Vg1 + (Vg2 / muG2) + g3Term;
        const double s = softplus(p_.C * Vctrl) / p_.C;
        if (s > 0.0)
            c.Ispace = p_.G * std::pow(s, p_.gamma);

        // Plate knee share: 2/pi * atan(Va / kVbEff), clamped to [0,1].
        const double kVbEff = std::max(
            1.0e-3,
            p_.kVb * (1.0 + p_.kVbG2 * std::max(0.0, Vg2)));
        const double VaPos = std::max(0.0, Va);
        constexpr double kTwoOverPi = 0.6366197723675814;
        c.plateShare = std::clamp(kTwoOverPi * std::atan(VaPos / kVbEff),
                                  0.0, 1.0);
        c.Ip = c.Ispace * c.plateShare;

        // Screen current base law (Koren eq.3 family).
        if (s > 0.0)
            c.Ig2 = p_.G2 * std::pow(s, p_.gamma2);

        // Secondary-emission pickup: rises when Vg2 is above Va.
        const double secondaryDrive = std::max(0.0, Vg2 - VaPos);
        if (secondaryDrive > 0.0)
        {
            const double secNorm = secondaryDrive
                / (secondaryDrive + std::max(p_.secondaryKnee, 1.0e-6));
            const double suppressorScale = 1.0 - 0.5 * std::tanh(g3Norm);
            c.Ig2 += p_.secondaryGain * suppressorScale * secNorm * c.Ispace;
        }

        // Positive g1 conduction.
        const double sg = softplus(p_.Cg1 * (Vg1 - p_.g1TurnOn)) / p_.Cg1;
        c.Ig1 = p_.Ig1Leak;
        if (sg > 0.0)
            c.Ig1 += p_.Gg1 * std::pow(sg, p_.xi);

        if (!std::isfinite(c.Ip) || c.Ip < 0.0) c.Ip = 0.0;
        if (!std::isfinite(c.Ig2) || c.Ig2 < 0.0) c.Ig2 = 0.0;
        if (!std::isfinite(c.Ig1) || c.Ig1 < 0.0) c.Ig1 = p_.Ig1Leak;
        if (!std::isfinite(c.Ispace) || c.Ispace < 0.0) c.Ispace = 0.0;
        if (!std::isfinite(c.plateShare)) c.plateShare = 0.0;
        return c;
    }

private:
    PentodeParams p_;
};

} // namespace valvra::dsp
