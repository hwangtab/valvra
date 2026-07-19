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

    // Transformer-coupled plate (SE output stages): the OPT primary is an
    // inductor, so the DC point sits only DCR·Ip below the rail while
    // signal excursions ride the Rp (reflected) AC load line through it —
    // the plate can even swing ABOVE the rail on the cutoff half.
    bool   plateLoadIsTransformer { false };
    double plateLoadDcr           { 60.0 };  ///< OPT primary DCR [Ω]

    // ─── Reactive reflected load (docs/34 §2.5, docs/04 §6.2) ───────────
    // A real SE output stage's reflected load is a LOUDSPEAKER, not a
    // resistor: the motional resonance (tens of Hz) multiplies |Z| several
    // fold and the voice-coil inductance lifts it again toward HF, so the
    // load line opens into a frequency-dependent ellipse — clipping
    // asymmetry becomes a function of frequency, the signature of an SE
    // amp on a speaker.  Modelled as the reflected network
    //   Z(s) = Rp·[1 + s/ω_vc] + Rm ∥ sLm ∥ 1/(sCm)
    // (series voice-coil R+L plus the parallel motional RLC), discretised
    // trapezoidally into a one-port companion: the plate Newton sees a
    // per-sample Thevenin (R_tot, V_hist) — same solve, exact reactance.
    // Only used on the transformer-loaded CC path; mid-band |Z| ≈ Rp keeps
    // the level calibration anchored.
    bool   plateLoadReactive { false };
    double loadResonanceHz   { 45.0 };   ///< motional resonance f₀
    double loadResonanceQ    { 1.2 };    ///< motional Q
    double loadPeakRatio     { 5.0 };    ///< Z(f₀) ≈ Rp·(1 + ratio−1)
    double loadVcCornerHz    { 2000.0 }; ///< voice-coil L corner (ωL = Rp)

    // ─── Next-stage grid-leak AC loading (docs/34 §3.8) ─────────────────
    // The following stage's grid-leak resistor hangs on this plate through
    // the coupling cap: DC sees only Rp, but SIGNAL sees Rp ∥ Rg — with a
    // 1 MΩ leak on a 100 kΩ plate that is a real ~9% gain and curvature
    // change the isolated-stage model missed.  The chain sets this to its
    // interstage Rg for every stage that feeds another stage (0 = none;
    // applies to the triode CC path — the topology every interstage
    // hand-off uses).  Level is preserved by outputMakeup_ as usual.
    double nextStageLoadR { 0.0 };
    double Rk          {  1500.0 };  ///< Cathode resistor [Ω]
    double Ck          {  25.0e-6 }; ///< Cathode bypass capacitor [F]
    bool   enableCathodeBounce { true }; ///< Slow cathode-bypass memory on/off

    // ─── Voltage-native interface (docs/34 §4.1) ────────────────────────
    // When set by the chain, this stage's input is already GRID VOLTS
    // (voltageNativeInput — the ×inputVoltageSwing map is skipped) and/or
    // its output is the plate AC in VOLTS around the STATIC rest point
    // (voltageNativeOutput — makeup/normalizer/outputGain become the
    // chain-side inter-stage pad, and the 0.5 Hz DC tracker no longer
    // eats the output: slow operating-point motion (sag, thermal) rides
    // through the interstage coupling cap to the next grid, exactly as
    // the real circuit pumps its bias).  Both default off = legacy
    // normalized hand-off, bit-compatible.
    bool voltageNativeInput  { false };
    bool voltageNativeOutput { false };

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

    // ─── Plate-node stray capacitance (docs/04 §7, physical basis) ──────
    // The plate node sees wiring stray + Cak + the NEXT stage's reflected
    // Miller capacitance.  Together with the operating-point-dependent
    // output impedance (Rp ∥ rp) it forms the REAL asymmetric slew limit:
    // while the tube conducts, rp is low and the node moves fast; as the
    // tube swings toward cutoff rp → ∞ and the node can only charge
    // through Rp — slow RC pull-up.  Solved implicitly inside the plate
    // load-line Newton so it costs nothing extra.
    double Cplate { 60.0e-12 };

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
    // Default OFF since the implicit plate-node solve (Cplate above) now
    // produces the physical slew asymmetry; this block remains as an
    // optional stylised override.  Note the legacy default rates were far
    // below real tube-stage slew (V/µs scale) and acted as a heavy HF
    // limiter — anyone re-enabling should prefer the physical values.
    bool   enableSlewLimit         { false };
    // Common-cathode physics: upward plate excursions (output rising)
    // happen through the plate-load resistor charging the stray cap —
    // slow RC pull-up.  Downward excursions use the low ON-state tube
    // impedance — fast pull-down.  Units: normalized-output per second.
    double slewRatePositive        { 1500.0 };  ///< Rising (slow, RC)
    double slewRateNegative        { 4000.0 };  ///< Falling (fast, tube
                                                ///  conducting)

    // ─── Miller feedthrough zero (docs/34 §3.5) ─────────────────────────
    // The Miller LPF models the input POLE of the Cgp feedback; the same
    // capacitor also feeds the plate signal FORWARD into the grid node.
    // In the linear regime that feedthrough is already embodied in the
    // pole, so only the NONLINEAR residual is injected here: when the
    // plate stops following the drive (clipping), the Miller feedback
    // collapses and the residual plate edge couples through Cgp·Zsrc —
    // the glassy "spit" on hard-clipped edges that a pure input pole
    // cannot produce.  Zero effect on small signals by construction.
    bool   enableMillerFeedthrough { true };

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

    // 1/f flicker noise weight relative to the white shot-noise floor.
    // Cathode-interface resistance fluctuations give real tubes a pink
    // LF noise slope (corner ~ 1 kHz) — the floor "breathes" instead of
    // hissing uniformly.  0 = legacy pure-white.  Kept moderate: the
    // pink component's slow RMS wander is the audible point, but past
    // ~0.4 the quiet modes' noise floor pumps more than real DI/HiFi
    // hardware measures (feel-verify micro-motion gate ≤ 0.9).
    double flickerNoiseRatio { 0.35 };

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
        else if (cfg.topology == TubeTopology::CathodeFollower)
        {
            // Cathode follower rest point: the plate sits at the rail, the
            // cathode rides at Ip·Rk.  Convention: cfg.Vg_bias is the
            // RESTING Vgk (matches the legacy presets), so only the
            // plate-cathode voltage needs the self-consistent solve:
            //   Ip = Ip(Vb − Vk, Vg_bias),  Vk = Ip·Rk
            double Vk = 0.0;
            for (int it = 0; it < 32; ++it)
            {
                const double Ip = std::max(
                    0.0, triode_.plateCurrent(
                             std::max(1.0, cfg.Vp_nominal - Vk),
                             cfg.Vg_bias));
                const double VkNew = Ip * cfg.Rk;
                Vk = 0.5 * Vk + 0.5 * VkNew;
                if (! std::isfinite(Vk)) { Vk = 0.0; break; }
            }
            Vk_rest_ = Vk;
            Ip_rest_ = (cfg.Rk > 1.0e-9) ? Vk / cfg.Rk
                       : std::max(0.0, triode_.plateCurrent(cfg.Vp_nominal,
                                                            cfg.Vg_bias));
            Vp_rest_ = cfg.Vp_nominal;
            cfVkLast_ = Vk_rest_;
            screenNodeV_ = cfg.screenSupplyVolts;
            lastScreenCurrent_ = 0.0;
        }
        else
        {
            // Common-cathode rest point WITH the plate load line.  The
            // legacy code evaluated Ip at Vp = B+ (no plate feedback),
            // which both misplaced the operating point and removed the
            // rp ∥ Rp gain compression — the deepest "static waveshaper"
            // error this engine had.  Solve  Vp = Vb − R_dc·Ip(Vp, Vg_bias)
            // by Newton (the residual is smooth and monotone in Vp).
            // For a transformer-coupled plate the DC drop is only the
            // winding DCR; the reflected Rp shapes AC excursions only.
            const double rDc = cfg.plateLoadIsTransformer
                ? std::max(cfg.plateLoadDcr, 0.0)
                : cfg.Rp;
            double Vp = cfg.plateLoadIsTransformer
                ? cfg.Vp_nominal * 0.95
                : cfg.Vp_nominal * 0.6;
            for (int it = 0; it < 32; ++it)
            {
                const auto d = triode_.evalWithDerivatives(Vp, cfg.Vg_bias);
                const double f  = Vp - cfg.Vp_nominal + rDc * d.Ip;
                const double fp = 1.0 + rDc * d.rpInv;
                Vp -= f / fp;
                if (! std::isfinite(Vp)) { Vp = cfg.Vp_nominal * 0.6; break; }
                Vp = std::clamp(Vp, 1.0, cfg.Vp_nominal);
            }
            Ip_rest_ = std::max(0.0,
                                triode_.plateCurrent(Vp, cfg.Vg_bias));
            Vk_rest_ = Ip_rest_ * cfg.Rk;
            Vp_rest_ = Vp;
            screenNodeV_ = cfg.screenSupplyVolts;
            lastScreenCurrent_ = 0.0;
        }
        VpLast_ = Vp_rest_;
        VaLast_ = std::max(0.0, Vp_rest_ - Vk_rest_);

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

            // Joint rest solve with per-side plate load lines: each side
            // satisfies Vp = Vb − Rp_side·Ip(Vp, Vg − Vk), and the tail
            // carries the summed current.
            auto restPlate = [&](const KorenTriode& tube, double Vgk,
                                 double Rp)
            {
                double Vp = cfg.Vp_nominal * 0.7;
                KorenTriode::IpDerivatives d {};
                for (int it = 0; it < 16; ++it)
                {
                    d = tube.evalWithDerivatives(Vp, Vgk);
                    const double f  = Vp - cfg.Vp_nominal + Rp * d.Ip;
                    const double fp = 1.0 + Rp * d.rpInv;
                    Vp -= f / fp;
                    if (! std::isfinite(Vp)) { Vp = cfg.Vp_nominal * 0.7; break; }
                    Vp = std::clamp(Vp, 1.0, cfg.Vp_nominal);
                }
                d = tube.evalWithDerivatives(Vp, Vgk);
                return std::pair<double, KorenTriode::IpDerivatives>{ Vp, d };
            };

            const double RpPos = cfg.Rp;
            const double RpNeg = cfg.Rp * rpRatio;
            double Vk = std::max(0.0, Vk_rest_);
            double VpPos = cfg.Vp_nominal * 0.7;
            double VpNeg = cfg.Vp_nominal * 0.7;
            KorenTriode::IpDerivatives p {}, n {};
            for (int it = 0; it < 24; ++it)
            {
                auto [vpP, dP] = restPlate(ltpTriodePos_, cfg.Vg_bias - Vk, RpPos);
                auto [vpN, dN] = restPlate(ltpTriodeNeg_, cfg.Vg_bias - Vk, RpNeg);
                VpPos = vpP; VpNeg = vpN; p = dP; n = dN;
                const double tailR = std::max(cfg.ltpTailR, 1.0);
                const double f = p.Ip + n.Ip - Vk / tailR;
                const double gmP = p.gm / (1.0 + RpPos * p.rpInv);
                const double gmN = n.gm / (1.0 + RpNeg * n.rpInv);
                const double fp = -(gmP + gmN) - 1.0 / tailR;
                if (std::abs(fp) < 1.0e-15) break;
                Vk -= f / fp;
                if (! std::isfinite(Vk)) { Vk = 0.0; break; }
                Vk = std::max(0.0, Vk);
            }

            Vk_rest_ = Vk;
            Ip_rest_ = 0.5 * (p.Ip + n.Ip);
            Vp_rest_ = 0.5 * (VpPos + VpNeg);
            ltpVkLast_ = Vk;
            ltpVpPosLast_ = VpPos;
            ltpVpNegLast_ = VpNeg;
            ltpVpPosRest_ = VpPos;
            ltpVpNegRest_ = VpNeg;
            outputMakeup_ = 1.0
                + cfg.Rp * std::max(0.0, 0.5 * (p.rpInv + n.rpInv));
            ltpOutRest_ = 0.5 * (VpNeg - VpPos)
                        + std::clamp(cfg.ltpCommonModeLeak, 0.0, 1.0)
                        * (0.5 * (VpPos + VpNeg) - Vp_rest_);
        }

        // Gain-calibration makeup for the plate load line: the physical
        // small-signal gain is the legacy gm·Rp divided by (1 + Rp·rpInv).
        // Multiplying the normalized output by that factor keeps every
        // preset's level calibration intact while the load line reshapes
        // curvature, asymmetry and dynamic headroom.
        if (cfg.topology == TubeTopology::CommonCathode)
        {
            if (cfg.enablePentodeModel)
            {
                const double Va  = std::max(2.0, Vp_rest_ - Vk_rest_);
                const double Vg1 = cfg.Vg_bias;   // net convention, as in process()
                const double Vg3 = cfg.suppressorTieToCathode
                    ? 0.0 : (cfg.suppressorBiasVolts - Vk_rest_);
                constexpr double h = 1.0;
                const double vg2Hi = cfg.pentodeTriodeStrap ? Va + h : screenNodeV_;
                const double vg2Lo = cfg.pentodeTriodeStrap ? Va - h : screenNodeV_;
                const double i1 = pentode_.evaluate(Va + h, Vg1, vg2Hi, Vg3).Ip;
                const double i0 = pentode_.evaluate(Va - h, Vg1, vg2Lo, Vg3).Ip;
                const double g  = std::max(0.0, (i1 - i0) / (2.0 * h));
                // Signal load = Rp ∥ Rg_next (docs/34 §3.8, extended to
                // the pentode path — docs/35 C2).  Level preserved:
                // makeup = av_legacy / av_physical
                //        = (gm·Rp) / (gm·rAc/(1 + rAc·g))
                //        = (Rp/rAc)·(1 + rAc·g).
                const double rAc = (! cfg.plateLoadIsTransformer
                                    && cfg.nextStageLoadR > 1.0)
                    ? cfg.Rp * cfg.nextStageLoadR
                      / (cfg.Rp + cfg.nextStageLoadR)
                    : cfg.Rp;
                outputMakeup_ = (cfg.Rp / std::max(rAc, 1.0))
                              * (1.0 + rAc * g);
            }
            else
            {
                // Full legacy-gain restoration: the physical rest point
                // sits at the loaded Vp (lower gm) and the load line adds
                // the rp ∥ Rp (∥ next-stage Rg) divider.  Ratio of the
                // legacy small-signal gain (gm at the unloaded B+ point
                // × Rp) to the new one keeps every downstream calibration
                // unchanged.
                const auto dNew = triode_.evalWithDerivatives(
                    std::max(1.0, Vp_rest_), cfg.Vg_bias);
                double avNew;
                if (cfg.plateLoadIsTransformer)
                {
                    const double rAcTotal =
                        cfg.Rp + std::max(cfg.plateLoadDcr, 0.0);
                    avNew = dNew.gm * cfg.Rp
                        / (1.0 + rAcTotal * std::max(0.0, dNew.rpInv));
                }
                else
                {
                    // Signal load = Rp ∥ Rg_next (docs/34 §3.8).
                    const double rAcEff = (cfg.nextStageLoadR > 1.0)
                        ? cfg.Rp * cfg.nextStageLoadR
                          / (cfg.Rp + cfg.nextStageLoadR)
                        : cfg.Rp;
                    avNew = dNew.gm * rAcEff
                        / (1.0 + rAcEff * std::max(0.0, dNew.rpInv));
                }
                const auto dLeg = triode_.evalWithDerivatives(
                    cfg.Vp_nominal, cfg.Vg_bias);
                const double avLegacy = dLeg.gm * cfg.Rp;
                outputMakeup_ = avLegacy / std::max(avNew, 1.0e-6);
            }
        }
        else if (cfg.topology != TubeTopology::LongTailedPair)
        {
            outputMakeup_ = 1.0;   // CF / SRPP / Cascode already node-solved
        }
        if (! std::isfinite(outputMakeup_)
            || outputMakeup_ < 1.0 || outputMakeup_ > 100.0)
            outputMakeup_ = 1.0;

        // SE-OPT magnetizing-coupling constants (docs/34 §2.2).  Only the
        // transformer-loaded triode CC path (the 300B SE output) sources
        // its OPT's magnetizing current.  seMagToAmps_ converts the OPT's
        // normalized drop to primary amperes (minus sign: primary
        // volt-seconds are Vb − Vp, the inverse of the stage's plate-
        // referenced output).  seMagZRest_ is the rest-point node impedance
        // rp ∥ (Rp+DCR) used to de-embed the LINEAR share of the response
        // so the OPT's own calibrated drop is not double-counted — only
        // the tube's nonlinear failure to supply the iron passes through.
        // Reactive-load companion constants (docs/34 §2.5, trapezoidal).
        // Series: Re(=Rp, mid-band reflected) + Le (voice-coil corner);
        // parallel motional RLC from (f₀, Q, peak ratio).  All reduce to a
        // per-sample one-port Thevenin (lrRtot_, history EMF).
        loadReactive_ = cfg.plateLoadIsTransformer && cfg.plateLoadReactive
            && cfg.topology == TubeTopology::CommonCathode
            && ! cfg.enablePentodeModel;
        lrEL_ = lrJC_ = lrJLm_ = 0.0;
        if (loadReactive_)
        {
            const double beta = 2.0 * sampleRate;
            const double w0 = 2.0 * M_PI * std::max(cfg.loadResonanceHz, 1.0);
            const double q  = std::max(cfg.loadResonanceQ, 0.1);
            const double Le = cfg.Rp
                / (2.0 * M_PI * std::max(cfg.loadVcCornerHz, 10.0));
            const double Rm = std::max(cfg.loadPeakRatio - 1.0, 0.1) * cfg.Rp;
            const double Lm = Rm / (w0 * q);
            const double Cm = q / (w0 * Rm);
            lrBLe_    = beta * Le;
            lrBCm_    = beta * Cm;
            lrInvBLm_ = 1.0 / (beta * Lm);
            lrGm_     = 1.0 / Rm + lrBCm_ + lrInvBLm_;
            lrRtot_   = cfg.Rp + lrBLe_ + 1.0 / lrGm_;
        }

        seMagToAmps_ = 0.0;
        seMagZRest_  = 0.0;
        seMagLastA_  = 0.0;
        extMagDropNorm_ = 0.0;
        if (cfg.plateLoadIsTransformer
            && cfg.topology == TubeTopology::CommonCathode
            && ! cfg.enablePentodeModel)
        {
            const auto dSe = triode_.evalWithDerivatives(
                std::max(1.0, Vp_rest_), cfg.Vg_bias);
            const double rTotSe = cfg.Rp + std::max(cfg.plateLoadDcr, 0.0);
            seMagZRest_ = 1.0 / (std::max(0.0, dSe.rpInv)
                                 + 1.0 / std::max(rTotSe, 1.0));
            seMagToAmps_ = -cfg.Vp_nominal
                / (std::max(cfg.outputGainLinear, 1.0e-6)
                   * outputMakeup_ * std::max(cfg.Rp, 1.0));
        }

        // Resting small-signal gain for the Miller model and the dynamic
        // output impedance handed to the next stage.  The effective AC
        // plate load includes the next stage's grid leak (docs/34 §3.8).
        {
            rAcEff_ = std::max(cfg.Rp, 1.0);
            if (! cfg.plateLoadIsTransformer && cfg.nextStageLoadR > 1.0
                && cfg.topology == TubeTopology::CommonCathode)
                rAcEff_ = cfg.Rp * cfg.nextStageLoadR
                        / (cfg.Rp + cfg.nextStageLoadR);
            const auto d = triode_.evalWithDerivatives(
                std::max(1.0, Vp_rest_), cfg.Vg_bias);
            lastGmLoaded_    = std::max(0.0, d.gm);
            lastRpInvLoaded_ = std::max(0.0, d.rpInv);
            avRest_ = lastGmLoaded_ * rAcEff_
                    / (1.0 + rAcEff_ * lastRpInvLoaded_);
            avSmooth_ = avRest_;
            avAlpha_  = 1.0 - std::exp(-1.0 / (0.0007 * sampleRate));
        }
        dynamicSourceZ_ = 0.0;

        // 0.5 Hz output-DC tracker, sample-rate independent.
        dcLeakAlpha_ = std::exp(-2.0 * M_PI * 0.5 / sampleRate);

        // Hoisted per-sample constants for the pentode path.
        {
            const double Rs = std::max(cfg.screenResistorOhms, 1.0);
            const double Cs = std::max(cfg.screenBypassFarads, 1.0e-12);
            screenAlpha_  = std::exp(-1.0 / (Rs * Cs * sampleRate));
            invVpNominal_ = 1.0 / std::max(cfg.Vp_nominal, 1.0);
        }
        pinkB0_ = pinkB1_ = pinkB2_ = 0.0;

        // New node-solver warm-start states.
        VpPrev_   = VpLast_;
        cfVkLast_ = Vk_rest_;
        cfVkPrev_ = Vk_rest_;

        // Grid-conduction network equilibrium at rest.  With the same
        // branch ratios used in process() (fastToSlowR = 7·R_leak,
        // slowBleedR = 18·R_leak):  slow = fast·18/25, and KCL at the
        // fast node gives fast = Ig_rest·R_leak / (1 + (1−18/25)/7).
        {
            const double IgRest = std::max(0.0,
                triode_.gridCurrent(cfg.Vg_bias + cfg.gridTurnOnVoltage));
            const double rLeak = std::max(cfg.gridLeakR, 1.0e3);
            const double slowShare = 18.0 / 25.0;
            const double fastRest = IgRest * rLeak
                                  / (1.0 + (1.0 - slowShare) / 7.0);
            gridChargeFastV_ = fastRest;
            gridChargeSlowV_ = slowShare * fastRest;
            gridChargeRestV_ = gridChargeFastV_ + gridChargeSlowV_;
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

        // Zero transient state.  NOTE: the grid-conduction branches are
        // deliberately NOT zeroed here — they were just primed to their
        // standing equilibrium above (lines ~647-657).  The chain rebuilds
        // through setup() on every load / reroll / per-stage edit (it does
        // not call reset()), so zeroing them here would re-inject the
        // 22 ms blocking-recovery startup transient on every such event.
        millerState_ = 0.0;
        slewState_   = 0.0;
        ftVpPrev_  = Vp_rest_;
        ftVpPrev2_ = Vp_rest_;
        ftVgPrev_  = 0.0;
        ftVgPrev2_ = 0.0;
        lrEL_ = lrJC_ = lrJLm_ = 0.0;
        outputDC_    = restingOutputDC();
    }

    // Reset state variables (e.g., on sample rate change or user "cold start").
    // Prime cathode bypass to resting DC to suppress startup transient.
    void reset(bool coldStart = true)
    {
        bounce_.primeTo(Vk_rest_);
        warmupCurrent_ = (coldStart && config_.enableWarmup) ? 0.85 : 1.0;
        millerState_   = 0.0;
        // Re-prime the grid network to its standing equilibrium (same
        // split as setup(): slow = fast·18/25 of the total rest charge).
        gridChargeFastV_ = gridChargeRestV_ * (25.0 / 43.0);
        gridChargeSlowV_ = gridChargeRestV_ * (18.0 / 43.0);
        ipAvgLong_     = Ip_rest_;
        slewState_     = 0.0;
        lastIp_        = Ip_rest_;  // keeps Miller programFactor = 1
        lastScreenCurrent_ = 0.0;
        screenNodeV_   = config_.screenSupplyVolts;
        Vmid_last_     = Vmid_rest_;
        ltpVkLast_     = Vk_rest_;
        ltpVpPosLast_  = ltpVpPosRest_;
        ltpVpNegLast_  = ltpVpNegRest_;
        VpLast_        = Vp_rest_;
        VpPrev_        = Vp_rest_;
        VaLast_        = std::max(0.0, Vp_rest_ - Vk_rest_);
        cfVkLast_      = Vk_rest_;
        cfVkPrev_      = Vk_rest_;
        avSmooth_      = avRest_;
        pinkB0_ = pinkB1_ = pinkB2_ = 0.0;
        extMagDropNorm_ = 0.0;
        seMagLastA_     = 0.0;
        ftVpPrev_  = Vp_rest_;
        ftVpPrev2_ = Vp_rest_;
        ftVgPrev_  = 0.0;
        ftVgPrev2_ = 0.0;
        lrEL_ = lrJC_ = lrJLm_ = 0.0;
        outputDC_      = restingOutputDC();
    }

    /// Carry the SLOW (musical-memory) state from a previous incarnation
    /// of this stage after a parameter-edit rebuild (docs/34 §4.3): warmup
    /// progress, thermal history, cathode-bounce / blocking-charge deltas,
    /// noise & hum phase continuity and the output-DC tracker are re-based
    /// onto the NEW rest point — automating Bias/Drive no longer
    /// cold-starts the amp.  Solver warm starts stay at the new rest (they
    /// re-converge within a sample).  Caller guarantees same topology.
    void carrySlowStateFrom(const TubeStage& o) noexcept
    {
        auto fin = [](double v, double fb)
        { return std::isfinite(v) ? v : fb; };

        warmupCurrent_ = std::clamp(fin(o.warmupCurrent_, 1.0), 0.5, 1.0);
        ipAvgLong_ = Ip_rest_ + fin(o.ipAvgLong_ - o.Ip_rest_, 0.0);
        // Rebase BOTH bounce states on the new rest (docs/35 C7): the
        // Vs−Vk separation is the in-progress DA bloom and must survive
        // the carry, not be flattened to a settled single value.
        bounce_.primeTo(
            Vk_rest_ + fin(o.bounce_.mainCapVolts() - o.Vk_rest_, 0.0),
            Vk_rest_ + fin(o.bounce_.soakageVolts() - o.Vk_rest_, 0.0));

        const double oF = o.gridChargeRestV_ * (25.0 / 43.0);
        const double oS = o.gridChargeRestV_ * (18.0 / 43.0);
        gridChargeFastV_ = std::max(0.0, gridChargeRestV_ * (25.0 / 43.0)
            + fin(o.gridChargeFastV_ - oF, 0.0));
        gridChargeSlowV_ = std::max(0.0, gridChargeRestV_ * (18.0 / 43.0)
            + fin(o.gridChargeSlowV_ - oS, 0.0));

        heaterPhase_ = fin(o.heaterPhase_, 0.0);
        if (o.shotRng_ != 0) shotRng_ = o.shotRng_;
        pinkB0_ = fin(o.pinkB0_, 0.0);
        pinkB1_ = fin(o.pinkB1_, 0.0);
        pinkB2_ = fin(o.pinkB2_, 0.0);
        outputDC_ = restingOutputDC()
            + fin(o.outputDC_ - o.restingOutputDC(), 0.0);
        avSmooth_ = avRest_;
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
                                         || ! std::isfinite(micY2_)
                                         || ! std::isfinite(VpLast_)
                                         || ! std::isfinite(VpPrev_)
                                         || ! std::isfinite(VaLast_)
                                         || ! std::isfinite(cfVkLast_)
                                         || ! std::isfinite(cfVkPrev_)
                                         || ! std::isfinite(avSmooth_))
        {
            outputDC_      = Vp_rest_;
            millerState_   = 0.0;
            warmupCurrent_ = 1.0;
            lastIp_        = Ip_rest_;
            gridChargeFastV_ = gridChargeRestV_ * (25.0 / 43.0);
            gridChargeSlowV_ = gridChargeRestV_ * (18.0 / 43.0);
            heaterPhase_   = 0.0;
            ipAvgLong_     = Ip_rest_;
            slewState_     = 0.0;
            micX1_ = micX2_ = micY1_ = micY2_ = 0.0;
            VpLast_   = Vp_rest_;
            VpPrev_   = Vp_rest_;
            VaLast_   = std::max(0.0, Vp_rest_ - Vk_rest_);
            cfVkLast_ = Vk_rest_;
            cfVkPrev_ = Vk_rest_;
            avSmooth_ = avRest_;
            ftVpPrev_  = Vp_rest_;
            ftVpPrev2_ = Vp_rest_;
            ftVgPrev_  = 0.0;
            ftVgPrev2_ = 0.0;
            lrEL_ = lrJC_ = lrJLm_ = 0.0;
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
        double vgSignal = config_.voltageNativeInput
            ? inputSample                              // already grid volts
            : inputSample * config_.inputVoltageSwing;
        if (config_.enableMillerFilter)
            vgSignal = applyMillerFilter(vgSignal);

        // 2b) Miller feedthrough zero (docs/34 §3.5): inject the NONLINEAR
        //     residual of the plate's last step — actual ΔVp minus the
        //     linear prediction −|Av|·Δvg — through Cgp and the grid's
        //     total source impedance.  Cancels identically while the tube
        //     tracks linearly; on clipped edges the collapsed feedback
        //     lets the plate edge spit into the grid (the physical
        //     "pole opens at clip" direction of docs/04 §3.2).
        if (config_.enableMillerFeedthrough && config_.enableMillerFilter
            && config_.topology == TubeTopology::CommonCathode
            && std::isfinite(ftVpPrev_) && std::isfinite(ftVpPrev2_)
            && std::isfinite(ftVgPrev_) && std::isfinite(ftVgPrev2_))
        {
            const double dVpAct = ftVpPrev_ - ftVpPrev2_;
            const double dVpLin = -avSmooth_ * (ftVgPrev_ - ftVgPrev2_);
            // Closed-loop divider: the raw feedthrough kRaw = Zsrc·Cgp·fs
            // feeds a loop whose one-sample gain is kRaw·|Av| (the plate
            // answers injected grid volts with −|Av| and the residual
            // re-enters here next sample).  The real circuit's loop is
            // algebraic and self-limits by exactly this divider — apply it
            // explicitly so the sampled recursion's gain stays < 1 for ANY
            // Zsrc·|Av| (kRaw·|Av| reaches ≈ 1 for a 12AU7 driven from a
            // 220 kΩ plate; the un-normalised form limit-cycled under
            // TP-switch stress).
            const double kRaw = gridDriveImpedance()
                              * config_.Cgp_miller * sampleRate_;
            const double kFt = kRaw
                / (1.0 + kRaw * std::max(avSmooth_, 0.0));
            double ft = kFt * (dVpAct - dVpLin);
            const double cap = 0.25 * std::max(config_.inputVoltageSwing,
                                               1.0e-3);
            ft = std::clamp(ft, -cap, cap);
            vgSignal += ft;
        }

        // 3) Cathode bypass dynamic bias shift:
        //    CathodeBounce tracks full Vk (including DC). We subtract Vk_rest
        //    to get only the AC perturbation, so that in resting state the
        //    effective bias equals config_.Vg_bias (no artificial offset).
        //
        //    Cathode followers are EXCLUDED: their cathode node is solved
        //    implicitly inside step 4 (the legacy one-sample-delayed Vk
        //    feedback had loop gain gm·Rk ≈ 7 and limit-cycled — the stage
        //    output was a fixed-amplitude square wave regardless of input).
        const bool isCathodeFollower =
            config_.topology == TubeTopology::CathodeFollower;
        // Unbypassed common cathode is handled by the implicit series-
        // feedback solve inside step 4 — feeding the (with Ck≈0 nearly
        // instantaneous) bounce voltage back here as well would apply
        // the same degeneration twice.
        const bool ccDegenerated =
            config_.topology == TubeTopology::CommonCathode
            && ! config_.enablePentodeModel
            && config_.Ck < 1.0e-9 && config_.Rk > 1.0;
        const double Vk_full = (config_.enableCathodeBounce
                                && ! isCathodeFollower && ! ccDegenerated)
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
            const double white = gaussianApprox_();
            // 1/f flicker component (Kellet pink filter): the LF noise
            // slope of real cathode emission, on top of the white shot
            // floor.  Both scale with conduction.
            pinkB0_ = 0.99765 * pinkB0_ + white * 0.0990460;
            pinkB1_ = 0.96300 * pinkB1_ + white * 0.2965164;
            pinkB2_ = 0.57000 * pinkB2_ + white * 1.0526913;
            const double pink =
                0.25 * (pinkB0_ + pinkB1_ + pinkB2_ + white * 0.1848);
            // Pentode partition noise: the random division of the space
            // current between plate and screen adds ~sqrt(1 + 2·Ig2/Ip)
            // over the triode shot floor — pentode stages genuinely hiss
            // more than triodes at the same current.
            double partition = 1.0;
            if (config_.enablePentodeModel
                && config_.topology == TubeTopology::CommonCathode)
                partition = std::min(3.0, std::sqrt(
                    1.0 + 2.0 * std::max(0.0, lastScreenCurrent_)
                              / std::max(std::abs(lastIp_), 1.0e-9)));
            Vg += config_.shotNoiseScale * ipRatio * partition
                * (white + config_.flickerNoiseRatio * pink);
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

            // Line fundamental from heater-cathode leakage plus a 2nd
            // harmonic: heating power goes as V², so the emission ripple
            // rides at 2×line — real hum spectra always show both.
            const double levelMod =
                1.0 + config_.heaterModDepth * std::abs(vgSignal);
            const double hum =
                config_.heaterHumAmplitude * levelMod
                * (std::sin(heaterPhase_)
                   + 0.35 * std::sin(2.0 * heaterPhase_ + 0.6));
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
            // Referenced to the resting equilibrium charge so that the
            // continuous law's standing grid-leak bias (~Ig0·R_leak) is
            // already folded into cfg.Vg_bias ("resting Vgk" convention).
            const double gridChargeV =
                gridChargeFastV_ + gridChargeSlowV_ - gridChargeRestV_;
            const double Vg_loaded = Vg - gridChargeV;

            // Continuous grid-current law: reuse the tube's own Dempwolf
            // Ig fit, shifted by the contact potential so conduction sets
            // in softly around Vgk ≈ −0.5 V (Rutt 1984) — the legacy hard
            // clamp at +0.5 V started a full volt late, had a derivative
            // discontinuity (aliasing at the conduction corner), and
            // disagreed with the Ig law already inside plateCurrent().
            //
            // The grid node also loads its source THROUGH the stopper
            // while conducting (positive-peak flattening in real time):
            //   Vg_eff = Vg_loaded − Ig(Vg_eff)·R_stop
            // solved by a 2-step Newton on the monotone residual (the
            // conduction slope dIg/dV·R_stop can exceed 1, so plain
            // fixed-point iteration would diverge).
            const double von   = config_.gridTurnOnVoltage;
            // The grid draws its conduction current THROUGH the same total
            // drive impedance the Miller model sees: the physical grid
            // stopper PLUS the upstream stage's instantaneous output
            // impedance (rp∥Rp of the previous stage, handed in by the
            // chain).  The legacy code loaded only the stopper, so a stage
            // fed by a high-Zout driver flattened positive peaks far too
            // little — the previous stage genuinely cannot source unlimited
            // grid current.  (Same zUp + stopper series impedance is used
            // in applyMillerFilter, keeping one grid node = one source Z.)
            const double rStop = gridDriveImpedance();
            // The g-k diode sees Vgk, not the grid DRIVE: for a cathode
            // follower the cathode rides up with the grid, so the actual
            // junction voltage stays pinned near the bias — without this
            // offset a follower would accumulate blocking charge it can
            // physically never generate.  (One-sample-stale Vk; the
            // follower gain ≈ 1 keeps the error a few mV.)
            const double cfVkOffset = isCathodeFollower
                ? (cfVkLast_ - Vk_rest_) : 0.0;
            // One-sample-stale plate-cathode voltage for the Ig division
            // region (docs/34 §3.1): when the plate has swung down toward
            // the grid, the grid competes for the space current and its
            // conduction (and thus blocking charge) rises several-fold.
            double VaPrev;
            if (isCathodeFollower)
                VaPrev = std::max(1.0, Vb_plus - cfVkLast_);
            else if (config_.enablePentodeModel
                     && config_.topology == TubeTopology::CommonCathode)
                VaPrev = std::max(1.0, VaLast_);
            else if (config_.topology == TubeTopology::SRPP
                     || config_.topology == TubeTopology::Cascode)
                VaPrev = std::max(1.0, Vmid_last_);
            else if (config_.topology == TubeTopology::LongTailedPair)
                VaPrev = std::max(1.0,
                    std::min(ltpVpPosLast_, ltpVpNegLast_) - ltpVkLast_);
            else
                VaPrev = std::max(1.0, VpLast_ - Vk_full);
            double VgEff = Vg_loaded;
            double Ig;
            if (Vg_loaded - cfVkOffset + von < -0.5)
            {
                // Far below conduction: Ig is leakage-dominated and the
                // stopper drop is microvolts — one cheap evaluation, no
                // Newton.  This is the common case at mix levels.
                Ig = std::max(0.0, triode_.gridCurrent(
                    Vg_loaded - cfVkOffset + von, VaPrev));
            }
            else
            {
                for (int it = 0; it < 2; ++it)
                {
                    const auto gd = triode_.gridCurrentWithDeriv(
                        VgEff - cfVkOffset + von, VaPrev);
                    const double g  = VgEff + gd.Ig * rStop - Vg_loaded;
                    const double gp = 1.0 + std::max(0.0, gd.dIg) * rStop;
                    VgEff -= g / gp;
                    if (! std::isfinite(VgEff)) { VgEff = Vg_loaded; break; }
                }
                Ig = std::max(0.0, triode_.gridCurrent(
                    VgEff - cfVkOffset + von, VaPrev));
            }

            // Two-time-constant blocking memory:
            //   fast branch   -> "attack squeeze"  (charges via Ig)
            //   slow branch   -> lingering recovery tail (dielectric
            //                    absorption share of the coupling cap)
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

            if (gridChargeFastV_ < 0.0) gridChargeFastV_ = 0.0;
            if (gridChargeSlowV_ < 0.0) gridChargeSlowV_ = 0.0;

            Vg = VgEff;   // triode sees the loaded, blocked grid voltage
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
            //   2) evaluate Ip/Ig2/Ig1 with explicit g3 coupling, with the
            //      plate LOAD LINE included (Va = Vb − Ip·Rp − Vk) via a
            //      damped warm-started fixed point
            //   3) feed Ig2 back into the screen RC state
            const double g3Drive =
                config_.suppressorDrivePolarity
                * std::max(0.0, config_.suppressorDriveMix)
                * std::abs(vgSignal);
            const double Vg3 = config_.suppressorTieToCathode
                ? 0.0 + g3Drive
                : (config_.suppressorBiasVolts - Vk_full + g3Drive);

            // Plate node via damped fixed point, warm-started from the
            // previous sample.  Pentode plate curves are flat (high rp),
            // so the contraction factor Rp·∂Ip/∂Va is well below 1 and
            // 3 damped iterations land within solver noise.
            double Va = std::isfinite(VaLast_)
                ? std::clamp(VaLast_, 1.0, std::max(1.0, Vb_plus))
                : std::max(1.0, Vb_plus - Vk_full);

            double Vg2 = Va;
            if (!config_.pentodeTriodeStrap)
            {
                const double Rs = std::max(config_.screenResistorOhms, 1.0);
                // The screen dropper hangs off the SAME B+ rail as the
                // plate, so PSU sag / ripple / ladder droop reach the
                // screen too — the legacy constant supply silently
                // bypassed every rail mechanism for pentode stages.
                // (screenAlpha_/invVpNominal_ are setup-time constants.)
                const double supplyEff = config_.screenSupplyVolts
                    * (Vb_plus * invVpNominal_);
                const double Vtarget =
                    supplyEff - lastScreenCurrent_ * Rs;
                screenNodeV_ = screenAlpha_ * screenNodeV_
                             + (1.0 - screenAlpha_) * Vtarget;
                if (!std::isfinite(screenNodeV_))
                    screenNodeV_ = supplyEff;
                Vg2 = std::max(0.0, screenNodeV_);
            }

            // Newton on the plate node with a numeric slope (the pentode
            // model's analytic plate derivative).  The residual
            //   g(Va) = Va + warm·Ip(Va)·Rp − (Vb − Vk)
            // is monotone in Va, so the damped Newton step cannot cycle —
            // unlike the previous fixed-point relaxation, which diverged
            // to the clamp floor on steep triode-strapped curves.
            KorenPentode::Currents pent {};
            const double VaTarget = std::max(1.0, Vb_plus - Vk_full);
            // Next-stage grid-leak AC loading (docs/34 §3.8, pentode
            // extension — docs/35 C2): DC anchors through the full Rp,
            // the signal load line is Rp ∥ Rg.  Thevenin pivot around
            // the standing current keeps the rest point untouched and
            // preserves dVa/dVb = 1 for slow rail moves.
            const double rgNextP = (! config_.plateLoadIsTransformer
                                    && config_.nextStageLoadR > 1.0)
                ? config_.nextStageLoadR : 0.0;
            const double rAcP = (rgNextP > 0.0)
                ? config_.Rp * rgNextP / (config_.Rp + rgNextP)
                : config_.Rp;
            const double vaAnchor = std::max(1.0,
                VaTarget - (config_.Rp - rAcP) * Ip_rest_);
            // Exact Newton using KorenPentode's analytic ∂Ip/∂Va.  The old
            // cross-sample secant was excellent for slow (steady-state)
            // input but took extra iterations on noisy/transient material,
            // where Va moves every sample — the analytic slope converges in
            // one or two evaluations regardless.  One pentode evaluation per
            // iteration, and the slope is exact rather than a stale guess.
            for (int it = 0; it < 3; ++it)
            {
                if (config_.pentodeTriodeStrap)
                    Vg2 = Va;
                pent = pentode_.evaluate(Va, Vg, Vg2, Vg3);
                const double g = Va
                    + warmupCurrent_ * std::max(0.0, pent.Ip) * rAcP
                    - vaAnchor;
                const double slope = std::clamp(
                    1.0 + warmupCurrent_ * rAcP
                        * std::max(0.0, pent.dIpdVa),
                    1.0, 1.0e12);
                pentSlope_ = slope;
                if (std::abs(g) < 1.0e-3) break;   // warm start converged
                Va -= g / slope;
                if (! std::isfinite(Va)) { Va = VaTarget; break; }
                Va = std::clamp(Va, 1.0, std::max(1.0, Vb_plus));
            }
            VaLast_ = Va;
            // Plate current from the circuit side of the converged node
            // (exact KCL, no extra pentode evaluation); the screen RC
            // uses the last in-loop Ig2, half a Newton step stale.
            Ip = std::max(0.0, (vaAnchor - Va)
                               / std::max(rAcP, 1.0)
                               / std::max(warmupCurrent_, 1.0e-3));
            // Warm-up is cathode-emission limiting, so it scales EVERY
            // electrode current, not just the plate.  Leaving Ig2 at full
            // amplitude while Ip ran at warm·Ip let the screen node sag
            // onto the lower branch of the bistable 6AS6 landscape during
            // warm-up and lock there — a ~17 V standing offset against the
            // (warm=1) rest calibration (docs/35 §S2 D-A).
            lastScreenCurrent_ = warmupCurrent_ * pent.Ig2;
            lastGridCurrent_   = warmupCurrent_ * std::max(0.0, pent.Ig1);

            // Operating-point hand-off for the Miller model and the
            // dynamic output impedance given to the next stage — the
            // setup()-time triode-proxy values would freeze the pentode's
            // program-dependent Miller and report a far-too-low Zout
            // (pentode rp ≫ triode rp).  rpInv comes exactly from the
            // cached residual slope (pentSlope_ = 1 + warm·Rp·dIp/dVa);
            // gm follows the Langmuir gm ∝ √I law around the rest point.
            lastRpInvLoaded_ = std::max(0.0,
                (pentSlope_ - 1.0) / std::max(config_.Rp, 1.0));
            lastGmLoaded_ = warmupCurrent_ * gmRest_
                * std::sqrt(std::max(0.0, Ip)
                            / std::max(Ip_rest_, 1.0e-9));
            if (!std::isfinite(lastScreenCurrent_) || lastScreenCurrent_ < 0.0)
                lastScreenCurrent_ = 0.0;

            // (No second RC advance here — the legacy "extra correction
            // step" stepped the screen integrator twice per sample, which
            // silently halved the Rs·Cs time constant.)
            if (config_.pentodeTriodeStrap)
                screenNodeV_ = Va;
        }
        else if (config_.topology == TubeTopology::LongTailedPair)
        {
            // The LTP solves its own pair + tail in the output-node block
            // below; carry the previous current so the meters stay sane.
            Ip = lastIp_;
        }
        else if (isCathodeFollower)
        {
            // Implicit cathode-node solve.  KCL at the cathode:
            //   warm·Ik(Vb − Vk, VG − Vk) = Vk/Rk + Ck·dVk/dt
            // (backward Euler on the bypass cap; Ck = 0 → pure algebraic).
            // Newton converges in 2–3 iterations from the warm start; the
            // residual derivative is strictly negative so steps never
            // change sign.  This replaces the legacy one-sample-delayed
            // Vk feedback, which was unstable for gm·Rk > 1.
            const double Rk   = std::max(config_.Rk, 1.0);
            const double dt   = 1.0 / sampleRate_;
            const double capG = std::max(config_.Ck, 0.0) / dt;
            const double VG   = Vg + Vk_rest_;   // absolute grid voltage
            double Vk = std::isfinite(cfVkLast_) ? cfVkLast_ : Vk_rest_;
            KorenTriode::IpDerivatives d {};
            for (int it = 0; it < 3; ++it)
            {
                d = triode_.evalWithDerivatives(
                    std::max(1.0, Vb_plus - Vk), VG - Vk);
                const double IpW = warmupCurrent_ * d.Ip;
                const double f   = IpW - Vk / Rk - capG * (Vk - cfVkPrev_);
                const double fp  = -warmupCurrent_ * (d.gm + d.rpInv)
                                 - 1.0 / Rk - capG;
                Vk -= f / fp;
                if (! std::isfinite(Vk)) { Vk = Vk_rest_; break; }
                Vk = std::clamp(Vk, 0.0, std::max(1.0, Vb_plus));
            }
            cfVkPrev_ = Vk;
            cfVkLast_ = Vk;
            d  = triode_.evalWithDerivatives(
                std::max(1.0, Vb_plus - Vk), VG - Vk);
            Ip = warmupCurrent_ * d.Ip;
            lastGmLoaded_    = warmupCurrent_ * d.gm;
            lastRpInvLoaded_ = warmupCurrent_ * d.rpInv;
        }
        else
        {
            // Common cathode WITH the plate load line + plate-node stray
            // capacitance, solved implicitly:
            //   Cp·dVp/dt + warm·Ip(Vp, Vgk) + (Vp − Vb)/Rp = 0
            // The legacy code evaluated Ip at Vp = B+ — no plate feedback
            // at all, i.e. a static waveshaper with gain gm·Rp instead of
            // the physical gm·(Rp ∥ rp).  Newton, warm-started; the
            // residual is monotone in Vp.
            const double dt    = 1.0 / sampleRate_;
            const double capG  = std::max(config_.Cplate, 0.0) / dt;
            // Transformer-coupled plates ride the AC load line through
            // the DCR-anchored idle: Thevenin vOpen = Vb + Rac·Ip_rest,
            // total slope Rac + DCR, and the plate may swing above the
            // rail on the cutoff half (inductive flyback).
            const bool trafoLoad = config_.plateLoadIsTransformer;
            // Next-stage grid-leak AC loading (docs/34 §3.8): DC still
            // anchors through Rp to the rail, but the signal load line is
            // Rp ∥ Rg (the coupling cap is transparent at audio).  The
            // Thevenin form pivots the slope around the standing current,
            // so slow rail moves (sag/ripple) still reach the plate with
            // the full dVp/dVb = 1 weighting.
            const double rgNext = (! trafoLoad && config_.nextStageLoadR > 1.0)
                ? config_.nextStageLoadR : 0.0;
            const double rAcEff = (rgNext > 0.0)
                ? config_.Rp * rgNext / (config_.Rp + rgNext)
                : std::max(config_.Rp, 1.0);
            // SE-OPT magnetizing current this plate must source (docs/34
            // §2.2) — one-sample-stale drop from the chain, converted and
            // clamped to the standing current (a gapped SE core cannot
            // demand more than order-of-idle before the tube cuts off).
            const double iMagSe = trafoLoad
                ? std::clamp(extMagDropNorm_ * seMagToAmps_,
                             -Ip_rest_, Ip_rest_)
                : 0.0;
            seMagLastA_ = iMagSe;
            // Reactive reflected load (docs/34 §2.5): the AC branch becomes
            // a one-port companion — same Newton, per-sample Thevenin.  The
            // magnetizing share is folded into the open-circuit voltage
            // (it bypasses the reflected network through the primary).
            const bool react = trafoLoad && loadReactive_;
            const double lrVh = react
                ? (lrJC_ - lrJLm_) / lrGm_ - lrEL_ : 0.0;
            const double iMagKcl = react ? 0.0 : iMagSe;
            const double rLoad = trafoLoad
                ? (react ? lrRtot_ + std::max(config_.plateLoadDcr, 0.0)
                         : config_.Rp + std::max(config_.plateLoadDcr, 0.0))
                : rAcEff;
            const double vOpen = trafoLoad
                ? (react ? Vb_plus + lrRtot_ * (Ip_rest_ + iMagSe) - lrVh
                         : Vb_plus + config_.Rp * Ip_rest_)
                : (rgNext > 0.0
                       ? Vb_plus - (config_.Rp - rAcEff) * Ip_rest_
                       : Vb_plus);
            const double vMax = trafoLoad
                ? std::max(1.0, (react ? 3.0 : 2.0) * Vb_plus)
                : std::max(1.0, Vb_plus);
            const double gLoad = 1.0 / std::max(rLoad, 1.0);
            double Vp = std::isfinite(VpLast_)
                ? std::clamp(VpLast_, 0.0, vMax)
                : Vp_rest_;
            KorenTriode::IpDerivatives d {};
            double IpW = 0.0;
            for (int it = 0; it < 2; ++it)
            {
                // (two warm-started Newton steps; the converged plate
                // current is then read from the CIRCUIT side below, so
                // no third device evaluation is needed)
                double VgkEff = Vg;
                double degenFactor = 1.0;
                if (ccDegenerated)
                {
                    // Instantaneous series feedback from the unbypassed
                    // cathode resistor, REST-REFERENCED: only the current
                    // deviation from idle degenerates the drive, keeping
                    // the project convention that cfg.Vg_bias is the
                    // resting Vgk (the setup() rest Newton evaluates at
                    // Vg_bias with no Rk term).  Solve
                    //   i = Ip(Vp, Vg − (i − Ip_rest)·Rk)
                    // by an inner Newton (g' = 1 + gm·Rk, always > 1).
                    double i = std::max(0.0, lastIp_);
                    KorenTriode::IpDerivatives di {};
                    for (int jt = 0; jt < 2; ++jt)
                    {
                        di = triode_.evalWithDerivatives(
                            Vp, Vg - (i - Ip_rest_) * config_.Rk);
                        const double g  = i - warmupCurrent_ * di.Ip;
                        const double gp = 1.0 + warmupCurrent_ * di.gm * config_.Rk;
                        i -= g / gp;
                        if (! std::isfinite(i)) { i = std::max(0.0, lastIp_); break; }
                        if (i < 0.0) i = 0.0;
                    }
                    VgkEff = Vg - (i - Ip_rest_) * config_.Rk;
                    // Degeneration also divides the effective slopes —
                    // reuse the inner solve's final evaluation (same
                    // arguments) instead of evaluating the device again.
                    degenFactor = 1.0 / (1.0 + warmupCurrent_ * di.gm * config_.Rk);
                    d = di;
                }
                else
                {
                    d = triode_.evalWithDerivatives(Vp, VgkEff);
                }
                IpW = warmupCurrent_ * d.Ip;
                const double f  = IpW - iMagKcl + (Vp - vOpen) * gLoad
                                + capG * (Vp - VpPrev_);
                const double fp = warmupCurrent_ * d.rpInv * degenFactor
                                + gLoad + capG;
                Vp -= f / fp;
                if (! std::isfinite(Vp)) { Vp = Vp_rest_; break; }
                Vp = std::clamp(Vp, 0.0, vMax);
            }
            // Read the converged current from the circuit-side KCL: at
            // the root these agree with the device law, and using the
            // circuit value keeps PSU draw / cathode charge EXACTLY
            // consistent with the solved node voltage (and saves a third
            // device evaluation per sample).  The magnetizing share adds
            // to the tube's total draw (it flows through the primary).
            IpW = (vOpen - Vp) * gLoad + iMagKcl - capG * (Vp - VpPrev_);
            if (IpW < 0.0) IpW = 0.0;
            VpPrev_ = Vp;
            VpLast_ = Vp;
            Ip = IpW;

            // Advance the reactive-load companion histories with the
            // solved AC branch current (trapezoidal, docs/34 §2.5).
            if (react)
            {
                const double iAc = Ip - Ip_rest_ - iMagSe;
                const double vM  = (iAc + (lrJC_ - lrJLm_)) / lrGm_;
                lrEL_  = 2.0 * lrBLe_ * iAc - lrEL_;
                lrJC_  = 2.0 * lrBCm_ * vM - lrJC_;
                lrJLm_ = 2.0 * vM * lrInvBLm_ + lrJLm_;
                if (! std::isfinite(lrEL_) || ! std::isfinite(lrJC_)
                    || ! std::isfinite(lrJLm_))
                    lrEL_ = lrJC_ = lrJLm_ = 0.0;
            }
            lastGmLoaded_    = warmupCurrent_ * d.gm;
            lastRpInvLoaded_ = warmupCurrent_ * d.rpInv;
        }
        // Warmup for the stacked topologies is applied after their joint
        // solve (legacy convention); the CC / CF / pentode paths fold it
        // into the node equations above.
        if (config_.topology == TubeTopology::SRPP
            || config_.topology == TubeTopology::Cascode)
            Ip *= warmupCurrent_;
        else if (config_.enablePentodeModel
                 && config_.topology == TubeTopology::CommonCathode)
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
        //     drift.  Driven by mean CATHODE current (docs/03 §2.3:
        //     sustained heavy conduction shifts emission and the
        //     operating point over seconds — "the amp sits down").
        //     Deliberately NOT anode dissipation: in class A the anode
        //     actually runs cooler under drive (power moves to the
        //     load), which is the wrong sign for the documented effect.
        if (config_.enableThermalDrift)
        {
            ipAvgLong_ = thermalAlpha_ * ipAvgLong_
                       + (1.0 - thermalAlpha_) * std::abs(Ip);
        }

        // 5) Update cathode bypass state with the new CATHODE current
        //    (for a pentode that is Ip + Ig2 — both pass through Rk; the
        //    legacy code dropped the screen share of the self-bias).
        //    Skipped for cathode followers — their cathode node is the
        //    implicitly solved cfVkLast_, not the slow bounce tracker.
        if (config_.enableCathodeBounce && ! isCathodeFollower)
        {
            // Cathode current returns through Rk: for a pentode that is
            // Ip + Ig2 + Ig1 (all three currents originate from the same
            // emitted space charge and flow back through the cathode
            // resistor — the g1-conduction share matters on hard-driven
            // Culture Vulture positive peaks).
            const bool isPentode = config_.enablePentodeModel
                && config_.topology == TubeTopology::CommonCathode;
            const double Ik = Ip
                + (isPentode ? std::max(0.0, lastScreenCurrent_)
                             + std::max(0.0, lastGridCurrent_)
                             : 0.0);
            bounce_.process(Ik);
        }

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
            // The implicitly-solved cathode voltage IS the output node.
            // A follower's voltage gain is ≈ µ/(µ+1) ≈ 1, so the natural
            // normalizer is the input swing — the legacy |Vk_rest|·2
            // normalizer was calibrated against the unstable explicit
            // solver and has no physical meaning.
            rawOut     = cfVkLast_;
            normalizer = std::max(config_.inputVoltageSwing, 1.0e-3);
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
            // Pair parameters are configured once in setup(); the legacy
            // per-sample setParams() rebuild was redundant.
            const double rpRatio = std::max(0.2, config_.ltpPlateRRatio);
            const double RpPos = config_.Rp;
            const double RpNeg = config_.Rp * rpRatio;

            const double vDrive = Vg - config_.Vg_bias;
            const double VgPos = config_.Vg_bias + vDrive;
            const double VgNeg = config_.Vg_bias - vDrive;

            // Per-side plate load-line Newton given (Vk, Vg_side):
            //   f(Vp) = warm·Ip(Vp, Vg − Vk) + (Vp − Vb)/Rp = 0
            auto solvePlate = [this, Vb_plus](const KorenTriode& tube,
                                              double VgSide, double Vk,
                                              double Rp, double& VpWarm)
            {
                const double gLoad = 1.0 / std::max(Rp, 1.0);
                double Vp = std::isfinite(VpWarm)
                    ? std::clamp(VpWarm, 0.0, std::max(1.0, Vb_plus))
                    : Vb_plus * 0.7;
                KorenTriode::IpDerivatives d {};
                for (int it = 0; it < 2; ++it)
                {
                    d = tube.evalWithDerivatives(Vp, VgSide - Vk);
                    const double f  = warmupCurrent_ * d.Ip
                                    + (Vp - Vb_plus) * gLoad;
                    const double fp = warmupCurrent_ * d.rpInv + gLoad;
                    Vp -= f / fp;
                    if (! std::isfinite(Vp)) { Vp = Vb_plus * 0.7; break; }
                    Vp = std::clamp(Vp, 0.0, std::max(1.0, Vb_plus));
                }
                d = tube.evalWithDerivatives(Vp, VgSide - Vk);
                VpWarm = Vp;
                return d;
            };

            double Vk = std::max(0.0, ltpVkLast_);
            const double tailR = std::max(config_.ltpTailR, 1.0);
            KorenTriode::IpDerivatives p {}, n {};
            for (int it = 0; it < std::max(1, config_.ltpSolverIters); ++it)
            {
                p = solvePlate(ltpTriodePos_, VgPos, Vk, RpPos, ltpVpPosLast_);
                n = solvePlate(ltpTriodeNeg_, VgNeg, Vk, RpNeg, ltpVpNegLast_);
                const double f = warmupCurrent_ * (p.Ip + n.Ip) - Vk / tailR;
                // With the plate load line active each side's current
                // responds to Vk with the degenerated slope gm/(1+Rp·rpInv).
                const double gmP = p.gm / (1.0 + RpPos * p.rpInv);
                const double gmN = n.gm / (1.0 + RpNeg * n.rpInv);
                const double fp = -warmupCurrent_ * (gmP + gmN) - 1.0 / tailR;
                if (std::abs(fp) < 1.0e-15) break;
                Vk -= f / fp;
                if (! std::isfinite(Vk)) { Vk = ltpVkLast_; break; }
                Vk = std::max(0.0, Vk);
            }
            ltpVkLast_ = Vk;

            // Use the last tail iteration's plate solutions directly —
            // a post-loop re-solve would duplicate both sides' Newton
            // work for a half-step refinement below solver noise.
            const double VpPos = ltpVpPosLast_;
            const double VpNeg = ltpVpNegLast_;

            const double diff = 0.5 * (VpNeg - VpPos);
            const double cm = 0.5 * (VpPos + VpNeg) - Vp_rest_;
            rawOut = diff + std::clamp(config_.ltpCommonModeLeak, 0.0, 1.0) * cm;
            normalizer = config_.Vp_nominal;
            Ip = warmupCurrent_ * 0.5 * (p.Ip + n.Ip);
            lastIp_ = Ip;
            lastGmLoaded_    = warmupCurrent_ * 0.5 * (p.gm + n.gm);
            lastRpInvLoaded_ = warmupCurrent_ * 0.5 * (p.rpInv + n.rpInv);
        }
        else  // CommonCathode (triode: solved plate node; pentode: VaLast_)
        {
            if (config_.enablePentodeModel)
            {
                rawOut = std::clamp(VaLast_ + Vk_full, 0.0,
                                    std::max(1.0, Vb_plus));
                VpLast_ = rawOut;
            }
            else
            {
                rawOut = VpLast_;
            }
            normalizer = config_.Vp_nominal;
        }

        // 7) AC-couple output (subtract slow-tracked DC operating point).
        //    The tracker corner is fixed in Hz (sample-rate derived) — the
        //    old hard-coded 0.9999 moved from 0.8 Hz at 1× to ~12 Hz at
        //    16× oversampling, eating bass as the user raised OS quality.
        //    The SE magnetizing de-embed subtracts the LINEAR share of the
        //    node's response to the injected magnetizing current (rest
        //    rp ∥ Rtot), so the downstream OPT's own calibrated drop is
        //    not double-counted; only the tube's nonlinear struggle to
        //    supply the iron reaches the output (docs/34 §2.2).
        const double ac = rawOut - outputDC_ - seMagZRest_ * seMagLastA_;
        outputDC_ = dcLeakAlpha_ * outputDC_ + (1.0 - dcLeakAlpha_) * rawOut;

        // Voltage-native output (docs/34 §4.1): plate AC in VOLTS around
        // the STATIC rest anchor — the tracker keeps running for meters,
        // but the slow operating-point wobble it would have eaten now
        // PASSES to the interstage coupling (real bias pumping).  The
        // level factors live in the chain-side pad instead.
        if (config_.voltageNativeOutput)
            return rawOut - restingOutputDC()
                 - seMagZRest_ * seMagLastA_;

        // outputMakeup_ restores the legacy small-signal gain calibration
        // (computed at setup from the resting rp ∥ Rp divider) so that the
        // load-line physics changes the CURVATURE — the distortion shape,
        // headroom feel, sag interaction — without re-leveling every
        // preset's gain staging.
        double yNorm = config_.outputGainLinear * outputMakeup_ * ac / normalizer;

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

        // Miller-feedthrough history (plate steps + the grid drive that
        // produced them) for next sample's nonlinear-residual term.
        ftVpPrev2_ = ftVpPrev_;
        ftVpPrev_  = VpLast_;
        ftVgPrev2_ = ftVgPrev_;
        ftVgPrev_  = vgSignal;

        return yNorm;
    }

    // Accessors for visualization / diagnostics (UI meters)
    double warmupProgress() const noexcept { return warmupCurrent_; }
    double lastPlateVoltage() const noexcept { return outputDC_; }
    double lastPlateCurrent() const noexcept { return lastIp_; }
    double lastScreenCurrent() const noexcept { return lastScreenCurrent_; }
    double lastScreenVoltage() const noexcept { return screenNodeV_; }
    double restingPlateCurrent() const noexcept { return Ip_rest_; }
    double restingPlateVoltage() const noexcept { return Vp_rest_; }
    /// Cathode bounce network (read-only) — carry/soakage guards (docs/35 C7).
    const CathodeBounce& cathodeBounce() const noexcept { return bounce_; }
    /// Live screen-grid node voltage (pentode path; supply volts otherwise).
    /// Exposed for the rest-vs-runtime consistency guards (docs/35 §S2 D-A).
    double screenNodeVoltage() const noexcept { return screenNodeV_; }
    /// Quiescent screen-grid current [A].  Right after setup() this holds
    /// the rest-point Ig2 from solvePentodeRestPoint (0 for non-pentode
    /// stages).  The rail-ladder calibration must budget for it: a
    /// screen-starved pentode (CV's 6AS6) draws 15–20× its plate current
    /// through the SAME B+ node, and a dropper sized on plate current
    /// alone leaves that node tens of volts under its documented rest
    /// voltage (docs/35 §S2 D-A).
    double restingScreenCurrent() const noexcept
    {
        return std::max(0.0, lastScreenCurrent_);
    }
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
        double deltaVk = 0.0;
        if (config_.topology == TubeTopology::CathodeFollower)
            deltaVk = std::abs(cfVkLast_ - Vk_rest_);
        else if (config_.enableCathodeBounce)
            deltaVk = std::abs(bounce_.currentBias() - Vk_rest_);
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

    /// Level-calibration factors for the chain's voltage-native pad
    /// synthesis (docs/34 §4.1): pad_i = outputGainLinear·makeup·swing_{i+1}
    /// / normalizer — the SAME product the legacy normalized hand-off
    /// applied, expressed as one explicit inter-stage attenuator.
    double outputMakeupFactor() const noexcept { return outputMakeup_; }
    double outputNormalizer() const noexcept
    {
        switch (config_.topology)
        {
            case TubeTopology::CathodeFollower:
                return std::max(config_.inputVoltageSwing, 1.0e-3);
            case TubeTopology::SRPP:
                return std::max(config_.Vp_nominal * 0.5, 1.0);
            default:
                return std::max(config_.Vp_nominal, 1.0);
        }
    }

    /// Normalized small-signal in→out gain at the resting operating point,
    /// assembled from the same setup-time quantities the level calibration
    /// uses (avRest_·outputMakeup_ ≈ the legacy calibrated gain for the CC
    /// paths).  Exact-ish for CC (the topology every NFB preset uses);
    /// conservative approximations for the stacked topologies.  Used by
    /// TubeAmpChain to derive the global-NFB β analytically — the sample
    /// probe it replaces rendered ~5k chain samples inside setup(), which
    /// runs on the audio thread during rebuilds (docs/34 §2.1).
    double smallSignalGainNorm() const noexcept
    {
        const double g = config_.outputGainLinear * outputMakeup_;
        switch (config_.topology)
        {
            case TubeTopology::CathodeFollower:
            {
                // Follower gain gm/(gm + 1/rp + 1/Rk) ≈ µ/(µ+1); the CF
                // normalizer is the input swing, so the swing cancels.
                const double gT = lastGmLoaded_ + lastRpInvLoaded_
                                + 1.0 / std::max(config_.Rk, 1.0);
                const double av = (gT > 1.0e-12) ? lastGmLoaded_ / gT : 1.0;
                return av * g;
            }
            case TubeTopology::SRPP:
                return config_.inputVoltageSwing * avRest_ * g
                     / std::max(config_.Vp_nominal * 0.5, 1.0);
            case TubeTopology::Cascode:
            case TubeTopology::LongTailedPair:
            case TubeTopology::CommonCathode:
            default:
                return config_.inputVoltageSwing * avRest_ * g
                     / std::max(config_.Vp_nominal, 1.0);
        }
    }

    /// Instantaneous output impedance at the solved operating point —
    /// what the NEXT stage's grid actually sees driving its Miller input
    /// capacitance (docs/04 §2).  Common cathode: Rp ∥ rp.  Cathode
    /// follower: (rp/(µ+1)) ∥ Rk == 1/(gm + 1/rp) ∥ Rk.  The stacked
    /// topologies approximate with their dominant term.
    double lastOutputImpedance() const noexcept
    {
        switch (config_.topology)
        {
            case TubeTopology::CathodeFollower:
            {
                const double gTube = lastGmLoaded_ + lastRpInvLoaded_;
                const double gRk   = 1.0 / std::max(config_.Rk, 1.0);
                return 1.0 / std::max(gTube + gRk, 1.0e-9);
            }
            case TubeTopology::SRPP:
                return 1.0 / std::max(lastGmLoaded_, 1.0e-9);
            case TubeTopology::Cascode:
                return std::max(config_.Rp_upper, 1.0);
            case TubeTopology::LongTailedPair:
            case TubeTopology::CommonCathode:
            default:
            {
                const double g = 1.0 / std::max(config_.Rp, 1.0)
                               + std::max(lastRpInvLoaded_, 0.0);
                return 1.0 / std::max(g, 1.0e-9);
            }
        }
    }

    /// Chain hook: the previous stage's instantaneous output impedance,
    /// used as the Miller-filter source impedance for this stage.  Pass
    /// 0 to fall back to the static config value.
    void setDynamicSourceImpedance(double z) noexcept
    {
        dynamicSourceZ_ = std::isfinite(z) ? z : 0.0;
    }

    /// Chain hook (docs/34 §2.2, single-ended OPT): normalized magnetizing
    /// drop reported by the output transformer.  Only meaningful when this
    /// stage's plate load IS the OPT (plateLoadIsTransformer) — the SE
    /// primary's magnetizing current is then sourced by this tube's plate
    /// in the node KCL next sample.  Sign note: the OPT integrates the
    /// stage's PLATE-VOLTAGE-proportional output, whereas the primary's
    /// physical volt-seconds are (Vb − Vp) — inverted — so the conversion
    /// constant (seMagToAmps_, setup-time) carries the minus sign.
    void setMagnetizingDropNorm(double dropNorm) noexcept
    {
        extMagDropNorm_ = std::isfinite(dropNorm) ? dropNorm : 0.0;
    }

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

        // Root of r(Vg2) = Vg2 − (supply − Ig2(Vg2)·Rs).  r is NOT always
        // monotone: in the 6AS6-class dynatron region Rs·dIg2/dVg2 < −1
        // bends it into stable/unstable/stable root triplets, and a naive
        // full-range bisection converges to the UNSTABLE middle root — the
        // runtime screen RC then walks ~20 V away from the reported rest
        // (docs/35 §S2 D-A).  The physical quiescent is the root reached at
        // power-up, when the bypass cap charges from 0 V: the FIRST upward
        // crossing of r from below.  Scan up for that bracket, bisect
        // inside it; for monotone tubes the first crossing is the only
        // root, so healthy presets keep their old rest point.
        const double Rs = std::max(config_.screenResistorOhms, 1.0);
        const double hiEnd = std::max(1.0, config_.screenSupplyVolts);
        const auto residual = [&](double v) noexcept
        {
            const auto p = pentode_.evaluate(Va, Vg1, v, Vg3);
            return v - (config_.screenSupplyVolts
                        - std::max(0.0, p.Ig2) * Rs);
        };
        constexpr int kScan = 32;
        double lo = 0.0;
        double hi = hiEnd;
        double rPrev = residual(0.0);
        for (int i = 1; i <= kScan; ++i)
        {
            const double v = hiEnd * static_cast<double>(i)
                           / static_cast<double>(kScan);
            const double rv = residual(v);
            if (rPrev < 0.0 && rv >= 0.0)
            {
                lo = hiEnd * static_cast<double>(i - 1)
                   / static_cast<double>(kScan);
                hi = v;
                break;
            }
            rPrev = rv;
        }
        double Vg2 = hi;
        for (int it = 0; it < 40; ++it)
        {
            Vg2 = 0.5 * (lo + hi);
            if (residual(Vg2) > 0.0) hi = Vg2; else lo = Vg2;
        }
        return Vg2;
    }

    void solvePentodeRestPoint(double Vb) noexcept
    {
        // Rest point with the plate LOAD LINE included (the legacy code
        // evaluated the pentode at Va = Vb − Vk, no Rp drop).  The inner
        // plate solve uses BISECTION on the monotone residual
        //   g(Va) = Va + Ip(Va)·Rp − (Vb − Vk)
        // — steep triode-strapped curves made relaxation iterate to the
        // clamp floor; bisection cannot.
        double Vk = std::max(0.0, Vk_rest_);
        double Ip = 0.0;
        double Ig2 = 0.0;
        double Va = Vb * 0.5;
        double Vg2 = config_.screenSupplyVolts;
        for (int it = 0; it < 16; ++it)
        {
            // GRID CONVENTION (docs/35 §S2 D-A): the runtime treats
            // config_.Vg_bias as the NET grid-cathode bias at rest — the
            // cathode enters the live grid only as a DEVIATION (deltaVk).
            // The rest solver must match, or it solves a colder-grid
            // operating point than the machine actually runs (on the
            // near-critical 6AS6 screen node, the −Vk_rest ≈ −1 V error
            // mapped to a +15 V screen-voltage error).  Vg3 and the plate
            // target keep the full Vk subtraction — exactly like process().
            const double Vg1 = config_.Vg_bias;
            const double Vg3 = config_.suppressorTieToCathode
                ? 0.0
                : (config_.suppressorBiasVolts - Vk);
            const double VaTarget = std::max(1.0, Vb - Vk);

            double lo = 1.0, hi = VaTarget;
            for (int b = 0; b < 40; ++b)
            {
                Va = 0.5 * (lo + hi);
                const double vg2Probe = config_.pentodeTriodeStrap
                    ? Va : solveScreenNodeQuiescent(Va, Vg1, Vg3);
                const auto probe = pentode_.evaluate(Va, Vg1, vg2Probe, Vg3);
                const double ipProbe = std::max(0.0, probe.Ip);
                const double g = Va + ipProbe * config_.Rp - VaTarget;
                if (g > 0.0) hi = Va; else lo = Va;
                Ip = ipProbe;
                Ig2 = std::max(0.0, probe.Ig2);
                Vg2 = vg2Probe;
            }

            // Self-bias: plate, screen AND control-grid currents all return
            // through Rk (Ig1 is leak-dominated at the negative resting bias,
            // but included for exact charge conservation with process()).
            const double Ig1Rest = std::max(0.0,
                pentode_.evaluate(Va, Vg1,
                    config_.pentodeTriodeStrap ? Va : Vg2, Vg3).Ig1);
            const double VkNew =
                std::max(0.0, (Ip + Ig2 + Ig1Rest) * config_.Rk);
            if (std::abs(VkNew - Vk) < 1.0e-4) { Vk = VkNew; break; }
            Vk = 0.5 * Vk + 0.5 * VkNew;
            if (!std::isfinite(Vk) || !std::isfinite(Ip))
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
            std::max(0.0, Vp_rest_ - Vk_rest_),
            config_.Vg_bias,
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
            const double Vg1Rest = config_.Vg_bias;   // net convention
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

    // Signal-dependent Miller low-pass (docs/04 §3.1–§3.2)
    //   C_in = Cgk + Cgp·(1 + |Av|)
    //   fc   = 1 / (2π · Zsource · C_in)
    //
    // |Av| now comes from the SOLVED operating point: Av = gm·(Rp ∥ rp),
    // refreshed each sample by the plate-node Newton.  This fixes two
    // legacy errors: (1) |Av| used gm·Rp — the same missing-load-line
    // factor of ~7 as the old transfer curve; (2) the program dependence
    // had the wrong sign — docs/04 §3.2: approaching saturation/cutoff
    // the incremental gain DROPS, so C_m shrinks and the top end OPENS,
    // whereas the old heuristic darkened loud passages.
    //
    // Zsource prefers the dynamic value handed in by the chain (the
    // previous stage's instantaneous rp ∥ Rp); the static config value is
    // the fallback for stage 1 / standalone use.
    // Total impedance driving the grid node: the upstream stage's
    // instantaneous output impedance (chain-injected rp∥Rp, or the static
    // config fallback for stage 1) in SERIES with the physical grid stopper.
    // Both the Miller input RC and the grid-conduction loading Newton read
    // this single value so one physical node has one source impedance.
    double gridDriveImpedance() const noexcept
    {
        const double zUp = (dynamicSourceZ_ > 1.0)
            ? dynamicSourceZ_ : config_.sourceImpedance;
        return std::max(1.0, zUp + std::max(0.0, config_.gridStopperR));
    }

    double applyMillerFilter(double x) noexcept
    {
        double avInst = avRest_;
        if (config_.Rp > 1.0)
            avInst = lastGmLoaded_ * rAcEff_
                   / (1.0 + rAcEff_ * lastRpInvLoaded_);
        avSmooth_ += avAlpha_ * (avInst - avSmooth_);

        const double depth = std::clamp(config_.millerSignalDepth, 0.0, 1.0);
        const double avEff = std::max(0.0,
            avRest_ + depth * (avSmooth_ - avRest_));

        const double Cin = config_.tube.Cgk
                         + config_.Cgp_miller * (1.0 + avEff);
        // Source impedance for the Miller RC = upstream Zout + grid stopper
        // (see gridDriveImpedance()).  The stopper's whole purpose is to
        // form this HF-taming RC with the grid input capacitance; the legacy
        // Miller ignored it, so the corner sat far above audio on every
        // preset regardless of the stopper the circuit actually has.
        const double zSrc = gridDriveImpedance();
        const double fc = 1.0 / (2.0 * M_PI * std::max(zSrc, 1.0)
                                 * std::max(Cin, 1.0e-13));

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
    double ltpVpPosLast_ { 0.0 };  ///< LTP per-side plate warm starts
    double ltpVpNegLast_ { 0.0 };
    double ltpVpPosRest_ { 0.0 };
    double ltpVpNegRest_ { 0.0 };

    // Plate / cathode node-solver state (load-line physics)
    double VpLast_   { 0.0 };  ///< CC solved plate voltage (warm start)
    double VpPrev_   { 0.0 };  ///< Cplate backward-Euler cap state
    double VaLast_   { 0.0 };  ///< Pentode plate-cathode warm start
    double pentSlope_{ 10.0 }; ///< Cached pentode-residual secant slope
    double screenAlpha_  { 0.999 };  ///< Screen RC pole (setup constant)
    double invVpNominal_ { 1.0 / 250.0 };
    double cfVkLast_ { 0.0 };  ///< CF solved cathode voltage
    double cfVkPrev_ { 0.0 };  ///< CF bypass-cap backward-Euler state
    double lastGmLoaded_    { 0.0 };  ///< gm at the solved operating point
    double lastRpInvLoaded_ { 0.0 };  ///< 1/rp at the solved operating point
    double avRest_   { 0.0 };  ///< Resting |Av| = gm·(RpEff ∥ rp)
    double avSmooth_ { 0.0 };  ///< ~0.7 ms smoothed instantaneous |Av|
    double avAlpha_  { 0.0 };
    double rAcEff_   { 100.0e3 }; ///< AC plate load Rp ∥ Rg_next (§3.8)
    double dynamicSourceZ_ { 0.0 };   ///< Chain-injected upstream Zout
    double outputMakeup_   { 1.0 };   ///< Legacy-gain calibration factor
    double dcLeakAlpha_    { 0.9999 };///< 0.5 Hz DC tracker (fs-derived)

    // SE-OPT magnetizing coupling (docs/34 §2.2, transformer-loaded CC)
    double extMagDropNorm_ { 0.0 };   ///< Chain-injected OPT drop (norm.)
    double seMagToAmps_    { 0.0 };   ///< dropNorm → primary amps (setup)
    double seMagZRest_     { 0.0 };   ///< rp ∥ Rtot at rest (de-embed)
    double seMagLastA_     { 0.0 };   ///< iMag used this sample [A]

    // Miller feedthrough history (docs/34 §3.5)
    double ftVpPrev_  { 0.0 };
    double ftVpPrev2_ { 0.0 };
    double ftVgPrev_  { 0.0 };
    double ftVgPrev2_ { 0.0 };

    // Reactive-load companion (docs/34 §2.5): setup constants + histories
    bool   loadReactive_ { false };
    double lrBLe_    { 0.0 };   ///< β·Le (voice-coil inductor companion R)
    double lrBCm_    { 0.0 };   ///< β·Cm
    double lrInvBLm_ { 0.0 };   ///< 1/(β·Lm)
    double lrGm_     { 1.0 };   ///< motional node conductance
    double lrRtot_   { 0.0 };   ///< per-sample port resistance
    double lrEL_     { 0.0 };   ///< inductor history EMF
    double lrJC_     { 0.0 };   ///< capacitor history source
    double lrJLm_    { 0.0 };   ///< motional-inductor history source

    // Time-varying states
    double warmupAlpha_    { 0.0 };
    double warmupCurrent_  { 1.0 };
    double warmupTarget_   { 1.0 };
    double millerState_    { 0.0 };
    double outputDC_       { 0.0 };
    double lastIp_         { 0.0 };  ///< Last computed plate current (for PSU)
    double lastScreenCurrent_ { 0.0 };
    double lastGridCurrent_   { 0.0 };  ///< Pentode g1 conduction current
    double screenNodeV_    { 0.0 };
    double gmRest_         { 0.0 };
    double gridChargeFastV_ { 0.0 }; ///< Fast blocking branch [V]
    double gridChargeSlowV_ { 0.0 }; ///< Slow recovery branch [V]
    double gridChargeRestV_ { 0.0 }; ///< Standing grid-leak equilibrium [V]
    double heaterPhase_    { 0.0 };
    double heaterPhaseInc_ { 0.0 };
    double thermalAlpha_   { 0.0 };  ///< IIR coeff for long-τ envelope
    double ipAvgLong_      { 0.0 };  ///< Slow mean-|Ip| envelope [A]
    double slewState_      { 0.0 };  ///< Rate-limited output carry
    std::uint64_t shotRng_ { 0xA5A5A5A5A5A5A5A5ULL };  ///< xorshift64 state
    double pinkB0_ { 0.0 }, pinkB1_ { 0.0 }, pinkB2_ { 0.0 };  ///< 1/f filter

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
    s.gridStopperR = 2.2e3;         // broadcast preamp: small stopper →
                                    // Miller corner stays well above audio
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
    s.gridStopperR = 2.2e3;         // broadcast driver, small stopper
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
    s.gridStopperR = 10.0e3;   // moderate stopper on the distortion input
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
        .sourceImpedance = 1.0e6,      // Hi-Z input
        .gridStopperR = 2.2e3          // modern DI: small grid stopper
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
    s.gridStopperR       = 1.0e3;    // HiFi: minimal stopper, wide open top
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
    s.gridStopperR       = 1.0e3;
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
        .enableCathodeBounce = true,
        .inputVoltageSwing = 12.0,   // moderate grid drive
        .outputGainLinear = 3.0,     // makeup so 300B's contribution shows
        .enableWarmup = true, .warmupTauSeconds = 60.0,  // DHT warms slow
        .enableMillerFilter = true,
        .Cgp_miller = 12.0e-12,                 // 300B has substantial Cgp
        .sourceImpedance = 1.0e3                 // CF buffer output
    };
    // The 300B's plate load IS the (gapped) SE output transformer: idle
    // sits just DCR below the 350 V rail with the full standing current
    // through the primary; Rp is the reflected speaker load the signal
    // swings against — including above-rail flyback on the cutoff half.
    s.plateLoadIsTransformer = true;
    s.plateLoadDcr           = 90.0;
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
    s.gridStopperR         = 1.0e3;             // DHT power triode, small
                                                // stopper (grid-current path)
    // Reflected LOUDSPEAKER load (docs/34 §2.5): motional resonance +
    // voice-coil inductance make the 300B's load line frequency-dependent
    // — the SE-amp-on-a-speaker signature the fixed resistor missed.
    s.plateLoadReactive = true;
    s.loadResonanceHz   = 45.0;
    s.loadResonanceQ    = 1.2;
    s.loadPeakRatio     = 5.0;
    s.loadVcCornerHz    = 2000.0;
    return s;
}

} // namespace presets

} // namespace valvra::dsp
