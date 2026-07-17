// ─────────────────────────────────────────────────────────────────────────────
// PowerSupplySag — dynamic B+ voltage compression under heavy signal load
//
// The B+ rail is not an ideal voltage source. Under sustained high plate
// current, the internal impedance Zsupply causes the rail to sag, which
// shifts the entire tube operating point — producing the characteristic
// "squish" of vintage tube rectifiers.
//
// Model:
//   ⟨Ip⟩      : envelope of plate current (RMS-like, τ = tau_sag)
//   Vb(t)     : B+ rail voltage
//   Vb(t)   = Vb_nom - ⟨Ip⟩(t) * Z_internal
//
// Typical values (docs/03, docs/22 §D.2):
//   GZ34/5AR4 (vacuum rectifier):  Z ≈ 200 Ω, sag 3–5 %, recovery 50–150 ms
//   5U4GB:                          Z ≈ 400–500 Ω, sag 10–15 %, recovery 100–200 ms
//   Solid-state bridge:             Z < 10 Ω, sag < 1 %
//
// References:
//   - docs/03-time-varying-nonlinearities.md §5
//   - docs/22-academic-quantitative-data.md §D.2
//   - Blencowe, M. (2012). Designing Valve Preamps (Ch. 5)
//   - Self, D. (2013). Audio Power Amplifier Design 6e.
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <algorithm>
#include <cmath>

namespace valvra::dsp {

struct PSUSagParams
{
    double Vb_nominal  {325.0};  ///< Nominal B+ [V] (Pultec-style)
    double Z_internal  {200.0};  ///< Effective source impedance [Ω]
    double tau_sag     {0.1};    ///< Envelope-follower time constant [s]
    double sampleRate  {48000.0};

    // ─── B+ rail ripple (100/120 Hz) ────────────────────────────────────
    // Full-wave rectification of the mains leaves a residual AC ripple on
    // the B+ rail at 2× line frequency (100 Hz in EU, 120 Hz in US).  The
    // smoothing choke + reservoir cap attenuate it but never to zero.
    // This ripple rides on top of the plate supply voltage, modulating
    // every tube stage's plate current at 120 Hz — which intermodulates
    // with the audio signal and produces "ghost" sidebands at f ± 120 Hz.
    // Those sidebands are what give Fender-era vintage amps their
    // characteristic gritty, "alive" feel.  Competing plugins almost
    // universally remove this in pursuit of a "clean" digital sound,
    // throwing away a core piece of the vintage character.
    //
    // Realistic ripple magnitude (docs/03 §5):
    //   Vacuum rectifier + single-π filter (GZ34): 80–150 mV rms
    //   Solid-state bridge + well-filtered:        10–30 mV rms
    double ripple_amp   {0.10};   ///< Peak ripple on B+ [V] (legacy path)
    double ripple_freq  {120.0};  ///< Ripple frequency [Hz]

    // ─── Physical rectifier + reservoir capacitor model ─────────────────
    // When enabled, the B+ node is an actual circuit simulation instead
    // of an envelope follower:
    //
    //   full-wave |sin| drive ──R_rect──▶──┬──── Vb (reservoir node)
    //            (diode: charges only      │
    //             while V_drive > Vc)     C_res        I_load (tubes)
    //                                      │              │
    //                                     GND ◀───────────┘
    //
    // Everything the envelope model faked now EMERGES:
    //   • sag amplitude  = conduction-angle-limited recharge (R_rect)
    //   • sag attack     = C discharging into the load surge
    //   • recovery       = R_rect·C — naturally SLOWER than the attack,
    //                      the vacuum-rectifier "bloom" asymmetry
    //   • ripple         = charge-pulse sawtooth whose amplitude scales
    //                      with load current (I/(2f·C)) and whose SHAPE
    //                      is the real asymmetric ramp, not a sine
    //   • tube vs SS     = R_rect (hundreds of Ω vs single digits)
    bool   enableReservoirModel { true };
    double reservoirFarads      { 47.0e-6 };  ///< C_res
    double rectifierOhms        { 0.0 };      ///< 0 → derive Z_internal/3

