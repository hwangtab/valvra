// ─────────────────────────────────────────────────────────────────────────────
// TubeStage — One full tube amplification stage with living dynamics
//
// Encapsulates a single common-cathode (or variant) tube stage with:
//   - Dempwolf/Koren triode model (KorenTriode)
//   - Cathode bypass dynamic bias (CathodeBounce)
//   - Shared B+ supply (PowerSupplySag, provided by host/chain)
//   - Thermal warmup (optional, per-stage)
//
// The stage exposes a single per-sample process() function that takes a grid
// input voltage and returns the plate output voltage (into a specified load).
// Chains are assembled by wiring multiple stages plus transformers in
// src/plugin/TubeAmpChain.h (see docs/20 §4.4).
//
// References:
//   docs/01 §Koren model
//   docs/03 §3 Cathode bounce
//   docs/07 §7.5 numerical safety
//   docs/24 §A–D (V72/Marshall/CV/RNDI mode presets)
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include "KorenTriode.h"
#include "CathodeBounce.h"
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace valvra::dsp {

// ─────────────────────────────────────────────────────────────────────────────
// Topology selector (docs/14)
// ─────────────────────────────────────────────────────────────────────────────
enum class TubeTopology : int
{
    CommonCathode    = 0, ///< Standard preamp stage
    CathodeFollower  = 1, ///< Buffer, low Zout
    SRPP             = 2, ///< Shunt-regulated push-pull (v2)
    LongTailedPair   = 3, ///< Differential (Output stage phase inverter)
    Cascode          = 4  ///< High bandwidth, low Miller (v2)
};

// ─────────────────────────────────────────────────────────────────────────────
// Stage configuration (passed once on setup; sample-rate changes re-initialize)
// ─────────────────────────────────────────────────────────────────────────────
struct TubeStageConfig
{
    // Tube
    DempwolfParams tube { params::kRSD_1 };

    // Topology
    TubeTopology topology { TubeTopology::CommonCathode };

    // Bias (DC operating point)
    double Vg_bias     { -1.5 };     ///< Grid DC bias [V] (negative class-A)
    double Vp_nominal  {  250.0 };   ///< Plate DC voltage at rest [V]
    double Rp          {  100.0e3 }; ///< Plate load resistor [Ω]
    double Rk          {  1500.0 };  ///< Cathode resistor [Ω]
    double Ck          {  25.0e-6 }; ///< Cathode bypass capacitor [F]
    bool   enableCathodeBounce { true }; ///< Slow cathode-bypass memory on/off

    // Input scaling — maps audio sample (±1.0) to grid voltage swing [V].
    // Typical preamps: 0.1 V peak input → grid swings ±0.1V around bias.
    double inputVoltageSwing { 1.0 };

    // Output scaling — maps plate voltage excursion back to audio sample.
    // Automatically calibrated such that resting Vp → 0.0 output.
    double outputGainLinear  { 1.0 };

    // Thermal warmup (docs/03 §2.1)
    bool   enableWarmup      { true };
    double warmupTauSeconds  { 20.0 };   ///< τ ≈ 15–30s for 12AX7

    // Miller capacitance emulation (docs/04 §3)
    bool   enableMillerFilter { true };
    double Cgp_miller         { 2.4e-12 }; ///< Grid-plate stray [F]
    double sourceImpedance    { 10.0e3 };  ///< Upstream Zout (Ω)

    // ─── Program-dependent Miller (docs/04 §3.3) ───────────────────────
    // Miller capacitance C_m = Cgp·(1 + |Av|) assumes |Av| is static —
    // yet a real tube's gain moves with the instantaneous bias point,
    // so C_m breathes with the signal too.  With this flag on, heavy
    // conduction increases C_m and pulls the HF cutoff down slightly;
    // quiescent sections let HF through.  The audible effect is a
    // self-compressing top end that most plugins (static Miller LPF)
    // cannot produce.  Depth 0.0 = legacy fixed Miller.
    double millerSignalDepth  { 0.5 };

    // ─── Grid conduction / blocking distortion (docs/04 §5) ─────────────
    // When the input swing drives the grid positive with respect to the
    // cathode, the grid-cathode junction conducts like a forward-biased
    // diode.  That current flows INTO the input coupling capacitor,
    // charging it up — which imposes a negative DC offset on the grid
    // that persists for τ = R_leak · C_coupling.  The audible result on
    // loud transients is a momentary bias shift that "sucks the signal
    // back" after the peak: this is the "blocking distortion" / "squish"
    // character of cranked guitar amps and pushed Marshall preamps.
    //
    // Charging is fast (R_stopper · C, typically a couple of ms);
    // discharging is slow (R_leak · C, tens of ms).  Most competing
    // plugins omit this entirely and use pure symmetric clippers, which
    // is why they fail to capture the "breathing" of a tube amp when
    // worked hard.
    bool   enableGridConduction { true };
    double gridLeakR            { 1.0e6 };    ///< Grid-leak resistor [Ω]
    double gridCouplingC        { 22.0e-9 };  ///< Input coupling cap [F]
    double gridStopperR         { 68.0e3 };   ///< Grid-stopper [Ω]
    double gridTurnOnVoltage    { 0.5 };      ///< g-k diode threshold [V]

    // ─── Heater-cathode hum coupling (docs/03 §7) ───────────────────────
    // AC-heated tubes have a small but nonzero capacitance + leakage
    // between the 6.3 V AC heater winding and the cathode.  That puts a
    // tiny 50/60 Hz ripple on the cathode potential, which shows up as a
    // quiet hum tone at the output.  The coupling isn't linear — the
    // cathode's own signal-dependent voltage excursion modulates the
    // heater-to-cathode potential difference, so loud passages "breathe"
    // the hum level up and down.  This program-dependent, never-quite-
    // silent texture is what most competing plugins scrub clean in the
    // name of "digital precision" — and exactly what listeners perceive
    // as vintage tube "aliveness".
    //
    // Realistic hum level for a well-designed preamp: around −85 to −75
    // dBFS in quiescent state.  At this level it's below most listener's
    // conscious hearing but well above the noise floor of a modern DAW,
    // so it's noticeable when A/Bed against a "sterile" plugin.
    bool   enableHeaterHum      { false };
    double heaterFrequency      { 60.0 };     ///< 50 Hz in EU gear
    double heaterHumAmplitude   { 2.0e-4 };   ///< ≈ −74 dB at 1 V ref
    double heaterModDepth       { 0.5 };      ///< how much signal level
                                              ///  modulates the hum

    // ─── Plate-dissipation thermal drift (docs/03 §2.3) ─────────────────
    // The fast cathode-bounce τ (R_k·C_k ≈ 37 ms) handles sample-level
    // bias wobble.  But sustained high average plate current also HEATS
    // the cathode / raises its emission / slowly shifts the operating
    // point over *seconds* — this is the "the amp sits down" impression
    // one gets from a real tube stage working hard for 10+ seconds: the
    // gain reduces a touch and the character rounds off.  It cannot be
    // recovered with fast compression because the underlying mechanism
    // is thermal.  Most plugins skip this entirely; with it on, a long
    // loud section quietly settles into a slightly different voice than
    // the opening few beats.
    //
    //   Ip_avg_long  ←  slow envelope follower on |Ip|, τ ≈ thermalTau
    //   ΔVg_bias     =  thermalBiasSensitivity · (Ip_avg_long − Ip_rest)
    //   → grid bias becomes more negative → gain drops
    bool   enableThermalDrift      { true };
    double thermalTauSeconds       { 8.0 };     ///< 5–30 s in real tubes
    double thermalBiasSensitivity  { 150.0 };   ///< V/A of bias shift per
                                                ///  1 A of sustained Ip

    // ─── Asymmetric slew-rate limit (docs/04 §7) ────────────────────────
    // A real tube stage's output node has finite stray/Miller capacitance,
    // driven by the tube's plate current through the plate-load resistor.
    // The physics are inherently *asymmetric*: when the tube cuts off
    // (grid goes negative), the plate rises through Rp charging Cstray —
    // that's a slow RC pull-up.  When the tube conducts (grid rises),
    // the low tube impedance can yank the plate down very quickly.  The
    // resulting rise/fall asymmetry is most of what "tube punch" actually
    // sounds like on percussive material.  Pure-waveshaper plugins clip
    // in zero time in both directions and thus flatten drum transients.
    //
    // Values here are in **normalized output units per second** at the
    // internal sample rate.  The defaults are deliberately conservative
    // so light material isn't affected, but fast transients pick up a
    // musical round-off that matches real hardware oscilloscope traces.
    bool   enableSlewLimit         { true };
    // Common-cathode physics: upward plate excursions (output rising)
    // happen through the plate-load resistor charging the stray cap —
    // slow RC pull-up.  Downward excursions use the low ON-state tube
    // impedance — fast pull-down.  Units: normalized-output per second.
    double slewRatePositive        { 1500.0 };  ///< Rising (slow, RC)
    double slewRateNegative        { 4000.0 };  ///< Falling (fast, tube
                                                ///  conducting)

    // ─── Cathode-cap dielectric absorption (matches CathodeBounceParams)
    // Exposed at stage level so Monte Carlo can perturb DA per-instance.
    bool   enableSoakage { true };
    double soakageAmount { 0.10 };
    double soakageTau    { 0.15 };

    // ─── Thermionic shot noise (docs/03 §8) ─────────────────────────────
    // Real vacuum tubes conduct in discrete electrons, not a smooth
    // current, so the plate current carries √(2·q·Ip·BW) shot-noise
    // fluctuations on top of the deterministic value.  The audible
    // signature is a faint hiss whose level MOVES WITH the program:
    // quiet passages get quieter, loud passages pick up more noise.
    // Most plugins either strip noise out entirely (for a sterile,
    // digital "clean") or inject white noise at a fixed level — both
    // miss the program-dependent breathing that real tube gear does.
    //
    // Amplitude units are normalized volts at the grid (additive).
    // The *relative* level tracks √(|Ip| / Ip_rest); scale sets the
    // reference level at resting plate current.
    bool   enableShotNoise { true };
    double shotNoiseScale  { 1.0e-5 };  ///< Conservative default (well
                                        ///  below perceptual threshold
                                        ///  at rest; scales with program)

    // ─── Microphonic coupling (docs/03 §10) ─────────────────────────────
    // A real tube has internal grid + plate elements held in place by
    // mica spacers and getter-tab welds.  Heavy plate-current swings
    // physically vibrate the tube envelope (magnetostriction in nearby
    // iron + chassis sympathetic resonance), shaking those elements
    // and modulating Cgk / gm at the body's mechanical resonance —
    // typically a damped peak between 80 Hz and 250 Hz.  The audible
    // result is a faint, program-dependent "ringing" texture that lives
    // in the low-mid range, especially noticeable on bass-heavy
    // material.  Plugins that ignore mechanical coupling sound static
    // and 2-D by comparison.
    //
    // Implemented as a parametric AM: |Ip variation| drives a 2-pole
    // resonator whose output modulates gm (i.e. multiplies plate
    // current).  Open-loop, so no oscillation risk.
    bool   enableMicrophonics { false };  ///< per-preset opt-in
    double micResonanceHz     { 120.0 };  ///< body resonance centre
    double micResonanceQ      { 4.0 };    ///< modest Q for damped peak
    double micDepth           { 1.5e-3 }; ///< gm modulation depth
};

// ─────────────────────────────────────────────────────────────────────────────
// TubeStage
// ─────────────────────────────────────────────────────────────────────────────
class TubeStage
{
public:
    TubeStage() = default;

    void setup(const TubeStageConfig& cfg, double sampleRate)
    {
        config_     = cfg;
        sampleRate_ = sampleRate;

        triode_.setParams(cfg.tube);

        // Heater hum phase accumulator.  Pre-compute the per-sample
        // phase increment so the inner loop does one add + one branch.
        heaterPhaseInc_ =
            2.0 * M_PI * cfg.heaterFrequency / sampleRate;

        // Thermal-drift envelope coefficient (seconds-long τ, very close
        // to 1 at typical sample rates).
        thermalAlpha_ = std::exp(
            -1.0 / (cfg.thermalTauSeconds * sampleRate));

        // Microphonic-coupling biquad bandpass (RBJ cookbook BPF).
        {
            const double w0   = 2.0 * M_PI * cfg.micResonanceHz / sampleRate;
            const double sinw = std::sin(w0);
            const double cosw = std::cos(w0);
            const double Q    = std::max(cfg.micResonanceQ, 0.1);
            const double alph = sinw / (2.0 * Q);
            const double a0   = 1.0 + alph;
            micB0_ =  alph / a0;
            micB1_ =  0.0;
            micB2_ = -alph / a0;
            micA1_ = -2.0 * cosw / a0;
            micA2_ = (1.0 - alph) / a0;
            micX1_ = micX2_ = micY1_ = micY2_ = 0.0;
        }

        // Cathode bounce state (CathodeBounceParams API)
        CathodeBounceParams bp;
        bp.Rk = cfg.Rk;
        bp.Ck = cfg.Ck;
        bp.sampleRate    = sampleRate;
        bp.enableSoakage = cfg.enableSoakage;
        bp.soakageAmount = cfg.soakageAmount;
        bp.soakageTau    = cfg.soakageTau;
        bounce_.setParams(bp);

        // Thermal warmup state
        warmupAlpha_ = cfg.enableWarmup
            ? std::exp(-1.0 / (cfg.warmupTauSeconds * sampleRate))
            : 0.0;
        warmupCurrent_ = cfg.enableWarmup ? 0.85 : 1.0; // start at 85% gm
        warmupTarget_  = 1.0;

        // Calibrate resting point
        Ip_rest_ = triode_.plateCurrent(cfg.Vp_nominal, cfg.Vg_bias);
        Vk_rest_ = Ip_rest_ * cfg.Rk;
        Vp_rest_ = cfg.Vp_nominal - Ip_rest_ * cfg.Rp;

        // Prime cathode bypass to its resting DC so the first sample does
        // not produce a 37.5 ms startup transient.
        bounce_.primeTo(Vk_rest_);

        // Thermal drift starts at rest — any extra bias shift should grow
        // only after a sustained loud passage, not at plugin load.
        ipAvgLong_ = Ip_rest_;

        // Prime lastIp_ to the resting current so the signal-dependent
        // Miller programFactor starts at 1.0 (= legacy fixed Miller).
        // Leaving it at zero would briefly brighten the first block as
        // factor collapses to (1 − millerSignalDepth).
        lastIp_ = Ip_rest_;

        // Zero transient state
        millerState_ = 0.0;
        slewState_   = 0.0;
        gridChargeV_ = 0.0;
        // Resting DC depends on where we tap the output:
        //   Common cathode / default:  output = Vp (plate)
        //   Cathode follower:          output = Vk (cathode)
        outputDC_    = (cfg.topology == TubeTopology::CathodeFollower)
            ? Vk_rest_
            : Vp_rest_;
    }

    // Reset state variables (e.g., on sample rate change or user "cold start").
    // Prime cathode bypass to resting DC to suppress startup transient.
    void reset(bool coldStart = true)
    {
        bounce_.primeTo(Vk_rest_);
        warmupCurrent_ = (coldStart && config_.enableWarmup) ? 0.85 : 1.0;
        millerState_   = 0.0;
        gridChargeV_   = 0.0;
        ipAvgLong_     = Ip_rest_;
        slewState_     = 0.0;
        lastIp_        = Ip_rest_;  // keeps Miller programFactor = 1
        outputDC_      = (config_.topology == TubeTopology::CathodeFollower)
            ? Vk_rest_
            : Vp_rest_;
    }

    // Trigger an instantaneous warm-up simulation (UI button)
    void simulateWarmup()
    {
        if (config_.enableWarmup)
            warmupCurrent_ = 0.85;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Process one audio sample.
    //
    //   inputSample : normalized audio [-1, +1] (approximate)
    //   Vb_plus     : current B+ supply voltage (from PowerSupplySag; pass
    //                 config_.Vp_nominal if no sag simulation upstream)
    //
    // Returns: normalized audio [-1, +1] after tube + cathode + miller
    // ─────────────────────────────────────────────────────────────────────────
    double process(double inputSample, double Vb_plus) noexcept
    {
        // NaN-recovery: reject any non-finite input or internal state before
        // letting it propagate into miller_ / outputDC_ / cathode bounce.
        if (! std::isfinite(inputSample) || ! std::isfinite(Vb_plus)
                                         || ! std::isfinite(outputDC_)
                                         || ! std::isfinite(millerState_)
                                         || ! std::isfinite(warmupCurrent_)
                                         || ! std::isfinite(gridChargeV_)
                                         || ! std::isfinite(heaterPhase_)
                                         || ! std::isfinite(ipAvgLong_)
                                         || ! std::isfinite(slewState_)
                                         || ! std::isfinite(micY1_)
                                         || ! std::isfinite(micY2_))
        {
            outputDC_      = Vp_rest_;
            millerState_   = 0.0;
            warmupCurrent_ = 1.0;
            lastIp_        = Ip_rest_;
            gridChargeV_   = 0.0;
            heaterPhase_   = 0.0;
            ipAvgLong_     = Ip_rest_;
            slewState_     = 0.0;
            micX1_ = micX2_ = micY1_ = micY2_ = 0.0;
            bounce_.primeTo(Vk_rest_);
            return 0.0;
        }

        // 1) Thermal warmup (slow)
        if (config_.enableWarmup)
        {
            warmupCurrent_ =
                warmupAlpha_ * warmupCurrent_ +
                (1.0 - warmupAlpha_) * warmupTarget_;
        }

        // 2) Miller capacitance low-pass (input side, signal-dependent cutoff)
        double vgSignal = inputSample * config_.inputVoltageSwing;
        if (config_.enableMillerFilter)
            vgSignal = applyMillerFilter(vgSignal);

        // 3) Cathode bypass dynamic bias shift:
        //    CathodeBounce tracks full Vk (including DC). We subtract Vk_rest
        //    to get only the AC perturbation, so that in resting state the
        //    effective bias equals config_.Vg_bias (no artificial offset).
        const double Vk_full = config_.enableCathodeBounce
            ? bounce_.currentBias()
            : Vk_rest_;
        const double deltaVk = Vk_full - Vk_rest_;
        double       Vg      = config_.Vg_bias + vgSignal - deltaVk;

        // 3-pre) Plate-dissipation thermal bias drift — very slow.  The
        //     average plate current drags the grid bias more negative
        //     over multiple seconds, so a long loud passage settles into
        //     a slightly lower-gain voice ("the amp sits down").
        if (config_.enableThermalDrift)
        {
            Vg -= config_.thermalBiasSensitivity
                * (ipAvgLong_ - Ip_rest_);
        }

        // 3c) Thermionic shot noise — amplitude tracks √(|Ip|/Ip_rest)
        //     based on the previous sample's plate current, producing
        //     the program-dependent "breath" that real tubes have.
        if (config_.enableShotNoise)
        {
            const double ipDenom = std::max(std::abs(Ip_rest_), 1.0e-9);
            const double ipRatio =
                std::sqrt(std::abs(lastIp_) / ipDenom);
            Vg += config_.shotNoiseScale * ipRatio * gaussianApprox_();
        }

        // 3a) Heater-cathode hum — inject a small 50/60 Hz ripple with
        //     signal-dependent amplitude.  The hum leaks onto the grid
        //     via the heater-cathode capacitance, so it looks to the
        //     triode like an extra bias wobble.
        if (config_.enableHeaterHum)
        {
            heaterPhase_ += heaterPhaseInc_;
            if (heaterPhase_ >= 2.0 * M_PI)
                heaterPhase_ -= 2.0 * M_PI;

            const double levelMod =
                1.0 + config_.heaterModDepth * std::abs(vgSignal);
            const double hum =
                config_.heaterHumAmplitude * levelMod * std::sin(heaterPhase_);
            Vg += hum;
        }

        // 3b) Grid conduction blocking.  If Vgk > turn-on voltage, the
        //     grid-cathode junction conducts → current charges the input
        //     coupling cap → negative DC bias shift that discharges slowly
        //     through the grid-leak resistor.  This is the physical origin
        //     of "blocking distortion" / "farting out" on pushed transients.
        if (config_.enableGridConduction && std::isfinite(gridChargeV_))
        {
            // Charge on the coupling cap shows up as a voltage offset that
            // *subtracts* from the grid drive (cap holds cathode-positive
            // charge after a grid-positive excursion).
            const double Vg_loaded = Vg - gridChargeV_;

            // Instantaneous grid current through the stopper resistor when
            // the grid is driven past the g-k turn-on voltage.
            const double Vover = Vg_loaded - config_.gridTurnOnVoltage;
            const double Ig    = (Vover > 0.0)
                ? Vover / config_.gridStopperR
                : 0.0;

            // Integrate the cap voltage:  C · dV/dt = Ig − V/R_leak
            //   charging τ ≈ R_stopper · C   (a few ms)
            //   discharge τ ≈ R_leak · C     (tens of ms)
            const double dt    = 1.0 / sampleRate_;
            const double dV    = (Ig / config_.gridCouplingC
                               -  gridChargeV_ / (config_.gridLeakR
                                                * config_.gridCouplingC)) * dt;
            gridChargeV_ = gridChargeV_ + dV;
            if (gridChargeV_ < 0.0) gridChargeV_ = 0.0;   // diode is one-way

            Vg = Vg_loaded;   // triode sees the blocked grid voltage
        }

        // 4) Plate current from Dempwolf model.
        //    Scale by warmup factor for emission ramp-up (thermal drift).
        double Ip = triode_.plateCurrent(Vb_plus, Vg) * warmupCurrent_;

        // 4-mic) Microphonic coupling — drive a body-resonance bandpass
        //     with Ip swing, then use its output to modulate gm.  The
        //     modulation is purely multiplicative on Ip, so the loop is
        //     OPEN and cannot self-oscillate regardless of depth.
        if (config_.enableMicrophonics
            && std::isfinite(micY1_) && std::isfinite(micY2_))
        {
            const double drive = Ip - Ip_rest_;
            const double y0 = micB0_ * drive
                            + micB1_ * micX1_ + micB2_ * micX2_
                            - micA1_ * micY1_ - micA2_ * micY2_;
            micX2_ = micX1_;  micX1_ = drive;
            micY2_ = micY1_;  micY1_ = y0;

            // Normalize the resonator output by Ip_rest so the depth
            // sets a sensible per-unit modulation regardless of preset.
            const double ipDenom = std::max(std::abs(Ip_rest_), 1.0e-9);
            const double modSig  = std::clamp(y0 / ipDenom, -2.0, 2.0);
            Ip *= 1.0 + config_.micDepth * modSig;
        }
        lastIp_ = Ip;

        // 4b) Advance the long-term average that drives thermal bias
        //     drift.  Use |Ip| so both polarities of signal heat the
        //     cathode identically (it's a thermal process, not a
        //     directional one).
        if (config_.enableThermalDrift)
        {
            ipAvgLong_ = thermalAlpha_ * ipAvgLong_
                       + (1.0 - thermalAlpha_) * std::abs(Ip);
        }

        // 5) Update cathode bypass state with new Ip (for next sample)
        const double Vk_new = config_.enableCathodeBounce
            ? bounce_.process(Ip)
            : Vk_rest_;

        // 6) Pick the appropriate output node.
        //    Common cathode / default → Vp = Vb+ − Ip·Rp (inverting amp)
        //    Cathode follower         → Vk  (non-inverting buffer, output at
        //                                     cathode; Rp unused)
        double rawOut;
        double normalizer;
        if (config_.topology == TubeTopology::CathodeFollower)
        {
            rawOut     = Vk_new;
            // Cathode follower: normalize by a typical cathode voltage scale.
            // Vk_rest is typically 1–10 V for a 12AX7/12AU7; use a floor to
            // avoid divide-by-zero if Rk is tiny.
            normalizer = std::max(std::abs(Vk_rest_) * 2.0, 1.0);
        }
        else
        {
            rawOut     = Vb_plus - Ip * config_.Rp;
            normalizer = config_.Vp_nominal;
        }

        // 7) AC-couple output (subtract slow-tracked DC operating point).
        const double ac = rawOut - outputDC_;

        constexpr double dcLeakAlpha = 0.9999;
        outputDC_ = dcLeakAlpha * outputDC_ + (1.0 - dcLeakAlpha) * rawOut;

        double yNorm = config_.outputGainLinear * ac / normalizer;

        // 8) Asymmetric slew-rate limit — operates on the normalized output
        //    so the rates are independent of preset / Vp scaling.  Rising
        //    and falling edges get different caps, which injects the
        //    low-order harmonics that define "tube punch" on percussive
        //    material.  For small signals (slow changes per sample) this
        //    branch is essentially transparent.
        if (config_.enableSlewLimit && std::isfinite(slewState_))
        {
            const double dt      = 1.0 / sampleRate_;
            const double maxRise = config_.slewRatePositive * dt;
            const double maxFall = config_.slewRateNegative * dt;
            double delta = yNorm - slewState_;
            if      (delta >  maxRise) delta =  maxRise;
            else if (delta < -maxFall) delta = -maxFall;
            slewState_ += delta;
            yNorm = slewState_;
        }
        return yNorm;
    }

    // Accessors for visualization / diagnostics (UI meters)
    double warmupProgress() const noexcept { return warmupCurrent_; }
    double lastPlateVoltage() const noexcept { return outputDC_; }
    double lastPlateCurrent() const noexcept { return lastIp_; }
    double restingPlateCurrent() const noexcept { return Ip_rest_; }

    /// Long-term thermal bias drift in volts (negative = bias pulled more
    /// negative by sustained heavy Ip).  Exposed for unit tests and the
    /// future UI "amp fatigue" indicator.
    double thermalBiasShift() const noexcept
    {
        return config_.enableThermalDrift
            ? config_.thermalBiasSensitivity * (ipAvgLong_ - Ip_rest_)
            : 0.0;
    }
    const KorenTriode& triode() const noexcept { return triode_; }
    KorenTriode& triodeRef() noexcept { return triode_; }

private:
    // Signal-dependent Miller low-pass (docs/04 §3.2–§3.3)
    //   fc = 1 / (2π · Zsource · Cmiller)
    //   Cmiller ≈ Cgp · (1 + |Av|) · programFactor
    //   |Av| static component uses quiescent gm.
    //   programFactor depends on the previous sample's |Ip| so heavy
    //   conduction momentarily grows C_m and rolls HF off a touch more.
    double applyMillerFilter(double x) noexcept
    {
        const double gmStatic =
            triode_.transconductance(config_.Vp_nominal, config_.Vg_bias) *
            warmupCurrent_;
        const double absAvStatic = std::abs(gmStatic * config_.Rp);

        // Dynamic gain factor driven by the last sample's conduction.
        // Physical reasoning: gm ∝ Ip (Koren); heavy conduction raises
        // |Av| → bigger Miller → HF cutoff drops.  Cutoff portions have
        // gm ≈ 0, but we *don't* want them to reduce C_m below the
        // quiescent value (the stray Cgp is still there even when the
        // tube's off), so we clamp the factor ≥ 1.
        const double ipDenom = std::max(std::abs(Ip_rest_), 1.0e-9);
        const double ipNorm  = std::abs(lastIp_) / ipDenom;
        const double factor  = std::max(
            1.0,
            1.0 + config_.millerSignalDepth * (ipNorm - 1.0));

        const double Cmiller = config_.Cgp_miller
                             * (1.0 + absAvStatic * factor);
        const double fc =
            1.0 / (2.0 * M_PI * config_.sourceImpedance * Cmiller);

        const double alpha =
            1.0 - std::exp(-2.0 * M_PI * fc / sampleRate_);
        millerState_ += alpha * (x - millerState_);
        return millerState_;
    }

    TubeStageConfig config_ {};
    double sampleRate_ { 44100.0 };

    KorenTriode   triode_ { params::kRSD_1 };
    CathodeBounce bounce_ {};

    // Resting-point cache (computed once in setup())
    double Ip_rest_ { 0.0 };
    double Vk_rest_ { 0.0 };
    double Vp_rest_ { 0.0 };

    // Time-varying states
    double warmupAlpha_    { 0.0 };
    double warmupCurrent_  { 1.0 };
    double warmupTarget_   { 1.0 };
    double millerState_    { 0.0 };
    double outputDC_       { 0.0 };
    double lastIp_         { 0.0 };  ///< Last computed plate current (for PSU)
    double gridChargeV_    { 0.0 };  ///< Grid-coupling-cap offset [V]
    double heaterPhase_    { 0.0 };
    double heaterPhaseInc_ { 0.0 };
    double thermalAlpha_   { 0.0 };  ///< IIR coeff for long-τ envelope
    double ipAvgLong_      { 0.0 };  ///< Slow Ip average [A]
    double slewState_      { 0.0 };  ///< Rate-limited output carry
    std::uint64_t shotRng_ { 0xA5A5A5A5A5A5A5A5ULL };  ///< xorshift64 state

    // Microphonic biquad bandpass state (RBJ form, DF-I)
    double micB0_ { 0.0 }, micB1_ { 0.0 }, micB2_ { 0.0 };
    double micA1_ { 0.0 }, micA2_ { 0.0 };
    double micX1_ { 0.0 }, micX2_ { 0.0 };
    double micY1_ { 0.0 }, micY2_ { 0.0 };

    // Fast xorshift64 + central-limit Gaussian for program-dependent
    // shot noise.  Avoids sqrt/log per sample; good enough for audio.
    std::uint64_t nextXorshift_() noexcept
    {
        std::uint64_t x = shotRng_;
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        shotRng_ = x;
        return x;
    }
    double uniformM11_() noexcept
    {
        // Map 24 random bits into [−1, +1]
        const std::uint64_t r = nextXorshift_() & 0xFFFFFFULL;
        return static_cast<double>(r) / 8388607.5 - 1.0;
    }
    double gaussianApprox_() noexcept
    {
        // Central-limit sum of 6 uniforms on [−1, +1] has σ² = 6/3 = 2
        // → scale by 1/√2 ≈ 0.7071 for unit variance.
        double a = uniformM11_() + uniformM11_() + uniformM11_()
                 + uniformM11_() + uniformM11_() + uniformM11_();
        return a * 0.70710678118;
    }

public:
    /// Seed the shot-noise RNG for per-instance reproducibility.  Called
    /// by the chain from the Monte Carlo seed on setup/reroll.
    void setShotNoiseSeed(std::uint64_t seed) noexcept
    {
        // Mix so the caller passing 0 doesn't produce a degenerate state.
        shotRng_ = seed ^ 0xA5A5A5A5A5A5A5A5ULL;
        if (shotRng_ == 0) shotRng_ = 0xDEADBEEFCAFEBABEULL;
    }

private:

public:
    /// Set an initial heater-hum phase.  The chain builder calls this so
    /// each stage starts at a different point in the 60 Hz cycle, and L
    /// vs R channels have uncorrelated hum — imitating the real world
    /// where no two tubes are wired identically to the heater winding.
    void setHeaterPhase(double phase) noexcept { heaterPhase_ = phase; }

private:
};

// ─────────────────────────────────────────────────────────────────────────────
// Mode preset factory (docs/24)
//
// Returns a TubeStageConfig preconfigured for a specific hardware mode.
// The chain (multi-stage + transformer) is assembled at a higher level
// in TubeAmpChain.
// ─────────────────────────────────────────────────────────────────────────────
namespace presets {

/// Preamp Mode Stage 1 — V72-style input pentode (12AX7 triode-strapped proxy).
/// Drives the tube hot enough to produce authentic 2nd-harmonic content
/// (the signature of single-ended class-A triode stages).  Stage 2 then
/// carries this H2 through without adding its own H3 load.
inline TubeStageConfig v72Stage1()
{
    TubeStageConfig s {
        .tube = params::kRSD_1,
        .topology = TubeTopology::CommonCathode,
        .Vg_bias = -1.5, .Vp_nominal = 260.0,
        .Rp = 220.0e3, .Rk = 1500.0, .Ck = 47.0e-6,
        .inputVoltageSwing = 0.8,     // push toward asymmetric saturation → H2
        .outputGainLinear = 4.0,      // substantial makeup for V72 preamp gain
        .enableWarmup = true, .warmupTauSeconds = 20.0,
        .enableMillerFilter = true,
        .Cgp_miller = 2.4e-12,
        .sourceImpedance = 600.0  // after input transformer step-up
    };
    s.enableHeaterHum = true;       // vintage AC-heated Neve V72 character
    return s;
}

/// Preamp Mode Stage 2 — V72-style driver (12AU7 / 6072).
/// Key tuning: the *inputVoltageSwing* is kept modest so stage 2 operates
/// in its linear region rather than being pushed into H3-dominant polynomial
/// territory. This lets the H2 from stage 1 (the authentic "tube warmth"
/// generator) pass through intact rather than being overridden.
inline TubeStageConfig v72Stage2()
{
    TubeStageConfig s {
        .tube = params::kECC82_Koren,
        .topology = TubeTopology::CommonCathode,
        .Vg_bias = -2.0, .Vp_nominal = 290.0,
        .Rp = 100.0e3, .Rk = 820.0, .Ck = 22.0e-6,
        .inputVoltageSwing = 0.4,  // linear region — let stage-1 H2 dominate
        .outputGainLinear = 3.0,   // recover gain lost to reduced swing
        .enableWarmup = true, .warmupTauSeconds = 25.0,
        .enableMillerFilter = true,
        .Cgp_miller = 1.5e-12,
        .sourceImpedance = 10.0e3  // output of Stage 1
    };
    s.enableHeaterHum = true;
    // Driver stage is further from the heater wiring loom → a touch less
    // coupling than Stage 1.
    s.heaterHumAmplitude = 1.2e-4;
    return s;
}

/// Culture Vulture-style input (EF86 triode-strapped proxy).
/// 3-stage cascade suffers compounded voltage-normalization loss (each
/// CC stage divides by Vp_nominal ≈ 250 V), so the input stage uses
/// generous drive swing + makeup gain to keep the signal above the
/// noise floor of subsequent stages.
inline TubeStageConfig cultureVultureInput()
{
    auto p = params::kRSD_1;
    p.mu = 38.0; p.gamma = 1.35;
    TubeStageConfig s {
        .tube = p,
        .topology = TubeTopology::CommonCathode,
        .Vg_bias = -1.0, .Vp_nominal = 250.0,
        .Rp = 100.0e3, .Rk = 820.0, .Ck = 1.0e-6,
        .inputVoltageSwing = 3.0,   // push into the nonlinear region
        .outputGainLinear = 8.0,    // heavy makeup for cascade
        .enableWarmup = true, .warmupTauSeconds = 20.0,
        .enableMillerFilter = true,
        .Cgp_miller = 0.1e-12,
        .sourceImpedance = 600.0
    };
    s.enableHeaterHum = true;  // boutique AC-heated unit with audible breath
    return s;
}

/// RNDI DI-style light saturation stage (12AX7 cathode follower)
inline TubeStageConfig rndiStage()
{
    return TubeStageConfig {
        .tube = params::kEHX_1,  // softer EHX variant
        .topology = TubeTopology::CathodeFollower,
        .Vg_bias = -1.5, .Vp_nominal = 300.0,
        .Rp = 0.0,       // cathode follower: output at cathode, no plate load needed
        .Rk = 4700.0, .Ck = 0.0,  // unbypassed for linearity
        .inputVoltageSwing = 1.0,
        .outputGainLinear = 1.0,
        .enableWarmup = true, .warmupTauSeconds = 15.0,
        .enableMillerFilter = false,  // follower has low Miller
        .sourceImpedance = 1.0e6       // Hi-Z input
    };
}

// ─────────────────────────────────────────────────────────────────────────────
// Marshall-style power stage (triode-strapped EL34 approximation).
// Full push-pull LTP implementation is a Tier-2 feature; this single-tube
// approximation captures the sonic character (gritty saturation, substantial
// harmonic content) via cascade of two hot-driven 12AX7 stages into an
// EL34-flavored output tube.
// ─────────────────────────────────────────────────────────────────────────────

/// Marshall-style Stage 1 — gain tube (12AX7 cold-biased for grit)
inline TubeStageConfig marshallStage1()
{
    TubeStageConfig s {
        .tube = params::kRSD_1,
        .topology = TubeTopology::CommonCathode,
        .Vg_bias = -1.2, .Vp_nominal = 320.0,  // higher B+, colder bias
        .Rp = 100.0e3, .Rk = 820.0, .Ck = 1.0e-6,  // partial bypass (bright)
        .enableCathodeBounce = false, // fixed-bias style: no slow cathode bounce
        .inputVoltageSwing = 0.3,
        .outputGainLinear = 1.0,
        .enableWarmup = true, .warmupTauSeconds = 15.0,
        .enableMillerFilter = true,
        .Cgp_miller = 2.4e-12,
        .sourceImpedance = 10.0e3
    };
    // Guitar amps are notoriously microphonic — chassis vibration plus
    // speaker-back coupling drive a clear body resonance.
    s.enableMicrophonics = true;
    return s;
}

/// Marshall-style Stage 2 — "hot driver" 12AX7 for heavy distortion
inline TubeStageConfig marshallStage2()
{
    TubeStageConfig s {
        .tube = params::kRSD_2,  // slightly different character (unmatched pair)
        .topology = TubeTopology::CommonCathode,
        .Vg_bias = -1.5, .Vp_nominal = 320.0,
        .Rp = 100.0e3, .Rk = 1500.0, .Ck = 22.0e-6,
        .enableCathodeBounce = false, // fixed-bias style: no slow cathode bounce
        .inputVoltageSwing = 3.0,  // pushed hard
        .outputGainLinear = 1.0,
        .enableWarmup = true, .warmupTauSeconds = 15.0,
        .enableMillerFilter = true,
        .Cgp_miller = 2.4e-12,
        .sourceImpedance = 100.0e3
    };
    s.enableMicrophonics = true;     // guitar amp body resonance
    s.micResonanceHz     = 95.0;     // slightly lower for V2 driver
    return s;
}

// ─────────────────────────────────────────────────────────────────────────────
// Culture Vulture–style "distortion core"
// (EF86-like pentode input → 6AS6-like distortion tube → 12AU7 buffer)
// ─────────────────────────────────────────────────────────────────────────────

/// CV distortion core — "6AS6" variable-mu proxy.  Warmer bias plus extra
/// makeup gain keeps signal above the noise floor through 3 stages.
inline TubeStageConfig cvDistortionCore()
{
    auto p  = params::kEHX_1;
    p.gamma = 1.45;
    return TubeStageConfig {
        .tube = p,
        .topology = TubeTopology::CommonCathode,
        .Vg_bias = -1.0, .Vp_nominal = 250.0,
        .Rp = 47.0e3, .Rk = 820.0, .Ck = 0.47e-6,
        .inputVoltageSwing = 1.0,
        .outputGainLinear = 6.0,   // substantial gain in this distortion stage
        .enableWarmup = true, .warmupTauSeconds = 20.0,
        .enableMillerFilter = true,
        .Cgp_miller = 0.5e-12,
        .sourceImpedance = 100.0e3
    };
}

/// CV output buffer (12AU7 cathode follower with makeup gain to compensate
/// for Vk/normalizer loss through the CF topology).
inline TubeStageConfig cvOutputBuffer()
{
    return TubeStageConfig {
        .tube = params::kECC82_Koren,
        .topology = TubeTopology::CathodeFollower,
        .Vg_bias = -2.0, .Vp_nominal = 250.0,
        .Rp = 0.0, .Rk = 4700.0, .Ck = 0.0,
        .inputVoltageSwing = 1.0,
        .outputGainLinear = 2.0,   // makeup for CF normalizer
        .enableWarmup = true, .warmupTauSeconds = 15.0,
        .enableMillerFilter = false,
        .sourceImpedance = 10.0e3
    };
}

} // namespace presets

} // namespace valvra::dsp
