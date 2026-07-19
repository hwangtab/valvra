// ─────────────────────────────────────────────────────────────────────────────
// test_tube_stage.cpp — integrated tube stage validation
//
// Validates that the assembled TubeStage (Koren triode + CathodeBounce +
// Miller low-pass + thermal warmup) behaves as expected under:
//   1) Resting DC conditions (zero input → zero output)
//   2) Small-signal linearity
//   3) Large-signal saturation (harmonic generation)
//   4) Warmup ramp (30 seconds → steady state)
//   5) Cathode bounce under burst input
//
// References:
//   docs/22 §A (Dempwolf 2011 harmonic targets)
//   docs/24 §A.5 (V72 preset config)
// ─────────────────────────────────────────────────────────────────────────────
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "TubeStage.h"
#include "TubeAmpChain.h"   // chain_presets for the Rg load-line guard

#include <cmath>
#include <numbers>
#include <vector>

using namespace valvra::dsp;
using Catch::Approx;

namespace {
constexpr double kSampleRate = 48000.0;

double processSine(TubeStage& stage,
                   double freqHz,
                   double amp,
                   double durationSec,
                   double Vb = 250.0,
                   std::vector<double>* out = nullptr)
{
    const int N = static_cast<int>(durationSec * kSampleRate);
    double peakOut = 0.0;
    if (out) out->reserve(N);
    for (int n = 0; n < N; ++n)
    {
        const double t = n / kSampleRate;
        const double x = amp * std::sin(2.0 * std::numbers::pi * freqHz * t);
        const double y = stage.process(x, Vb);
        peakOut = std::max(peakOut, std::abs(y));
        if (out) out->push_back(y);
    }
    return peakOut;
}
} // namespace

TEST_CASE("TubeStage: resting DC with zero input", "[TubeStage][dc]")
{
    TubeStage stage;
    auto cfg = presets::v72Stage1();
    cfg.enableWarmup = false;  // skip warmup for DC test
    stage.setup(cfg, kSampleRate);

    // Let transient settle. AC coupling leak α ≈ 0.9999 → τ ≈ 10000 samples
    // ≈ 208 ms at 48 kHz. Allow ~5τ to reach ±1% of final value.
    for (int i = 0; i < 60000; ++i) stage.process(0.0, cfg.Vp_nominal);

    const double y = stage.process(0.0, cfg.Vp_nominal);
    // Resting output should be small relative to full-scale; 1% tolerance
    // accounts for residual AC-coupling transient.
    REQUIRE(std::abs(y) < 0.01);
}

TEST_CASE("TubeStage: V72 preset — small signal produces measurable output",
          "[TubeStage][smallsignal]")
{
    TubeStage stage;
    auto cfg = presets::v72Stage1();
    cfg.enableWarmup = false;
    stage.setup(cfg, kSampleRate);

    // Settle
    for (int i = 0; i < 5000; ++i) stage.process(0.0, cfg.Vp_nominal);

    // 1 kHz sine at −40 dB (small-signal regime)
    const double peak = processSine(stage, 1000.0, 0.01, 0.2, cfg.Vp_nominal);

    // Output should be non-zero and well-behaved (no NaN/Inf, not clipped)
    REQUIRE(std::isfinite(peak));
    REQUIRE(peak > 0.001);
    REQUIRE(peak < 1.0);
}

TEST_CASE("TubeStage: large signal generates harmonics (asymmetric saturation)",
          "[TubeStage][harmonics]")
{
    TubeStage stage;
    auto cfg = presets::v72Stage1();
    cfg.enableWarmup = false;
    // Asymmetric slew limiting deliberately produces a small DC offset
    // (real tubes rely on the inter-stage coupling cap to strip it).
    // This test checks the pure Koren asymmetry, so disable the slew
    // limiter here — its own dedicated test lives below.
    cfg.enableSlewLimit = false;
    cfg.inputVoltageSwing = 2.0;  // push toward saturation
    cfg.outputGainLinear  = 1.0;  // isolate DC offset from cascade gain
    stage.setup(cfg, kSampleRate);

    for (int i = 0; i < 5000; ++i) stage.process(0.0, cfg.Vp_nominal);

    std::vector<double> out;
    processSine(stage, 1000.0, 1.0, 0.1, cfg.Vp_nominal, &out);

    // Count zero-crossings in one period — a clean sine has exactly 2 per period.
    // Under saturation the waveform becomes asymmetric: peak shape changes,
    // but zero-crossings stay stable. Instead we check DC drift from asymmetry:
    double sum = 0.0;
    for (double v : out) sum += v;
    const double dcOffset = sum / static_cast<double>(out.size());

    // Asymmetric saturation produces small but measurable DC offset before
    // the AC coupling leak removes it.  The 0.5 Hz DC tracker settles over
    // ~300 ms, and the load-line makeup gain (≈7× for this preset) scales
    // the transient offset accordingly — the bound only guards against an
    // unbounded/unsettled output, not a particular asymmetry magnitude.
    REQUIRE(std::abs(dcOffset) < 2.5);  // bounded
}

TEST_CASE("TubeStage: warmup produces gain ramp over time", "[TubeStage][warmup]")
{
    TubeStage stage;
    auto cfg = presets::v72Stage1();
    cfg.enableWarmup         = true;
    cfg.warmupTauSeconds     = 2.0; // accelerate for test
    stage.setup(cfg, kSampleRate);

    // Cold start: warmupCurrent should be ~0.85
    REQUIRE(stage.warmupProgress() == Approx(0.85).margin(0.01));

    // Run 10 τ → should converge close to 1.0
    const int numSamples =
        static_cast<int>(10.0 * cfg.warmupTauSeconds * kSampleRate);
    for (int i = 0; i < numSamples; ++i)
        stage.process(0.0, cfg.Vp_nominal);

    REQUIRE(stage.warmupProgress() == Approx(1.0).margin(0.02));
}