    // ─── Space-charge (Child-Langmuir) rectifier conduction (docs/34 §3.3)
    // A vacuum rectifier conducts as I ∝ ΔV^{3/2}: its internal resistance
    // is current-dependent, so the rail drop grows only as I^{2/3} and the
    // supply "firms up" progressively under load — the sag-then-compress
    // feel a linear resistor cannot produce.  A silicon bridge is nearly
    // ohmic (exponent → 1).  The charge conductance in process() scales as
    // ΔV^{rectExponent−1}, anchored to the calibrated rectChargeCoeff_ at
    // rectRefVolts so the documented Z_internal sag still holds at the
    // operating point while the curvature emerges around it.
    double rectExponent { 1.5 };   ///< 1.5 vacuum, 1.0 silicon (ohmic)
    double rectRefVolts { 20.0 };  ///< conduction-overvoltage anchor [V]

    // ─── Choke / π-filter second-order resonance (docs/34 §3.4) ─────────
    // A choke-input or C-L-C π filter puts a series inductor between the
    // reservoir cap and the output cap.  The L-C forms a low-Q resonance
    // (a few Hz to tens of Hz) so the rail's recovery from a transient sag
    // slightly OVERSHOOTS — the pumping "bounce" of GZ34 + choke supplies.
    // Off by default (a plain reservoir has no choke); vacuum-rectifier
    // presets opt in.  Output is taken at the post-choke cap; the resonance
    // cannot exceed the unloaded crest (clamped to vPeak_).
    bool   enableChoke   { false };
    double chokeHenries  { 10.0 };    ///< smoothing choke L [H]
    double chokeDCR      { 150.0 };   ///< choke winding resistance [Ω] (damps)
    double chokeOutFarads { 47.0e-6 };///< post-choke output cap [F]
};

// ─────────────────────────────────────────────────────────────────────────────
// Rectifier-based presets (Pultec uses 6X4; GZ34 provides similar character
// with stronger sag, Blencowe 2012).
// ─────────────────────────────────────────────────────────────────────────────
namespace psu_presets
{
    // NOTE: when adding new presets, remember to include ripple_amp /
    // ripple_freq — the defaults in PSUSagParams assume a vintage US
    // vacuum-rectifier rail.  Reservoir sizes follow period schematics:
    // vintage vacuum-rectified amps used 16–60 µF (the rectifier's peak-
    // current rating forbade more), solid-state rails use hundreds of µF.
    constexpr PSUSagParams kGZ34 = {
        .Vb_nominal = 325.0, .Z_internal = 200.0, .tau_sag = 0.1,
        .sampleRate = 48000.0,
        .ripple_amp = 0.12, .ripple_freq = 120.0,
        .enableReservoirModel = true,
        .reservoirFarads = 47.0e-6, .rectifierOhms = 0.0,
        .rectExponent = 1.5, .rectRefVolts = 20.0,   // vacuum space-charge
        .enableChoke = true, .chokeHenries = 10.0,   // classic GZ34 + choke
        .chokeDCR = 150.0, .chokeOutFarads = 47.0e-6
    };
    constexpr PSUSagParams k5U4GB = {
        .Vb_nominal = 325.0, .Z_internal = 450.0, .tau_sag = 0.15,
        .sampleRate = 48000.0,
        .ripple_amp = 0.20, .ripple_freq = 120.0,   // loosely filtered
        .enableReservoirModel = true,
        .reservoirFarads = 40.0e-6, .rectifierOhms = 0.0,
        .rectExponent = 1.5, .rectRefVolts = 25.0,   // higher-drop rectifier
        .enableChoke = true, .chokeHenries = 8.0,
        .chokeDCR = 180.0, .chokeOutFarads = 40.0e-6
    };
    constexpr PSUSagParams kSolidState = {
        .Vb_nominal = 325.0, .Z_internal = 5.0, .tau_sag = 0.01,
        .sampleRate = 48000.0,
        .ripple_amp = 0.02, .ripple_freq = 120.0,   // much cleaner
        .enableReservoirModel = true,
        .reservoirFarads = 220.0e-6, .rectifierOhms = 0.0,
        .rectExponent = 1.0, .rectRefVolts = 20.0    // silicon: ohmic diode
    };
    constexpr PSUSagParams k6X4_Pultec = {          // Pultec's rectifier
        .Vb_nominal = 350.0, .Z_internal = 250.0, .tau_sag = 0.12,
        .sampleRate = 48000.0,
        .ripple_amp = 0.10, .ripple_freq = 120.0,
        .enableReservoirModel = true,
        .reservoirFarads = 22.0e-6, .rectifierOhms = 0.0,
        .rectExponent = 1.5, .rectRefVolts = 20.0    // vacuum 6X4, no choke
    };
}

class PowerSupplySag
{
public:
    explicit PowerSupplySag(PSUSagParams p = psu_presets::k6X4_Pultec) noexcept
        : p_ { p }, Ip_avg_ { 0.0 }
    {
        updateCoeff();
    }

