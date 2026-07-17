// ─────────────────────────────────────────────────────────────────────────────
// test_push_pull_stage.cpp — LTP + class-AB push-pull validation
//
// Three properties anyone using a real hardware push-pull power section
// hears, that competitors' "fake push-pull" plugins miss:
//
//   1) Resting silence — at zero input, both halves of the pair conduct
//      identically and their difference current is zero.  Output sits at
//      DC (or at AC-coupled zero after the high-pass).  No idle hiss.
//
//   2) Even-harmonic suppression in class-A — when input is small enough
//      that both tubes stay conducting, each tube's H2 contribution is
//      identical in amplitude but phase-inverted between the two halves.
//      The OPT primary takes the *difference*, so H2 cancels.  This is
//      WHY push-pull amps measure cleaner at quiet levels than single-
//      ended ones with the same tubes.
//
//   3) Class-AB asymmetric clipping — when input is hot enough to push
//      one tube into cutoff, the difference current goes asymmetric.  H3
//      (and other odd harmonics) jump up above class-A levels.  This is
//      the source of the "British crunch" tone.
//
// Reference: docs/14 §14.4, docs/24 §B.2.
// ─────────────────────────────────────────────────────────────────────────────
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "PushPullStage.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include <vector>

using namespace valvra::dsp;
using Catch::Approx;

namespace {

constexpr double kSampleRate = 48000.0;

// Goertzel single-bin amplitude.  Returns linear magnitude at freq.
double goertzelMagnitude(const std::vector<double>& x, double freq)
{
    const double k = freq * x.size() / kSampleRate;
    const double w = 2.0 * std::numbers::pi * k / x.size();
    const double cosw = std::cos(w);
    const double coeff = 2.0 * cosw;
    double s_prev = 0.0, s_prev2 = 0.0;
    for (double v : x)
    {
        const double s = v + coeff * s_prev - s_prev2;
        s_prev2 = s_prev;
        s_prev  = s;
    }
    const double real = s_prev - s_prev2 * cosw;
    const double imag = s_prev2 * std::sin(w);
    // Normalize so a unit-amplitude sine returns ~1.0 (not 0.5*N).
    return 2.0 * std::sqrt(real * real + imag * imag) / x.size();
}

std::vector<double> render(PushPullStage& pp, double freq, double amp,
                           int numSamples, double Vb = 450.0)
{
    std::vector<double> out;
    out.reserve(static_cast<std::size_t>(numSamples));
    for (int n = 0; n < numSamples; ++n)
    {
        const double t = n / kSampleRate;
        const double x = amp * std::sin(2.0 * std::numbers::pi * freq * t);
        out.push_back(pp.process(x, Vb));
    }
    return out;
}

double rms(const std::vector<double>& v, int startN = 0)
{
    double s = 0.0;
    int count = 0;
    for (std::size_t i = static_cast<std::size_t>(startN); i < v.size(); ++i)
    {
        s += v[i] * v[i];
        ++count;
    }
    return std::sqrt(s / std::max(count, 1));
}

} // namespace

TEST_CASE("PushPullStage: resting state has zero output (matched-pair difference)",
          "[pushpull][rest]")
{
    PushPullStageConfig cfg;
    cfg.tubeAsymmetry = 0.0;        // perfectly matched pair → exact null
    cfg.enableWarmup  = false;       // stay at full gm

    PushPullStage pp;
    pp.setup(cfg, kSampleRate);
    pp.reset(false);

    // Push silence for a while; let DC tracker settle.
    std::vector<double> out;
    out.reserve(8192);
    for (int n = 0; n < 8192; ++n)
        out.push_back(pp.process(0.0, 450.0));

    // After settle, output should be at AC-coupled zero (within float epsilon).
    const double tail = rms(out, 4096);
    REQUIRE(tail < 1.0e-6);
}