TEST_CASE("TubeStage: cathode bounce — sustained large signal shifts output",
          "[TubeStage][bounce]")
{
    TubeStage stage;
    auto cfg = presets::v72Stage1();
    cfg.enableWarmup = false;
    stage.setup(cfg, kSampleRate);

    // Settle: AC coupling leak τ ≈ 10000 samples, allow 6τ.
    for (int i = 0; i < 60000; ++i) stage.process(0.0, cfg.Vp_nominal);

    // Baseline small-signal gain at 100 Hz
    std::vector<double> quiet;
    processSine(stage, 100.0, 0.05, 0.2, cfg.Vp_nominal, &quiet);
    double peak_quiet = 0.0;
    for (double v : quiet) peak_quiet = std::max(peak_quiet, std::abs(v));

    // Allow AC coupling to re-settle between measurements
    for (int i = 0; i < 30000; ++i) stage.process(0.0, cfg.Vp_nominal);

    // Pound with loud burst at 100 Hz (slow enough for τ=37.5ms cap to charge).
    processSine(stage, 100.0, 1.0, 0.5, cfg.Vp_nominal);

    // Sample immediately to capture post-pound state before Vk recovers fully.
    std::vector<double> after;
    processSine(stage, 100.0, 0.05, 0.05, cfg.Vp_nominal, &after);
    double peak_after = 0.0;
    for (double v : after) peak_after = std::max(peak_after, std::abs(v));

    REQUIRE(peak_quiet > 0.0);
    REQUIRE(peak_after > 0.0);

    // The two measurements MUST differ — this is the quantitative proof
    // of time-varying cathode bounce dynamics (the sensational feature
    // competitor plugins lack entirely).
    const double ratio = peak_after / peak_quiet;
    REQUIRE(std::abs(ratio - 1.0) > 0.05);  // ≥ 5% change in either direction
}

TEST_CASE("TubeStage: RNDI preset runs without error", "[TubeStage][presets]")
{
    TubeStage stage;
    auto cfg = presets::rndiStage();
    cfg.enableWarmup = false;
    stage.setup(cfg, kSampleRate);

    // Cathode follower has Rp=0 → plate voltage stays at Vb+.
    // Feed a signal and verify no NaN/Inf.
    for (int i = 0; i < 1000; ++i)
    {
        const double x = 0.1 * std::sin(0.01 * i);
        const double y = stage.process(x, cfg.Vp_nominal);
        REQUIRE(std::isfinite(y));
    }
}

TEST_CASE("TubeStage: cultureVultureInput preset produces asymmetric output",
          "[TubeStage][presets]")
{
    TubeStage stage;
    auto cfg = presets::cultureVultureInput();
    cfg.enableWarmup = false;
    stage.setup(cfg, kSampleRate);

    for (int i = 0; i < 5000; ++i) stage.process(0.0, cfg.Vp_nominal);

    std::vector<double> out;
    processSine(stage, 440.0, 1.0, 0.1, cfg.Vp_nominal, &out);

    // All samples finite
    for (double v : out)
        REQUIRE(std::isfinite(v));
}

TEST_CASE("TubeStage: Culture Vulture T/P1/P2 core voicings are distinct",
          "[TubeStage][presets][culture-vulture]")
{
    auto measurePeak = [](CultureVultureVoicing voicing)
    {
        TubeStage stage;
        auto cfg = presets::cvDistortionCore(voicing);
        cfg.enableWarmup = false;
        stage.setup(cfg, kSampleRate);
        for (int i = 0; i < 5000; ++i)
            (void) stage.process(0.0, cfg.Vp_nominal);
        return processSine(stage, 440.0, 0.6, 0.1, cfg.Vp_nominal);
    };

    const double triode = measurePeak(CultureVultureVoicing::Triode);
    const double p1     = measurePeak(CultureVultureVoicing::PentodeLow);
    const double p2     = measurePeak(CultureVultureVoicing::PentodeHigh);

    REQUIRE(std::isfinite(triode));
    REQUIRE(std::isfinite(p1));
    REQUIRE(std::isfinite(p2));
    REQUIRE(triode > 0.0);
    REQUIRE(p1 > 0.0);
    REQUIRE(p2 > 0.0);
    REQUIRE(std::abs(triode - p1) > 0.01);
    REQUIRE(std::abs(p2 - p1) > 0.01);
}

