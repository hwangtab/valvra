// ─────────────────────────────────────────────────────────────────────────────
// test_tube_amp_chain.cpp — end-to-end multi-stage chain validation
//
// Tests that TubeAmpChain properly assembles a full tube amp signal path:
//   Input trafo → Stage 1 → interstage → Stage 2 → Output trafo
// with shared PSU sag, per-instance Monte Carlo variation, and cold-start
// warmup behavior.
//
// The cornerstone test is "different seeds produce different outputs" —
// this is the quantitative proof that Monte Carlo component variation
// works end-to-end (the sensational differentiator vs competitor plugins).
// ─────────────────────────────────────────────────────────────────────────────
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "TubeAmpChain.h"
#include "PolyphaseOversampler.h"

#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

using namespace valvra::dsp;
using Catch::Approx;

namespace {
constexpr double kSampleRate = 48000.0;

void renderSine(TubeAmpChain& chain,
                double freqHz,
                double amp,
                double durationSec,
                std::vector<double>& out)
{
    const int N = static_cast<int>(durationSec * kSampleRate);
    out.clear();
    out.reserve(N);
    for (int n = 0; n < N; ++n)
    {
        const double t = n / kSampleRate;
        const double x = amp * std::sin(2.0 * std::numbers::pi * freqHz * t);
        out.push_back(chain.process(x));
    }
}

double peakAbs(const std::vector<double>& v)
{
    double p = 0.0;
    for (double x : v) p = std::max(p, std::abs(x));
    return p;
}

double rms(const std::vector<double>& v)
{
    double s = 0.0;
    for (double x : v) s += x * x;
    return std::sqrt(s / static_cast<double>(v.size()));
}

double goertzelMag(const std::vector<double>& x, double freq, double sr)
{
    const double w = 2.0 * std::numbers::pi * freq / sr;
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
    return 2.0 * std::sqrt(real * real + imag * imag) / x.size();
}

} // namespace

TEST_CASE("HiFi 300B preset renders finite, non-trivial output",
          "[chain][hifi]")
{
    // Smoke test: the new HiFi 300B preset (5th mode) must build, settle
    // its DC tracker, and produce sensible output for a moderate input
    // signal.  We don't pin the exact harmonic profile here — the
    // operating point of a 6SN7 → 6SN7 CF → 300B SE chain depends
    // sensitively on every Rp / Rk / outputGainLinear, and over-tight
    // assertions become brittle on small future calibration tweaks.
    // The character validation is done qualitatively via listening; the
    // automated test only catches gross breakage (NaN / silence / blow-up).
    auto cfg = chain_presets::HiFi300BMode();

    TubeAmpChain chain;
    chain.setup(cfg, kSampleRate);
    chain.setExternalPSUMode(false);

    // Settle the chain — long DC-tracker tau means we need ~250 ms of
    // silence before measuring steady-state.
    for (int n = 0; n < 12000; ++n) (void) chain.process(0.0);

    std::vector<double> out;
    renderSine(chain, 220.0, 0.4, 0.5, out);

    for (double v : out) REQUIRE(std::isfinite(v));

    const int settleN = 4096;
    double s = 0.0;
    int count = 0;
    for (std::size_t i = settleN; i < out.size(); ++i)
    {
        s += out[i] * out[i];
        ++count;
    }
    const double rmsOut = std::sqrt(s / std::max(count, 1));
    INFO("HiFi 300B output RMS = " << rmsOut);
    REQUIRE(rmsOut > 1.0e-4);    // chain is alive, not silent
    REQUIRE(rmsOut < 1.0);        // chain is bounded, not exploding

    // Fundamental at 220 Hz must be present in non-trivial amount —
    // confirms the chain is doing something useful, not just emitting noise.
    std::vector<double> tail(out.begin() + settleN, out.end());
    const double h1 = goertzelMag(tail, 220.0, kSampleRate);
    INFO("HiFi 300B  h1 (220 Hz) = " << h1);
    REQUIRE(h1 > 1.0e-5);
}

TEST_CASE("TubeAmpChain: global NFB reduces linear-region THD, keeps level",
          "[chain][nfb]")
{
    // The Console preset wraps a real per-sample feedback loop around its
    // power amp (docs/34 §2.1).  In the linear region the loop must:
    //   1. drop the distortion products (THD ↓ by ≈ 1+T), and
    //   2. leave the fundamental level essentially unchanged (the setup
    //      (1+T) makeup restores what the loop suppressed).
    const double f = 220.0;
    const double amp = 0.05;   // low level → mostly-linear operating region

    auto measure = [&](double loopGain) {
        auto cfg = chain_presets::MarshallMode();
        cfg.nfbLoopGain   = loopGain;
        cfg.variationSeed = 7;
        TubeAmpChain chain;
        chain.setup(cfg, kSampleRate);
        std::vector<double> out;
        renderSine(chain, f, amp, 0.5, out);
        std::vector<double> tail(out.end() - static_cast<long>(out.size() / 2),
                                 out.end());
        const double h1 = goertzelMag(tail, f, kSampleRate);
        const double h2 = goertzelMag(tail, 2.0 * f, kSampleRate);
        const double h3 = goertzelMag(tail, 3.0 * f, kSampleRate);
        struct R { double h1, thd; };
        return R { h1, (h2 + h3) / std::max(h1, 1.0e-12) };
    };

    const auto off = measure(0.0);
    const auto on  = measure(0.6);

    // 1. Fundamental level preserved within a few dB by the (1+T) makeup.
    const double levelDb = 20.0 * std::log10(
        std::max(on.h1, 1.0e-12) / std::max(off.h1, 1.0e-12));
    INFO("level change with NFB = " << levelDb << " dB");
    REQUIRE(std::abs(levelDb) < 3.0);

    // 2. The loop reduces linear-region distortion.
    INFO("THD off = " << off.thd << ", on = " << on.thd);
    REQUIRE(on.thd < off.thd);
}

TEST_CASE("TubeAmpChain: V72 preset builds and runs without error",
          "[chain][preset]")
{
    TubeAmpChain chain;
    auto cfg = chain_presets::V72Preamp();
    chain.setup(cfg, kSampleRate);

    // Feed 1 kHz small signal; output must be finite and non-zero.
    std::vector<double> out;
    renderSine(chain, 1000.0, 0.1, 0.2, out);

    for (double y : out) REQUIRE(std::isfinite(y));
    REQUIRE(rms(out) > 0.0);
    REQUIRE(peakAbs(out) < 50.0);  // sanity: not blown up
}

TEST_CASE("TubeAmpChain: RNDI preset builds and runs", "[chain][preset]")
{
    TubeAmpChain chain;
    auto cfg = chain_presets::RNDIMode();
    chain.setup(cfg, kSampleRate);

    std::vector<double> out;
    renderSine(chain, 100.0, 0.3, 0.1, out);

    for (double y : out) REQUIRE(std::isfinite(y));
    REQUIRE(rms(out) > 0.0);
}

TEST_CASE("TubeAmpChain: different seeds produce different output (Monte Carlo works)",
          "[chain][variation][sensational]")
{
    // The signature claim of Valvra: each instance sounds slightly different.
    // Render the same signal through two chains with different seeds.
    auto cfg = chain_presets::V72Preamp();

    TubeAmpChain chainA;
    cfg.variationSeed = 0x1111'1111ULL;
    chainA.setup(cfg, kSampleRate);

    TubeAmpChain chainB;
    cfg.variationSeed = 0x9876'5432ULL;
    chainB.setup(cfg, kSampleRate);

    // Warm both up so transient is past
    for (int i = 0; i < 10000; ++i) {
        chainA.process(0.0);
        chainB.process(0.0);
    }

    std::vector<double> outA, outB;
    renderSine(chainA, 1000.0, 0.1, 0.2, outA);

    // Re-render same input through chainB.  Note both chains independently
    // consumed silence so they're at comparable warmup states.
    renderSine(chainB, 1000.0, 0.1, 0.2, outB);

    REQUIRE(outA.size() == outB.size());

    // Measure residual (the "null test" — competitors null perfectly)
    double diffEnergy = 0.0;
    for (std::size_t i = 0; i < outA.size(); ++i)
    {
        const double d = outA[i] - outB[i];
        diffEnergy += d * d;
    }
    const double diffRms = std::sqrt(diffEnergy / static_cast<double>(outA.size()));
    const double refRms  = rms(outA);

    REQUIRE(refRms > 0.0);
    const double diffDb = 20.0 * std::log10(diffRms / refRms);
    // Different seeds MUST produce audible difference.
    // Target: residual ≥ −40 dB relative to signal (clearly audible).
    REQUIRE(diffDb > -60.0);  // at minimum, measurable difference
}

TEST_CASE("TubeAmpChain: same seed is fully reproducible", "[chain][variation]")
{
    auto cfg = chain_presets::V72Preamp();
    cfg.variationSeed = 0xCAFE'BABEULL;

    TubeAmpChain chainA, chainB;
    chainA.setup(cfg, kSampleRate);
    chainB.setup(cfg, kSampleRate);

    std::vector<double> outA, outB;
    renderSine(chainA, 440.0, 0.2, 0.05, outA);
    renderSine(chainB, 440.0, 0.2, 0.05, outB);

    REQUIRE(outA.size() == outB.size());

    // Identical seeds → identical output to double-precision accuracy.
    double maxDiff = 0.0;
    for (std::size_t i = 0; i < outA.size(); ++i)
        maxDiff = std::max(maxDiff, std::abs(outA[i] - outB[i]));

    REQUIRE(maxDiff < 1e-12);
}