TEST_CASE("PushPullStage: class-A region suppresses even harmonics",
          "[pushpull][class-a]")
{
    PushPullStageConfig cfg;
    cfg.tubeAsymmetry = 0.0;          // perfect pair to isolate the cancel
    cfg.enableWarmup  = false;

    PushPullStage pp;
    pp.setup(cfg, kSampleRate);
    pp.reset(false);

    // Small input — well within class-A: drive=0.1 × 28V/unit ≈ 2.8 V
    // peak swing per side, far less than the 36 V bias headroom.
    constexpr int N = 16384;
    auto out = render(pp, 220.0, 0.1, N);

    // Skip warmup section
    constexpr int settleN = 4096;
    std::vector<double> settled(out.begin() + settleN, out.end());

    const double h1 = goertzelMagnitude(settled, 220.0);
    const double h2 = goertzelMagnitude(settled, 440.0);
    const double h3 = goertzelMagnitude(settled, 660.0);

    INFO("h1=" << h1 << " h2=" << h2 << " h3=" << h3);
    REQUIRE(h1 > 1.0e-4);                   // some signal got through
    // H2 should be heavily suppressed relative to H1 — at least 30 dB
    // below.  This is the matched-pair even-harmonic null.
    REQUIRE(20.0 * std::log10(h2 / h1) < -30.0);
}

TEST_CASE("PushPullStage: hot drive pushes class-AB cutoff (more H3)",
          "[pushpull][class-ab]")
{
    PushPullStageConfig cfg;
    cfg.tubeAsymmetry = 0.0;
    cfg.enableWarmup  = false;

    PushPullStage pp;
    pp.setup(cfg, kSampleRate);
    pp.reset(false);

    // Hot drive — pushes well past class-AB cutoff.  drive=2.0 × 28V/unit
    // = 56 V peak swing per side, easily exceeding the 36 V bias.
    constexpr int N = 16384;
    auto out = render(pp, 220.0, 2.0, N);

    constexpr int settleN = 4096;
    std::vector<double> settled(out.begin() + settleN, out.end());

    const double h1 = goertzelMagnitude(settled, 220.0);
    const double h3 = goertzelMagnitude(settled, 660.0);
    const double h5 = goertzelMagnitude(settled, 1100.0);

    INFO("HOT: h1=" << h1 << " h3=" << h3 << " h5=" << h5);
    REQUIRE(h1 > 1.0e-3);
    // Under heavy class-AB drive, H3 should rise to within ~32 dB of H1
    // — a clear signature of asymmetric clipping (a pure linear amp
    // would have H3 well under −50 dBc).  The exact figure depends on
    // bias point and drive headroom; we use a conservative threshold
    // that still excludes anything resembling clean class-A behaviour.
    REQUIRE(20.0 * std::log10(h3 / h1) > -32.0);
}

TEST_CASE("PushPullStage: tube asymmetry leaks small even harmonics",
          "[pushpull][asymmetry]")
{
    // Same drive amplitude with two different asymmetries; the more
    // mismatched pair should leak more H2.

    auto runWithAsym = [](double asym) -> double
    {
        PushPullStageConfig cfg;
        cfg.tubeAsymmetry = asym;
        cfg.enableWarmup  = false;

        PushPullStage pp;
        pp.setup(cfg, kSampleRate);
        pp.reset(false);

        constexpr int N = 16384;
        auto out = render(pp, 220.0, 0.4, N);   // moderate drive
        std::vector<double> settled(out.begin() + 4096, out.end());

        const double h1 = goertzelMagnitude(settled, 220.0);
        const double h2 = goertzelMagnitude(settled, 440.0);
        return (h1 > 1.0e-9) ? (h2 / h1) : 0.0;
    };

    const double matched_h2_ratio = runWithAsym(0.0);
    const double mismatched_h2_ratio = runWithAsym(0.10);  // 10% mismatch

    INFO("matched H2/H1=" << matched_h2_ratio
         << " mismatched H2/H1=" << mismatched_h2_ratio);
    // Mismatched pair must produce *more* H2 than the matched one.
    REQUIRE(mismatched_h2_ratio > matched_h2_ratio);
}

