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
#include "KorenPentode.h"
#include "CathodeBounce.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

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
    SRPP             = 2, ///< Shunt-regulated push-pull stacked pair
    LongTailedPair   = 3, ///< Differential (Output stage phase inverter)
    Cascode          = 4  ///< High bandwidth, low Miller stacked pair
};

enum class CultureVultureVoicing : int
{
    Triode      = 0,
    PentodeLow  = 1,
    PentodeHigh = 2
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

    // ─── Compound (stacked) topology fields (SRPP, Cascode) ─────────────
    // These two topologies share the same physical structure: two triodes
    // stacked vertically with the SAME plate current flowing through both.
    // The output is taken at the junction V_mid (SRPP) or at the upper
    // tube's plate (Cascode).  Per-sample we Newton-Raphson on V_mid until
    // the two tubes' Ip values match.
    //
    // The "upper" tube parameters default to the same as the main tube
    // (matched pair, the most common configuration); presets can override
    // for unusual choices like a 12AU7 over a 12AX7 active load.
    DempwolfParams upperTube      { params::kRSD_1 };

    // Cascode-only: optional plate-load resistor at the very top of the
    // stack (above the upper tube).  0 = no separate plate load (output
    // taken directly from upper plate node, which is then Vb).  Typical
    // cascode designs use a few hundred kΩ here.
    double Rp_upper               { 100.0e3 };

    // Cascode-only: fixed DC bias on the UPPER triode's grid.  In a real
    // cascode this is provided by a stable voltage divider.  Held about
    // half-way up the rail in a typical design.
    double Vg_upper_bias          { 90.0 };

    // Compound-topology Newton-Raphson solver depth.  2 iterations is
    // sufficient with warm-start (V_mid barely moves between samples) for
    // <0.1% residual.  Bump to 3 for tighter accuracy at extreme drive.
    int    compoundSolverIters    { 2 };

    // Long-Tailed Pair (phase inverter) internals.  Previous revisions used
    // a purely ideal phase split in PushPullStage; these fields model the
    // non-ideal LTP behaviour inside TubeStage: finite tail impedance and
    // pair mismatch produce imperfect cancellation and finite CMRR.
    double ltpTailR               { 47.0e3 }; // shared cathode tail [ohm]
    double ltpPlateRRatio         { 1.0 };    // plate-load mismatch ratio
    double ltpTubeMismatch        { 0.0 };    // ±fractional tube mismatch
    double ltpCommonModeLeak      { 0.03 };   // 0 ideal diff, >0 finite CMRR
    int    ltpSolverIters         { 2 };

    // 6AS6 / Culture Vulture-style variable-mu control.  A real 6AS6 uses
    // suppressor-grid bias to make gm collapse as the tube is driven.  The
    // triode-equivalent solver keeps the plate-current math stable; this
    // envelope bends the effective grid swing before the current law.
    bool   enableVariableMu        { false };
    double variableMuDepth         { 0.0 };
    double variableMuKneeVolts     { 1.0 };

    // Full pentode model path (screen + suppressor solver).
    bool   enablePentodeModel      { false };
    PentodeParams pentode          { pentode_params::k6AS6 };
    bool   pentodeTriodeStrap      { false };     // g2 tied to plate
    double screenSupplyVolts       { 170.0 };     // B+ -> Rs -> g2
    double screenResistorOhms      { 47.0e3 };
    double screenBypassFarads      { 1.0e-6 };
    double suppressorBiasVolts     { 0.0 };       // fixed g3 bias
    bool   suppressorTieToCathode  { true };      // classic pentode wiring
    double suppressorDriveMix      { 0.0 };       // g3 reacts to |signal|
    double suppressorDrivePolarity { -1.0 };      // +1 expands, -1 compresses
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
        triode_upper_.setParams(cfg.upperTube);
        pentode_.setParams(cfg.pentode);

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

        // Calibrate resting point.  For simple topologies (CC / CF) this
        // is a closed-form one-shot calc.  For compound topologies (SRPP
        // / Cascode) we solve the two-triode equilibrium via 16 iterations
        // of the same Newton-Raphson kernel that runs per sample at audio
        // rate — converges to <0.1 mV in practice, well below audibility.
        if (cfg.enablePentodeModel && cfg.topology == TubeTopology::CommonCathode)
        {
            solvePentodeRestPoint(cfg.Vp_nominal);
        }
        else
        {
            Ip_rest_ = triode_.plateCurrent(cfg.Vp_nominal, cfg.Vg_bias);
            Vk_rest_ = Ip_rest_ * cfg.Rk;
            Vp_rest_ = cfg.Vp_nominal - Ip_rest_ * cfg.Rp;
            screenNodeV_ = cfg.screenSupplyVolts;
            lastScreenCurrent_ = 0.0;
        }