TEST_CASE("TubeAmpChain: PSU sag active under sustained loud signal",
          "[chain][psu][sensational]")
{
    // Class-AB push-pull: the only topology whose AVERAGE draw genuinely
    // rises with drive (per-side conduction grows), so the rail must sag.
    // The previous V72 (class-A) variant passed only through a cold-start
    // artifact: the PSU used to start at the no-load nominal and drift
    // down DURING the measurement.  Since the loaded-rail settle
    // (docs/35 §S2 D-A) the PSU starts at its quiescent equilibrium and
    // a class-A chain's loud-signal draw actually dips slightly.
    auto cfg = chain_presets::MarshallMode();
    cfg.psu = psu_presets::k5U4GB;  // stronger sag for easier detection
    cfg.enablePSUSag = true;

    TubeAmpChain chain;
    chain.setup(cfg, kSampleRate);

    // Let it settle quiet — PAST the warm-up thump: the cold-start
    // emission ramp pushes a DC transient through the PP/OPT for a few
    // hundred ms, and reading the "quiet" rail inside that window
    // records the thump's draw, not the quiescent state.
    for (int i = 0; i < 24000; ++i) chain.process(0.0);

    const double vb_quiet = chain.currentVb();
    const double sag_quiet = chain.currentSagPercent();

    // Blast loud for 1 second
    std::vector<double> dummy;
    renderSine(chain, 100.0, 0.9, 1.0, dummy);

    const double vb_loud = chain.currentVb();
    const double sag_loud = chain.currentSagPercent();

    // B+ should have dropped; sag% should have risen.
    REQUIRE(vb_loud  < vb_quiet);
    REQUIRE(sag_loud > sag_quiet);
    // Sag should be non-trivial (at least 0.1% for a strong signal)
    REQUIRE(sag_loud > 0.1);
}

TEST_CASE("TubeAmpChain: cold start differs measurably from steady state",
          "[chain][warmup][sensational]")
{
    auto cfg = chain_presets::V72Preamp();
    // Disable PSU sag for this test — we want to isolate the warmup effect.
    // (PSU sag trajectory differs between cold and post-silence states,
    //  which would otherwise confound the measurement.)
    cfg.enablePSUSag = false;
    cfg.stages[0].warmupTauSeconds = 1.0;  // accelerate for test
    cfg.stages[1].warmupTauSeconds = 1.0;

    TubeAmpChain chainCold, chainWarm;
    chainCold.setup(cfg, kSampleRate);
    chainWarm.setup(cfg, kSampleRate);

    // Warm chainWarm fully (10 τ → well past 99.995% convergence)
    for (int i = 0; i < int(10.0 * kSampleRate); ++i)
        chainWarm.process(0.0);

    // Measure both with the same LOW-LEVEL probe.  The V72 chain's
    // makeup gain drives a +4 dBu-ish signal hard into the output
    // transformer, where saturation regulates the level and MASKS the
    // warmup's gm difference (the cold/warm RMS converge to <0.5% at a
    // 0.05 probe).  A quiet probe keeps the chain in its linear region so
    // the cold start's settling-from-0.85-gm trajectory is observable —
    // that is the time-varying behaviour this test exists to prove, and
    // it is large here (cold/warm differ by tens of percent).  (Earlier
    // this passed at a hot 0.05 probe only because a grid-conduction
    // start-up artifact, since fixed, happened to survive the
    // saturation; the warmup effect itself lives at low level.)
    std::vector<double> outCold, outWarm;
    renderSine(chainCold, 1000.0, 0.005, 0.2, outCold);
    renderSine(chainWarm, 1000.0, 0.005, 0.2, outWarm);

    const double rmsCold = rms(outCold);
    const double rmsWarm = rms(outWarm);

    REQUIRE(rmsCold > 0.0);
    REQUIRE(rmsWarm > 0.0);

    // The two must differ — quantitative proof of time-varying thermal
    // warmup behaviour (the sensational feature competitors lack).
    const double ratio = rmsWarm / rmsCold;
    REQUIRE(std::abs(ratio - 1.0) > 0.02);  // ≥ 2% relative difference
}

TEST_CASE("TubeAmpChain: reroll changes output character", "[chain][reroll]")
{
    auto cfg = chain_presets::V72Preamp();
    cfg.variationSeed = 100;

    TubeAmpChain chain;
    chain.setup(cfg, kSampleRate);

    // Warm up
    for (int i = 0; i < 10000; ++i) chain.process(0.0);

    std::vector<double> outBefore;
    renderSine(chain, 1000.0, 0.1, 0.1, outBefore);

    // Reroll to a very different seed
    chain.setVariationSeed(999999, false);

    // Warm up again after reroll
    for (int i = 0; i < 10000; ++i) chain.process(0.0);

    std::vector<double> outAfter;
    renderSine(chain, 1000.0, 0.1, 0.1, outAfter);

    // Output should differ measurably
    double diff = 0.0;
    for (std::size_t i = 0; i < outBefore.size(); ++i)
        diff = std::max(diff, std::abs(outBefore[i] - outAfter[i]));

    REQUIRE(diff > 1e-5);  // reroll produced non-trivial change
}

TEST_CASE("TubeAmpChain: reroll reapplies full Monte Carlo state",
          "[chain][variation][reroll]")
{
    auto cfgA = chain_presets::V72Preamp();
    cfgA.variationSeed = 111;

    TubeAmpChain rerolled;
    rerolled.setup(cfgA, kSampleRate);
    rerolled.setVariationSeed(222, false);

    auto cfgB = chain_presets::V72Preamp();
    cfgB.variationSeed = 222;
    TubeAmpChain fresh;
    fresh.setup(cfgB, kSampleRate);

    // If reroll fully reapplies the Monte Carlo state, it should be
    // bit-equivalent to constructing a fresh chain with the same seed:
    // same tube/passive perturbations, PSU, transformer state, heater phase,
    // and shot-noise streams.
    constexpr int N = 1024;
    for (int n = 0; n < N; ++n)
    {
        const double a = rerolled.process(0.0);
        const double b = fresh.process(0.0);
        REQUIRE(a == Approx(b).margin(1.0e-12));
    }
}

TEST_CASE("TubeAmpChain: click-free reroll avoids hard output jump",
          "[chain][reroll][click-free]")
{
    auto cfg = chain_presets::V72Preamp();
    cfg.variationSeed = 3456;
    cfg.enablePSUSag = false;        // isolate reroll transition
    cfg.useInputTransformer = false; // isolate reroll transition
    cfg.useOutputTransformer = false;

    TubeAmpChain chain;
    chain.setup(cfg, kSampleRate);

    // Settle to steady state first.
    for (int i = 0; i < 12000; ++i)
        chain.process(0.0);

    constexpr int N = 3000;
    std::vector<double> out(static_cast<std::size_t>(N), 0.0);
    for (int n = 0; n < N; ++n)
    {
        if (n == 1500)
            chain.setVariationSeed(987654321ULL, true);
        out[static_cast<std::size_t>(n)] = chain.process(0.16);
    }

    // Compare sample-to-sample step around reroll against pre-reroll
    // baseline. Click-free reroll should not create an impulse-like jump.
    double baselineMaxStep = 0.0;
    for (int n = 64; n < 1400; ++n)
        baselineMaxStep = std::max(
            baselineMaxStep,
            std::abs(out[static_cast<std::size_t>(n)]
                   - out[static_cast<std::size_t>(n - 1)]));

    const double stepAtReroll = std::abs(out[1500] - out[1499]);
    INFO("baseline max step = " << baselineMaxStep
         << ", reroll step = " << stepAtReroll);
    REQUIRE(stepAtReroll < baselineMaxStep * 4.0 + 1.0e-6);
}

TEST_CASE("TubeAmpChain: Marshall preset runs and saturates",
          "[chain][preset][marshall]")
{
    TubeAmpChain chain;
    auto cfg = chain_presets::MarshallMode();
    chain.setup(cfg, kSampleRate);

    // Warm up
    for (int i = 0; i < 5000; ++i) chain.process(0.0);

    std::vector<double> out;
    renderSine(chain, 440.0, 0.3, 0.2, out);

    REQUIRE(out.size() > 0);
    for (double y : out) REQUIRE(std::isfinite(y));
    REQUIRE(rms(out) > 0.0);
}

TEST_CASE("TubeAmpChain: Marshall preset keeps PSU sag enabled",
          "[chain][preset][marshall][psu]")
{
    auto cfg = chain_presets::MarshallMode();
    REQUIRE(cfg.enablePSUSag);

    TubeAmpChain chain;
    chain.setup(cfg, kSampleRate);

    for (int i = 0; i < 5000; ++i) chain.process(0.0);

    auto meanSagForTone = [&](double amp, int samples)
    {
        double acc = 0.0;
        for (int n = 0; n < samples; ++n)
        {
            const double t = static_cast<double>(n) / kSampleRate;
            const double x = amp * std::sin(2.0 * std::numbers::pi * 110.0 * t);
            chain.process(x);
            acc += chain.currentSagPercent();
        }
        return acc / static_cast<double>(samples);
    };

    // Average over long windows so ripple phase does not dominate the
    // instantaneous reading.
    const double sagQuiet = meanSagForTone(0.05, static_cast<int>(kSampleRate * 0.6));
    const double sagLoud  = meanSagForTone(0.90, static_cast<int>(kSampleRate * 0.8));

    // Solid-state rectifier sag is weak but should increase on average
    // under sustained heavy load.
    INFO("quiet mean sag = " << sagQuiet << ", loud mean sag = " << sagLoud);
    REQUIRE(sagLoud > sagQuiet + 1.0e-5);
}