TEST_CASE("PushPullStage: tail-coupled solver converges (stays finite)",
          "[pushpull][solver]")
{
    PushPullStageConfig cfg;
    cfg.solveTailCoupling = true;
    cfg.tailSolverIters   = 2;

    PushPullStage pp;
    pp.setup(cfg, kSampleRate);
    pp.reset(false);

    // Massive drive pulse — the kind of input a Newton-Raphson solver
    // could potentially diverge on if not warm-started.  Output must
    // stay finite throughout.
    for (int n = 0; n < 8192; ++n)
    {
        const double sweep = (n < 4096) ? 0.0 : 5.0;  // sudden hot drive
        const double t = n / kSampleRate;
        const double x = sweep * std::sin(2.0 * std::numbers::pi * 440.0 * t);
        const double y = pp.process(x, 450.0);
        REQUIRE(std::isfinite(y));
    }
}

TEST_CASE("PushPullStage: setTubeParams hot-update changes character "
          "without state click",
          "[pushpull][reroll]")
{
    PushPullStageConfig cfg;
    cfg.tubeAsymmetry = 0.03;
    cfg.enableWarmup  = false;

    PushPullStage pp;
    pp.setup(cfg, kSampleRate);
    pp.reset(false);

    // Render a stable signal first.
    constexpr int Npre = 2048;
    auto pre = render(pp, 330.0, 0.4, Npre);

    // Hot-swap the tube model.
    auto altered = params::kEL34_TriodeStrapped;
    altered.G  *= 1.10;     // 10% more perveance
    altered.mu *= 0.92;     // 8% less mu — together = different character
    pp.setTubeParams(altered);

    // Render the same signal again — should NOT be identical to pre.
    constexpr int Npost = 2048;
    auto post = render(pp, 330.0, 0.4, Npost);

    // Must remain finite (the hot-update should never destabilise state).
    for (double v : post) REQUIRE(std::isfinite(v));

    // Character should shift (RMS or harmonic ratio differs).  We use
    // RMS as a robust scalar proxy.
    const double r_pre  = rms(pre, 1024);
    const double r_post = rms(post, 1024);
    INFO("pre RMS=" << r_pre << " post RMS=" << r_post);
    REQUIRE(std::abs(r_pre - r_post) > 1.0e-4);
}

TEST_CASE("PushPullStage: LTP phase-splitter path diverges from ideal split",
          "[pushpull][ltp]")
{
    PushPullStageConfig ideal;
    ideal.enableWarmup = false;
    ideal.useLtpPhaseSplitter = false;

    PushPullStageConfig ltp = ideal;
    ltp.useLtpPhaseSplitter = true;
    ltp.ltpTubeMismatch = 0.03;
    ltp.ltpPlateRRatio = 0.90;
    ltp.ltpCommonModeLeak = 0.05;
    ltp.ltpToPowerGridGain = 0.22;

    PushPullStage ppIdeal;
    ppIdeal.setup(ideal, kSampleRate);
    ppIdeal.reset(false);

    PushPullStage ppLtp;
    ppLtp.setup(ltp, kSampleRate);
    ppLtp.reset(false);

    constexpr int N = 16384;
    std::vector<double> idealOut;
    std::vector<double> ltpOut;
    idealOut.reserve(static_cast<std::size_t>(N));
    ltpOut.reserve(static_cast<std::size_t>(N));

    for (int n = 0; n < N; ++n)
    {
        const double t = n / kSampleRate;
        const double x = 0.45 * std::sin(2.0 * std::numbers::pi * 220.0 * t);
        idealOut.push_back(ppIdeal.process(x, 450.0));
        ltpOut.push_back(ppLtp.process(x, 450.0));
    }

    // Compare post-settle RMS: finite-tail LTP + branch mismatch should
    // produce a measurably different transfer than the ideal anti-phase
    // split path.
    const double rIdeal = rms(idealOut, 4096);
    const double rLtp   = rms(ltpOut, 4096);
    INFO("ideal RMS=" << rIdeal << " ltp RMS=" << rLtp);
    REQUIRE(std::abs(rIdeal - rLtp) > 1.0e-4);
}

