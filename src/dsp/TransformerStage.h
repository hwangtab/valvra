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

#include <algorithm>
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
    //
    // NOTE: only used on the LEGACY (non-flux) path.  With flux drive on,
    // both the LF corner and its level dependence emerge from the physics
    // (flux = ∫V dt with the Lm leak; saturation eats the incremental
    // permeability), so no heuristic is needed.
    bool   enableLFSaturation { true };
    double lfSatDepth         { 0.7 };      ///< 0 = static, 1 = heavy

    // ─── Voltage-driven flux integration (docs/02 §1, §5.1) ────────────
    // A transformer primary is driven by VOLTAGE; the core flux is its
    // time integral (Faraday), and the secondary EMF is −dΦ/dt.  The
    // legacy path drove the JA core with the instantaneous sample, which
    // erased the single most characteristic transformer behaviour:
    // saturation onset scales as 1/f.  A 50 Hz bass note drives ~20×
    // more flux than a 1 kHz tone at the same level — real iron audibly
    // grips low end first.  Implementation:
    //   flux[n]  = a_lm·flux[n−1] + x[n]          (leak a_lm ⇔ fc_low)
    //   H[n]     = k_f·flux[n] + H_dc − k_e·(dB/dt)[n−1]
    //   B[n]     ∝ H + M(JA)
    //   y[n]     = (B[n] − B[n−1])·g_d            (EMF, unity at f_ref)
    // The Lm rolloff (1st-order HPF at fc_low) and the 1/f saturation
    // law both fall out of the integrate→saturate→differentiate loop;
    // the legacy HPF and LF-saturation heuristic are bypassed.
    bool   enableFluxDrive { true };
    double fluxRefHz       { 1000.0 };  ///< Calibration frequency: at this
                                        ///  frequency the flux drive hits
                                        ///  the JA core exactly as hard as
                                        ///  the legacy direct drive did.

    // ─── Eddy-current core loss (docs/02 §4.1, Steinmetz k_e·f²·B²) ────
    // Eddy currents circulate to oppose flux change, acting as a drag
    // field −k_e·dB/dt.  Loss grows with f² — the physical origin of the
    // frequency-dependent loop widening (hysteresis loops fatten at HF)
    // and part of the natural HF rolloff.  Dimensionless coefficient,
    // scaled internally by the Langevin parameter a.
    double eddyLossCoeff   { 0.02 };

    // ─── Standing DC magnetization (docs/02 §6) ─────────────────────────
    // Net DC ampere-turns on the core: push-pull idle imbalance, DC-
    // coupled drift, or single-ended standing current.  Shifts the B-H
    // loop off centre → asymmetric saturation → even-harmonic "punch".
    double H_dc            { 0.0 };     ///< [A/m]
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

        // Flux-drive coefficients for the shunt-loss loop in process():
        // fluxToH_ maps the flux integral to A/m so the core is excited
        // at the calibrated level at fluxRefHz; kLossH_ sets the
        // magnetizing-current drop so the linear loop pole lands exactly
        // on the Lm corner.  (Passband insertion gain is unity by the
        // loop's construction — no separate gain restoration exists.)
        {
            const double fRef = std::max(cfg.fluxRefHz, 1.0);
            // Pure drive-calibration constant: the five chain presets'
            // measured drive values (0.12…1.6 → core peaks of 0.5–0.7·a
            // at their respective OPT-node levels) were calibrated with
            // this scale in place.  Changing it rescales every preset's
            // saturation onset by the same ratio.
            constexpr double kFluxDriveTrim = 0.577;
            fluxToH_ = cfg.H_scale * (2.0 * M_PI * fRef / sampleRate)
                     * kFluxDriveTrim;
            // Magnetizing-loss coefficient: in the linear regime the
            // shunt drop reproduces exactly the Lm corner at fc_low
            // (loop pole 1 − kLossH·fluxToH = 1 − 2π·fc_low/fs).
            kLossH_ = (2.0 * M_PI * cfg.fc_low / sampleRate)
                    / std::max(fluxToH_, 1.0e-12);

            // Small-signal susceptibility of THIS core (Monte Carlo
            // varies it per instance) — used as the B-demand scale and
            // the secant slope seed of the per-sample JA inverse.
            {
                JilesAtherton probe { cfg.ja };
                probe.reset();
                const double hAmp = 0.02 * std::max(cfg.ja.a, 1.0e-3);
                double mPk = 0.0;
                constexpr int kCycle = 64;
                for (int i = 0; i < 4 * kCycle; ++i)
                {
                    const double h = hAmp
                        * std::sin(2.0 * M_PI * i / double(kCycle));
                    const double m = probe.process(h);
                    if (i >= 3 * kCycle) mPk = std::max(mPk, std::abs(m));
                }
                chi0_ = std::max(mPk / hAmp, 1.0);
            }

            // Standing DC magnetization (SE idle current / PP imbalance):
            // ramp the real core to H_dc so it starts on the shifted
            // minor-loop branch, and record the demanded B there.
            ja_.reset();
            if (std::abs(cfg.H_dc) > 1.0e-9)
            {
                for (int i = 1; i <= 256; ++i)
                    ja_.step(cfg.H_dc * i / 256.0, ja_.stateRef());
            }
            hPrev_   = cfg.H_dc;
            bDcRest_ = ja_.observedM(cfg.H_dc, ja_.stateRef()) + cfg.H_dc;
        }

        x_hp_prev_ = 0.0;
        y_hp_prev_ = 0.0;
        y_lp_     = 0.0;
        M_prev_   = 0.0;
        flux_     = 0.0;
        presX1_ = presX2_ = presY1_ = presY2_ = 0.0;
    }

    /// Carry the core's magnetic REMANENCE from a previous incarnation
    /// after a parameter-edit rebuild (docs/34 §4.3): the iron keeps its
    /// hysteretic memory (Mirr, clamped to the new core's Ms) instead of
    /// snapping back to the virgin curve.  The flux-loop anchor states
    /// (flux_/hPrev_/M_prev_) are deliberately NOT carried: setup()
    /// re-derives the rest-branch references (bDcRest_, χ), and restoring
    /// a mid-signal flux against fresh anchors leaves a slowly-annealing
    /// LF error that outlives the rebuild fade (observed as a sustained
    /// wet spike under TP-switch stress).  The loop re-anchors from the
    /// rest branch within ~1/fc_low — inside the fade window.
    void carryCoreStateFrom(const TransformerStage& o) noexcept
    {
        auto fin = [](double v) { return std::isfinite(v) ? v : 0.0; };

        JAState s = ja_.stateRef();       // keep setup's H_prev/delta
        double mirr = o.ja_.stateRef().Mirr;
        if (! std::isfinite(mirr)) mirr = 0.0;
        s.Mirr = std::clamp(mirr, -config_.ja.Ms, config_.ja.Ms);
        ja_.stateRef() = s;

        // CRITICAL anchor re-derivation: setup() computed bDcRest_ from its
        // own freshly-ramped branch; with the carried Mirr the shunt loop's
        // B demand must re-anchor on the RESTORED branch, or the mismatch
        // parks the solve in the wiping-out / saturated zone where the
        // incremental susceptibility collapses — and the calibrated loop's
        // discrete pole exceeds 1 at 1× oversampling (observed as an
        // exponential blow-up under rebuild stress).  Re-deriving from the
        // restored state seats g(hPrev)=0 exactly: zero carry transient.
        bDcRest_ = ja_.observedM(hPrev_, ja_.stateRef()) + hPrev_;

        // Linear output filters: fast, signal-continuity only.
        y_lp_   = fin(o.y_lp_);
        presX1_ = fin(o.presX1_);
        presX2_ = fin(o.presX2_);
        presY1_ = fin(o.presY1_);
        presY2_ = fin(o.presY2_);
    }

    void reset()
    {
        ja_.reset();
        // Re-establish the standing DC magnetization branch.
        if (std::abs(config_.H_dc) > 1.0e-9)
            for (int i = 1; i <= 256; ++i)
                ja_.step(config_.H_dc * i / 256.0, ja_.stateRef());
        hPrev_    = config_.H_dc;
        x_hp_prev_ = y_hp_prev_ = 0.0;
        y_lp_     = 0.0;
        M_prev_   = 0.0;
        flux_     = 0.0;
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
                               || ! std::isfinite(flux_)
                               || ! std::isfinite(hPrev_)
                               || ! std::isfinite(presY1_)
                               || ! std::isfinite(presY2_))
        {
            x_hp_prev_ = 0.0;
            y_hp_prev_ = 0.0;
            y_lp_      = 0.0;
            M_prev_    = 0.0;
            flux_      = 0.0;
            hPrev_     = config_.H_dc;
            presX1_ = presX2_ = presY1_ = presY2_ = 0.0;
            return 0.0;
        }

        double preFilter;
        if (config_.enableFluxDrive)
        {
            // ── Physical path: shunt-loss transformer ───────────────────
            // An ideal transformer passes the source voltage at the turns
            // ratio; ALL of the iron's behaviour enters as the voltage
            // drop of the (nonlinear) magnetizing current across the
            // source/winding impedance, plus the eddy shunt:
            //   v_out = v_in − k_loss·H_mag − k_eddy·(dΦ/dt)
            //   Φ    += v_out                     (Faraday)
            //   H_mag = JA⁻¹(B demanded by Φ)     (secant, warm start)
            // Linear regime: H = fluxToH·Φ and the loop pole IS the Lm
            // corner at fc_low — unity passband, exact rolloff.  Driven
            // hard, the inverse demands disproportionate H (saturation),
            // the drop eats the fundamental and writes the hysteresis
            // shape onto the output, with the 1/f flux law making bass
            // saturate first.  No susceptibility normalisation appears
            // anywhere — insertion gain is turns-ratio unity by
            // construction, exactly like the real device.
            // Eddy currents form a loss RESISTANCE in parallel with Lm
            // (their current is ∝ dΦ/dt = v_out), so the drop across the
            // source divides the loop output by (1 + k_e) ALGEBRAICALLY.
            // A one-sample-delayed "−k·vOut[n−1]" form is wrong physics:
            // at Nyquist the delayed sample is anti-phase, turning the
            // intended loss into a 1/(1−k) HF BOOST (and a divergence as
            // k → 1).  The frequency-dependent part of eddy behaviour is
            // the f² POWER loss, not a transfer-function tilt — the HF
            // rolloff itself belongs to the leakage LPF downstream.
            // kLossHScale_ tracks the driving stage's instantaneous output
            // impedance (docs/34 §3.9): the magnetizing drop is R_src·i_mag,
            // so a softer source (tube toward cutoff) both deepens the drop
            // and lowers the effective Lm corner — exactly as measured
            // transformer THD follows its drive impedance.
            //
            // FULLY IMPLICIT shunt loop: h solves
            //   M(h) + h = k2·[flux + (u − kL·(h−H_dc))/(1+ke)] + bDcRest
            // i.e. THIS sample's flux update is inside the residual.  The
            // legacy explicit form (flux += vOut(h[n−1]) first) had a
            // discrete loop pole of 1 − (2π·fc/fs)·(χ+1)·(dh/dB) — fine on
            // the normal branch where dh/dB ≈ 1/(χ+1), but in deep
            // saturation / wiping-out zones dh/dB → 1 and at 1× OS the
            // magnitude exceeds 1: an exponential blow-up (observed when a
            // state-carry landed the core off-branch; latent for any
            // pathological drive).  The implicit residual keeps
            //   dr/dh = (dM/dh + 1) + k2·kL/(1+ke) > 0
            // monotone and the update unconditionally stable at every rate
            // and saturation depth, while matching the explicit loop to
            // O(2π·fc/fs) in the linear regime (calibration preserved).
            const double u   = x * config_.drive;
            const double k2  = fluxToH_ * (chi0_ + 1.0);
            const double ke1 = 1.0 + std::max(0.0, config_.eddyLossCoeff);
            const double kL  = kLossH_ * kLossHScale_;
            const double rhs = k2 * flux_ + (k2 / ke1) * u
                             + (k2 * kL / ke1) * config_.H_dc + bDcRest_;
            const double hGain = k2 * kL / ke1;   // extra dr/dh term

            double h = hPrev_;
            double slope = chi0_ + 1.0 + hGain;
            double hIt = h, gIt = 0.0;
            bool first = true;
            // Convergence tolerance: ~1e-3 A/m of field error referred to
            // B units — orders below audibility.  Warm-started steady
            // state and quiet passages exit after a single evaluation.
            const double gTol = 1.0e-3 * (chi0_ + 1.0);
            JAState trial = ja_.stateRef();
            double mTrial = 0.0;
            double hUsed = h;
            for (int it = 0; it < 4; ++it)
            {
                hUsed = h;
                trial = ja_.stateRef();
                // Real-time stepping: pass the sample period so the core's
                // Bertotti excess loss (rate-dependent loop widening) is
                // active — HF drive fattens the loop like measured iron.
                ja_.step(hUsed, trial, invSR_);
                mTrial = ja_.observedM(hUsed, trial);
                const double g = mTrial + hUsed + hGain * hUsed - rhs;
                if (std::abs(g) < gTol) break;
                if (! first && std::abs(hUsed - hIt) > 1.0e-12)
                    slope = std::clamp((g - gIt) / (hUsed - hIt),
                                       1.0, 1.0e9);
                first = false;
                hIt = hUsed; gIt = g;
                h = hUsed - g / slope;
                if (! std::isfinite(h)) { h = hPrev_; break; }
            }
            // Commit the LAST TRIAL state directly — re-running step()
            // on the live state would duplicate a full RK4 pass per
            // sample for a state we already integrated.  hUsed is the
            // field that trial was stepped with, keeping the warm start
            // and the loss term exactly consistent with the state.
            ja_.stateRef() = trial;
            M_prev_ = std::isfinite(mTrial) ? mTrial : 0.0;
            hPrev_  = std::isfinite(hUsed) ? hUsed : config_.H_dc;

            // Flux update with the SOLVED field (backward-Euler consistent
            // with the residual above).
            double vOut = (u - kL * (hPrev_ - config_.H_dc)) / ke1;
            if (! std::isfinite(vOut)) vOut = 0.0;
            flux_ += vOut;

            // Undo the drive scaling so insertion level stays unity —
            // "drive" sets how hard the iron is hit, not the gain.
            preFilter = vOut / std::max(config_.drive, 1.0e-6);
        }
        else
        {
            // ── Legacy path: direct sample-as-H drive ───────────────────
            // 1) Drive the core
            const double H = x * config_.drive * config_.H_scale;

            // 2) Jiles-Atherton integration (nonlinear memory, hysteresis)
            const double M = ja_.process(H);
            M_prev_ = std::isfinite(M) ? M : 0.0;

            // 3) Normalize magnetization → audio-range signal
            const double shaped = M_prev_ / config_.ja.Ms;

            // 4a) Program-dependent LF corner shift (heuristic stand-in
            //     for the flux physics; see config docs).
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

            // 4) Linear Lm-rolloff HPF
            const double yhp = alpha_hp_eff
                             * (y_hp_prev_ + shaped - x_hp_prev_);
            x_hp_prev_ = shaped;
            y_hp_prev_ = yhp;
            preFilter = yhp;
        }

        // Leakage-inductance + stray-capacitance HF rolloff (both paths)
        y_lp_ += alpha_lp_ * (preFilter - y_lp_);

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

    /// Set the REST output impedance of the stage driving this transformer
    /// (chain setup-time anchor for the dynamic ratio below).
    void setRestSourceImpedance(double z) noexcept
    {
        zSrcRest_ = (std::isfinite(z) && z > 1.0) ? z : 0.0;
    }

    /// Per-sample instantaneous source impedance from the driving stage.
    /// A real transformer's distortion is proportional to the source
    /// impedance its magnetizing current works against (docs/34 §3.9):
    /// when the driving tube's rp rises toward cutoff the iron distorts
    /// more, when it conducts hard the stiffer source damps the core.
    /// Scales the magnetizing-drop coefficient around its calibrated
    /// value; clamped ±2× so a cutoff spike cannot destabilise the loop.
    void setSourceImpedance(double z) noexcept
    {
        if (zSrcRest_ <= 1.0 || ! std::isfinite(z) || z <= 0.0)
        {
            kLossHScale_ = 1.0;
            return;
        }
        kLossHScale_ = std::clamp(z / zSrcRest_, 0.5, 2.0);
    }

    /// Magnetizing-current voltage drop of the last processed sample, in
    /// NORMALIZED (insertion-level) units — i.e. the same domain as the
    /// stage's input/output signal.  This is kLossH·H_ac, the drop the
    /// nonlinear magnetizing current produces across the implied source
    /// impedance, de-scaled by drive.  The chain feeds it back to the
    /// driving power stage's plate KCL so the TUBES supply the magnetizing
    /// current (docs/34 §2.2): at LF and near saturation the iron demands
    /// disproportionate current and the power stage genuinely works harder.
    /// Zero on the legacy (non-flux) path.
    double lastMagnetizingDropNorm() const noexcept
    {
        if (! config_.enableFluxDrive) return 0.0;
        const double hAc = hPrev_ - config_.H_dc;
        const double d = kLossH_ * kLossHScale_ * hAc
                       / std::max(config_.drive, 1.0e-6);
        return std::isfinite(d) ? d : 0.0;
    }

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

    // Flux-drive (shunt-loss transformer) state
    double fluxToH_   { 1.0 };    ///< Integrated flux → A/m calibration
    double kLossH_    { 0.0 };    ///< Magnetizing-current drop coefficient
    double kLossHScale_ { 1.0 };  ///< Dynamic source-impedance ratio (§3.9)
    double zSrcRest_    { 0.0 };  ///< Driving stage's rest Zout anchor
    double chi0_      { 800.0 };  ///< Measured small-signal susceptibility
    double bDcRest_   { 0.0 };    ///< Demanded B at the standing H_dc
    double hPrev_     { 0.0 };    ///< Solved magnetizing field (warm start)
    double flux_      { 0.0 };    ///< ∫v_out dt (core flux, input units)

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
    c.eddyLossCoeff      = 0.02;   // laminated Ni-permalloy
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
    c.eddyLossCoeff      = 0.05;   // Si-steel: lossiest of the family
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
    c.eddyLossCoeff      = 0.012;  // 80% Ni: thin laminations, low loss
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
    c.eddyLossCoeff      = 0.006;  // amorphous ribbon: minimal eddy loss
    return c;
}

} // namespace transformer_presets

} // namespace valvra::dsp