    void setParams(const PSUSagParams& p) noexcept
    {
        p_ = p;
        updateCoeff();
    }

    void reset() noexcept
    {
        Ip_avg_ = 0.0;
        ripplePhase_ = 0.0;
        Vc_ = p_.Vb_nominal;
        Vc2_ = p_.Vb_nominal;
        iChoke_ = 0.0;
        vbSlow_ = p_.Vb_nominal;
    }

    /// Carry the rail's slow state (reservoir charge, choke current, sag
    /// envelope, ripple phase) from a previous incarnation after a
    /// parameter-edit rebuild (docs/34 §4.3) — the supply keeps sagging
    /// through an automation move instead of snapping to nominal.
    void carryStateFrom(const PowerSupplySag& o) noexcept
    {
        auto fin = [](double v, double fb)
        { return std::isfinite(v) ? v : fb; };
        const double vCap = p_.Vb_nominal * 1.2;
        Vc_     = std::clamp(fin(o.Vc_, p_.Vb_nominal), 0.0, vCap);
        Vc2_    = std::clamp(fin(o.Vc2_, p_.Vb_nominal), 0.0, vCap);
        iChoke_ = fin(o.iChoke_, 0.0);
        Ip_avg_ = std::max(0.0, fin(o.Ip_avg_, 0.0));
        ripplePhase_ = fin(o.ripplePhase_, 0.0);
        vbSlow_ = std::clamp(fin(o.vbSlow_, p_.Vb_nominal), 0.0, vCap);
    }

    /// Seed the ripple oscillator's phase.  Chain/processor sets a
    /// per-instance value derived from the Monte Carlo seed so two
    /// Valvra instances on different tracks don't sum coherently at
    /// 120 Hz (same idea as the heater-hum phase scatter).
    void setRipplePhase(double phase) noexcept { ripplePhase_ = phase; }

