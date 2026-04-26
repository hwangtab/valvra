// ─────────────────────────────────────────────────────────────────────────────
// TransformerStage — realtime Jiles-Atherton transformer with physical
// frequency shaping
//
// This is the feature most competitor plugins omit entirely. A real audio
// transformer is NOT a simple LPF+HPF filter — it exhibits:
//   1. Nonlinear B-H hysteresis (soft saturation with memory)
//   2. Low-frequency rolloff from primary inductance Lm
//   3. High-frequency rolloff from leakage inductance + stray capacitance
//   4. Signal-dependent harmonic character driven by core saturation
//
// Our model pipeline (per sample):
//   audio → scale to H [A/m] → JA.process(H) → M [A/m]
//         → emf = −dΦ/dt → shape with Lm HPF + leakage LPF
//         → rescale to audio
//
// The JA state is the authentic nonlinear memory; the two filters are linear
// physical limits.
//
// References:
//   docs/02 §1–§2 (JA theory)
//   docs/22 §C (parameter estimates for Ni-permalloy, mu-metal, Si-steel)
//   docs/24 §C (Culture Vulture transformer, Jensen JT-DB-E)
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include "JilesAtherton.h"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace valvra::dsp {

// ─────────────────────────────────────────────────────────────────────────────
// Configuration: JA parameters + physical frequency bounds + drive scale
// ─────────────────────────────────────────────────────────────────────────────
struct TransformerStageConfig
{
    JAParams ja { ja_params::kNiPermalloy_Peerless };

    // Physical frequency limits (linear shaping around the nonlinear JA core)
    double fc_low  { 15.0 };      ///< Primary Lm rolloff [Hz]
    double fc_high { 18000.0 };   ///< Leakage/stray rolloff [Hz]

    // Audio → magnetic field conversion.
    // In a real transformer  H = N_primary · I_primary / (length of magnetic path).
    // For plugin purposes we treat the audio sample as a proxy for primary
    // current via a user-tunable drive scale.  A drive of 1.0 with a +4 dBu
    // signal lands the core in the linear region; 3–5× drives it into
    // saturation for the "transformer colour" most engineers want.
    double drive { 1.0 };
    double H_scale { 80.0 };      ///< A/m per unit normalized audio sample

    // Output normalization: how much to emphasise the magnetization response.
    // The JA output M is on the order of Ms (~400 kA/m). We scale back to
    // audio range and apply a makeup gain so unity drive → unity RMS.
    double outputGain { 1.0 };

    // ─── Leakage-inductance resonance peak (docs/02 §4) ─────────────────
    // A real transformer's secondary leakage inductance (Ll) together
    // with its distributed + inter-winding capacitance (Cs) forms a
    // parallel resonance at:  f_res = 1 / (2π·√(Ll·Cs))
    // For high-end input/output transformers this typically lands in
    // the 12–22 kHz range with a modest Q (0.7–1.5) and a peak of
    // +0.5 to +2.5 dB.  The result is the vintage "air", "silk", or
    // "sheen" that plugin designers keep trying to dial in manually as
    // a static shelf — but the peak is resonant, not a shelf, and
    // competitor plugins that approximate it with one-pole LPFs drop
    // the wrong signature entirely.  See docs/02 §4 for the Neve 1073,
    // Jensen JT-11, and UTC A-12 measured peaks that seeded the
    // per-transformer values in this file.
    bool   enablePresencePeak { true };
    double presence_freq      { 16000.0 };  ///< Resonance centre [Hz]
    double presence_gain_dB   { 1.5 };      ///< Peak boost [dB]
    double presence_Q         { 1.0 };      ///< Q of the biquad peak

    // ─── Program-dependent primary-inductance saturation (docs/02 §3) ──
    // A real transformer's primary inductance Lp is only constant in the
    // small-signal regime.  When the core approaches magnetic saturation
    // the incremental permeability dB/dH drops, so Lp itself drops, and
    // the low-frequency corner  fc_low = Rsrc / (2π·Lp)  MOVES UPWARD
    // as the signal gets louder.  On a big bass note the transformer
    // rolls off more low end than it does on a quiet passage — exactly
    // the "muscle that makes bass sit in the pocket" of Neve / API
    // consoles.  Most plugins use a FIXED HPF and thus completely miss
    // this program-dependent LF tightening.
    bool   enableLFSaturation { true };
    double lfSatDepth         { 0.7 };      ///< 0 = static, 1 = heavy
};

// ─────────────────────────────────────────────────────────────────────────────
// TransformerStage
// ─────────────────────────────────────────────────────────────────────────────
class TransformerStage
{
public:
    TransformerStage() = default;

    void setup(const TransformerStageConfig& cfg, double sampleRate)
    {
        config_     = cfg;
        sampleRate_ = sampleRate;

        ja_.setParams(cfg.ja);
        ja_.reset();

        // High-pass filter (Lm low-frequency rolloff).  We also cache the
        // RC-sample ratio so process() can re-derive alpha on the fly
        // when LF saturation is active (no exp() per sample required).
        {
            const double rc = 1.0 / (2.0 * M_PI * cfg.fc_low);
            alpha_hp_ = rc / (rc + 1.0 / sampleRate);
            invSR_    = 1.0 / sampleRate;
        }

        // Low-pass filter (leakage high-frequency rolloff)
        alpha_lp_ = 1.0 - std::exp(-2.0 * M_PI * cfg.fc_high / sampleRate);

        // Peaking-EQ biquad for the leakage-inductance resonance.
        // RBJ cookbook — peaking EQ.  Stable for typical presence
        // values (fc < fs/2, Q > 0.3, |gain_dB| < 12).
        {
            // Clamp the resonance frequency safely below Nyquist.  Without
            // this the Lundahl preset's 25 kHz peak produces NaN biquad
            // coefficients on the 48 kHz (OS = 1×) path.  At upsampled
            // rates the clamp is a no-op.
            const double fcClamp = std::min(cfg.presence_freq,
                                            sampleRate * 0.45);
            const double w0   = 2.0 * M_PI * fcClamp / sampleRate;
            const double cosw = std::cos(w0);
            const double sinw = std::sin(w0);
            const double A    = std::pow(10.0, cfg.presence_gain_dB / 40.0);
            const double Q    = std::max(cfg.presence_Q, 0.1);
            const double alph = sinw / (2.0 * Q);

            const double b0 = 1.0 + alph * A;
            const double b1 = -2.0 * cosw;
            const double b2 = 1.0 - alph * A;
            const double a0 = 1.0 + alph / A;
            const double a1 = -2.0 * cosw;
            const double a2 = 1.0 - alph / A;
            presB0_ = b0 / a0;
            presB1_ = b1 / a0;
            presB2_ = b2 / a0;
            presA1_ = a1 / a0;
            presA2_ = a2 / a0;
        }

        x_hp_prev_ = 0.0;
        y_hp_prev_ = 0.0;
        y_lp_     = 0.0;
        M_prev_   = 0.0;
        presX1_ = presX2_ = presY1_ = presY2_ = 0.0;
    }

    void reset()
    {
        ja_.reset();
        x_hp_prev_ = y_hp_prev_ = 0.0;
        y_lp_     = 0.0;
        M_prev_   = 0.0;
        presX1_ = presX2_ = presY1_ = presY2_ = 0.0;
    }

    // Per-sample: audio in → audio out.
    // Steps:
    //   1. Scale audio → H (A/m)
    //   2. JA integration → M
    //   3. Use M/Ms as "magnetization ratio" — this IS the nonlinear transfer.
    //      (For a steady-state sinusoid, M tracks H through a hysteresis loop;
    //       the ratio saturates smoothly as |H| grows.)
    //   4. Apply physical frequency shaping (HPF for Lm rolloff, LPF for
    //      leakage inductance + stray capacitance rolloff).
    //
    // This formulation is sample-rate-independent and numerically well-behaved.
    // It treats the transformer as a "nonlinear saturating coupling" rather
    // than trying to compute instantaneous EMF, which would amplify any
    // numerical noise in M by fs and cause catastrophic output.
    double process(double x) noexcept
    {
        // Pre-flight sanity: reject non-finite input without letting it
        // poison the HPF/LPF delay lines.
        if (! std::isfinite(x) || ! std::isfinite(y_hp_prev_)
                               || ! std::isfinite(y_lp_)
                               || ! std::isfinite(x_hp_prev_)
                               || ! std::isfinite(presY1_)
                               || ! std::isfinite(presY2_))
        {
            x_hp_prev_ = 0.0;
            y_hp_prev_ = 0.0;
            y_lp_      = 0.0;
            M_prev_    = 0.0;
            presX1_ = presX2_ = presY1_ = presY2_ = 0.0;
            return 0.0;
        }

        // 1) Drive the core
        const double H = x * config_.drive * config_.H_scale;

        // 2) Jiles-Atherton integration (nonlinear memory, hysteresis)
        const double M = ja_.process(H);
        M_prev_ = std::isfinite(M) ? M : 0.0;

        // 3) Normalize magnetization → audio-range signal
        const double shaped = M_prev_ / config_.ja.Ms;

        // 4a) Program-dependent LF corner shift.  When the magnetization
        //     ratio approaches unity (core saturating), incremental Lp
        //     drops → fc_low rises.  Quadratic of |M/Ms| gives a
        //     smooth, musical tightening — large bass gets visibly
        //     firmer while small signals retain the static corner.
        double alpha_hp_eff = alpha_hp_;
        if (config_.enableLFSaturation)
        {
            const double msatRatio = std::min(1.0, std::abs(shaped));
            const double q2 = msatRatio * msatRatio;
            const double lpRatio = std::max(
                1.0 - config_.lfSatDepth * q2, 0.05);
            const double rcEff   = 1.0
                / (2.0 * M_PI * config_.fc_low / lpRatio);
            alpha_hp_eff = rcEff / (rcEff + invSR_);
        }

        // 4) Linear physical shaping: HPF (Lm rolloff) and LPF (leakage)
        const double yhp = alpha_hp_eff
                         * (y_hp_prev_ + shaped - x_hp_prev_);
        x_hp_prev_ = shaped;
        y_hp_prev_ = yhp;

        y_lp_ += alpha_lp_ * (yhp - y_lp_);

        // 5) Leakage-L / stray-C presence peak (biquad, RBJ peaking EQ).
        //    Applied on top of the LPF so the resonance sits *inside*
        //    the transformer's passband like the real circuit.
        double out = y_lp_;
        if (config_.enablePresencePeak)
        {
            const double x0 = y_lp_;
            double y0 = presB0_ * x0 + presB1_ * presX1_ + presB2_ * presX2_
                      - presA1_ * presY1_ - presA2_ * presY2_;
            if (! std::isfinite(y0)) y0 = 0.0;
            presX2_ = presX1_;  presX1_ = x0;
            presY2_ = presY1_;  presY1_ = y0;
            out = y0;
        }
        return config_.outputGain * out;
    }

    // Diagnostics — current magnetization (for B-H loop visualization UI)
    double currentM() const noexcept { return M_prev_; }
    double currentH() const noexcept { return ja_.currentH(); }
    double currentMsat() const noexcept { return config_.ja.Ms; }
    const JilesAtherton& ja() const noexcept { return ja_; }

private:
    TransformerStageConfig config_ {};
    double sampleRate_ { 48000.0 };

    JilesAtherton ja_ {};

    double alpha_hp_  { 0.99 };
    double alpha_lp_  { 0.1  };
    double invSR_     { 1.0 / 48000.0 };  ///< Cached 1/fs for LF satur.
    double x_hp_prev_ { 0.0 };
    double y_hp_prev_ { 0.0 };
    double y_lp_      { 0.0 };
    double M_prev_    { 0.0 };

    // Presence-peak biquad (RBJ peaking EQ, DF-I state)
    double presB0_ { 1.0 }, presB1_ { 0.0 }, presB2_ { 0.0 };
    double presA1_ { 0.0 }, presA2_ { 0.0 };
    double presX1_ { 0.0 }, presX2_ { 0.0 };
    double presY1_ { 0.0 }, presY2_ { 0.0 };
};

// ─────────────────────────────────────────────────────────────────────────────
// Preset factories for the five transformer types the plugin exposes
// ─────────────────────────────────────────────────────────────────────────────
namespace transformer_presets {

/// Marinair / Neve-style: Ni-permalloy, warm top rolloff + signature
/// 16 kHz presence peak (measured on 1073 samples).  The peak is what
/// engineers usually describe as the "silk" of a 1073 input.
inline TransformerStageConfig Marinair()
{
    TransformerStageConfig c {
        .ja = ja_params::kNiPermalloy_Peerless,
        .fc_low = 25.0, .fc_high = 18000.0,
        .drive = 1.0, .H_scale = 80.0, .outputGain = 1.0
    };
    c.enablePresencePeak = true;
    c.presence_freq      = 16000.0;
    c.presence_gain_dB   = 1.8;
    c.presence_Q         = 1.0;
    return c;
}

/// UTC A-12 vintage: Si-steel, early saturation, coloured.  Strong
/// 18 kHz peak gives it the "aggressive top" of the era.
inline TransformerStageConfig UTC_A12()
{
    TransformerStageConfig c {
        .ja = ja_params::kSiSteel_M6,
        .fc_low = 40.0, .fc_high = 14000.0,
        .drive = 1.2, .H_scale = 120.0, .outputGain = 1.0
    };
    c.enablePresencePeak = true;
    c.presence_freq      = 18000.0;
    c.presence_gain_dB   = 2.3;
    c.presence_Q         = 1.2;
    return c;
}

/// Jensen JT-11 style: 80% Ni, wide BW, very low distortion — subtler
/// ~14 kHz peak preserves the "clean but not sterile" character.
inline TransformerStageConfig JensenJT11()
{
    TransformerStageConfig c {
        .ja = ja_params::kNiPermalloy_Peerless,
        .fc_low = 15.0, .fc_high = 30000.0,
        .drive = 0.6, .H_scale = 60.0, .outputGain = 1.0
    };
    c.enablePresencePeak = true;
    c.presence_freq      = 14000.0;
    c.presence_gain_dB   = 1.0;
    c.presence_Q         = 0.8;
    return c;
}

/// Lundahl LL1582 style: amorphous core, very clean
inline TransformerStageConfig Lundahl()
{
    // Treat amorphous as a permalloy-class linear preset — the spec
    // trades "character" for neutrality.  Presence peak pushed out to
    // ~25 kHz and made very small, in line with Lundahl measurements.
    TransformerStageConfig c;
    c.ja           = ja_params::kMuMetal_Triad;
    c.fc_low       = 10.0;
    c.fc_high      = 50000.0;
    c.drive        = 0.4;
    c.H_scale      = 40.0;
    c.outputGain   = 1.0;
    c.enablePresencePeak = true;
    c.presence_freq      = 25000.0;
    c.presence_gain_dB   = 0.5;
    c.presence_Q         = 0.5;
    return c;
}

} // namespace transformer_presets

} // namespace valvra::dsp