TEST_CASE("PushPullStage: coupled OPT is calibration-neutral in class A",
          "[pushpull][coupledopt]")
{
    // The differential load line v_half = (Raa/4)·(i1 − i2) reduces
    // EXACTLY to the legacy per-side Raa/2 line under symmetric class-A
    // drive, so a matched pair at moderate drive must produce (nearly)
    // identical output with the coupling on or off — the class-AB physics
    // must not re-voice the linear region (docs/34 §2.3).
    auto renderCfg = [&](bool coupled) {
        PushPullStageConfig cfg;
        cfg.tubeAsymmetry = 0.0;            // matched pair: exact symmetry
        cfg.enablePowerGridConduction = false;
        cfg.coupleOPTDifferential = coupled;
        cfg.enableWarmup = false;
        PushPullStage pp;
        pp.setup(cfg, kSampleRate);
        return render(pp, 220.0, 0.4, 9600); // well inside class A
    };

    const auto on  = renderCfg(true);
    const auto off = renderCfg(false);

    double diffSq = 0.0, refSq = 0.0;
    for (std::size_t i = 4800; i < on.size(); ++i)
    {
        diffSq += (on[i] - off[i]) * (on[i] - off[i]);
        refSq  += off[i] * off[i];
    }
    const double relErr = std::sqrt(diffSq / std::max(refSq, 1.0e-30));
    INFO("class-A relative deviation = " << relErr);
    REQUIRE(relErr < 0.02);
}

TEST_CASE("PushPullStage: coupled OPT flies the cut-off plate above the rail",
          "[pushpull][coupledopt][flyback]")
{
    // In class-AB the cut-off side's half-primary is driven by the
    // SURVIVOR's swing through the shared flux: its plate rides above the
    // rail following the partner (dynamic flyback), beyond the fixed
    // vOpen ceiling the legacy independent load lines allowed.
    auto maxPlate = [&](bool coupled) {
        PushPullStageConfig cfg;
        cfg.coupleOPTDifferential = coupled;
        cfg.enablePowerGridConduction = false;
        cfg.enableWarmup = false;
        PushPullStage pp;
        pp.setup(cfg, kSampleRate);
        double vMax = 0.0;
        const double Vb = 450.0;
        for (int n = 0; n < 9600; ++n)
        {
            const double t = n / kSampleRate;
            const double x = 3.0 * std::sin(2.0 * std::numbers::pi * 110.0 * t);
            pp.process(x, Vb);
            if (n > 4800)
                vMax = std::max(vMax, std::max(pp.posPlateVoltage(),
                                               pp.negPlateVoltage()));
        }
        return vMax;
    };

    const double coupledMax = maxPlate(true);
    const double legacyMax  = maxPlate(false);
    INFO("coupled max plate = " << coupledMax
         << " V, legacy max = " << legacyMax << " V");
    REQUIRE(std::isfinite(coupledMax));
    REQUIRE(coupledMax > 450.0 + 30.0);        // well above the rail
    REQUIRE(coupledMax > legacyMax + 15.0);    // beyond the legacy ceiling
}

