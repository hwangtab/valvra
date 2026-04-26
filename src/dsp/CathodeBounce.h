// ─────────────────────────────────────────────────────────────────────────────
// CathodeBounce — dynamic bias shift from cathode bypass capacitor
//
// Large-signal input charges/discharges the cathode bypass cap Ck, causing
// the bias point to drift on a time constant τ = Rk·Ck. This is the
// physical origin of "tube compression" feel — the natural ducking that
// competing static plugins do NOT model.
//
// Time-domain behavior:
//   Vk[n] = α · Vk[n-1] + (1-α) · (Ip · Rk)
//   α = exp(-1 / (τ·fs))
//
// Audibility threshold: ≈ 5-15 mV bias shift (Jones, Valve Amplifiers 2011)
// Typical Rk/Ck: 1.5 kΩ / 25 μF → τ ≈ 37.5 ms, 3τ recovery ≈ 110 ms
//
// References:
//   - docs/03-time-varying-nonlinearities.md §3
//   - docs/22-academic-quantitative-data.md §D.1
//   - Jones, M. (2011). Valve Amplifiers 4th ed.
//   - Blencowe, M. (2012). Designing Valve Preamps.
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <cmath>
#include <cstdint>

namespace valvra::dsp {

struct CathodeBounceParams
{
    double Rk         {1500.0};  ///< Cathode resistor [Ω]
    double Ck         {25.0e-6}; ///< Cathode bypass cap [F]
    double sampleRate {48000.0};

    // ─── Dielectric absorption / "soakage" (docs/03 §6) ─────────────────
    // Real electrolytic caps don't release all stored charge when the
    // external voltage drops — a fraction is trapped in a dielectric
    // relaxation network and bleeds out with a much longer time constant
    // than the main τ = Rk·Ck.  The audible consequence is "bloom":
    // after a loud transient the bias recovers to NEAR resting quickly,
    // then creeps the rest of the way back over 100–300 ms.  This is a
    // non-trivial ingredient of the "vintage sustain" impression that
    // competing plugins (which use a single-pole RC) cannot produce.
    //
    //   Vk_effective = (1 − k_soak) · Vm + k_soak · Vs
    //   Vs tracks Vm with τ_soak (slow)
    //
    // Typical values: k_soak ≈ 0.10 for a decent electrolytic, 0.20+
    // for cheap/aged ones; τ_soak ≈ 100–300 ms.
    bool   enableSoakage { true };
    double soakageAmount { 0.10 };   ///< Fraction of Vk attributed to DA
    double soakageTau    { 0.15 };   ///< Slow relaxation τ [s]
};

class CathodeBounce
{
public:
    explicit CathodeBounce(CathodeBounceParams p = {}) noexcept
        : p_ { p }, Vk_ { 0.0 }
    {
        updateCoeff();
    }

    void setParams(const CathodeBounceParams& p) noexcept
    {
        p_ = p;
        updateCoeff();
    }

    void reset() noexcept { Vk_ = 0.0; Vs_ = 0.0; }

    // Prime Vk (and its soakage partner) to a specific DC value — typically
    // Ip_rest · Rk.  Used on setup() to avoid a startup bounce transient
    // that would contaminate the first second of audio after plugin load.
    void primeTo(double Vk_initial) noexcept
    {
        Vk_ = Vk_initial;
        Vs_ = Vk_initial;   // soakage starts fully settled, not at zero
    }

    // Scale Ck for per-instance Monte Carlo variation (±20% for electrolytics,
    // docs/06 §2.2). Applied multiplicatively to the default Ck.
    void setCkScale(double scale) noexcept
    {
        p_.Ck = 25.0e-6 * scale;   // default Ck * scale
        updateCoeff();
    }

    // Returns the dynamic cathode voltage Vk[n] given instantaneous plate
    // current Ip. The voltage is fed back to subtract from the grid voltage
    // (biasing the tube more negative when Ip is high → gain reduction).
    //
    // NaN-recovery: if Ip or the running state becomes non-finite (e.g. a
    // transient upstream divergence), collapse to zero rather than carry the
    // corruption into every future sample.
    double process(double Ip) noexcept
    {
        if (! std::isfinite(Ip) || ! std::isfinite(Vk_)
                                 || ! std::isfinite(Vs_))
        {
            Vk_ = 0.0;
            Vs_ = 0.0;
            return 0.0;
        }
        const double target = Ip * p_.Rk;
        Vk_ = alpha_ * Vk_ + (1.0 - alpha_) * target;

        if (p_.enableSoakage)
        {
            // Slow network follows the main cap.  When Vk_ swings quickly
            // (user-audible transient), Vs_ lags — after the transient,
            // Vs_ is still elevated and pulls the effective bias back
            // high, giving the "bloom" that vintage caps produce.
            Vs_ = alphaSoak_ * Vs_ + (1.0 - alphaSoak_) * Vk_;
            return (1.0 - p_.soakageAmount) * Vk_
                 + p_.soakageAmount       * Vs_;
        }
        return Vk_;
    }

    double currentBias() const noexcept
    {
        if (p_.enableSoakage)
            return (1.0 - p_.soakageAmount) * Vk_
                 + p_.soakageAmount       * Vs_;
        return Vk_;
    }

    // Effective grid voltage seen by the tube: Vg_effective = Vg_input − Vk
    // This is the correct way to apply the bounce back into the Koren model.
    double biasedGrid(double Vg_input) const noexcept
    {
        return Vg_input - Vk_;
    }

private:
    void updateCoeff() noexcept
    {
        const double tau = p_.Rk * p_.Ck;
        alpha_     = std::exp(-1.0 / (tau * p_.sampleRate));
        alphaSoak_ = std::exp(-1.0 / (p_.soakageTau * p_.sampleRate));
    }

    CathodeBounceParams p_;
    double alpha_     {0.0};
    double alphaSoak_ {0.0};
    double Vk_        {0.0};
    double Vs_        {0.0};
};

} // namespace valvra::dsp