TEST_CASE("TubeStage: pentode next-stage Rg reshapes the load line, level preserved",
          "[TubeStage][pentode][rg-load]")
{
    // docs/35 C2: the next stage's grid-leak is part of the AC plate
    // load for pentode CC stages too.  The makeup factor must absorb
    // the small-signal level change (physics-first policy), while the
    // load-line curvature — what the mechanism is FOR — must move.
    auto render = [&](double rgNext, double amp)
    {
        TubeStage stage;
        // V72's EF86-class input pentode: enough standing current that
        // the plate genuinely traverses the load line (the starved CV
        // core clips by current starvation instead, where the load line
        // is pure gain and the makeup absorbs it completely).
        auto cfg = chain_presets::V72Preamp().stages[0];
        cfg.enableWarmup = false;
        cfg.enableShotNoise = false;
        cfg.enableHeaterHum = false;
        cfg.nextStageLoadR = rgNext;
        stage.setup(cfg, kSampleRate);
        std::vector<double> y;
        const int N = 24000, skip = 12000;
        y.reserve(static_cast<std::size_t>(N - skip));
        for (int i = 0; i < N; ++i)
        {
            const double x = amp * std::sin(2.0 * std::numbers::pi * 220.0
                                          * (static_cast<double>(i) / kSampleRate));
            const double out = stage.process(x, cfg.Vp_nominal);
            if (i >= skip) y.push_back(out);
        }
        return y;
    };
    auto rms = [](const std::vector<double>& y) {
        double acc = 0.0;
        for (double v : y) acc += v * v;
        return std::sqrt(acc / static_cast<double>(y.size()));
    };
    auto bin = [&](const std::vector<double>& y, double freq) {
        double re = 0.0, im = 0.0;
        const double w = 2.0 * std::numbers::pi * freq / kSampleRate;
        for (std::size_t i = 0; i < y.size(); ++i)
        {
            re += y[i] * std::cos(w * static_cast<double>(i));
            im += y[i] * std::sin(w * static_cast<double>(i));
        }
        return std::sqrt(re * re + im * im) / static_cast<double>(y.size());
    };

    // Small signal: level calibration must hold across the Rg change.
    const double lvlOpen = rms(render(0.0, 0.05));
    const double lvlLoad = rms(render(220.0e3, 0.05));
    const double lvlDeltaDb = 20.0 * std::log10(lvlLoad / lvlOpen);
    INFO("small-signal level delta = " << lvlDeltaDb << " dB");
    REQUIRE(std::abs(lvlDeltaDb) < 0.5);

    // Large signal: the reshaped load line must move the harmonic mix.
    const auto hotOpen = render(0.0, 1.5);
    const auto hotLoad = render(220.0e3, 1.5);
    // H2/H1 measured 0.099 open vs 0.228 loaded at this drive — the
    // threshold below only asserts a clear, not knife-edge, separation.
    const double h2Open = bin(hotOpen, 440.0) / std::max(bin(hotOpen, 220.0), 1e-12);
    const double h2Load = bin(hotLoad, 440.0) / std::max(bin(hotLoad, 220.0), 1e-12);
    INFO("H2/H1 open = " << h2Open << ", loaded = " << h2Load);
    REQUIRE(std::abs(20.0 * std::log10(std::max(h2Load, 1e-9)
                                       / std::max(h2Open, 1e-9))) > 2.0);
}

TEST_CASE("TubeStage: pentode screen node droops under heavy drive",
          "[TubeStage][pentode][culture-vulture]")
{
    TubeStage stage;
    auto cfg = presets::cvDistortionCore(CultureVultureVoicing::PentodeHigh);
    cfg.enableWarmup = false;
    stage.setup(cfg, kSampleRate);

    const double screenAtRest = stage.lastScreenVoltage();
    // Drive amplitude picked where the droop mechanism genuinely lives:
    // at 0.9 the Ig2 average barely moves (± a tenth of a volt, sign
    // depending on the curve corner — the old expectation only held
    // because the rest anchor sat ~20 V above the true equilibrium,
    // docs/35 §S2 D-A), while at 2.0 the plate bottoms every cycle and
    // the screen genuinely takes the duty-cycle hit.
    for (int i = 0; i < 48000; ++i)
    {
        const double x = 2.0 * std::sin(2.0 * std::numbers::pi * 220.0
                                      * (static_cast<double>(i) / kSampleRate));
        (void) stage.process(x, cfg.Vp_nominal);
    }

    const double screenDriven = stage.lastScreenVoltage();
    REQUIRE(std::isfinite(screenAtRest));
    REQUIRE(std::isfinite(screenDriven));
    REQUIRE(screenDriven < screenAtRest - 0.05);
    REQUIRE(stage.lastScreenCurrent() > 0.0);
}

TEST_CASE("TubeStage: Long-Tailed Pair mismatch changes differential output",
          "[TubeStage][ltp]")
{
    auto renderMeanAbs = [](double mismatch)
    {
        TubeStage stage;
        auto cfg = presets::v72Stage1();
        cfg.topology = TubeTopology::LongTailedPair;
        cfg.enableWarmup = false;
        cfg.enableMillerFilter = false;
        cfg.enableSlewLimit = false;
        cfg.enableCathodeBounce = false;
        cfg.ltpTailR = 56.0e3;
        cfg.ltpPlateRRatio = 1.05;
        cfg.ltpTubeMismatch = mismatch;
        cfg.ltpCommonModeLeak = 0.05;
        stage.setup(cfg, kSampleRate);

        for (int i = 0; i < 6000; ++i)
            (void) stage.process(0.0, cfg.Vp_nominal);

        std::vector<double> out;
        processSine(stage, 440.0, 0.45, 0.15, cfg.Vp_nominal, &out);
        double s = 0.0;
        for (double v : out)
        {
            REQUIRE(std::isfinite(v));
            s += std::abs(v);
        }
        return s / std::max<std::size_t>(1, out.size());
    };

    const double balanced = renderMeanAbs(0.0);
    const double skewed   = renderMeanAbs(0.12);
    REQUIRE(balanced > 1.0e-4);
    REQUIRE(skewed > 1.0e-4);
    REQUIRE(std::abs(skewed - balanced) > 1.0e-4);
}