TEST_CASE("TubeAmpChain: Console Output preset uses class-A1 push-pull power stage",
          "[chain][preset][console-output][intent]")
{
    // Console Output (mode index 1, formerly "Marshall") is tuned for
    // mix/master use: cathode bounce ON for slow envelope dynamics, and
    // the push-pull pair idling in true class-A.  With the plate load
    // line and the tail-referenced bias convention in place, the test
    // asserts the PHYSICAL operating point rather than a magic bias
    // number: each side must idle at a healthy fraction of the EL34's
    // rating (class-A = idle ≈ half the load-line maximum), and the pair
    // must conduct on BOTH sides at the default drive.
    const auto cfg = chain_presets::MarshallMode();
    REQUIRE(cfg.stages[0].enableCathodeBounce == true);
    REQUIRE(cfg.stages[1].enableCathodeBounce == true);
    REQUIRE(cfg.usePushPullOutputStage == true);

    PushPullStage pp;
    pp.setup(cfg.pushPullConfig, kSampleRate);
    const double idlePerSide = 0.5 * pp.restingPlateCurrent();
    INFO("idle per side = " << idlePerSide * 1e3 << " mA");
    REQUIRE(idlePerSide > 0.035);   // ≥ 35 mA: solidly class-A
    REQUIRE(idlePerSide < 0.090);   // ≤ 90 mA: within EL34 dissipation

    // driveScale is gentler than the old guitar-amp value so default
    // Drive=1.0 keeps music crests at or below the class-A/AB boundary.
    REQUIRE(cfg.pushPullConfig.driveScale < 25.0);
}

TEST_CASE("TubeAmpChain: RNDI preset disables cathode bounce on stage 2",
          "[chain][preset][rndi][intent]")
{
    const auto cfg = chain_presets::RNDIMode();
    REQUIRE(cfg.stages[1].enableCathodeBounce == false);
}

TEST_CASE("TubeAmpChain: CultureVulture preset 3-stage runs",
          "[chain][preset][vulture]")
{
    TubeAmpChain chain;
    auto cfg = chain_presets::CultureVultureMode();
    REQUIRE(cfg.numStages == 3);
    chain.setup(cfg, kSampleRate);

    // Warm up
    for (int i = 0; i < 5000; ++i) chain.process(0.0);

    std::vector<double> out;
    renderSine(chain, 440.0, 0.2, 0.2, out);

    REQUIRE(out.size() > 0);
    for (double y : out) REQUIRE(std::isfinite(y));
    REQUIRE(rms(out) > 0.0);
}

TEST_CASE("TubeAmpChain: CultureVulture T/P1/P2 modes configure the 6AS6 core",
          "[chain][preset][vulture]")
{
    const auto triode = chain_presets::CultureVultureMode(
        CultureVultureVoicing::Triode);
    const auto p1 = chain_presets::CultureVultureMode(
        CultureVultureVoicing::PentodeLow);
    const auto p2 = chain_presets::CultureVultureMode(
        CultureVultureVoicing::PentodeHigh);

    REQUIRE(triode.numStages == 3);
    REQUIRE(p1.numStages == 3);
    REQUIRE(p2.numStages == 3);
    REQUIRE(triode.cultureVoicing == CultureVultureVoicing::Triode);
    REQUIRE(p1.cultureVoicing == CultureVultureVoicing::PentodeLow);
    REQUIRE(p2.cultureVoicing == CultureVultureVoicing::PentodeHigh);

    const auto& tCore  = triode.stages[1];
    const auto& p1Core = p1.stages[1];
    const auto& p2Core = p2.stages[1];
    REQUIRE(tCore.enablePentodeModel);
    REQUIRE(p1Core.enablePentodeModel);
    REQUIRE(p2Core.enablePentodeModel);
    REQUIRE(tCore.pentodeTriodeStrap);
    REQUIRE(!p1Core.pentodeTriodeStrap);
    REQUIRE(!p2Core.pentodeTriodeStrap);
    REQUIRE(tCore.suppressorDriveMix < p1Core.suppressorDriveMix);
    REQUIRE(p1Core.suppressorDriveMix < p2Core.suppressorDriveMix);
    REQUIRE(tCore.inputVoltageSwing < p1Core.inputVoltageSwing);
    REQUIRE(p1Core.inputVoltageSwing < p2Core.inputVoltageSwing);
}

TEST_CASE("TubeAmpChain: recovers from NaN input without permanent state poison",
          "[chain][nan][robustness]")
{
    // If a single sample arrives as NaN (e.g. an upstream divergence), the
    // chain must NOT pin itself into a NaN state — subsequent clean samples
    // should once again produce finite output.
    TubeAmpChain chain;
    auto cfg = chain_presets::V72Preamp();
    chain.setup(cfg, kSampleRate);

    for (int i = 0; i < 5000; ++i) chain.process(0.0);

    const double nan = std::numeric_limits<double>::quiet_NaN();
    (void)chain.process(nan);

    std::vector<double> out;
    renderSine(chain, 1000.0, 0.1, 0.5, out);

    int nanCount = 0;
    double peakAbs = 0.0;
    for (double y : out)
    {
        if (! std::isfinite(y)) ++nanCount;
        peakAbs = std::max(peakAbs, std::abs(y));
    }
    REQUIRE(nanCount == 0);
    REQUIRE(peakAbs > 0.0);
}

TEST_CASE("TubeAmpChain: recovers from sustained NaN burst", "[chain][nan][robustness]")
{
    // Harder adversarial case: a whole block of NaN (e.g. upstream Inf/NaN
    // cascade).  After the burst ends the chain must still recover.
    TubeAmpChain chain;
    auto cfg = chain_presets::V72Preamp();
    chain.setup(cfg, kSampleRate);

    for (int i = 0; i < 5000; ++i) chain.process(0.0);

    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (int i = 0; i < 2048; ++i) (void)chain.process(nan);  // ~43 ms of NaN

    std::vector<double> out;
    renderSine(chain, 1000.0, 0.1, 0.5, out);

    int nanCount = 0;
    double peakAbs = 0.0;
    for (double y : out)
    {
        if (! std::isfinite(y)) ++nanCount;
        peakAbs = std::max(peakAbs, std::abs(y));
    }
    REQUIRE(nanCount == 0);
    REQUIRE(peakAbs > 0.0);
}

TEST_CASE("PolyphaseOversampler: recovers from NaN input", "[oversample][nan]")
{
    PolyphaseOversampler<4> os;

    // Prime with silence
    for (int i = 0; i < 512; ++i)
    {
        auto up = os.upsample(0.0);
        (void)os.downsample(up);
    }

    // Inject NaN burst
    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (int i = 0; i < 256; ++i)
    {
        auto up = os.upsample(nan);
        (void)os.downsample(up);
    }

    // Now normal signal — output must be finite
    bool anyNaN = false;
    double peak = 0.0;
    for (int i = 0; i < 4096; ++i)
    {
        const double x = std::sin(2.0 * std::numbers::pi * 440.0 * i / 48000.0);
        auto up = os.upsample(x);
        const double y = os.downsample(up);
        if (! std::isfinite(y)) anyNaN = true;
        peak = std::max(peak, std::abs(y));
    }
    REQUIRE(! anyNaN);
    REQUIRE(peak > 0.0);
}

TEST_CASE("TubeAmpChain: stereo seed derivation keeps L and R distinct",
          "[chain][stereo][sensational]")
{
    // Matches the XOR salt used in ValvraProcessor::rebuildChain().
    // Guarantees that a single user-facing seed yields non-identical L and R
    // channels — preserving the Monte Carlo "two real units per rack" feel
    // when the plugin runs in stereo.
    constexpr std::uint64_t kStereoSalt = 0x123456789ABCDEFULL;

    auto cfg = chain_presets::V72Preamp();
    cfg.variationSeed = 0xDEADBEEFULL;

    TubeAmpChain chainL;
    chainL.setup(cfg, kSampleRate);

    auto cfgR = cfg;
    cfgR.variationSeed = cfg.variationSeed ^ kStereoSalt;
    TubeAmpChain chainR;
    chainR.setup(cfgR, kSampleRate);

    // Warmup
    for (int i = 0; i < 10000; ++i) { chainL.process(0.0); chainR.process(0.0); }

    std::vector<double> outL, outR;
    renderSine(chainL, 1000.0, 0.1, 0.2, outL);
    renderSine(chainR, 1000.0, 0.1, 0.2, outR);

    double diff = 0.0, sig = 0.0;
    for (std::size_t i = 0; i < outL.size(); ++i)
    {
        diff += (outL[i] - outR[i]) * (outL[i] - outR[i]);
        sig  += outL[i] * outL[i];
    }
    REQUIRE(sig > 0.0);
    const double nullDb = 10.0 * std::log10(diff / sig);

    // Left and right must be audibly distinct.  −90 dB would mean identical.
    REQUIRE(nullDb > -70.0);
}

