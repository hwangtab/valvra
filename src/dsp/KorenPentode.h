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
    // 6AS6 / 5725 variable-mu pentode.  muG2 set to the tube's g1→g2
    // amplification factor (≈ the triode-strap μ = 28 used by the triode
    // proxy in KorenTriode.h) — the legacy 7.0 over-leveraged the screen
    // ~4×, which forced compensating errors elsewhere.  G/G2 refit at the
    // Tung-Sol point Va = 120 V, Vg2 = 120 V, Vg1 = −1 V → Ia ≈ 4 mA,
    // Ig2 ≈ 2.5 mA (high screen fraction is characteristic of the 6AS6).
    constexpr PentodeParams k6AS6 = {
        .G = 9.4e-4, .gamma = 1.62, .C = 4.6, .muG2 = 28.0,
        .g3Gain = 0.95, .g3Ref = 0.0, .g3Scale = 1.4,
        .kVb = 14.0, .kVbG2 = 0.10,
        .G2 = 4.2e-4, .gamma2 = 1.50,
        .secondaryGain = 0.26, .secondaryKnee = 10.0,
        .Gg1 = 5.8e-4, .xi = 1.34, .Cg1 = 11.5, .g1TurnOn = 0.30,
        .Ig1Leak = 1.1e-7
    };

    // EF86 pentode.  muG2 = 38 (the g1→g2 μ, identical to the tube's
    // triode-strap μ — the legacy 8.0 made Vg2/8 over-drive the control
    // law an order of magnitude).  G/G2 refit at the Mullard point
    // Va = 250 V, Vg2 = 140 V, Vg1 = −2 V → Ia ≈ 3 mA, Ig2 ≈ 0.6 mA.
    constexpr PentodeParams kEF86 = {
        .G = 1.66e-3, .gamma = 1.48, .C = 4.0, .muG2 = 38.0,
        .g3Gain = 0.30, .g3Ref = 0.0, .g3Scale = 2.4,
        .kVb = 18.0, .kVbG2 = 0.06,
        .G2 = 2.9e-4, .gamma2 = 1.42,
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
        double dIpdVa;     // ∂Ip/∂Va [A/V] — analytic, for the plate Newton
    };

    Currents evaluate(double Va, double Vg1, double Vg2, double Vg3) const noexcept
    {
        Currents c {};

        const double g3Scale = std::max(p_.g3Scale, 1.0e-3);
        const double g3Norm  = (Vg3 - p_.g3Ref) / g3Scale;

        // Koren-like effective control using g1 and g2.  The suppressor
        // does NOT belong here: it sits behind the screen, so it cannot
        // modulate the space current the cathode emits — it gates what
        // the PLATE collects (see plateShare below).  The legacy g3 term
        // inside Vctrl made suppressor drive choke the whole cathode
        // current, so hard suppressor bias REDUCED screen current — the
        // opposite of the measured 6AS6 behaviour the Culture Vulture
        // mode is built on.
        const double muG2 = std::max(p_.muG2, 1.0e-6);
        const double Vctrl = Vg1 + (Vg2 / muG2);
        const double s = softplus(p_.C * Vctrl) / p_.C;
        if (s > 0.0)
            c.Ispace = p_.G * std::pow(s, p_.gamma);

        // Positive g1 conduction — computed BEFORE the plate/screen
        // partition so it can be subtracted from the emitted space current.
        // A conducting control grid physically intercepts electrons that
        // then reach neither plate nor screen: charge conservation is
        // Ik = Ip + Ig2 + Ig1, not Ik = Ip + Ig2.  The legacy code
        // computed Ig1 only at the very end and never removed it from the
        // space current, so at hard drive (CV positive-peak conduction)
        // the grid-stolen electrons were double-counted at the plate.
        const double sg = softplus(p_.Cg1 * (Vg1 - p_.g1TurnOn)) / p_.Cg1;
        c.Ig1 = p_.Ig1Leak;
        if (sg > 0.0)
            c.Ig1 += p_.Gg1 * std::pow(sg, p_.xi);
        // Space current available to the plate + screen after the control
        // grid takes its share (clamped so a pathological Ig1 can never
        // drive the remainder negative).
        const double IspaceAvail = std::max(0.0, c.Ispace - c.Ig1);

        // Plate knee share: 2/pi * atan(Va / kVbEff), clamped to [0,1].
        const double kVbEff = std::max(
            1.0e-3,
            p_.kVb * (1.0 + p_.kVbG2 * std::max(0.0, Vg2)));
        const double VaPos = std::max(0.0, Va);
        constexpr double kTwoOverPi = 0.6366197723675814;
        const double atanArg = VaPos / kVbEff;
        const double psRaw = kTwoOverPi * std::atan(atanArg);
        c.plateShare = std::clamp(psRaw, 0.0, 1.0);
        // ∂plateShare/∂Va through the arctan (0 in the clamped region or at
        // Va ≤ 0): dPS = (2/π)·(1/kVbEff)/(1+(Va/kVbEff)²) · g3Collect,
        // finished after g3Collect below.
        double dPS = (psRaw > 0.0 && psRaw < 1.0 && Va > 0.0)
            ? kTwoOverPi / (kVbEff * (1.0 + atanArg * atanArg))
            : 0.0;

        // Suppressor collection gate: negative g3 repels plate-bound
        // electrons back toward the screen.  Unity at the reference bias
        // (no recalibration of existing operating points), falling toward
        // zero as g3 swings negative — the physical variable-mu mechanism
        // of the 6AS6, where plate gm collapses while the cathode keeps
        // emitting and the screen absorbs the difference.
        const double g3Collect = std::clamp(
            1.0 + p_.g3Gain * std::tanh(g3Norm), 0.0, 1.0);
        c.plateShare *= g3Collect;
        dPS *= g3Collect;

        // Charge-conserving partition: the space current divides between
        // plate and screen, Ik = Ip + Ig2 exactly.  The G2 law sets the
        // BASELINE screen fraction (geometry: grid wires intercepting the
        // beam at full plate voltage); whatever the plate-knee share does
        // not collect at low Va is routed to the screen, not destroyed —
        // this is precisely the current that spikes Ig2 and sags the
        // screen supply when the plate swings into the knee.  (The
        // previous formulation dropped up to 77% of the cathode current
        // at low Va, starving the pentode's signature compression loop.)
        double baseFrac = 0.0;
        if (s > 0.0 && c.Ispace > 1.0e-15)
            baseFrac = std::clamp(
                (p_.G2 * std::pow(s, p_.gamma2)) / c.Ispace, 0.0, 0.45);
        // Partition the AVAILABLE space current (after Ig1) between plate
        // and screen so Ik = Ip + Ig2 + Ig1 holds exactly.
        const double IpPre = IspaceAvail * c.plateShare * (1.0 - baseFrac);
        c.Ip  = IpPre;
        c.Ig2 = IspaceAvail - c.Ip;
        // ∂IpPre/∂Va — only plateShare depends on Va.
        const double dIpPre = IspaceAvail * (1.0 - baseFrac) * dPS;
        c.dIpdVa = dIpPre;

        // Secondary emission moves plate-emitted electrons to the screen
        // when Vg2 > Va — a plate→screen transfer, still conserving Ik.
        const double secondaryDrive = std::max(0.0, Vg2 - VaPos);
        if (secondaryDrive > 0.0)
        {
            const double knee = std::max(p_.secondaryKnee, 1.0e-6);
            const double secNorm = secondaryDrive / (secondaryDrive + knee);
            const double suppressorScale = 1.0 - 0.5 * std::tanh(g3Norm);
            const double fRaw = p_.secondaryGain * suppressorScale * secNorm;
            const double f = std::clamp(fRaw, 0.0, 0.9);
            const double iSec = f * IpPre;
            c.Ip  -= iSec;
            c.Ig2 += iSec;
            // Ip = IpPre·(1−f); ∂Ip/∂Va = ∂IpPre/∂Va·(1−f) − IpPre·∂f/∂Va.
            // ∂secDrive/∂Va = −1 (Va>0 & Vg2>VaPos); ∂secNorm/∂secDrive =
            // knee/(secDrive+knee)²; f clamped ⇒ ∂f/∂Va = 0 outside (0,0.9).
            double df = 0.0;
            if (fRaw > 0.0 && fRaw < 0.9 && Va > 0.0)
            {
                const double dSecNorm = knee
                    / ((secondaryDrive + knee) * (secondaryDrive + knee));
                df = p_.secondaryGain * suppressorScale * dSecNorm * (-1.0);
            }
            c.dIpdVa = dIpPre * (1.0 - f) - IpPre * df;
        }

        if (!std::isfinite(c.Ip) || c.Ip < 0.0) c.Ip = 0.0;
        if (!std::isfinite(c.dIpdVa)) c.dIpdVa = 0.0;
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