        if (cfg.topology == TubeTopology::SRPP
            || cfg.topology == TubeTopology::Cascode)
        {
            // Joint fixed-point on (V_mid, Vk_lower).  The straight CC
            // calculation above used Vp = cfg.Vp_nominal which is wrong
            // for a compound topology where the lower tube actually sees
            // V_mid (typically much lower than Vp_nominal).  That over-
            // estimates Ip_rest, which over-estimates Vk_rest, which
            // self-biases the lower tube into cutoff if we feed it back
            // into the solver.
            //
            // Fix: iterate V_mid via Newton-Raphson WHILE updating Vk in
            // lock-step from the latest Ip_l estimate.  Convergence of
            // the joint problem is still fast (~16 iters) because both
            // sub-problems are smooth.
            double Vmid = cfg.Vp_nominal * 0.5;
            double Vk   = 0.0;          // start from "no cathode drop"
            double Ip_l_last = 0.0;
            for (int it = 0; it < 24; ++it)
            {
                const auto pair =
                    solveStackPair(Vmid, cfg.Vg_bias, Vk, cfg.Vp_nominal);
                const double f = pair.Ip_lower - pair.Ip_upper;
                if (std::abs(pair.fprime) > 1.0e-15)
                    Vmid -= f / pair.fprime;
                if (Vmid < 1.0)            Vmid = 1.0;
                if (Vmid > cfg.Vp_nominal) Vmid = cfg.Vp_nominal - 1.0;
                // Update Vk in lock-step from the latest lower-tube
                // current — this is what makes the joint problem
                // self-consistent (Vk = Ip · Rk by Ohm's law).
                Vk = pair.Ip_lower * cfg.Rk;
                Ip_l_last = pair.Ip_lower;
            }
            Vmid_rest_ = Vmid;
            Vmid_last_ = Vmid;
            Vk_rest_   = Vk;
            Ip_rest_   = Ip_l_last;
        }
        else if (cfg.topology == TubeTopology::LongTailedPair)
        {
            auto at = [](const KorenTriode& triode, double Vp, double Vgk)
            {
                return triode.evalWithDerivatives(Vp, Vgk);
            };

            // Build static mismatched pair around the stage's base triode.
            const double mm = std::clamp(cfg.ltpTubeMismatch, -0.45, 0.45);
            const double rpRatio = std::max(0.2, cfg.ltpPlateRRatio);
            auto tubeP = cfg.tube;
            auto tubeN = cfg.tube;
            tubeP.G  *= (1.0 + mm);
            tubeP.mu *= (1.0 - 0.5 * mm);
            tubeN.G  *= (1.0 - mm);
            tubeN.mu *= (1.0 + 0.5 * mm);
            ltpTriodePos_.setParams(tubeP);
            ltpTriodeNeg_.setParams(tubeN);

            double Vk = std::max(0.0, Vk_rest_);
            for (int it = 0; it < 24; ++it)
            {
                const auto p = at(ltpTriodePos_, cfg.Vp_nominal, cfg.Vg_bias - Vk);
                const auto n = at(ltpTriodeNeg_, cfg.Vp_nominal, cfg.Vg_bias - Vk);
                const double f = p.Ip + n.Ip - Vk / std::max(cfg.ltpTailR, 1.0);
                const double fp = -(p.gm + n.gm) - 1.0 / std::max(cfg.ltpTailR, 1.0);
                if (std::abs(fp) < 1.0e-15) break;
                Vk -= f / fp;
                if (! std::isfinite(Vk)) { Vk = 0.0; break; }
                Vk = std::max(0.0, Vk);
            }

            const auto p = at(ltpTriodePos_, cfg.Vp_nominal, cfg.Vg_bias - Vk);
            const auto n = at(ltpTriodeNeg_, cfg.Vp_nominal, cfg.Vg_bias - Vk);
            const double VpPos = cfg.Vp_nominal - p.Ip * cfg.Rp;
            const double VpNeg = cfg.Vp_nominal - n.Ip * cfg.Rp * rpRatio;

            Vk_rest_ = Vk;
            Ip_rest_ = 0.5 * (p.Ip + n.Ip);
            Vp_rest_ = 0.5 * (VpPos + VpNeg);
            ltpVkLast_ = Vk;
            ltpOutRest_ = 0.5 * (VpNeg - VpPos)
                        + std::clamp(cfg.ltpCommonModeLeak, 0.0, 1.0)
                        * (0.5 * (VpPos + VpNeg) - Vp_rest_);
        }

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
        gmRest_ = computeRestingGm();