TEST_CASE("PushPullStage: power-grid blocking charges on overdrive only",
          "[pushpull][blocking]")
{
    // Overdriven positive grid peaks must accumulate blocking charge on
    // the coupling caps (crossover shift); mix-level drive must not
    // (docs/34 §2.4).  After the overdrive stops the charge bleeds off
    // through the grid leak.
    PushPullStageConfig cfg;
    cfg.enablePowerGridConduction = true;
    cfg.enableWarmup = false;
    PushPullStage pp;
    pp.setup(cfg, kSampleRate);

    // Mix-level: Vgk stays ~30 V below conduction — charge stays at the
    // (microvolt) grid-leak equilibrium.
    render(pp, 220.0, 0.5, 9600);
    const double chargeMix = pp.blockingChargeVolts();
    INFO("mix-level blocking charge = " << chargeMix << " V");
    REQUIRE(chargeMix < 0.1);

    // Overdrive: ±42 V grid swings push past conduction each cycle.
    // The charge settles at the self-consistent equilibrium where the
    // blocked peaks just graze conduction — for this cathode-biased pair
    // at 4× full-scale drive that lands in the tens of volts (the tail
    // voltage collapses with the blocked currents, warming Vgk back).
    // The upper bound pins the NEGATIVE-feedback direction: a sign-flipped
    // charge (adding to the drive) would run away unboundedly within the
    // render window, blowing far past the physical equilibrium.
    const auto hot = render(pp, 220.0, 6.0, 14400);
    for (double v : hot) REQUIRE(std::isfinite(v));
    const double chargeHot = pp.blockingChargeVolts();
    INFO("overdrive blocking charge = " << chargeHot << " V");
    REQUIRE(chargeHot > 0.5);
    REQUIRE(chargeHot < 150.0);

    // Recovery: silence bleeds the charge through the grid leak.
    render(pp, 220.0, 0.0, 14400);   // 0.3 s of silence
    const double chargeRec = pp.blockingChargeVolts();
    INFO("post-silence blocking charge = " << chargeRec << " V");
    REQUIRE(chargeRec < 0.5 * chargeHot);
}

TEST_CASE("PushPullStage: magnetizing injection sign pins the plate response",
          "[pushpull][magcoupling][sign]")
{
    // A positive normalized magnetizing drop must convert to a positive
    // primary differential current: the POS side sources it (its plate
    // node relieved upward), the NEG side sinks it.  Pins the sign of
    // setMagnetizingDropNorm's conversion AND the ±half KCL plumbing —
    // a swapped iMagPos/iMagNeg or a flipped conversion inverts both
    // inequalities (docs/34 §2.2).
    PushPullStageConfig cfg;
    cfg.enableWarmup = false;
    cfg.enablePowerGridConduction = false;
    PushPullStage pp;
    pp.setup(cfg, kSampleRate);

    for (int n = 0; n < 2000; ++n) pp.process(0.0, 450.0);
    const double posRest = pp.posPlateVoltage();
    const double negRest = pp.negPlateVoltage();

    for (int n = 0; n < 200; ++n)
    {
        pp.setMagnetizingDropNorm(0.2);
        pp.process(0.0, 450.0);
    }
    const double posShift = pp.posPlateVoltage() - posRest;
    const double negShift = pp.negPlateVoltage() - negRest;
    INFO("pos plate shift = " << posShift << " V, neg = " << negShift << " V");
    REQUIRE(posShift > 0.3);
    REQUIRE(negShift < -0.3);
}

TEST_CASE("PushPullStage: ultralinear tap sits between triode and pentode",
          "[pushpull][ultralinear]")
{
    // docs/14 / docs/34 v2 — with the screen on a 43% OPT tap the
    // effective plate resistance must land ABOVE the triode-strap value
    // (tap = 1, today's default, bit-compatible) and rise monotonically
    // as the tap fraction shrinks toward pentode behaviour.
    auto zoutAt = [&](double tap) {
        PushPullStageConfig cfg;
        cfg.ulTapRatio = tap;
        cfg.enableWarmup = false;
        cfg.enablePowerGridConduction = false;
        PushPullStage pp;
        pp.setup(cfg, kSampleRate);
        render(pp, 220.0, 0.3, 4800);      // settle at moderate drive
        return pp.lastOutputImpedance();
    };

    const double zTriode = zoutAt(1.0);
    const double zUl     = zoutAt(0.43);
    const double zStiff  = zoutAt(0.15);
    INFO("Zout: triode=" << zTriode << " UL=" << zUl
         << " near-pentode=" << zStiff);
    REQUIRE(zUl > 1.2 * zTriode);
    REQUIRE(zStiff > zUl);

    // And the UL pair still renders finite, bounded audio at hot drive.
    PushPullStageConfig cfg;
    cfg.ulTapRatio = 0.43;
    cfg.enableWarmup = false;
    PushPullStage pp;
    pp.setup(cfg, kSampleRate);
    const auto out = render(pp, 220.0, 2.0, 9600);
    for (double v : out) REQUIRE(std::isfinite(v));
}