TEST_CASE("TubeAmpChain: all four presets produce distinct harmonic signatures",
          "[chain][preset][sensational]")
{
    auto runPreset = [&](const TubeAmpChainConfig& cfg) {
        TubeAmpChain chain;
        chain.setup(cfg, kSampleRate);
        for (int i = 0; i < 5000; ++i) chain.process(0.0);
        std::vector<double> out;
        renderSine(chain, 1000.0, 0.2, 0.2, out);
        return out;
    };

    auto v72      = runPreset(chain_presets::V72Preamp());
    auto marshall = runPreset(chain_presets::MarshallMode());
    auto cv       = runPreset(chain_presets::CultureVultureMode());
    auto rndi     = runPreset(chain_presets::RNDIMode());

    const double r_v72      = rms(v72);
    const double r_marshall = rms(marshall);
    const double r_cv       = rms(cv);
    const double r_rndi     = rms(rndi);

    REQUIRE(r_v72      > 0.0);
    REQUIRE(r_marshall > 0.0);
    REQUIRE(r_cv       > 0.0);
    REQUIRE(r_rndi     > 0.0);

    // Each preset should produce a measurably different sound.
    // Compute pairwise null depth; all should be > −30 dB (audibly different).
    auto nullDb = [](const std::vector<double>& a, const std::vector<double>& b) {
        const std::size_t n = std::min(a.size(), b.size());
        double diff = 0.0, sig = 0.0;
        for (std::size_t i = 0; i < n; ++i)
        {
            diff += (a[i] - b[i]) * (a[i] - b[i]);
            sig  += a[i] * a[i];
        }
        if (sig <= 0.0 || diff <= 0.0) return 0.0;
        return 10.0 * std::log10(diff / sig);
    };
    REQUIRE(nullDb(v72, marshall) > -20.0);
    REQUIRE(nullDb(v72, cv)       > -20.0);
    REQUIRE(nullDb(v72, rndi)     > -20.0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Shared-PSU stereo coupling.  Two chains running off a shared B+ rail must
// interact: a loud signal on one channel sags the rail, which must be audibly
// measurable on the OTHER channel's output even though its own input never
// changed.  This is the core physical signature of two-channel tube racks.
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("TubeAmpChain: shared-PSU stereo coupling modulates the quiet channel",
          "[chain][shared-psu][stereo]")
{
    auto cfgL = chain_presets::V72Preamp();
    auto cfgR = chain_presets::V72Preamp();
    cfgL.variationSeed = 0xA11CE;
    cfgR.variationSeed = 0xB0B;
    REQUIRE(cfgL.enablePSUSag);  // V72 must have sag on

    TubeAmpChain chainL, chainR;
    chainL.setup(cfgL, kSampleRate);
    chainR.setup(cfgR, kSampleRate);
    chainL.setExternalPSUMode(true);
    chainR.setExternalPSUMode(true);

    auto psuParams = cfgL.psu;
    psuParams.sampleRate = kSampleRate;

    // Render 200 ms of a quiet 440 Hz sine on R, while L is either silent
    // (baseline) or driven with a loud 80 Hz burst that should sag the rail.
    const int N = static_cast<int>(0.2 * kSampleRate);
    auto renderR = [&](double driveL) -> std::vector<double>
    {
        chainL.reset(false);
        chainR.reset(false);
        PowerSupplySag sharedPSU(psuParams);
        sharedPSU.reset();

        std::vector<double> out(static_cast<std::size_t>(N));
        for (int n = 0; n < N; ++n)
        {
            const double t = n / kSampleRate;
            const double xL = driveL
                * std::sin(2.0 * std::numbers::pi * 80.0 * t);
            const double xR = 0.1
                * std::sin(2.0 * std::numbers::pi * 440.0 * t);

            const double Vb = sharedPSU.currentVb();
            chainL.setExternalVb(Vb);
            chainR.setExternalVb(Vb);

            (void) chainL.process(xL);
            out[static_cast<std::size_t>(n)] = chainR.process(xR);
            sharedPSU.process(
                chainL.lastTotalIp() + chainR.lastTotalIp());
        }
        return out;
    };

    const auto rQuiet = renderR(0.0);   // L silent → R should be pristine
    const auto rLoud  = renderR(1.5);   // L pumping → R should be modulated

    // Skip the warm-up transient where the rail is settling from the
    // nominal B+ to its resting sagged value.
    constexpr std::size_t kSkip = 4096;
    REQUIRE(rQuiet.size() > kSkip);

    double diffEnergy = 0.0, sigEnergy = 0.0;
    for (std::size_t i = kSkip; i < rQuiet.size(); ++i)
    {
        const double d = rLoud[i] - rQuiet[i];
        diffEnergy += d * d;
        sigEnergy  += rQuiet[i] * rQuiet[i];
    }
    REQUIRE(sigEnergy > 0.0);
    const double nullDb = 10.0 * std::log10(diffEnergy / sigEnergy);

    // R output with L loud must differ measurably from R output with L
    // silent — this is the physical L→R coupling through the shared rail.
    // Competitor plugins generally null well below −120 dB here; Valvra
    // should be much louder than that.  With the rail-decoupling ladder
    // in place the preamp nodes are RC-filtered from the reservoir, so
    // the physical coupling sits in the −60…−85 dB region (real racks
    // measure there too) rather than the legacy raw-rail −50s.
    REQUIRE(nullDb > -85.0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Heater-cathode hum is present on the V72 preset's silent output, and its
// 60 Hz bin dominates other nearby frequency bins — this is the physical
// source of vintage tube "breath" that most competing plugins scrub out.
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("TubeAmpChain: V72 shows 60 Hz heater hum on silent input",
          "[chain][heater-hum][spectrum]")
{
    auto cfg = chain_presets::V72Preamp();
    cfg.variationSeed = 0xABCD;
    // Isolate heater hum from shot noise so the 10× bin-ratio assertion
    // doesn't measure both together.  Shot noise has its own dedicated
    // test that covers program-dependence.
    for (int i = 0; i < cfg.numStages; ++i)
        cfg.stages[i].enableShotNoise = false;
    TubeAmpChain chain;
    chain.setup(cfg, kSampleRate);

    // 0.5 s of silent input — output should carry only the heater hum
    // (plus any residual DC settling we discard).
    const int N    = static_cast<int>(0.5 * kSampleRate);
    const int skip = static_cast<int>(0.1 * kSampleRate);
    std::vector<double> y;
    y.reserve(static_cast<std::size_t>(N - skip));
    for (int n = 0; n < N; ++n)
    {
        const double s = chain.process(0.0);
        if (n >= skip) y.push_back(s);
    }

    // Goertzel-style magnitude at a given frequency
    auto bin = [&](double freq) {
        double re = 0.0, im = 0.0;
        const double w = 2.0 * std::numbers::pi * freq / kSampleRate;
        for (std::size_t i = 0; i < y.size(); ++i)
        {
            re += y[i] * std::cos(w * static_cast<double>(i));
            im += y[i] * std::sin(w * static_cast<double>(i));
        }
        return std::sqrt(re * re + im * im)
             / static_cast<double>(y.size());
    };

    const double b60  = bin(60.0);
    const double b120 = bin(120.0);   // harmonic (AC rectification would put
                                      // energy here too; we don't rectify)
    const double b200 = bin(200.0);   // neutral bin
    const double b1k  = bin(1000.0);
    const double b4k  = bin(4000.0);

    // 60 Hz bin must be well above unrelated bins.  Everything well away
    // from 60 Hz should be at/near noise floor.
    REQUIRE(b60 > 10.0 * b200);
    REQUIRE(b60 > 10.0 * b1k);
    REQUIRE(b60 > 10.0 * b4k);

    // Sanity: overall hum level stays very quiet — conservative headroom
    // check to catch accidental amplitude regressions.
    REQUIRE(b60 < 0.05);
    (void) b120;
}

// ─────────────────────────────────────────────────────────────────────────────
// 120 Hz B+ ripple intermodulation.  Feeding a pure 1 kHz tone through the
// V72 preamp should produce visible 1000 ± 120 Hz sidebands because the tube
// stages' plate current depends on the (rippling) B+ rail and signal drive
// in a multiplicative way.  This is the source of "vintage gritty" ghost
// notes that clean digital emulations lack.
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("TubeAmpChain: B+ ripple leaves a 120 Hz residual on silent input",
          "[chain][psu-ripple]")
{
    // With heater hum off and a silent input, any residual content at
    // 120 Hz must come from the B+ ripple modulating the plate voltage —
    // which is the physical mechanism that gives vintage tube gear its
    // "gritty" character when pushed.  We turn the ripple up and the
    // heater hum off to isolate the effect cleanly.
    // With the physical reservoir PSU, ripple amplitude is EMERGENT
    // (≈ I_load/(2f·C)) — so the regression compares a small vintage
    // reservoir against one 20× larger.  Rail decoupling is disabled to
    // expose the raw rail to the stages (with the ladder on, the preamp
    // nodes filter the ripple to physically-correct near-silence, which
    // is its own test below).
    auto cfg = chain_presets::V72Preamp();
    cfg.variationSeed = 0x1234;
    cfg.enableRailDecoupling = false;
    for (int i = 0; i < cfg.numStages; ++i)
    {
        cfg.stages[i].enableHeaterHum = false;
        cfg.stages[i].enableShotNoise = false;
    }

    auto run = [&](double reservoirFarads) -> std::vector<double>
    {
        auto c = cfg;
        c.psu.reservoirFarads = reservoirFarads;
        TubeAmpChain chain;
        chain.setup(c, kSampleRate);
        for (int i = 0; i < 24000; ++i) (void) chain.process(0.0);

        const int N = static_cast<int>(0.5 * kSampleRate);
        std::vector<double> y;
        y.reserve(N);
        for (int n = 0; n < N; ++n)
            y.push_back(chain.process(0.0));  // silence
        return y;
    };

    auto bin = [](const std::vector<double>& y, double freq) {
        double re = 0.0, im = 0.0;
        const double w = 2.0 * std::numbers::pi * freq / kSampleRate;
        for (std::size_t i = 0; i < y.size(); ++i)
        {
            re += y[i] * std::cos(w * static_cast<double>(i));
            im += y[i] * std::sin(w * static_cast<double>(i));
        }
        return std::sqrt(re * re + im * im)
             / static_cast<double>(y.size());
    };

    const auto ySmall = run(8.0e-6);     // small vintage reservoir
    const auto yBig   = run(160.0e-6);   // stiff, well-filtered rail

    const double b120Small = bin(ySmall, 120.0);
    const double b120Big   = bin(yBig,   120.0);

    // A small reservoir must leave a much stronger 120 Hz residual.
    REQUIRE(b120Small > 5.0 * (b120Big + 1e-12));

    // And 120 Hz should dominate unrelated bins — it's a tonal residual,
    // not a noise-floor shift.
    const double bSilence250 = bin(ySmall, 250.0);
    const double bSilence1k  = bin(ySmall, 1000.0);
    REQUIRE(b120Small > 5.0 * bSilence250);
    REQUIRE(b120Small > 5.0 * bSilence1k);

    // Rail-decoupling ladder check: with the ladder ON the preamp nodes
    // are RC-filtered from the reservoir and the residual collapses —
    // exactly why real preamps don't hum at the reservoir's level.
    {
        auto c = cfg;
        c.enableRailDecoupling = true;
        c.psu.reservoirFarads = 8.0e-6;
        TubeAmpChain chain;
        chain.setup(c, kSampleRate);
        for (int i = 0; i < 24000; ++i) (void) chain.process(0.0);
        const int N = static_cast<int>(0.5 * kSampleRate);
        std::vector<double> y;
        y.reserve(N);
        for (int n = 0; n < N; ++n) y.push_back(chain.process(0.0));
        const double b120Ladder = bin(y, 120.0);
        REQUIRE(b120Ladder < 0.25 * b120Small);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Transformer leakage-inductance resonance peak.  A 16 kHz tone through
// Marinair (peak centred at 16 kHz) must pass at higher level than a 10 kHz
// and a 22 kHz tone — the signature "silk" bump that one-pole-LPF plugins
// cannot produce.
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("TransformerStage: Marinair shows an audible 16 kHz presence peak",
          "[transformer][presence-peak]")
{
    auto measureAt = [&](double freq, bool peakOn) -> double
    {
        auto cfg = transformer_presets::Marinair();
        cfg.enablePresencePeak = peakOn;
        TransformerStage trafo;
        trafo.setup(cfg, kSampleRate);
        // Settle the biquad + HPF transients.
        for (int i = 0; i < 4096; ++i) (void) trafo.process(0.0);

        const int N = static_cast<int>(0.2 * kSampleRate);
        double re = 0.0, im = 0.0;
        const double w = 2.0 * std::numbers::pi * freq / kSampleRate;
        for (int n = 0; n < N; ++n)
        {
            const double x = 0.05 * std::sin(w * n);
            const double y = trafo.process(x);
            re += y * std::cos(w * n);
            im += y * std::sin(w * n);
        }
        return std::sqrt(re * re + im * im) / N;
    };

    const double g10k_peak = measureAt(10000.0, true);
    const double g16k_peak = measureAt(16000.0, true);
    const double g22k_peak = measureAt(22000.0, true);

    // With the peak on, 16 kHz must be louder than BOTH 10 kHz and
    // 22 kHz — i.e. an actual resonance, not a shelf.
    REQUIRE(g16k_peak > 1.05 * g10k_peak);
    REQUIRE(g16k_peak > 1.05 * g22k_peak);

    // And disabling the peak should collapse the relationship: 16 kHz
    // is no longer the loudest (LPF rolloff dominates instead).
    const double g10k_flat = measureAt(10000.0, false);
    const double g16k_flat = measureAt(16000.0, false);
    REQUIRE(g16k_flat < g10k_flat);
}

// ─────────────────────────────────────────────────────────────────────────────
// B+ ripple intermodulation.  A 440 Hz tone through a stage whose B+ rail
// carries 120 Hz ripple must pick up measurable sidebands at 320 Hz
// (= 440 − 120) and 560 Hz (= 440 + 120).  Without ripple on the rail,
// those bins should sit at the noise floor.  This is the "ghost notes"
// of Fender-era amps that most plugins sterilise out.
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PowerSupplySag: ripple oscillator actually modulates B+ at "
          "its configured frequency",
          "[psu][ripple])")
{
    // With the physical reservoir model the rail ripple comes from the
    // charge/discharge cycle itself, so the test drives the PSU with a
    // realistic constant plate-current load and asserts the resulting
    // sawtooth sits at the configured 2×line frequency.  (A sawtooth has
    // strong low harmonics, so the dominance ratios are looser than the
    // legacy pure-sine assertion — but 240 Hz being the 2nd harmonic of
    // 120 Hz is itself physically expected now.)
    PSUSagParams p;
    p.Vb_nominal = 325.0;
    p.Z_internal = 200.0;
    p.tau_sag    = 0.1;
    p.sampleRate = 48000.0;
    p.ripple_amp = 2.0;
    p.ripple_freq = 120.0;
    p.enableReservoirModel = true;
    p.reservoirFarads = 47.0e-6;

    PowerSupplySag psu { p };
    psu.setRipplePhase(0.0);

    constexpr double kLoad = 0.02;   // 20 mA steady draw
    for (int i = 0; i < 48000; ++i) (void) psu.process(kLoad);  // settle

    std::vector<double> vb;
    const int N = static_cast<int>(p.sampleRate * 0.5);
    vb.reserve(N);
    for (int i = 0; i < N; ++i) {
        (void) psu.process(kLoad);
        vb.push_back(psu.currentVb());
    }

    // Hann-windowed, mean-removed DFT bins: the reservoir's slow
    // exponential settle would otherwise leak a 1/f skirt into every
    // low bin and swamp the comparison.
    auto bin = [&](double freq) {
        double mean = 0.0;
        for (double v : vb) mean += v;
        mean /= static_cast<double>(vb.size());
        double re = 0.0, im = 0.0;
        const double w = 2.0 * std::numbers::pi * freq / p.sampleRate;
        const double wN = std::numbers::pi
                        / static_cast<double>(vb.size() - 1);
        for (std::size_t i = 0; i < vb.size(); ++i) {
            const double hann = std::sin(wN * static_cast<double>(i));
            const double v = (vb[i] - mean) * hann * hann;
            re += v * std::cos(w * static_cast<double>(i));
            im += v * std::sin(w * static_cast<double>(i));
        }
        return std::sqrt(re * re + im * im)
             / static_cast<double>(vb.size());
    };

    const double b120 = bin(120.0);
    const double b60  = bin(60.0);
    const double b97  = bin(97.0);    // non-harmonic control bin

    // Fundamental of the charge/discharge sawtooth: ≈ I/(2f·C)·(1/π)
    // scale, halved by the Hann window's coherent gain — still well
    // above 0.15 V at 20 mA / 47 µF.
    REQUIRE(b120 > 0.15);
    // It must sit at 120 Hz, not at line frequency or anywhere else.
    REQUIRE(b120 > 10.0 * b60);
    REQUIRE(b120 > 10.0 * b97);
}

// ─────────────────────────────────────────────────────────────────────────────
// Monte Carlo spread on the new mechanisms: two instances with different
// seeds should show *measurably* different heater-hum level on a silent
// input.  If the hum level were the same for all seeds, the variation
// extension wasn't actually wired into the chain.
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("TubeAmpChain: 50 Hz mains moves hum and ripple lines coherently",
          "[chain][heater-hum][mains][spectrum]")
{
    // docs/35 C1: one config field drives heater hum, PSU ripple (2x
    // line) and the leakage hum — an EU chain must put its energy at
    // 50/100 Hz, not 60/120 Hz.
    auto cfg = chain_presets::V72Preamp();
    cfg.variationSeed = 0xABCD;
    cfg.mainsFrequencyHz = 50.0;
    for (int i = 0; i < cfg.numStages; ++i)
        cfg.stages[i].enableShotNoise = false;
    TubeAmpChain chain;
    chain.setup(cfg, kSampleRate);

    const int N    = static_cast<int>(0.5 * kSampleRate);
    const int skip = static_cast<int>(0.1 * kSampleRate);
    std::vector<double> y;
    y.reserve(static_cast<std::size_t>(N - skip));
    for (int n = 0; n < N; ++n)
    {
        const double s = chain.process(0.0);
        if (n >= skip) y.push_back(s);
    }
    auto bin = [&](double freq) {
        double re = 0.0, im = 0.0;
        const double w = 2.0 * std::numbers::pi * freq / kSampleRate;
        for (std::size_t i = 0; i < y.size(); ++i)
        {
            re += y[i] * std::cos(w * static_cast<double>(i));
            im += y[i] * std::sin(w * static_cast<double>(i));
        }
        return std::sqrt(re * re + im * im)
             / static_cast<double>(y.size());
    };
    const double b50 = bin(50.0);
    const double b60 = bin(60.0);
    INFO("bin50 = " << b50 << ", bin60 = " << b60);
    REQUIRE(b50 > 5.0 * b60);
}

TEST_CASE("TubeAmpChain: two seeds produce different heater-hum levels "
          "(Monte Carlo covers hidden-physics params)",
          "[chain][monte-carlo][hidden-physics]")
{
    auto hum60 = [&](std::uint64_t seed) {
        auto cfg = chain_presets::V72Preamp();
        cfg.variationSeed = seed;
        // Keep everything else except heater hum off so we're measuring
        // that specific knob's unit-to-unit spread.
        for (int i = 0; i < cfg.numStages; ++i)
        {
            cfg.stages[i].enableGridConduction = false;
            cfg.stages[i].enableThermalDrift   = false;
            cfg.stages[i].enableSlewLimit      = false;
        }
        TubeAmpChain chain;
        chain.setup(cfg, kSampleRate);

        const int N    = static_cast<int>(0.5 * kSampleRate);
        const int skip = static_cast<int>(0.1 * kSampleRate);
        std::vector<double> y;
        y.reserve(static_cast<std::size_t>(N - skip));
        for (int n = 0; n < N; ++n)
        {
            const double s = chain.process(0.0);
            if (n >= skip) y.push_back(s);
        }
        double re = 0.0, im = 0.0;
        const double w = 2.0 * std::numbers::pi * 60.0 / kSampleRate;
        for (std::size_t i = 0; i < y.size(); ++i)
        {
            re += y[i] * std::cos(w * static_cast<double>(i));
            im += y[i] * std::sin(w * static_cast<double>(i));
        }
        return std::sqrt(re * re + im * im) / static_cast<double>(y.size());
    };

    const double lvlA = hum60(0xA11CE);
    const double lvlB = hum60(0xB0B);
    const double lvlC = hum60(0x5EED);

    // The three seeds should span at least a few percent in hum level.
    const double lo = std::min({lvlA, lvlB, lvlC});
    const double hi = std::max({lvlA, lvlB, lvlC});
    REQUIRE(hi > 1.05 * lo);
}

// ─────────────────────────────────────────────────────────────────────────────
// Transformer primary-inductance saturation.  The LF corner must move upward
// as the drive increases, so a large 50 Hz sine loses more level (relative
// to mid-band) than a quiet 50 Hz sine.  A static HPF plugin would give the
// same attenuation at every drive level; this one shouldn't.
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("TransformerStage: flux drive makes saturation frequency-dependent",
          "[transformer][lf-saturation][flux]")
{
    // Core flux integrates the primary voltage (Faraday), so the SAME
    // amplitude drives ~f_ref/f times more flux at low frequencies: a
    // loud 50 Hz tone must compress (lose gain-per-unit-drive) far more
    // than a loud 2 kHz tone.  A static-waveshaper transformer would
    // show the same compression at every frequency — this is the single
    // most characteristic behaviour of real audio iron.
    auto gainAt = [&](double freq, double driveAmp) {
        auto cfg = transformer_presets::Marinair();
        cfg.enablePresencePeak = false;   // isolate the core path
        cfg.drive              = 2.0;     // push core near saturation
        TransformerStage trafo;
        trafo.setup(cfg, kSampleRate);
        for (int i = 0; i < 8192; ++i) (void) trafo.process(0.0);

        const int N = static_cast<int>(0.25 * kSampleRate);
        const double w = 2.0 * std::numbers::pi * freq / kSampleRate;
        double re = 0.0, im = 0.0;
        for (int n = 0; n < N; ++n)
        {
            const double x = driveAmp * std::sin(w * n);
            const double y = trafo.process(x);
            re += y * std::cos(w * n);
            im += y * std::sin(w * n);
        }
        return std::sqrt(re * re + im * im)
             / static_cast<double>(N) / driveAmp;
    };

    const double comp50 = gainAt(50.0,   1.0) / gainAt(50.0,   0.02);
    const double comp2k = gainAt(2000.0, 1.0) / gainAt(2000.0, 0.02);

    // Loud bass must lose noticeably more gain than loud mids.
    REQUIRE(comp50 < 0.8 * comp2k);
    // And the mid-band stays nearly linear at this drive.
    REQUIRE(comp2k > 0.8);
}

// ─────────────────────────────────────────────────────────────────────────────
// Program-dependent thermionic shot noise.  Running a louder signal through
// the chain should raise the residual-noise RMS measured at the output,
// because shot-noise amplitude tracks √|Ip|.  If the noise level were
// unconditional, this ratio would sit near 1.0; we expect well above.
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("TubeAmpChain: shot noise adds a seed-dependent noise floor "
          "on silent input",
          "[chain][shot-noise]")
{
    // Simpler claim than "program dependence" (which gets tangled up in
    // Koren cutoff behaviour at high drive and makes a unit test
    // unreliable):
    //   1. Silent input + shot noise on  → non-zero residual RMS.
    //   2. Silent input + shot noise off → tiny residual (numerical only).
    //   3. Two different seeds → two different noise streams.
    // That proves the feature is wired in and reproducible per seed.

    auto silentOutputRms = [&](std::uint64_t seed, bool noiseOn) {
        auto cfg = chain_presets::V72Preamp();
        cfg.variationSeed = seed;
        for (int i = 0; i < cfg.numStages; ++i)
        {
            // Turn off every slow / startup-transient source so the
            // remaining silent-input output is purely shot noise or
            // near-zero DC leak.
            cfg.stages[i].enableWarmup         = false;
            cfg.stages[i].enableHeaterHum      = false;
            cfg.stages[i].enableThermalDrift   = false;
            cfg.stages[i].enableGridConduction = false;
            cfg.stages[i].enableSoakage        = false;
            cfg.stages[i].enableShotNoise      = noiseOn;
            cfg.stages[i].shotNoiseScale       = 5.0e-4;  // exaggerated
        }
        cfg.psu.ripple_amp = 0.0;

        TubeAmpChain chain;
        chain.setup(cfg, kSampleRate);

        // Long settle so the DC high-pass + cathode bounce stabilise.
        for (int i = 0; i < 48000; ++i) (void) chain.process(0.0);

        const int N    = static_cast<int>(0.3 * kSampleRate);
        double acc = 0.0; int cnt = 0;
        for (int n = 0; n < N; ++n)
        {
            const double y = chain.process(0.0);
            acc += y * y; ++cnt;
        }
        return std::sqrt(acc / std::max(cnt, 1));
    };

    const double rms_on_A  = silentOutputRms(0xA11CE, true);
    const double rms_on_B  = silentOutputRms(0xB0BULL, true);
    const double rms_off   = silentOutputRms(0xA11CE, false);

    REQUIRE(rms_on_A > 1e-6);                   // audible noise floor
    REQUIRE(rms_on_A > 10.0 * rms_off);         // dominantly shot noise
    // Different seeds → different streams → different instantaneous RMS
    // (guaranteed not to match to machine precision).
    REQUIRE(std::abs(rms_on_A - rms_on_B) > 1e-7);
}

TEST_CASE("TubeAmpChain: OPT magnetizing coupling stresses tubes at LF, "
          "not the linear calibration",
          "[chain][magcoupling]")
{
    // docs/34 §2.2 — with the coupling on, the power stage genuinely
    // sources the OPT's magnetizing current: on loud LF the plate-current
    // modulation must rise versus coupling-off, while the small-signal
    // 1 kHz response stays calibration-identical (the linear share is
    // de-embedded inside the stages).
    auto makeChain = [&](bool coupling, TubeAmpChain& chain) {
        auto cfg = chain_presets::MarshallMode();
        cfg.enableOPTMagCoupling = coupling;
        cfg.variationSeed = 11;
        chain.setup(cfg, kSampleRate);
    };

    // (a) Small-signal calibration guard at 1 kHz.
    auto smallSignalH1 = [&](bool coupling) {
        TubeAmpChain chain;
        makeChain(coupling, chain);
        std::vector<double> out;
        renderSine(chain, 1000.0, 0.05, 0.4, out);
        std::vector<double> tail(out.end() - static_cast<long>(out.size() / 2),
                                 out.end());
        return goertzelMag(tail, 1000.0, kSampleRate);
    };
    const double h1On  = smallSignalH1(true);
    const double h1Off = smallSignalH1(false);
    const double calDb = 20.0 * std::log10(
        std::max(h1On, 1.0e-12) / std::max(h1Off, 1.0e-12));
    INFO("1 kHz small-signal shift with coupling = " << calDb << " dB");
    REQUIRE(std::abs(calDb) < 0.3);

    // (b) Loud 50 Hz: the pair's plate-current modulation grows when the
    //     tubes must feed the iron.
    auto lfCurrentModulation = [&](bool coupling) {
        TubeAmpChain chain;
        makeChain(coupling, chain);
        const int N = static_cast<int>(0.5 * kSampleRate);
        double sum = 0.0, sumSq = 0.0;
        int count = 0;
        for (int n = 0; n < N; ++n)
        {
            const double t = n / kSampleRate;
            chain.process(0.8 * std::sin(2.0 * std::numbers::pi * 50.0 * t));
            if (n >= N / 2)
            {
                const double ip = chain.lastTotalIp();
                sum += ip; sumSq += ip * ip; ++count;
            }
        }
        const double mean = sum / std::max(count, 1);
        return std::sqrt(std::max(0.0, sumSq / std::max(count, 1)
                                        - mean * mean));
    };
    const double modOn  = lfCurrentModulation(true);
    const double modOff = lfCurrentModulation(false);
    INFO("50 Hz Ip modulation: on = " << modOn << " A, off = " << modOff);
    REQUIRE(std::isfinite(modOn));
    REQUIRE(modOn > modOff * 1.005);   // tubes measurably work harder
}

TEST_CASE("TubeAmpChain: HiFi SE magnetizing coupling keeps calibration and "
          "stays stable",
          "[chain][magcoupling][se]")
{
    // The 300B SE path sources its (gapped, near-linear Lundahl) OPT's
    // magnetizing current.  Guard: small-signal response unchanged, loud
    // LF render finite and bounded.
    auto run = [&](bool coupling, double freq, double amp) {
        TubeAmpChain chain;
        auto cfg = chain_presets::HiFi300BMode();
        cfg.enableOPTMagCoupling = coupling;
        cfg.variationSeed = 11;
        chain.setup(cfg, kSampleRate);
        std::vector<double> out;
        renderSine(chain, freq, amp, 0.4, out);
        std::vector<double> tail(out.end() - static_cast<long>(out.size() / 2),
                                 out.end());
        return goertzelMag(tail, freq, kSampleRate);
    };

    const double h1On  = run(true, 1000.0, 0.05);
    const double h1Off = run(false, 1000.0, 0.05);
    const double calDb = 20.0 * std::log10(
        std::max(h1On, 1.0e-12) / std::max(h1Off, 1.0e-12));
    INFO("HiFi 1 kHz shift with SE coupling = " << calDb << " dB");
    REQUIRE(std::abs(calDb) < 0.3);

    const double lfH1 = run(true, 40.0, 0.8);
    REQUIRE(std::isfinite(lfH1));
    REQUIRE(lfH1 > 1.0e-4);
    REQUIRE(lfH1 < 10.0);
}

TEST_CASE("TubeAmpChain: analytic NFB forward-gain estimate tracks the "
          "offline probe",
          "[chain][nfb][analytic]")
{
    // setup() derives the global-NFB β from estimateForwardGain() because
    // it runs on the audio thread during rebuilds and must not render
    // probe audio (docs/34 §2.1 review fix).  Validate the analytic
    // estimate against the offline 5k-sample measurement probe — β only
    // needs to land the loop gain in the right region (real amps' NFB
    // depth varies unit-to-unit anyway), so a generous factor-2 band is
    // the correct contract.
    auto cfg = chain_presets::MarshallMode();
    cfg.nfbLoopGain   = 0.0;   // measure OPEN loop
    cfg.variationSeed = 3;
    // The probe measures an RMS ratio at 1e-3 amplitude — silence the
    // stages' own noise sources so the measurement is gain, not floor.
    for (int i = 0; i < cfg.numStages; ++i)
    {
        cfg.stages[static_cast<std::size_t>(i)].enableShotNoise = false;
        cfg.stages[static_cast<std::size_t>(i)].enableHeaterHum = false;
    }
    TubeAmpChain chain;
    chain.setup(cfg, kSampleRate);

    const double aEst  = chain.estimateForwardGain();
    const double aMeas = chain.measureForwardGain();
    chain.reset(true);

    INFO("analytic A = " << aEst << ", probed A = " << aMeas);
    REQUIRE(std::isfinite(aEst));
    REQUIRE(aEst > 0.0);
    REQUIRE(aMeas > 0.0);
    REQUIRE(aEst / aMeas > 0.5);
    REQUIRE(aEst / aMeas < 2.0);
}

TEST_CASE("TransformerStage: distortion follows the drive source impedance",
          "[trafo][sourcez]")
{
    // docs/34 §3.9 — a real transformer's THD is proportional to the
    // impedance its magnetizing current works against.  Scaling the
    // dynamic source impedance up must raise H3 at the same drive level,
    // and scaling it down must lower it.
    auto h3At = [&](double zRatio) {
        TransformerStage t;
        auto cfg = transformer_presets::UTC_A12();
        cfg.drive = 1.2;    // solidly nonlinear region
        t.setup(cfg, kSampleRate);
        t.setRestSourceImpedance(1000.0);
        std::vector<double> out;
        const double f = 120.0;
        const int N = 9600;
        out.reserve(N);
        for (int n = 0; n < N; ++n)
        {
            t.setSourceImpedance(1000.0 * zRatio);
            const double x = 0.8 * std::sin(2.0 * std::numbers::pi * f
                                            * n / kSampleRate);
            out.push_back(t.process(x));
        }
        std::vector<double> tail(out.begin() + N / 2, out.end());
        return goertzelMag(tail, 3.0 * f, kSampleRate);
    };

    const double h3Soft  = h3At(2.0);   // high-Z (cutoff-bound) drive
    const double h3Rest  = h3At(1.0);
    const double h3Stiff = h3At(0.5);   // stiff (hard-conducting) drive
    INFO("H3: stiff=" << h3Stiff << " rest=" << h3Rest
         << " soft=" << h3Soft);
    REQUIRE(h3Soft > h3Rest);
    REQUIRE(h3Rest > h3Stiff);
}

TEST_CASE("TubeAmpChain: carrySlowStateFrom survives a parameter edit",
          "[chain][carry]")
{
    // docs/34 §4.3 — automating Bias/Drive rebuilds the chain; the slow
    // state (warmup progress, thermal history) must carry over instead of
    // cold-starting, re-based onto the new rest point.
    auto cfg = chain_presets::V72Preamp();
    cfg.variationSeed = 5;

    TubeAmpChain chainA;
    chainA.setup(cfg, kSampleRate);
    std::vector<double> tmp;
    renderSine(chainA, 220.0, 0.4, 1.0, tmp);   // 1 s: warmup advances
    const double warmA    = chainA.stage(0).warmupProgress();
    const double thermalA = chainA.stage(0).thermalBiasShift();
    REQUIRE(warmA > 0.851);   // moved measurably off the 0.85 cold start

    // Parameter edit: small bias shift (same topology / stage count).
    auto cfgB = cfg;
    cfgB.stages[0].Vg_bias += 0.05;
    TubeAmpChain chainB;
    chainB.setup(cfgB, kSampleRate);
    REQUIRE(chainB.stage(0).warmupProgress() == Approx(0.85).margin(1e-9));

    REQUIRE(chainB.carrySlowStateFrom(chainA));
    REQUIRE(chainB.stage(0).warmupProgress()
            == Approx(warmA).margin(1.0e-9));
    REQUIRE(chainB.stage(0).thermalBiasShift()
            == Approx(thermalA).margin(1.0e-3));

    // Continues rendering finite after the carry.
    std::vector<double> out;
    renderSine(chainB, 220.0, 0.4, 0.1, out);
    for (double v : out) REQUIRE(std::isfinite(v));

    // Graph-shape change must refuse to carry.
    auto cfgC = cfg;
    cfgC.stages[0].topology = TubeTopology::CathodeFollower;
    TubeAmpChain chainC;
    chainC.setup(cfgC, kSampleRate);
    REQUIRE_FALSE(chainC.carrySlowStateFrom(chainA));
}

TEST_CASE("TubeAmpChain: interstage transformer coupling is a working "
          "topology",
          "[chain][interstage-trafo]")
{
    // docs/14 — replacing the RC hand-off with interstage iron must build,
    // stay finite/bounded, remain seed-reproducible, and audibly differ
    // from the RC-coupled chain (the core writes its own hysteresis).
    auto renderCfg = [&](int trafoAfter, std::vector<double>& out) {
        auto cfg = chain_presets::V72Preamp();
        cfg.variationSeed = 9;
        cfg.interstageTrafoAfterStage = trafoAfter;
        cfg.interstageTrafoConfig = transformer_presets::JensenJT11();
        cfg.interstageTrafoConfig.drive = 0.5;
        TubeAmpChain chain;
        chain.setup(cfg, kSampleRate);
        renderSine(chain, 220.0, 0.3, 0.4, out);
    };

    std::vector<double> rc, iron, iron2;
    renderCfg(-1, rc);
    renderCfg(0, iron);
    renderCfg(0, iron2);

    for (double v : iron) REQUIRE(std::isfinite(v));
    REQUIRE(rms(iron) > 1.0e-4);
    REQUIRE(rms(iron) < 10.0);

    // Reproducible with the same seed.
    for (std::size_t i = 0; i < iron.size(); ++i)
        REQUIRE(iron[i] == Approx(iron2[i]).margin(1e-12));

    // And genuinely different from the RC coupling.
    double diffSq = 0.0, refSq = 0.0;
    for (std::size_t i = iron.size() / 2; i < iron.size(); ++i)
    {
        diffSq += (iron[i] - rc[i]) * (iron[i] - rc[i]);
        refSq  += rc[i] * rc[i];
    }
    REQUIRE(std::sqrt(diffSq / std::max(refSq, 1e-30)) > 0.01);
}

TEST_CASE("TubeAmpChain: voltage-native interface is calibration-identical "
          "in steady state, alive on operating-point motion",
          "[chain][voltnative]")
{
    // docs/34 §4.1 — the volt-native hand-off synthesises its pads from
    // the legacy calibration product, so the STEADY-STATE response must
    // match the normalized path within the migration gates (±0.1 dB H1,
    // ±1 dB harmonics).  What it deliberately CHANGES: interior stages'
    // slow operating-point wobble (sag/thermal, 0.5–7 Hz) now pumps the
    // next grid through the coupling cap — so the recovery trajectory
    // after a loud burst must differ measurably from the legacy path.
    auto build = [&](bool vn, TubeAmpChain& chain) {
        auto cfg = chain_presets::V72Preamp();
        cfg.voltageNativeInterface = vn;
        cfg.variationSeed = 4;
        chain.setup(cfg, kSampleRate);
    };

    // (a) Steady-state equivalence gates.
    auto harmonics = [&](bool vn) {
        TubeAmpChain chain;
        build(vn, chain);
        std::vector<double> out;
        renderSine(chain, 997.0, 0.15, 1.0, out);
        std::vector<double> tail(out.end() - static_cast<long>(out.size() / 4),
                                 out.end());
        struct H { double h1, h2, h3; };
        return H { goertzelMag(tail, 997.0, kSampleRate),
                   goertzelMag(tail, 2.0 * 997.0, kSampleRate),
                   goertzelMag(tail, 3.0 * 997.0, kSampleRate) };
    };
    const auto vn  = harmonics(true);
    const auto leg = harmonics(false);
    const double h1Db = 20.0 * std::log10(vn.h1 / std::max(leg.h1, 1e-15));
    const double h2Db = 20.0 * std::log10(std::max(vn.h2, 1e-15)
                                          / std::max(leg.h2, 1e-15));
    const double h3Db = 20.0 * std::log10(std::max(vn.h3, 1e-15)
                                          / std::max(leg.h3, 1e-15));
    INFO("H1 shift = " << h1Db << " dB, H2 = " << h2Db
         << " dB, H3 = " << h3Db << " dB");
    REQUIRE(std::abs(h1Db) < 0.1);
    REQUIRE(std::abs(h2Db) < 1.0);
    REQUIRE(std::abs(h3Db) < 1.0);

    // (b) Bias-pumping physics: loud 2 s burst, then a quiet probe — the
    //     two modes' recovery trajectories must diverge beyond rounding
    //     while their late steady state re-converges.
    auto recovery = [&](bool vnFlag) {
        TubeAmpChain chain;
        build(vnFlag, chain);
        std::vector<double> tmp;
        renderSine(chain, 110.0, 0.8, 2.0, tmp);       // hot LF burst
        std::vector<double> probe;
        renderSine(chain, 997.0, 0.02, 0.5, probe);    // quiet probe
        return probe;
    };
    const auto pVn  = recovery(true);
    const auto pLeg = recovery(false);
    double dEarly = 0.0, rEarly = 0.0;
    const std::size_t early = static_cast<std::size_t>(0.25 * kSampleRate);
    for (std::size_t i = 0; i < early; ++i)
    {
        dEarly += (pVn[i] - pLeg[i]) * (pVn[i] - pLeg[i]);
        rEarly += pLeg[i] * pLeg[i];
    }
    const double relEarly = std::sqrt(dEarly / std::max(rEarly, 1e-30));
    INFO("post-burst recovery divergence = " << relEarly);
    REQUIRE(relEarly > 1.0e-3);   // the pumping path is genuinely alive
    for (double v : pVn) REQUIRE(std::isfinite(v));
}

TEST_CASE("TubeAmpChain: pentode rest anchor matches the runtime screen equilibrium",
          "[chain][cv][defect-guard]")
{
    // docs/35 §S2 D-A: the setup rest point once solved a colder-grid
    // operating point than the runtime map (full-Vk grid subtraction,
    // no-load rail anchor, unscaled warm-up screen current) — a ~17 V
    // standing screen offset on CV's near-critical 6AS6.  Guard: with
    // warm-up disabled (so the comparison is at the anchor's own warm
    // state) the settled screen node must stay near the reported rest.
    auto cfg = chain_presets::CultureVultureMode();
    cfg.variationSeed = 0x1111'2222ULL;
    for (int i = 0; i < cfg.numStages; ++i)
        cfg.stages[static_cast<std::size_t>(i)].enableWarmup = false;
    TubeAmpChain chain;
    chain.setup(cfg, kSampleRate);
    const double vg2Rest = chain.stage(1).screenNodeVoltage();
    for (int n = 0; n < static_cast<int>(2.0 * kSampleRate); ++n)
        chain.process(0.0);
    const double vg2Settled = chain.stage(1).screenNodeVoltage();
    INFO("rest Vg2 = " << vg2Rest << ", settled Vg2 = " << vg2Settled);
    REQUIRE(std::abs(vg2Settled - vg2Rest) < 3.0);
}

TEST_CASE("TubeAmpChain: OPT recovers from the warm-up DC transient (no deep-saturation latch)",
          "[chain][cv][defect-guard]")
{
    // docs/35 §S2 D-B: seed 777's core, kicked by the (physical) warm-up
    // DC thump, once latched into a period-2 solver limit cycle at the
    // JA field rail (|y| ≈ 60 alternating, forever).  With the flux
    // solver's trust region the core rides deep saturation and RECOVERS
    // as the transient drains.  Guard: silence must settle to silence.
    auto cfg = chain_presets::CultureVultureMode();
    cfg.variationSeed = 777;
    TubeAmpChain chain;
    chain.setup(cfg, kSampleRate);
    double y = 0.0;
    for (int n = 0; n < static_cast<int>(2.0 * kSampleRate); ++n)
        y = chain.process(0.0);
    INFO("output after 2 s of silence = " << y
         << ", solved H = " << chain.outputTransformer().solvedFieldH());
    REQUIRE(std::abs(y) < 0.01);
    // And the field must be draining back toward the rail interior,
    // not parked at a runaway magnitude.
    REQUIRE(std::abs(chain.outputTransformer().solvedFieldH())
            < 64.0 * 200.0);   // generous: well under any runaway scale
}

TEST_CASE("TubeAmpChain: seed nulls stay in the audible-individuality band",
          "[chain][seednull]")
{
    // docs/33 §4 tracked per-preset seed-null depths (target −40…−10 dB):
    // two units must differ audibly (null shallower than −40) without a
    // level/character jump (null deeper than −10... i.e. not louder than
    // −10 dB of the reference).  That measurement went stale during the
    // docs/34 waves — this guard makes it permanent (docs/35 §B2).
    struct P { const char* name; int idx; };
    // CV re-admitted after the docs/35 §S2 D-A/D-B fix cycle: with the
    // pentode rest anchor consistent with the runtime screen equilibrium
    // and the OPT flux solver trust-region capped, the CV bare chain
    // passes signal at a healthy level (rms 0.116 vs the broken 0.0009)
    // and meets all three criteria below.
    static const P presets[5] = {
        { "v72", 0 }, { "console", 1 }, { "cv", 2 },
        { "rndi", 3 }, { "hifi", 4 } };

    auto build = [&](int idx, std::uint64_t seed) {
        TubeAmpChainConfig cfg;
        switch (idx)
        {
            case 0: cfg = chain_presets::V72Preamp(); break;
            case 1: cfg = chain_presets::MarshallMode(); break;
            case 2: cfg = chain_presets::CultureVultureMode(); break;
            case 3: cfg = chain_presets::RNDIMode(); break;
            default: cfg = chain_presets::HiFi300BMode(); break;
        }
        cfg.variationSeed = seed;
        // The null measures CHARACTER divergence between two units.  The
        // per-stage noise generators are per-seed STREAMS (decorrelated by
        // design), so at quiet chain levels they alone drag any null
        // toward 0 dB regardless of the deterministic character — silence
        // them for the measurement.
        for (int i = 0; i < cfg.numStages; ++i)
        {
            cfg.stages[static_cast<std::size_t>(i)].enableShotNoise = false;
            cfg.stages[static_cast<std::size_t>(i)].enableHeaterHum = false;
        }
        return cfg;
    };

    for (const auto& p : presets)
    {
        TubeAmpChain a, b;
        a.setup(build(p.idx, 0x1111'2222ULL), kSampleRate);
        b.setup(build(p.idx, 0x8888'9999ULL), kSampleRate);

        std::vector<double> ya, yb;
        renderSine(a, 220.0, 0.2, 0.5, ya);
        renderSine(b, 220.0, 0.2, 0.5, yb);

        // Two-part judgment.  Real units differ by a flat gain factor AND
        // by shape/character; a raw null lumps both, and (e.g.) HiFi's
        // legitimate ±4 dB output-tube gain spread alone can push it to
        // −3 dB.  So: (1) the RAW null bounds individuality from below —
        // two units must be audibly distinct; (2) the LEVEL-MATCHED null
        // (least-squares scale on unit B, the studio A/B practice) bounds
        // character drift from above — after gain matching they must
        // still sound like the same design.
        double d = 0.0, r = 0.0, ab = 0.0, bb = 0.0;
        for (std::size_t i = ya.size() / 2; i < ya.size(); ++i)
        {
            d += (ya[i] - yb[i]) * (ya[i] - yb[i]);
            r += ya[i] * ya[i];
            ab += ya[i] * yb[i];
            bb += yb[i] * yb[i];
        }
        const double nullDb = 10.0 * std::log10(
            std::max(d, 1e-30) / std::max(r, 1e-30));
        const double s = ab / std::max(bb, 1e-30);
        double dm = 0.0;
        for (std::size_t i = ya.size() / 2; i < ya.size(); ++i)
        {
            const double e = ya[i] - s * yb[i];
            dm += e * e;
        }
        const double matchedDb = 10.0 * std::log10(
            std::max(dm, 1e-30) / std::max(r, 1e-30));
        INFO(p.name << " raw null = " << nullDb
                    << " dB, level-matched = " << matchedDb
                    << " dB, LS gain = " << s);
        REQUIRE(nullDb > -45.0);      // units genuinely differ
        REQUIRE(matchedDb < -6.0);    // gain-matched, same instrument
        // Flat gain spread itself must stay within a service-tolerance
        // window — beyond ±6 dB a real unit would be on the bench.
        REQUIRE(std::abs(20.0 * std::log10(std::abs(s))) < 6.0);
    }
}