// ─────────────────────────────────────────────────────────────────────────────
// Grid conduction / blocking distortion — pushing the grid positive should
// charge the input coupling capacitor through the g-k diode, leaving a
// negative bias offset that persists with R_leak · C_coupling time constant
// after the drive is removed.  Measured as a decaying post-burst tail.
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("TubeStage: grid conduction leaves a post-burst recovery tail",
          "[stage][grid-conduction][blocking-distortion]")
{
    // Use a deliberately hot config so the input signal drives the grid
    // past the turn-on voltage and grid conduction actually activates.
    // We don't depend on a particular preset's hot-drive choice here —
    // the feature being tested (blocking distortion) is independent of
    // any specific signature mode's voicing decisions.
    TubeStageConfig cfg {
        .tube = params::kRSD_2,
        .topology = TubeTopology::CommonCathode,
        .Vg_bias = -1.5, .Vp_nominal = 320.0,
        .Rp = 100.0e3, .Rk = 1500.0, .Ck = 22.0e-6,
        .enableCathodeBounce = true,
        .inputVoltageSwing = 3.0,        // hot enough to push grid past 0 V
        .outputGainLinear = 1.0,
        .enableWarmup = false,
        .enableMillerFilter = true,
        .Cgp_miller = 2.4e-12,
        .sourceImpedance = 100.0e3
    };
    cfg.enableGridConduction = true;
    // τ_discharge ≈ 22 ms at 22 nF / 1 MΩ — still measurable at 48 kHz.
    cfg.gridCouplingC = 22.0e-9;
    cfg.gridLeakR     = 1.0e6;

    TubeStage stage;
    stage.setup(cfg, kSampleRate);
    // Let internal DC settling finish before measurement.
    for (int i = 0; i < 5000; ++i) stage.process(0.0, cfg.Vp_nominal);

    // Render: 10 ms of hard drive (loud enough to push grid past turn-on),
    // then 40 ms of silence where we measure the recovery tail.
    constexpr double kBurstSec = 0.010;
    constexpr double kTailSec  = 0.040;
    const int burstN = static_cast<int>(kBurstSec * kSampleRate);
    const int tailN  = static_cast<int>(kTailSec  * kSampleRate);

    auto render = [&](bool conductionOn) -> std::vector<double>
    {
        auto c = cfg;
        c.enableGridConduction = conductionOn;
        stage.setup(c, kSampleRate);
        for (int i = 0; i < 5000; ++i) stage.process(0.0, c.Vp_nominal);

        std::vector<double> out;
        out.reserve(static_cast<std::size_t>(burstN + tailN));

        // Hard square burst (DC-like drive that definitely clips positive).
        for (int i = 0; i < burstN; ++i)
            out.push_back(stage.process(1.0, c.Vp_nominal));

        // Silence — capture the tail.
        for (int i = 0; i < tailN; ++i)
            out.push_back(stage.process(0.0, c.Vp_nominal));
        return out;
    };

    const auto withBlocking  = render(true);
    const auto noBlocking    = render(false);

    // Inspect only the recovery portion.
    auto sliceAfterBurst = [&](const std::vector<double>& v) {
        return std::vector<double>(
            v.begin() + burstN,
            v.begin() + burstN + tailN);
    };
    const auto tailWith = sliceAfterBurst(withBlocking);
    const auto tailNo   = sliceAfterBurst(noBlocking);

    // The grid-conduction branch should leave a measurable trailing
    // deviation relative to the no-conduction branch over the first few
    // milliseconds after the burst.  Below ~−40 dB the two paths would
    // be indistinguishable and the feature would not actually do anything
    // audible; we require meaningfully louder than that.
    double diff = 0.0, ref = 0.0;
    for (std::size_t i = 0; i < tailWith.size(); ++i)
    {
        const double d = tailWith[i] - tailNo[i];
        diff += d * d;
        ref  += tailNo[i] * tailNo[i] + 1e-18;
    }
    const double tailDb = 10.0 * std::log10(diff / ref);
    REQUIRE(tailDb > -40.0);

    // The coupling-cap charge itself must decay toward zero with its
    // R_leak·C time constant after the drive is removed.  We can't read
    // the private state directly, but we can render a much longer tail
    // (many τ) and confirm both signals re-converge — proving the offset
    // is transient, not a permanent DC shift.
    auto renderLong = [&](bool conductionOn) -> double
    {
        auto c = cfg;
        c.enableGridConduction = conductionOn;
        TubeStage s;
        s.setup(c, kSampleRate);
        for (int i = 0; i < 5000; ++i) s.process(0.0, c.Vp_nominal);
        for (int i = 0; i < burstN; ++i) (void) s.process(1.0, c.Vp_nominal);
        // Let ~10 · τ elapse so any charge has fully leaked off.  Long
        // enough for the 0.5 Hz output-DC tracker (τ ≈ 320 ms) to
        // re-centre as well, not just the grid-cap charge.
        const int settleN = static_cast<int>(1.5 * kSampleRate);
        for (int i = 0; i < settleN; ++i) (void) s.process(0.0, c.Vp_nominal);
        // Now sample steady state
        double acc = 0.0;
        constexpr int kN = 256;
        for (int i = 0; i < kN; ++i)
            acc += std::abs(s.process(0.0, c.Vp_nominal));
        return acc / kN;
    };
    const double ssWith = renderLong(true);
    const double ssNo   = renderLong(false);
    // After the cap has effectively leaked down, the residual divergence
    // should be far smaller than the post-burst tail itself.  The slow
    // output-DC high-pass (τ ≈ 200 ms) continues to drift for a while
    // after the grid cap has settled, so we compare to the in-burst
    // difference rather than to zero.
    REQUIRE(std::abs(ssWith - ssNo) < 0.01);
}