        // Zero transient state
        millerState_ = 0.0;
        slewState_   = 0.0;
        gridChargeFastV_ = 0.0;
        gridChargeSlowV_ = 0.0;
        outputDC_    = restingOutputDC();
    }

    // Reset state variables (e.g., on sample rate change or user "cold start").
    // Prime cathode bypass to resting DC to suppress startup transient.
    void reset(bool coldStart = true)
    {
        bounce_.primeTo(Vk_rest_);
        warmupCurrent_ = (coldStart && config_.enableWarmup) ? 0.85 : 1.0;
        millerState_   = 0.0;
        gridChargeFastV_ = 0.0;
        gridChargeSlowV_ = 0.0;
        ipAvgLong_     = Ip_rest_;
        slewState_     = 0.0;
        lastIp_        = Ip_rest_;  // keeps Miller programFactor = 1
        lastScreenCurrent_ = 0.0;
        screenNodeV_   = config_.screenSupplyVolts;
        Vmid_last_     = Vmid_rest_;
        ltpVkLast_     = Vk_rest_;
        outputDC_      = restingOutputDC();
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
                                         || ! std::isfinite(gridChargeFastV_)
                                         || ! std::isfinite(gridChargeSlowV_)
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
            gridChargeFastV_ = 0.0;
            gridChargeSlowV_ = 0.0;
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

        if (config_.enableVariableMu
            && !(config_.enablePentodeModel
                 && config_.topology == TubeTopology::CommonCathode))
        {
            const double knee = std::max(config_.variableMuKneeVolts, 1.0e-3);
            const double driveNorm = std::abs(vgSignal) / knee;
            const double gmScale = 1.0
                / (1.0 + std::max(0.0, config_.variableMuDepth) * driveNorm);
            Vg = config_.Vg_bias + (Vg - config_.Vg_bias) * gmScale;
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
        if (config_.enableGridConduction
            && std::isfinite(gridChargeFastV_)
            && std::isfinite(gridChargeSlowV_))
        {
            // Charge on the coupling cap shows up as a voltage offset that
            // *subtracts* from the grid drive (cap holds cathode-positive
            // charge after a grid-positive excursion).
            const double gridChargeV = gridChargeFastV_ + gridChargeSlowV_;
            const double Vg_loaded = Vg - gridChargeV;

            // Instantaneous grid current through the stopper resistor when
            // the grid is driven past the g-k turn-on voltage.
            const double Vover = Vg_loaded - config_.gridTurnOnVoltage;
            const double IgRaw = (Vover > 0.0)
                ? Vover / config_.gridStopperR
                : 0.0;

            // Cross damping: under sustained thermal drift, reduce how fast
            // grid-conduction memory can accumulate in the same direction.
            const double ipDenom = std::max(std::abs(Ip_rest_), 1.0e-9);
            const double thermalNorm = config_.enableThermalDrift
                ? std::clamp((ipAvgLong_ - Ip_rest_) / ipDenom, 0.0, 6.0)
                : 0.0;
            const double thermalDamping = 1.0 / (1.0 + 0.35 * thermalNorm);
            const double Ig = IgRaw * thermalDamping;

            // Two-time-constant blocking memory:
            //   fast branch   -> "attack squeeze"
            //   slow branch   -> lingering recovery tail
            const double dt    = 1.0 / sampleRate_;
            const double c = std::max(config_.gridCouplingC, 1.0e-12);
            const double rLeak = std::max(config_.gridLeakR, 1.0e3);
            const double fastToSlowR = 7.0 * rLeak;
            const double slowBleedR = 18.0 * rLeak;

            const double dVfast = (Ig / c
                - gridChargeFastV_ / (rLeak * c)
                - (gridChargeFastV_ - gridChargeSlowV_) / (fastToSlowR * c)) * dt;
            const double dVslow = ((gridChargeFastV_ - gridChargeSlowV_) / (fastToSlowR * c)
                - gridChargeSlowV_ / (slowBleedR * c)) * dt;

            gridChargeFastV_ += dVfast;
            gridChargeSlowV_ += dVslow;

            const double extraBleed = dt * (1.0e-4 + thermalNorm * 5.0e-4);
            gridChargeFastV_ *= std::max(0.0, 1.0 - extraBleed);
            gridChargeSlowV_ *= std::max(0.0, 1.0 - extraBleed * 0.35);

            if (gridChargeFastV_ < 0.0) gridChargeFastV_ = 0.0;
            if (gridChargeSlowV_ < 0.0) gridChargeSlowV_ = 0.0;

            Vg = Vg_loaded;   // triode sees the blocked grid voltage
        }

        // 4) Plate current.
        //    For simple topologies (CC / CF) this is a single Dempwolf
        //    plateCurrent() call.  For compound topologies (SRPP / Cascode)
        //    we Newton-Raphson on the junction voltage V_mid until the
        //    two stacked tubes' currents agree, then take the converged
        //    Ip as the plate current driving the OPT (or downstream stage).
        //    Warm-start V_mid from the previous sample → 1–2 iterations
        //    suffice in audio-rate steady state.
        double Ip;
        if (config_.topology == TubeTopology::SRPP
            || config_.topology == TubeTopology::Cascode)
        {
            double Vmid = Vmid_last_;
            for (int it = 0; it < config_.compoundSolverIters; ++it)
            {
                const auto pair = solveStackPair(Vmid, Vg, Vk_full, Vb_plus);
                const double f = pair.Ip_lower - pair.Ip_upper;
                if (std::abs(pair.fprime) < 1.0e-15) break;
                Vmid -= f / pair.fprime;
                if (! std::isfinite(Vmid))
                {
                    Vmid = Vmid_last_;
                    break;
                }
                // Loose physical clamp — V_mid lives between cathode-side
                // (~0 V) and the plate-supply rail.  Clamp prevents a bad
                // step from sending the solver into infinity.
                if (Vmid < 1.0)         Vmid = 1.0;
                if (Vmid > Vb_plus)     Vmid = Vb_plus - 1.0;
            }
            Vmid_last_ = Vmid;
            const auto final_ = solveStackPair(Vmid, Vg, Vk_full, Vb_plus);
            // Average of the two halves (slight residual from the finite
            // iteration count gets split equally between them).
            Ip = 0.5 * (final_.Ip_lower + final_.Ip_upper);
        }
        else if (config_.enablePentodeModel
                 && config_.topology == TubeTopology::CommonCathode)
        {
            // Full pentode solver path:
            //   1) solve screen node (g2) from Rs/Cs and last Ig2
            //   2) evaluate Ip/Ig2/Ig1 with explicit g3 coupling
            //   3) feed Ig2 back into the screen RC state
            const double Va = std::max(0.0, Vb_plus - Vk_full);
            const double g3Drive =
                config_.suppressorDrivePolarity
                * std::max(0.0, config_.suppressorDriveMix)
                * std::abs(vgSignal);
            const double Vg3 = config_.suppressorTieToCathode
                ? 0.0 + g3Drive
                : (config_.suppressorBiasVolts - Vk_full + g3Drive);

            double Vg2 = Va;
            if (!config_.pentodeTriodeStrap)
            {
                const double Rs = std::max(config_.screenResistorOhms, 1.0);
                const double Cs = std::max(config_.screenBypassFarads, 1.0e-12);
                const double dt = 1.0 / sampleRate_;
                const double alpha = std::exp(-dt / (Rs * Cs));
                const double Vtarget =
                    config_.screenSupplyVolts - lastScreenCurrent_ * Rs;
                screenNodeV_ = alpha * screenNodeV_ + (1.0 - alpha) * Vtarget;
                if (!std::isfinite(screenNodeV_))
                    screenNodeV_ = config_.screenSupplyVolts;
                Vg2 = std::max(0.0, screenNodeV_);
            }

            const auto pent = pentode_.evaluate(Va, Vg, Vg2, Vg3);
            Ip = pent.Ip;
            lastScreenCurrent_ = pent.Ig2;
            if (!std::isfinite(lastScreenCurrent_) || lastScreenCurrent_ < 0.0)
                lastScreenCurrent_ = 0.0;

            // One extra correction step tightens g2 equilibrium under hard drive.
            if (!config_.pentodeTriodeStrap)
            {
                const double Rs = std::max(config_.screenResistorOhms, 1.0);
                const double Cs = std::max(config_.screenBypassFarads, 1.0e-12);
                const double dt = 1.0 / sampleRate_;
                const double alpha = std::exp(-dt / (Rs * Cs));
                const double Vtarget =
                    config_.screenSupplyVolts - lastScreenCurrent_ * Rs;
                screenNodeV_ = alpha * screenNodeV_ + (1.0 - alpha) * Vtarget;
                screenNodeV_ = std::max(0.0, screenNodeV_);
            }
            else
            {
                screenNodeV_ = Va;
            }
        }
        else
        {
            Ip = triode_.plateCurrent(Vb_plus, Vg);
        }
        Ip *= warmupCurrent_;

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
        //    Common cathode      → Vp = Vb+ − Ip·Rp (inverting amp, default)
        //    Cathode follower    → Vk  (non-inverting buffer at cathode)
        //    SRPP                → V_mid (junction between the two tubes —
        //                                  characteristic SRPP behaviour:
        //                                  symmetric clip on both swings,
        //                                  low output impedance)
        //    Cascode             → Vb_plus − Ip·Rp_upper (upper plate;
        //                                  same Ip flows through both
        //                                  tubes by KCL at the junction)
        double rawOut;
        double normalizer;
        if (config_.topology == TubeTopology::CathodeFollower)
        {
            rawOut     = Vk_new;
            normalizer = std::max(std::abs(Vk_rest_) * 2.0, 1.0);
        }
        else if (config_.topology == TubeTopology::SRPP)
        {
            rawOut     = Vmid_last_;
            // SRPP swings around the mid-rail Vmid_rest_; normalise by
            // its full-scale excursion (≈ Vp_nominal/2).
            normalizer = std::max(config_.Vp_nominal * 0.5, 1.0);
        }
        else if (config_.topology == TubeTopology::Cascode)
        {
            rawOut     = Vb_plus - Ip * config_.Rp_upper;
            normalizer = config_.Vp_nominal;
        }
        else if (config_.topology == TubeTopology::LongTailedPair)
        {
            const double mm = std::clamp(config_.ltpTubeMismatch, -0.45, 0.45);
            const double rpRatio = std::max(0.2, config_.ltpPlateRRatio);
            auto tubeP = config_.tube;
            auto tubeN = config_.tube;
            tubeP.G  *= (1.0 + mm);
            tubeP.mu *= (1.0 - 0.5 * mm);
            tubeN.G  *= (1.0 - mm);
            tubeN.mu *= (1.0 + 0.5 * mm);
            ltpTriodePos_.setParams(tubeP);
            ltpTriodeNeg_.setParams(tubeN);

            const double vDrive = Vg - config_.Vg_bias;
            const double VgPos = config_.Vg_bias + vDrive;
            const double VgNeg = config_.Vg_bias - vDrive;

            double Vk = std::max(0.0, ltpVkLast_);
            const double tailR = std::max(config_.ltpTailR, 1.0);
            for (int it = 0; it < std::max(1, config_.ltpSolverIters); ++it)
            {
                const auto p = ltpTriodePos_.evalWithDerivatives(Vb_plus, VgPos - Vk);
                const auto n = ltpTriodeNeg_.evalWithDerivatives(Vb_plus, VgNeg - Vk);
                const double f = p.Ip + n.Ip - Vk / tailR;
                const double fp = -(p.gm + n.gm) - 1.0 / tailR;
                if (std::abs(fp) < 1.0e-15) break;
                Vk -= f / fp;
                if (! std::isfinite(Vk)) { Vk = ltpVkLast_; break; }
                Vk = std::max(0.0, Vk);
            }
            ltpVkLast_ = Vk;

            const auto p = ltpTriodePos_.evalWithDerivatives(Vb_plus, VgPos - Vk);
            const auto n = ltpTriodeNeg_.evalWithDerivatives(Vb_plus, VgNeg - Vk);
            const double VpPos = Vb_plus - p.Ip * config_.Rp;
            const double VpNeg = Vb_plus - n.Ip * config_.Rp * rpRatio;

            const double diff = 0.5 * (VpNeg - VpPos);
            const double cm = 0.5 * (VpPos + VpNeg) - Vp_rest_;
            rawOut = diff + std::clamp(config_.ltpCommonModeLeak, 0.0, 1.0) * cm;
            normalizer = config_.Vp_nominal;
            Ip = 0.5 * (p.Ip + n.Ip);
            lastIp_ = Ip;
        }
        else  // CommonCathode
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
    double lastScreenCurrent() const noexcept { return lastScreenCurrent_; }
    double lastScreenVoltage() const noexcept { return screenNodeV_; }
    double restingPlateCurrent() const noexcept { return Ip_rest_; }
    double blockingMemoryVolts() const noexcept
    {
        return std::max(0.0, gridChargeFastV_ + gridChargeSlowV_);
    }
    double blockingMemoryDrive() const noexcept
    {
        return std::clamp(blockingMemoryVolts() / 0.75, 0.0, 1.0);
    }
    double cathodeStressDrive() const noexcept
    {
        const double deltaVk = config_.enableCathodeBounce
            ? std::abs(bounce_.currentBias() - Vk_rest_)
            : 0.0;
        return std::clamp(deltaVk / 3.0, 0.0, 1.0);
    }
    double thermalMemoryDrive() const noexcept
    {
        return std::clamp(std::abs(thermalBiasShift()) / 1.2, 0.0, 1.0);
    }
    double stageCurrentDrive() const noexcept
    {
        const double denom = std::max(std::abs(Ip_rest_) * 4.0, 1.0e-9);
        return std::clamp(std::abs(lastIp_ - Ip_rest_) / denom, 0.0, 1.0);
    }

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
    // ─────────────────────────────────────────────────────────────────────
    // Compound topology helpers (SRPP / Cascode).  Both share the same
    // "two triodes stacked, same Ip flows through both" structure — only
    // the upper tube's grid bias and plate connection differ.
    // ─────────────────────────────────────────────────────────────────────
    struct StackProbe { double Ip_lower; double Ip_upper; double fprime; };

    StackProbe solveStackPair(double Vmid,
                              double Vg_lower,
                              double Vk_lower,
                              double Vb) const noexcept
    {
        StackProbe p {};

        // Lower tube common to both topologies: signal-driven grid,
        // cathode at Vk_lower (cathode-bypass dynamic bias), plate at
        // the junction V_mid.  We pull Ip and its slope ∂Ip/∂Vp in a
        // single Dempwolf evaluation rather than burning a probe.
        const auto lower = triode_.evalWithDerivatives(Vmid,
                                                       Vg_lower - Vk_lower);
        p.Ip_lower = lower.Ip;

        // ∂Ip_lower / ∂V_mid:  Vp_lower = V_mid (Vgk fixed for the solver)
        //                       → ∂Ip_lower/∂V_mid = rpInv_lower
        const double dIpL_dVmid = lower.rpInv;

        if (config_.topology == TubeTopology::SRPP)
        {
            // SRPP active-load variant: upper tube's grid floats with
            // V_mid via a small DC offset → upper Vgk stays constant as
            // V_mid swings.  Modelled by fixing Vgk to the lower tube's
            // bias (matched-pair operating point).  Cathode at V_mid,
            // plate at Vb → Vp_upper = Vb − V_mid.
            const auto upper = triode_upper_.evalWithDerivatives(
                Vb - Vmid, config_.Vg_bias);
            p.Ip_upper = upper.Ip;

            // ∂Ip_upper / ∂V_mid: only Vp_upper depends on V_mid (Vgk
            // is constant by construction).  ∂Vp_upper/∂V_mid = −1
            // → contribution = −rpInv_upper.
            const double dIpU_dVmid = -upper.rpInv;
            p.fprime = dIpL_dVmid - dIpU_dVmid;  // = rpInv_l + rpInv_u
        }
        else  // Cascode
        {
            // Cascode: upper plate sits Rp_upper above V_mid, fed from
            // Vb through that resistor.  Same Ip flows through Rp_upper
            // and the upper tube by KCL at the junction.
            const double Vp_upper = Vb - lower.Ip * config_.Rp_upper;
            const auto upper = triode_upper_.evalWithDerivatives(
                Vp_upper - Vmid, config_.Vg_upper_bias - Vmid);
            p.Ip_upper = upper.Ip;

            // ∂Ip_upper / ∂V_mid via two paths:
            //   (a) Vp_upper = Vb − Ip_l·Rp_upper depends on V_mid via
            //       Ip_l → ∂Vp_upper/∂V_mid = −Rp_upper · dIpL_dVmid
            //       contribution: rpInv_upper · (−Rp_upper · dIpL_dVmid)
            //   (b) Vgk_upper = Vg_upper_bias − V_mid:
            //       ∂Vgk_upper/∂V_mid = −1 → contribution: −gm_upper
            //   ∂Vp_upper depends additionally on V_mid through the
            //   cathode-at-V_mid term, but the plate-cathode separation
            //   in this topology is Vp_upper − V_mid → that's already
            //   captured by passing (Vp_upper − V_mid) above; the slope
            //   is rpInv_upper at the (Vp_upper, Vgk_upper) operating
            //   point and is multiplied by the chain-ruled
            //   ∂(Vp_upper − V_mid)/∂V_mid = −1 + ∂Vp_upper/∂V_mid.
            //
            // Putting it all together:
            //   df/dV_mid = dIpL_dVmid
            //             − [ rpInv_upper · (∂Vp_upper/∂V_mid − 1) − gm_upper ]
            // where ∂Vp_upper/∂V_mid = −Rp_upper · dIpL_dVmid.
            const double dVpU_dVmid =
                -config_.Rp_upper * dIpL_dVmid;
            const double dIpU_dVmid =
                upper.rpInv * (dVpU_dVmid - 1.0) - upper.gm;
            p.fprime = dIpL_dVmid - dIpU_dVmid;
        }

        return p;
    }

    double solveScreenNodeQuiescent(double Va,
                                    double Vg1,
                                    double Vg3) const noexcept
    {
        if (config_.pentodeTriodeStrap)
            return std::max(0.0, Va);

        const double Rs = std::max(config_.screenResistorOhms, 1.0);
        double Vg2 = std::max(0.0, config_.screenSupplyVolts);
        for (int it = 0; it < 10; ++it)
        {
            const auto p = pentode_.evaluate(Va, Vg1, Vg2, Vg3);
            const double next = std::max(0.0, config_.screenSupplyVolts - p.Ig2 * Rs);
            Vg2 = 0.6 * Vg2 + 0.4 * next;
        }
        return Vg2;
    }

    void solvePentodeRestPoint(double Vb) noexcept
    {
        double Vk = std::max(0.0, Vk_rest_);
        double Ip = 0.0;
        double Vg2 = config_.screenSupplyVolts;
        for (int it = 0; it < 24; ++it)
        {
            const double Va = std::max(0.0, Vb - Vk);
            const double Vg1 = config_.Vg_bias - Vk;
            const double Vg3 = config_.suppressorTieToCathode
                ? 0.0
                : (config_.suppressorBiasVolts - Vk);
            Vg2 = solveScreenNodeQuiescent(Va, Vg1, Vg3);
            const auto p = pentode_.evaluate(Va, Vg1, Vg2, Vg3);
            Ip = p.Ip;
            const double VkNew = std::max(0.0, Ip * config_.Rk);
            Vk = 0.65 * Vk + 0.35 * VkNew;
            if (!std::isfinite(Vk))
            {
                Vk = 0.0;
                Ip = 0.0;
                Vg2 = config_.screenSupplyVolts;
                break;
            }
        }
        Ip_rest_ = std::max(0.0, Ip);
        Vk_rest_ = std::max(0.0, Vk);
        Vp_rest_ = std::max(0.0, Vb - Ip_rest_ * config_.Rp);
        screenNodeV_ = std::max(0.0, Vg2);
        lastScreenCurrent_ = pentode_.evaluate(
            std::max(0.0, Vb - Vk_rest_),
            config_.Vg_bias - Vk_rest_,
            screenNodeV_,
            config_.suppressorTieToCathode ? 0.0 : (config_.suppressorBiasVolts - Vk_rest_)
        ).Ig2;
        if (!std::isfinite(lastScreenCurrent_) || lastScreenCurrent_ < 0.0)
            lastScreenCurrent_ = 0.0;
    }

    double computeRestingGm() const noexcept
    {
        if (config_.enablePentodeModel
            && config_.topology == TubeTopology::CommonCathode)
        {
            const double Va = std::max(0.0, config_.Vp_nominal - Vk_rest_);
            const double Vg3 = config_.suppressorTieToCathode
                ? 0.0
                : (config_.suppressorBiasVolts - Vk_rest_);
            const double Vg2 = config_.pentodeTriodeStrap
                ? Va
                : std::max(0.0, screenNodeV_);
            const double Vg1Rest = config_.Vg_bias - Vk_rest_;
            constexpr double h = 1.0e-3;
            const double ip1 = pentode_.evaluate(Va, Vg1Rest + h, Vg2, Vg3).Ip;
            const double ip0 = pentode_.evaluate(Va, Vg1Rest - h, Vg2, Vg3).Ip;
            return (ip1 - ip0) / (2.0 * h);
        }
        return triode_.transconductance(config_.Vp_nominal, config_.Vg_bias);
    }

    // Output-node DC at the resting operating point.  Used by setup and
    // reset to seed the slow DC-leak tracker so the stage doesn't emit a
    // startup thump.
    double restingOutputDC() const noexcept
    {
        switch (config_.topology)
        {
            case TubeTopology::CathodeFollower: return Vk_rest_;
            case TubeTopology::SRPP:            return Vmid_rest_;
            case TubeTopology::Cascode:
                return config_.Vp_nominal - Ip_rest_ * config_.Rp_upper;
            case TubeTopology::LongTailedPair:
                return ltpOutRest_;
            case TubeTopology::CommonCathode:
            default:                            return Vp_rest_;
        }
    }

    // Signal-dependent Miller low-pass (docs/04 §3.2–§3.3)
    //   fc = 1 / (2π · Zsource · Cmiller)
    //   Cmiller ≈ Cgp · (1 + |Av|) · programFactor
    //   |Av| static component uses quiescent gm.
    //   programFactor depends on the previous sample's |Ip| so heavy
    //   conduction momentarily grows C_m and rolls HF off a touch more.
    double applyMillerFilter(double x) noexcept
    {
        const double gmStatic = gmRest_ * warmupCurrent_;
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

    KorenTriode   triode_       { params::kRSD_1 };
    KorenTriode   triode_upper_ { params::kRSD_1 };  // SRPP / Cascode upper tube
    KorenTriode   ltpTriodePos_ { params::kRSD_1 };
    KorenTriode   ltpTriodeNeg_ { params::kRSD_1 };
    KorenPentode  pentode_      { pentode_params::k6AS6 };
    CathodeBounce bounce_ {};

    // Resting-point cache (computed once in setup())
    double Ip_rest_ { 0.0 };
    double Vk_rest_ { 0.0 };
    double Vp_rest_ { 0.0 };

    // Compound topology (SRPP / Cascode) state
    double Vmid_rest_ { 100.0 };  ///< Junction voltage between stacked tubes
    double Vmid_last_ { 100.0 };  ///< Newton-Raphson warm start
    double ltpVkLast_ { 0.0 };
    double ltpOutRest_ { 0.0 };

    // Time-varying states
    double warmupAlpha_    { 0.0 };
    double warmupCurrent_  { 1.0 };
    double warmupTarget_   { 1.0 };
    double millerState_    { 0.0 };
    double outputDC_       { 0.0 };
    double lastIp_         { 0.0 };  ///< Last computed plate current (for PSU)
    double lastScreenCurrent_ { 0.0 };
    double screenNodeV_    { 0.0 };
    double gmRest_         { 0.0 };
    double gridChargeFastV_ { 0.0 }; ///< Fast blocking branch [V]
    double gridChargeSlowV_ { 0.0 }; ///< Slow recovery branch [V]
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
    TubeStageConfig s {
        .tube = params::kEF86_TriodeStrapped,
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
    s.enablePentodeModel = true;
    s.pentode = pentode_params::kEF86;
    s.pentodeTriodeStrap = false;
    s.screenSupplyVolts = 145.0;
    s.screenResistorOhms = 100.0e3;
    s.screenBypassFarads = 0.47e-6;
    s.suppressorTieToCathode = true;
    s.suppressorDriveMix = 0.0;
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
// Marshall-style power tube primitive (triode-strapped EL34 approximation).
// Used both for standalone stage-editor work and inside the push-pull output
// section in TubeAmpChain.
// ─────────────────────────────────────────────────────────────────────────────

/// Console Output Stage 1 — 12AX7 gain stage at warm class-A1 bias.
/// Re-tuned from the original cold-biased Marshall input so the default
/// drive sits in the linear region for mix/master use (drive past ~2.0
/// still walks into the gritty zone for users who want guitar-amp colour).
inline TubeStageConfig marshallStage1()
{
    TubeStageConfig s {
        .tube = params::kRSD_1,
        .topology = TubeTopology::CommonCathode,
        .Vg_bias = -1.6, .Vp_nominal = 300.0,    // warmer bias (was −1.2)
        .Rp = 100.0e3, .Rk = 1500.0, .Ck = 22.0e-6,  // full bypass (smooth)
        .enableCathodeBounce = true,             // cathode-bypass dynamics on
        .inputVoltageSwing = 0.5,
        .outputGainLinear = 1.5,
        .enableWarmup = true, .warmupTauSeconds = 15.0,
        .enableMillerFilter = true,
        .Cgp_miller = 2.4e-12,
        .sourceImpedance = 10.0e3
    };
    // No microphonics in console use — chassis isn't on a vibrating
    // guitar amp cab, so the body resonance is irrelevant.
    s.enableMicrophonics = false;
    return s;
}

/// Console Output Stage 2 — 12AX7 driver into the push-pull section.
/// Operating point chosen so the pair never overshoots class-A1 unless
/// the user actively pushes Drive past about 2.0.
inline TubeStageConfig marshallStage2()
{
    TubeStageConfig s {
        .tube = params::kRSD_2,  // slightly different character (matched pair imperfection)
        .topology = TubeTopology::CommonCathode,
        .Vg_bias = -1.8, .Vp_nominal = 300.0,
        .Rp = 100.0e3, .Rk = 1500.0, .Ck = 22.0e-6,
        .enableCathodeBounce = true,
        .inputVoltageSwing = 0.5,        // moderate (was 3.0)
        .outputGainLinear = 1.2,
        .enableWarmup = true, .warmupTauSeconds = 15.0,
        .enableMillerFilter = true,
        .Cgp_miller = 2.4e-12,
        .sourceImpedance = 100.0e3
    };
    s.enableMicrophonics = false;
    return s;
}

// ─────────────────────────────────────────────────────────────────────────────
// Culture Vulture–style "distortion core"
// (EF86-like pentode input → 6AS6-like distortion tube → 12AU7 buffer)
// ─────────────────────────────────────────────────────────────────────────────

/// CV distortion core — 6AS6 variable-mu stage.  T/P1/P2 follow the
/// Culture Vulture control idea from docs/24: triode-style warm H2, pentode
/// low H3 emphasis, and pentode-high extreme bias collapse.
inline TubeStageConfig cvDistortionCore(
    CultureVultureVoicing voicing = CultureVultureVoicing::PentodeLow)
{
    TubeStageConfig s {
        .tube = params::k6AS6_VariableMu,
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

    s.enablePentodeModel = true;
    s.pentode = pentode_params::k6AS6;
    s.pentodeTriodeStrap = false;
    s.screenSupplyVolts = 150.0;
    s.screenResistorOhms = 47.0e3;
    s.screenBypassFarads = 1.0e-6;
    s.suppressorTieToCathode = false;
    s.suppressorBiasVolts = -0.8;
    s.suppressorDriveMix = 0.85;
    s.suppressorDrivePolarity = -1.0;

    switch (voicing)
    {
        case CultureVultureVoicing::Triode:
            s.Vg_bias = -0.75;
            s.Rp = 68.0e3;
            s.Rk = 680.0;
            s.Ck = 1.0e-6;
            s.inputVoltageSwing = 0.75;
            s.outputGainLinear = 4.5;
            s.Cgp_miller = 0.9e-12;
            s.pentodeTriodeStrap = true;
            s.suppressorTieToCathode = true;
            s.suppressorDriveMix = 0.0;
            break;
        case CultureVultureVoicing::PentodeHigh:
            s.Vg_bias = -1.85;
            s.Rp = 33.0e3;
            s.Rk = 1200.0;
            s.Ck = 0.22e-6;
            s.inputVoltageSwing = 1.8;
            s.outputGainLinear = 8.5;
            s.Cgp_miller = 0.25e-12;
            s.screenSupplyVolts = 175.0;
            s.screenResistorOhms = 27.0e3;
            s.screenBypassFarads = 2.2e-6;
            s.suppressorBiasVolts = -2.2;
            s.suppressorDriveMix = 1.7;
            break;
        case CultureVultureVoicing::PentodeLow:
        default:
            break;
    }

    return s;
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

// ─────────────────────────────────────────────────────────────────────────────
// HiFi 300B mode (single-ended, audiophile voicing)
//
// Three-stage chain echoing classic SE 300B amp topology:
//   Stage 1: 6SN7 common-cathode (gentle, low-distortion gain block)
//   Stage 2: 6SN7 cathode follower (high-Z buffer, drives 300B grid)
//   Stage 3: 300B common-cathode SE power triode (the legendary tube)
// followed by a Lundahl LL-grade output transformer.
//
// Character contrasts:
//   • V72  — vintage broadcast preamp, transformer-rich
//   • Marshall — guitar amp, dynamic class-AB push-pull
//   • Culture Vulture — extreme distortion box
//   • RNDI — DI / line color
//   • HiFi 300B — audiophile mastering: very low THD, even-harmonic-
//     dominant, "liquid" SE-class-A1 character
// ─────────────────────────────────────────────────────────────────────────────

/// HiFi Mode Stage 1 — 6SN7 common-cathode at audiophile bias.
/// Low-distortion gain block with the 6SN7's smooth even-harmonic warm.
/// outputGainLinear is generous to compensate for the lower μ (≈20 vs
/// 12AX7's ≈100) so the chain delivers comparable level to the V72
/// preset.
inline TubeStageConfig hifi6SN7Input()
{
    TubeStageConfig s {
        .tube = params::k6SN7,
        .topology = TubeTopology::CommonCathode,
        .Vg_bias = -8.0, .Vp_nominal = 250.0,
        .Rp = 47.0e3, .Rk = 1500.0, .Ck = 47.0e-6,
        .inputVoltageSwing = 1.0,    // moderate signal swing
        .outputGainLinear = 6.0,     // 6SN7 gain ≈ 17 → 6× makeup keeps
                                     // chain level comparable to V72
        .enableWarmup = true, .warmupTauSeconds = 30.0,
        .enableMillerFilter = true,
        .Cgp_miller = 4.0e-12,       // 6SN7 has higher Cgp than 12AX7
        .sourceImpedance = 1.0e3     // line input
    };
    // HiFi voicing — clean signal path, no vintage hum / microphonics.
    s.enableHeaterHum    = false;
    s.enableShotNoise    = true;     // still some thermal "breath"
    s.shotNoiseScale     = 1.2e-6;   // very low
    s.enableMicrophonics = false;
    s.enableSlewLimit    = false;    // 6SN7 is fast enough that the
                                     // slew-limit isn't audible in linear
    return s;
}

/// HiFi Mode Stage 2 — 6SN7 cathode follower buffer.
/// CF gives a low output impedance to drive the 300B's input cap
/// without the upstream stage seeing it.  outputGainLinear is heavy
/// because the CF normalizer (Vk·2 ≈ 18 V) is much smaller than the
/// CC normalizer (Vp_nominal ≈ 250 V) — without makeup the CF stage
/// would *attenuate* in the chain's normalized signal space.
inline TubeStageConfig hifi6SN7Buffer()
{
    TubeStageConfig s {
        .tube = params::k6SN7,
        .topology = TubeTopology::CathodeFollower,
        .Vg_bias = -8.0, .Vp_nominal = 250.0,
        .Rp = 0.0, .Rk = 10.0e3, .Ck = 0.0,    // unbypassed: linear
        .inputVoltageSwing = 1.0,
        .outputGainLinear = 4.0,     // CF normalizer is small (Vk·2);
                                     // 4× makeup keeps the chain at
                                     // unity-ish overall gain
        .enableWarmup = true, .warmupTauSeconds = 30.0,
        .enableMillerFilter = false,           // CF has very low Miller
        .sourceImpedance = 47.0e3              // output of stage 1
    };
    s.enableHeaterHum    = false;
    s.enableShotNoise    = true;
    s.shotNoiseScale     = 1.2e-6;
    s.enableMicrophonics = false;
    return s;
}

/// HiFi Mode Stage 3 — 300B single-ended class-A1 power triode.
/// The defining sonic element of the HiFi mode.  The 300B's μ ≈ 3.85
/// means it needs a big grid swing — but the upstream chain delivers
/// a few volts max (in normalized units the chain has been gain-budgeted
/// to ~unity), so we calibrate inputVoltageSwing accordingly: a ±1.0
/// audio sample maps to roughly ±10 V at the 300B grid, which is enough
/// to drive a meaningful plate swing while staying clear of grid
/// conduction at all but the most extreme inputs.
///
/// Rp tuned to land at a sensible operating point: with Ip_rest ≈ 120 mA
/// (the Dempwolf-form 2× idle convention) and Rp = 1.5 kΩ, the resting
/// Vp_drop = 180 V leaves ~170 V headroom on the 350 V supply.
inline TubeStageConfig hifi300BPower()
{
    TubeStageConfig s {
        .tube = params::k300B,
        .topology = TubeTopology::CommonCathode,
        .Vg_bias = -76.0, .Vp_nominal = 350.0,
        .Rp = 1.5e3, .Rk = 1.2e3, .Ck = 470.0e-6,
        .inputVoltageSwing = 12.0,   // moderate grid drive
        .outputGainLinear = 3.0,     // makeup so 300B's contribution shows
        .enableCathodeBounce = true,
        .enableWarmup = true, .warmupTauSeconds = 60.0,  // DHT warms slow
        .enableMillerFilter = true,
        .Cgp_miller = 12.0e-12,                 // 300B has substantial Cgp
        .sourceImpedance = 1.0e3                 // CF buffer output
    };
    // HiFi voicing on the power tube too:
    s.enableHeaterHum      = false;             // DHT 300B is well-filtered
    s.enableShotNoise      = true;
    s.shotNoiseScale       = 2.0e-6;
    s.enableMicrophonics   = false;             // chassis-mounted, isolated
    s.enableThermalDrift   = true;              // long-passage settle
    s.thermalTauSeconds    = 30.0;              // 300B has big thermal mass
    s.enableGridConduction = true;              // when overdriven hard,
                                                // the 300B's grid does
                                                // start to conduct
    return s;
}

} // namespace presets

} // namespace valvra::dsp