    // Feed current plate current, returns sagged B+ voltage for use by
    // the tube model in the same sample.
    //
    // NaN-recovery: a single non-finite Ip would otherwise pin Ip_avg_ to
    // NaN forever, breaking every downstream stage.  Reset the envelope
    // and return nominal B+ if that happens.
    double process(double Ip_current) noexcept
    {
        if (! std::isfinite(Ip_current) || ! std::isfinite(Ip_avg_)
                                         || ! std::isfinite(Vc_)
                                         || ! std::isfinite(Vc2_)
                                         || ! std::isfinite(iChoke_)
                                         || ! std::isfinite(vbSlow_)
                                         || ! std::isfinite(ripplePhase_))
        {
            Ip_avg_      = 0.0;
            ripplePhase_ = 0.0;
            Vc_          = p_.Vb_nominal;
            Vc2_         = p_.Vb_nominal;
            iChoke_      = 0.0;
            vbSlow_      = p_.Vb_nominal;
            return p_.Vb_nominal;
        }
        const double abs_ip = std::abs(Ip_current);
        Ip_avg_ = alpha_ * Ip_avg_ + (1.0 - alpha_) * abs_ip;

        // Advance the 2×line-frequency ripple oscillator.  Kept in lock-step
        // with each call to process() so it runs at whatever internal rate
        // the PSU is configured for (base SR or upsampled SR).
        constexpr double kTwoPi = 2.0 * 3.14159265358979323846;
        ripplePhase_ += ripplePhaseInc_;
        if (ripplePhase_ >= kTwoPi) ripplePhase_ -= kTwoPi;

        if (p_.enableReservoirModel)
        {
            // Physical reservoir node.  Full-wave drive |cos| peaks twice
            // per line cycle (phase already advances at 2·f_line).  The
            // rectifier conducts only while the winding is above the cap
            // voltage — short charge pulses near each crest, exactly the
            // real conduction-angle mechanism.
            const double vDrive =
                vPeak_ * std::abs(std::cos(0.5 * ripplePhase_));
            const double dt = 1.0 / p_.sampleRate;

            if (vDrive > Vc_)
            {
                const double overV = vDrive - Vc_;
                // Space-charge (Child-Langmuir) conduction: the charge
                // conductance scales as ΔV^{exponent−1}.  Anchored so that
                // at rectRefV_ the coefficient equals the Z_internal-
                // calibrated rectChargeCoeff_ — vacuum rectifiers (exp 1.5)
                // then firm up under load, silicon (exp 1.0) stays ohmic.
                double conduct = rectChargeCoeff_;
                if (rectExpM1_ != 0.0)
                {
                    const double r = std::max(overV, 1.0e-9) / rectRefV_;
                    // Fast path for the canonical 3/2 law (exponent−1 = 0.5):
                    // sqrt is far cheaper than pow in the per-sample hot loop.
                    conduct *= (rectExpM1_ == 0.5)
                        ? std::sqrt(r) : std::pow(r, rectExpM1_);
                }
                conduct = std::clamp(conduct, 0.0, 1.0);
                Vc_ += overV * conduct;                     // charge pulse
            }

            // The reservoir is discharged by whatever draws from it: the
            // load directly (no choke) or the choke current (choke fed).
            const double reservoirDraw = p_.enableChoke ? iChoke_ : abs_ip;
            Vc_ -= (reservoirDraw * dt) / cRes_;
            Vc_ = std::max(Vc_, 0.0);

            if (p_.enableChoke)
            {
                // Series R+L choke from the reservoir Vc to the output cap
                // Vc2, load on Vc2.  The L-C resonance makes recovery from a
                // transient sag overshoot slightly (the choke "bounce").
                // Forward Euler is stable — the resonance is a few Hz, far
                // below Nyquist.  Clamp the output to the unloaded crest so
                // the ring can rise above the sagged level but never above
                // the transformer secondary peak (keeps Vb ≤ Vb_nominal).
                iChoke_ += dt * ((Vc_ - Vc2_ - iChoke_ * chokeR_) / chokeL_);
                Vc2_    += dt * ((iChoke_ - abs_ip) / cOut_);
                Vc2_ = std::clamp(Vc2_, 0.0, vPeak_);
                vbSlow_ += sagMeterAlpha_ * (Vc2_ - vbSlow_);
                return Vc2_;
            }

            // Slow average for the sag% meter (the UI wants the DC trend,
            // not the ripple sawtooth).
            vbSlow_ += sagMeterAlpha_ * (Vc_ - vbSlow_);
            return Vc_;
        }
        return currentVb();
    }

