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
    double ripple_amp   {0.10};   ///< Peak ripple on B+ [V]
    double ripple_freq  {120.0};  ///< Ripple frequency [Hz]
};

// ─────────────────────────────────────────────────────────────────────────────
// Rectifier-based presets (Pultec uses 6X4; GZ34 provides similar character
// with stronger sag, Blencowe 2012).
// ─────────────────────────────────────────────────────────────────────────────
namespace psu_presets
{
    // NOTE: when adding new presets, remember to include ripple_amp /
    // ripple_freq — the defaults in PSUSagParams assume a vintage US
    // vacuum-rectifier rail.
    constexpr PSUSagParams kGZ34 = {
        .Vb_nominal = 325.0, .Z_internal = 200.0, .tau_sag = 0.1,
        .sampleRate = 48000.0,
        .ripple_amp = 0.12, .ripple_freq = 120.0
    };
    constexpr PSUSagParams k5U4GB = {
        .Vb_nominal = 325.0, .Z_internal = 450.0, .tau_sag = 0.15,
        .sampleRate = 48000.0,
        .ripple_amp = 0.20, .ripple_freq = 120.0   // loosely filtered
    };
    constexpr PSUSagParams kSolidState = {
        .Vb_nominal = 325.0, .Z_internal = 5.0, .tau_sag = 0.01,
        .sampleRate = 48000.0,
        .ripple_amp = 0.02, .ripple_freq = 120.0   // much cleaner
    };
    constexpr PSUSagParams k6X4_Pultec = {          // Pultec's rectifier
        .Vb_nominal = 350.0, .Z_internal = 250.0, .tau_sag = 0.12,
        .sampleRate = 48000.0,
        .ripple_amp = 0.10, .ripple_freq = 120.0
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

    void reset() noexcept { Ip_avg_ = 0.0; ripplePhase_ = 0.0; }

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
                                         || ! std::isfinite(ripplePhase_))
        {
            Ip_avg_      = 0.0;
            ripplePhase_ = 0.0;
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

        return currentVb();
    }

    double currentVb() const noexcept
    {
        const double vbDc = p_.Vb_nominal - Ip_avg_ * p_.Z_internal;
        return vbDc + p_.ripple_amp * std::sin(ripplePhase_);
    }

    double sagPercent() const noexcept
    {
        return (Ip_avg_ * p_.Z_internal / p_.Vb_nominal) * 100.0;
    }

private:
    void updateCoeff() noexcept
    {
        alpha_ = std::exp(-1.0 / (p_.tau_sag * p_.sampleRate));
        constexpr double kTwoPi = 2.0 * 3.14159265358979323846;
        ripplePhaseInc_ = kTwoPi * p_.ripple_freq / p_.sampleRate;
    }

    PSUSagParams p_;
    double alpha_          {0.0};
    double Ip_avg_         {0.0};
    double ripplePhase_    {0.0};
    double ripplePhaseInc_ {0.0};
};

} // namespace valvra::dsp