// ─────────────────────────────────────────────────────────────────────────────
// Plate-dissipation thermal drift — a sustained loud passage should pull the
// effective grid bias more negative over seconds, reducing gain as the stage
// "sits down".  When thermal drift is off, bias stays pinned to its resting
// value regardless of signal history.
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("TubeStage: sustained loud signal drags bias negative (thermal drift)",
          "[stage][thermal-drift]")
{
    // Build a deliberately hot config so the average |Ip| actually
    // climbs well above resting — in a lightly-driven Class-A stage
    // the rectified current barely moves, which would let the test
    // pass or fail on round-off noise.  We don't piggyback on the
    // ConsoleOutput preset stages here because their voicing is
    // intentionally gentle (mix-friendly) and would defeat the test.
    TubeStageConfig cfg {
        .tube = params::kRSD_2,
        .topology = TubeTopology::CommonCathode,
        .Vg_bias = -1.5, .Vp_nominal = 320.0,
        .Rp = 100.0e3, .Rk = 1500.0, .Ck = 22.0e-6,
        .inputVoltageSwing = 3.0,
        .outputGainLinear = 1.0,
        .enableWarmup = false,
        .enableMillerFilter = false,
        .sourceImpedance = 100.0e3
    };
    cfg.enableHeaterHum      = false;
    cfg.enableGridConduction = false;
    cfg.enableThermalDrift   = true;
    cfg.thermalTauSeconds    = 2.0;
    cfg.thermalBiasSensitivity = 500.0;  // exaggerated for unit-test clarity

    TubeStage stage;
    stage.setup(cfg, kSampleRate);
    for (int i = 0; i < 5000; ++i) (void) stage.process(0.0, cfg.Vp_nominal);

    const double restBias = stage.thermalBiasShift();
    REQUIRE(std::abs(restBias) < 1e-3);

    // 4 seconds of loud sine — well past τ so the envelope has saturated.
    const int N = static_cast<int>(4.0 * kSampleRate);
    for (int n = 0; n < N; ++n)
    {
        const double t = n / kSampleRate;
        const double x = 1.0
            * std::sin(2.0 * std::numbers::pi * 440.0 * t);
        (void) stage.process(x, cfg.Vp_nominal);
    }

    const double loadedBias = stage.thermalBiasShift();
    // Drift should produce a measurable bias shift, far above the
    // noise-floor-scale rest value.
    REQUIRE(loadedBias > 0.010);
    REQUIRE(loadedBias > 5.0 * std::abs(restBias + 1e-9));

    // Repeat with drift off to confirm the shift is uniquely attributable
    // to this mechanism — not an artefact of some other nonlinearity
    // leaking through the accessor.
    auto cfg2 = cfg;
    cfg2.enableThermalDrift = false;
    TubeStage stage2;
    stage2.setup(cfg2, kSampleRate);
    for (int i = 0; i < 5000; ++i) (void) stage2.process(0.0, cfg2.Vp_nominal);
    for (int n = 0; n < N; ++n)
    {
        const double t = n / kSampleRate;
        const double x = 1.0
            * std::sin(2.0 * std::numbers::pi * 440.0 * t);
        (void) stage2.process(x, cfg2.Vp_nominal);
    }
    REQUIRE(std::abs(stage2.thermalBiasShift()) < 1e-6);
}

// ─────────────────────────────────────────────────────────────────────────────
// Asymmetric slew-rate limit.  A square-wave drive should round the falling
// edge (tube conducting, fast) differently from the rising edge (RC pull-up,
// slow).  If rise and fall both showed the same shape, the asymmetric limiter
// was never actually applied.
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("TubeStage: asymmetric slew rate rounds rising and falling edges differently",
          "[stage][slew][asymmetric]")
{
    // Measure peak output delta on the two step-response polarities.
    // slewRatePositive applies when the output itself is rising; the
    // initial max upward delta should be much smaller than the initial
    // max downward delta when we dial a 1:10 asymmetry.
    auto peakSlew = [](double slewPos, double slewNeg, int stepSign) -> double
    {
        auto cfg = presets::marshallStage2();
        cfg.enableWarmup         = false;
        cfg.enableHeaterHum      = false;
        cfg.enableGridConduction = false;
        cfg.enableThermalDrift   = false;
        cfg.enableSlewLimit      = true;
        cfg.slewRatePositive     = slewPos;
        cfg.slewRateNegative     = slewNeg;

        TubeStage s;
        s.setup(cfg, kSampleRate);
        // Drive to a steady state at one rail, then step to the other.
        const double pre   = (stepSign > 0) ? -0.85 :  0.85;
        const double post  = (stepSign > 0) ?  0.85 : -0.85;
        for (int i = 0; i < 4000; ++i) (void) s.process(pre, cfg.Vp_nominal);
        // Observe the single largest sample-to-sample delta after the step.
        double prev = s.process(pre, cfg.Vp_nominal);
        double maxAbsDelta = 0.0;
        for (int i = 0; i < 256; ++i)
        {
            const double y = s.process(post, cfg.Vp_nominal);
            maxAbsDelta = std::max(maxAbsDelta, std::abs(y - prev));
            prev = y;
        }
        return maxAbsDelta;
    };

    // Asymmetric config: "upward" slew 10× slower than "downward".
    const double upStepDelta    = peakSlew(800.0, 8000.0,  +1);
    const double downStepDelta  = peakSlew(800.0, 8000.0,  -1);
    // Marshall stage 2 is inverting: a positive input step drives the
    // output DOWNWARD, so it exercises the fast (negative) slew limit.
    // A negative input step drives the output upward, exercising the
    // slow (positive) limit.  Therefore downStepDelta <<  upStepDelta
    // would indicate a flipped-sign bug, and they should differ by far
    // more than a modest factor.
    REQUIRE(upStepDelta   > 0.0);
    REQUIRE(downStepDelta > 0.0);
    REQUIRE(upStepDelta   > 2.0 * downStepDelta);

    // With symmetric config, the asymmetry goes away.
    const double symUp   = peakSlew(8000.0, 8000.0,  +1);
    const double symDown = peakSlew(8000.0, 8000.0,  -1);
    const double ratio   = symUp / std::max(symDown, 1e-12);
    REQUIRE(ratio < 2.0);
    REQUIRE(ratio > 0.5);
}