    double currentVb() const noexcept
    {
        if (p_.enableReservoirModel)
            return p_.enableChoke ? Vc2_ : Vc_;
        const double vbDc = p_.Vb_nominal - Ip_avg_ * p_.Z_internal;
        return vbDc + p_.ripple_amp * std::sin(ripplePhase_);
    }

    double sagPercent() const noexcept
    {
        if (p_.enableReservoirModel)
            return std::max(0.0,
                (p_.Vb_nominal - vbSlow_) / p_.Vb_nominal) * 100.0;
        return (Ip_avg_ * p_.Z_internal / p_.Vb_nominal) * 100.0;
    }

private:
    void updateCoeff() noexcept
    {
        alpha_ = std::exp(-1.0 / (p_.tau_sag * p_.sampleRate));
        constexpr double kTwoPi = 2.0 * 3.14159265358979323846;
        ripplePhaseInc_ = kTwoPi * p_.ripple_freq / p_.sampleRate;

        // Reservoir-model coefficients.  R_rect defaults to Z_internal/3:
        // the rectifier conducts over roughly a third of each half-cycle,
        // so the EFFECTIVE average source impedance lands near the
        // documented Z_internal while the instantaneous physics stays a
        // pulse-charged capacitor.
        cRes_ = std::max(p_.reservoirFarads, 1.0e-6);
        const double rRect = (p_.rectifierOhms > 0.0)
            ? p_.rectifierOhms
            : std::max(p_.Z_internal / 3.0, 0.5);
        rectChargeCoeff_ = std::min(
            (1.0 / p_.sampleRate) / (rRect * cRes_), 1.0);

        // Peak winding voltage: with light load the cap rides the crest,
        // so Vpk ≈ Vb_nominal places the unloaded rail at the documented
        // nominal value (matching the legacy model's zero-load point).
        vPeak_ = p_.Vb_nominal;

        sagMeterAlpha_ = 1.0 - std::exp(
            -1.0 / (0.05 * p_.sampleRate));   // 50 ms meter smoothing

        // Space-charge conduction + choke coefficients.
        rectExpM1_ = p_.rectExponent - 1.0;
        rectRefV_  = std::max(p_.rectRefVolts, 1.0e-3);
        chokeL_    = std::max(p_.chokeHenries, 1.0e-3);
        chokeR_    = std::max(p_.chokeDCR, 0.1);
        cOut_      = std::max(p_.chokeOutFarads, 1.0e-9);

        if (! std::isfinite(Vc_) || Vc_ <= 0.0) Vc_ = p_.Vb_nominal;
        if (! std::isfinite(Vc2_) || Vc2_ <= 0.0) Vc2_ = p_.Vb_nominal;
        if (! std::isfinite(iChoke_)) iChoke_ = 0.0;
        if (! std::isfinite(vbSlow_) || vbSlow_ <= 0.0) vbSlow_ = p_.Vb_nominal;
    }

    PSUSagParams p_;
    double alpha_          {0.0};
    double Ip_avg_         {0.0};
    double ripplePhase_    {0.0};
    double ripplePhaseInc_ {0.0};

    // Reservoir-model state
    double Vc_             {325.0};  ///< Reservoir capacitor voltage
    double vbSlow_         {325.0};  ///< 50 ms-smoothed rail for sag meter
    double cRes_           {47.0e-6};
    double rectChargeCoeff_{0.0};
    double vPeak_          {325.0};
    double sagMeterAlpha_  {0.0};

    // Space-charge conduction + choke state
    double rectExpM1_      {0.5};    ///< rectExponent − 1 (0 = ohmic)
    double rectRefV_       {20.0};   ///< conduction-overvoltage anchor
    double Vc2_            {325.0};  ///< post-choke output cap voltage
    double iChoke_         {0.0};    ///< choke inductor current [A]
    double chokeL_         {10.0};   ///< choke inductance [H]
    double chokeR_         {150.0};  ///< choke DCR [Ω]
    double cOut_           {47.0e-6};///< post-choke output cap [F]
};

} // namespace valvra::dsp