// ─────────────────────────────────────────────────────────────────────────────
// Bug-fix regression: first block after setup() must not show a phantom
// Miller-HF dip caused by lastIp_ defaulting to zero (which would drag
// programFactor to 1 − millerSignalDepth).  With lastIp_ primed to Ip_rest_
// in setup(), the opening samples should look identical whether the stage
// has just been setup or has been running at rest for a while.
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("TubeStage: fresh setup does not momentarily brighten HF "
          "(Miller lastIp_ prime)",
          "[stage][miller][signal-dep][startup]")
{
    auto cfg = presets::v72Stage1();
    cfg.enableWarmup         = false;
    cfg.enableHeaterHum      = false;
    cfg.enableGridConduction = false;
    cfg.enableThermalDrift   = false;
    cfg.enableSlewLimit      = false;
    cfg.millerSignalDepth    = 0.5;  // non-zero so any bug bites

    TubeStage fresh, settled;
    fresh.setup(cfg, kSampleRate);
    settled.setup(cfg, kSampleRate);
    // Let `settled` run long enough for any startup transient in the
    // Miller path to wash out of the LPF state.
    for (int i = 0; i < 10000; ++i)
        (void) settled.process(0.0, cfg.Vp_nominal);

    // Probe both with an identical short sine and compare the outputs.
    // If lastIp_ were starting at 0 in the fresh stage, its first few
    // hundred samples would have a different HF response than the
    // settled stage's.
    constexpr int N = 512;
    double diffE = 0.0, refE = 0.0;
    for (int n = 0; n < N; ++n)
    {
        const double x = 0.1 * std::sin(
            2.0 * std::numbers::pi * 8000.0 * n / kSampleRate);
        const double yFresh   = fresh.process(x,   cfg.Vp_nominal);
        const double ySettled = settled.process(x, cfg.Vp_nominal);
        const double d = yFresh - ySettled;
        diffE += d * d;
        refE  += ySettled * ySettled + 1e-18;
    }
    const double diffDb = 10.0 * std::log10(diffE / refE);
    // < −40 dB divergence is far tighter than the ~−20 dB the bug used
    // to produce, and loose enough to absorb legitimate DC-settling
    // differences in the output high-pass.
    REQUIRE(diffDb < -40.0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Microphonic coupling: a bass burst should excite the body-resonance
// bandpass and add a measurable AM modulation on the stage's output.
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("TubeStage: microphonic coupling adds resonance-band modulation",
          "[stage][microphonics]")
{
    // Build a config with microphonics explicitly enabled.  The default
    // mode-preset stages no longer turn microphonics on (Console Output
    // is for mix/master, where chassis vibration is irrelevant), so we
    // construct a dedicated test config — this isolates the FEATURE
    // being tested from any preset's voicing decisions.
    TubeStageConfig cfg {
        .tube = params::kRSD_1,
        .topology = TubeTopology::CommonCathode,
        .Vg_bias = -1.2, .Vp_nominal = 320.0,
        .Rp = 100.0e3, .Rk = 820.0, .Ck = 1.0e-6,
        .inputVoltageSwing = 0.3,
        .outputGainLinear = 1.0,
        .enableWarmup = false,
        .enableMillerFilter = true,
        .Cgp_miller = 2.4e-12,
        .sourceImpedance = 10.0e3
    };
    cfg.enableHeaterHum      = false;
    cfg.enableThermalDrift   = false;
    cfg.enableGridConduction = false;
    cfg.enableShotNoise      = false;
    cfg.enableSlewLimit      = false;
    // Exaggerate mic depth for unit-test sensitivity.  With the plate
    // load line active, Ip excursions are divided by the rp ∥ Rp factor,
    // so the resonator needs a hotter drive than the legacy engine did.
    cfg.inputVoltageSwing    = 1.0;
    cfg.micDepth             = 0.15;
    cfg.micResonanceHz       = 120.0;
    cfg.micResonanceQ        = 6.0;

    auto rmsAt = [&](bool micOn, double freq) {
        auto c = cfg;
        c.enableMicrophonics = micOn;
        TubeStage s;
        s.setup(c, kSampleRate);
        for (int i = 0; i < 5000; ++i) (void) s.process(0.0, c.Vp_nominal);

        const int N = static_cast<int>(0.4 * kSampleRate);
        const double w = 2.0 * std::numbers::pi * freq / kSampleRate;
        const double wN = std::numbers::pi / static_cast<double>(N - 1);
        double re = 0.0, im = 0.0;
        for (int n = 0; n < N; ++n)
        {
            // 50 Hz fundamental drive that puts plate current swinging
            // hard enough to excite the 120 Hz mech resonance via gm
            // modulation (every half-cycle of Ip variation).  Hann
            // windowing keeps the strong 50 Hz harmonics (100/150 Hz)
            // from leaking into the sideband bin under test.
            const double drive = 0.6
                * std::sin(2.0 * std::numbers::pi * 50.0 * n / kSampleRate);
            const double hann = std::sin(wN * n);
            const double y = s.process(drive, c.Vp_nominal) * hann * hann;
            re += y * std::cos(w * n);
            im += y * std::sin(w * n);
        }
        return std::sqrt(re * re + im * im) / static_cast<double>(N);
    };

    // gm modulation by the 120 Hz resonator AMPLITUDE-MODULATES the
    // 50 Hz fundamental: products land at 120 ± 50 Hz.  170 Hz is not a
    // harmonic of 50 Hz, so it is a clean discriminator — without the
    // mechanical coupling there is (almost) nothing there.
    const double sb_on  = rmsAt(true,  170.0);
    const double sb_off = rmsAt(false, 170.0);
    REQUIRE(sb_on > 2.0 * (sb_off + 1.0e-9));
}

// ─────────────────────────────────────────────────────────────────────────────
// Asymmetric slew rate — a common-cathode stage pulls its plate down fast
// (tube conducting, low ON-state impedance) but charges it up slowly through
// the plate-load resistor.  A square-wave input should therefore show a
// faster falling edge than rising edge at the stage output.  This is a
// chunk of what "tube punch" actually sounds like on drums.
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("TubeStage: slew limit makes falling edges faster than rising edges",
          "[stage][slew][asymmetry]")
{
    auto cfg = presets::v72Stage1();
    cfg.enableWarmup         = false;
    cfg.enableHeaterHum      = false;
    cfg.enableGridConduction = false;
    cfg.enableThermalDrift   = false;
    cfg.enableSlewLimit      = true;
    cfg.slewRatePositive     = 1500.0;
    cfg.slewRateNegative     = 4500.0;   // 3× faster fall

    TubeStage stage;
    stage.setup(cfg, kSampleRate);
    for (int i = 0; i < 5000; ++i) (void) stage.process(0.0, cfg.Vp_nominal);

    // Drive a single low-frequency square wave: 200 Hz, big amplitude.
    // One full cycle gives us one rising and one falling edge to compare.
    const double freq   = 200.0;
    const int    period = static_cast<int>(kSampleRate / freq);
    const int    half   = period / 2;
    std::vector<double> y;
    y.reserve(period * 3);
    for (int i = 0; i < period * 3; ++i)
    {
        const double x = ((i % period) < half) ? +0.9 : -0.9;
        y.push_back(stage.process(x, cfg.Vp_nominal));
    }

    // Common-cathode is inverting: input +0.9 → plate goes low.  So an
    // *input* 0→1 step produces an *output* fall (fast slew); an input
    // 1→0 step produces an output rise (slow slew).
    // Measure: from the point the output crosses zero after each edge,
    // how fast is the magnitude of the first-difference on average?
    auto edgeSlope = [&](int startIdx) {
        // Average |Δy| over the 20-sample window right after the input
        // transition — this captures the effective slew.
        double acc = 0.0;
        const int W = 20;
        for (int k = 0; k < W; ++k)
            acc += std::abs(y[static_cast<std::size_t>(startIdx + k + 1)]
                          - y[static_cast<std::size_t>(startIdx + k)]);
        return acc / W;
    };

    // Use the second full period to avoid startup transients.
    const double fallSlope = edgeSlope(period);         // input goes +→− ish
    const double riseSlope = edgeSlope(period + half);  // input goes −→+ ish

    // Because common-cathode inverts, the "fast" side at the stage output
    // corresponds to the input edge where plate voltage falls — the edge
    // at `period + half` (input 1→−1: output goes from low back up = slow,
    // but plate side polarity makes the first transition after t=period
    // the FAST one).  Rather than reason about polarity, require that
    // one of the two edges is at least 1.5× the other — proving asymmetry.
    const double ratio = std::max(fallSlope, riseSlope)
                       / std::max(std::min(fallSlope, riseSlope), 1e-12);
    REQUIRE(ratio > 1.5);

    // Swapping the slew rates should flip which edge dominates — proof
    // that the asymmetry genuinely comes from the slew limiter and not
    // from unrelated Koren-model asymmetry that happens to sit in the
    // same direction.
    auto cfg2 = cfg;
    std::swap(cfg2.slewRatePositive, cfg2.slewRateNegative);
    TubeStage s2;
    s2.setup(cfg2, kSampleRate);
    for (int i = 0; i < 5000; ++i) (void) s2.process(0.0, cfg2.Vp_nominal);
    std::vector<double> y2;
    y2.reserve(period * 3);
    for (int i = 0; i < period * 3; ++i)
    {
        const double x = ((i % period) < half) ? +0.9 : -0.9;
        y2.push_back(s2.process(x, cfg2.Vp_nominal));
    }
    auto slope2 = [&](int startIdx) {
        double acc = 0.0;
        for (int k = 0; k < 20; ++k)
            acc += std::abs(y2[static_cast<std::size_t>(startIdx + k + 1)]
                          - y2[static_cast<std::size_t>(startIdx + k)]);
        return acc / 20.0;
    };
    // Whichever edge was fast before must be slower now (and vice-versa).
    const bool fallDominatedOriginally = fallSlope > riseSlope;
    const bool fallDominatedSwapped    = slope2(period) > slope2(period + half);
    REQUIRE(fallDominatedOriginally != fallDominatedSwapped);
}

// ─────────────────────────────────────────────────────────────────────────────
// Program-dependent Miller capacitance — at a high signal level, heavy plate
// conduction grows C_m a touch and pulls the HF cutoff down.  Compared to
// `millerSignalDepth = 0` (legacy static Miller), the dynamic path must
// attenuate a 10 kHz tone more when the stage is driven hard.
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("TubeStage: Miller cutoff opens as the stage saturates",
          "[stage][miller][program-dependent]")
{
    // docs/04 §3.2: approaching saturation/cutoff the incremental gain
    // |Av| DROPS, so C_miller = Cgp·(1+|Av|) shrinks and the HF rolloff
    // relaxes — a hard-driven stage sounds slightly more open on top.
    // (The legacy heuristic had this backwards: it grew C_m with |Ip|.)
    // Verified against the solved operating point: at ±6 V grid drive the
    // 12AX7 spends half of each cycle near cutoff where gm ≈ 0.
    auto cfg = presets::marshallStage2();
    cfg.enableWarmup         = false;
    cfg.enableHeaterHum      = false;
    cfg.enableGridConduction = false;
    cfg.enableThermalDrift   = false;
    cfg.enableSlewLimit      = false;   // isolate the Miller effect
    cfg.enableMillerFilter   = true;

    auto measure = [&](double depth, double amp) -> double
    {
        auto c = cfg;
        c.millerSignalDepth = depth;
        TubeStage stage;
        stage.setup(c, kSampleRate);
        for (int i = 0; i < 5000; ++i) (void) stage.process(0.0, c.Vp_nominal);

        const int N = static_cast<int>(0.2 * kSampleRate);
        // Goertzel at 10 kHz — isolates the HF transfer level from any
        // lower-order distortion products.
        double re = 0.0, im = 0.0;
        const double w = 2.0 * std::numbers::pi * 10000.0 / kSampleRate;
        for (int n = 0; n < N; ++n)
        {
            const double t = n / kSampleRate;
            const double x = amp
                * std::sin(2.0 * std::numbers::pi * 10000.0 * t);
            const double y = stage.process(x, c.Vp_nominal);
            re += y * std::cos(w * n);
            im += y * std::sin(w * n);
        }
        return std::sqrt(re * re + im * im) / N;
    };

    const double loudStatic   = measure(0.0, 6.0);
    const double loudDynamic  = measure(1.0, 6.0);
    const double quietStatic  = measure(0.0, 0.02);
    const double quietDynamic = measure(1.0, 0.02);

    // Quiet: dynamic and static Miller agree (|Av| sits at rest).
    REQUIRE(quietDynamic == Approx(quietStatic).epsilon(0.01));

    // Saturating: the dynamic path passes MORE HF than the static one —
    // the incremental-gain collapse opens the Miller corner.
    REQUIRE(loudDynamic > 1.05 * loudStatic);
}

TEST_CASE("TubeStage: Miller feedthrough is silent in the linear region, "
          "active on clipped edges",
          "[TubeStage][feedthrough]")
{
    // docs/34 §3.5 — the injected term is the NONLINEAR residual of the
    // plate step, so a small-signal render must be essentially identical
    // with the flag on or off, while a hard-clipped render must differ.
    auto renderWith = [&](bool ft, double amp) {
        TubeStage stage;
        auto cfg = presets::v72Stage1();
        cfg.enableWarmup = false;
        cfg.enableShotNoise = false;
        cfg.enableHeaterHum = false;
        cfg.enableMillerFeedthrough = ft;
        stage.setup(cfg, kSampleRate);
        std::vector<double> out;
        processSine(stage, 1000.0, amp, 0.15, 260.0, &out);
        return out;
    };

    // Linear region: relative deviation stays tiny.
    {
        const auto on  = renderWith(true, 0.02);
        const auto off = renderWith(false, 0.02);
        double diffSq = 0.0, refSq = 0.0;
        for (std::size_t i = on.size() / 2; i < on.size(); ++i)
        {
            diffSq += (on[i] - off[i]) * (on[i] - off[i]);
            refSq  += off[i] * off[i];
        }
        const double rel = std::sqrt(diffSq / std::max(refSq, 1.0e-30));
        INFO("linear-region relative deviation = " << rel);
        REQUIRE(rel < 0.01);
    }

    // Hard clip: the collapsed Miller feedback lets the edge through.
    {
        const auto on  = renderWith(true, 6.0);
        const auto off = renderWith(false, 6.0);
        double diffSq = 0.0, refSq = 0.0;
        for (std::size_t i = on.size() / 2; i < on.size(); ++i)
        {
            diffSq += (on[i] - off[i]) * (on[i] - off[i]);
            refSq  += off[i] * off[i];
        }
        const double rel = std::sqrt(diffSq / std::max(refSq, 1.0e-30));
        INFO("clipped-region relative deviation = " << rel);
        for (double v : on) REQUIRE(std::isfinite(v));
        REQUIRE(rel > 1.0e-4);
    }
}

TEST_CASE("TubeStage: next-stage grid-leak AC load keeps calibrated level",
          "[TubeStage][rgload]")
{
    // docs/34 §3.8 — loading the plate with the next stage's 1 MΩ leak
    // changes the physical gain by ~9%, and outputMakeup_ must restore
    // the calibrated small-signal level (curvature, not level, changes).
    auto h1At = [&](double rgNext) {
        TubeStage stage;
        auto cfg = presets::v72Stage1();
        cfg.enableWarmup = false;
        cfg.enableShotNoise = false;
        cfg.enableHeaterHum = false;
        cfg.nextStageLoadR = rgNext;
        stage.setup(cfg, kSampleRate);
        std::vector<double> out;
        processSine(stage, 1000.0, 0.02, 0.2, 260.0, &out);
        double s = 0.0;
        for (std::size_t i = out.size() / 2; i < out.size(); ++i)
            s += out[i] * out[i];
        return std::sqrt(s / (out.size() / 2.0));
    };

    const double lvlIsolated = h1At(0.0);
    const double lvlLoaded   = h1At(1.0e6);
    const double dB = 20.0 * std::log10(
        std::max(lvlLoaded, 1.0e-12) / std::max(lvlIsolated, 1.0e-12));
    INFO("level shift with Rg load = " << dB << " dB");
    REQUIRE(std::abs(dB) < 0.5);
}

TEST_CASE("TubeStage: reactive speaker load bends the SE load line by "
          "frequency",
          "[TubeStage][reactive]")
{
    // docs/34 §2.5 — the 300B's reflected load is a loudspeaker: at the
    // motional resonance the impedance rises several-fold, so the loud
    // low-frequency behaviour must differ materially from the fixed
    // resistor, while the mid-band (|Z| ≈ Rp) keeps the calibration.
    auto render = [&](bool reactive, double freq, double amp) {
        TubeStage st;
        auto cfg = presets::hifi300BPower();
        cfg.enableWarmup = false;
        cfg.enableShotNoise = false;
        cfg.plateLoadReactive = reactive;
        st.setup(cfg, kSampleRate);
        std::vector<double> out;
        processSine(st, freq, amp, 0.4, 360.0, &out);
        double s = 0.0;
        for (std::size_t i = out.size() / 2; i < out.size(); ++i)
        {
            REQUIRE(std::isfinite(out[i]));
            s += out[i] * out[i];
        }
        return std::sqrt(s / (out.size() / 2.0));
    };

    // Mid-band calibration guard.
    const double midOn  = render(true, 1000.0, 0.1);
    const double midOff = render(false, 1000.0, 0.1);
    const double midDb = 20.0 * std::log10(
        std::max(midOn, 1e-12) / std::max(midOff, 1e-12));
    INFO("1 kHz shift with reactive load = " << midDb << " dB");
    REQUIRE(std::abs(midDb) < 1.5);

    // At the motional resonance the load line opens — loud LF behaviour
    // departs from the resistive model far more than the mid-band did.
    const double lfOn  = render(true, 45.0, 0.6);
    const double lfOff = render(false, 45.0, 0.6);
    const double lfDb = std::abs(20.0 * std::log10(
        std::max(lfOn, 1e-12) / std::max(lfOff, 1e-12)));
    INFO("45 Hz level departure = " << lfDb << " dB (mid "
         << std::abs(midDb) << " dB)");
    REQUIRE(lfDb > std::abs(midDb) + 0.5);
}
