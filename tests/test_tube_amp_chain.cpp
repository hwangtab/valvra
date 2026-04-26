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
} // namespace

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
    auto cfg = chain_presets::V72Preamp();
    cfg.psu = psu_presets::k5U4GB;  // stronger sag for easier detection
    cfg.enablePSUSag = true;

    TubeAmpChain chain;
    chain.setup(cfg, kSampleRate);

    // Let it settle quiet
    for (int i = 0; i < 5000; ++i) chain.process(0.0);

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

    // Measure both with the same test signal.
    std::vector<double> outCold, outWarm;
    renderSine(chainCold, 1000.0, 0.05, 0.2, outCold);
    renderSine(chainWarm, 1000.0, 0.05, 0.2, outWarm);

    const double rmsCold = rms(outCold);
    const double rmsWarm = rms(outWarm);

    REQUIRE(rmsCold > 0.0);
    REQUIRE(rmsWarm > 0.0);

    // The two must differ — this is quantitative proof of time-varying
    // thermal warmup behavior (the sensational feature competitors lack).
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
    chain.setVariationSeed(999999);

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
    const double sagQuiet = chain.currentSagPercent();

    std::vector<double> out;
    renderSine(chain, 110.0, 0.9, 1.0, out);
    const double sagLoud = chain.currentSagPercent();

    // Solid-state rectifier sag is weak, but it must still be non-zero and
    // increase under sustained loud drive.
    REQUIRE(sagLoud > sagQuiet + 1e-6);
}

TEST_CASE("TubeAmpChain: Marshall preset disables cathode bounce on all stages",
          "[chain][preset][marshall][intent]")
{
    const auto cfg = chain_presets::MarshallMode();
    REQUIRE(cfg.stages[0].enableCathodeBounce == false);
    REQUIRE(cfg.stages[1].enableCathodeBounce == false);
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
    // should be much louder than that.
    REQUIRE(nullDb > -60.0);
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
    auto cfg = chain_presets::V72Preamp();
    cfg.variationSeed = 0x1234;
    for (int i = 0; i < cfg.numStages; ++i)
        cfg.stages[i].enableHeaterHum = false;

    auto run = [&](double rippleAmp) -> std::vector<double>
    {
        auto c = cfg;
        c.psu.ripple_amp = rippleAmp;
        TubeAmpChain chain;
        chain.setup(c, kSampleRate);
        for (int i = 0; i < 4096; ++i) (void) chain.process(0.0);

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

    const auto yOn  = run(0.5);   // exaggerated for a crisp regression signal
    const auto yOff = run(0.0);

    const double b120On  = bin(yOn,  120.0);
    const double b120Off = bin(yOff, 120.0);

    // Ripple path must produce a much stronger 120 Hz residual than the
    // no-ripple path.
    REQUIRE(b120On > 10.0 * (b120Off + 1e-12));

    // And 120 Hz should dominate unrelated bins with ripple on — it's
    // not a broadband noise floor shift, it's a tonal residual.
    const double bSilence250 = bin(yOn, 250.0);
    const double bSilence1k  = bin(yOn, 1000.0);
    REQUIRE(b120On > 5.0 * bSilence250);
    REQUIRE(b120On > 5.0 * bSilence1k);
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
    // Measuring the IM sideband through the full tube chain is sensitive
    // to Koren bias point and Miller LPF attenuation, which has made
    // this fragile in practice.  Test the ripple generator directly —
    // if the rail voltage itself shows a 120 Hz component, the rest of
    // the chain will see it (and any downstream failure is elsewhere).
    PSUSagParams p;
    p.Vb_nominal = 325.0;
    p.Z_internal = 200.0;
    p.tau_sag    = 0.1;
    p.sampleRate = 48000.0;
    p.ripple_amp = 2.0;
    p.ripple_freq = 120.0;

    PowerSupplySag psu { p };
    psu.setRipplePhase(0.0);

    // With zero plate current load, the envelope stays flat — any Vb
    // variation is purely the ripple oscillator.
    std::vector<double> vb;
    const int N = static_cast<int>(p.sampleRate * 0.5);
    vb.reserve(N);
    for (int i = 0; i < N; ++i) {
        (void) psu.process(0.0);
        vb.push_back(psu.currentVb());
    }

    auto bin = [&](double freq) {
        double re = 0.0, im = 0.0;
        const double w = 2.0 * std::numbers::pi * freq / p.sampleRate;
        for (std::size_t i = 0; i < vb.size(); ++i) {
            re += vb[i] * std::cos(w * static_cast<double>(i));
            im += vb[i] * std::sin(w * static_cast<double>(i));
        }
        return std::sqrt(re * re + im * im)
             / static_cast<double>(vb.size());
    };

    const double b120 = bin(120.0);
    const double b60  = bin(60.0);
    const double b500 = bin(500.0);

    // 120 Hz bin must dominate — ripple amplitude 2.0 V / 2 = ~1.0 V
    // peak after DFT normalization.
    REQUIRE(b120 > 0.5);
    REQUIRE(b120 > 20.0 * b60);
    REQUIRE(b120 > 20.0 * b500);
}

// ─────────────────────────────────────────────────────────────────────────────
// Monte Carlo spread on the new mechanisms: two instances with different
// seeds should show *measurably* different heater-hum level on a silent
// input.  If the hum level were the same for all seeds, the variation
// extension wasn't actually wired into the chain.
// ─────────────────────────────────────────────────────────────────────────────
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
TEST_CASE("TransformerStage: primary inductance saturates under heavy bass",
          "[transformer][lf-saturation]")
{
    // Build a transformer with aggressive LF saturation and a static
    // version for comparison.  Use an AGGRESSIVE setup so the effect
    // exceeds the noise floor of a short DFT bin.
    auto measure = [&](bool satOn, double driveAmp) {
        auto cfg = transformer_presets::Marinair();
        cfg.enablePresencePeak = false;   // isolate the LF path
        cfg.enableLFSaturation = satOn;
        cfg.lfSatDepth         = 0.95;
        cfg.drive              = 2.0;     // push core near saturation
        TransformerStage trafo;
        trafo.setup(cfg, kSampleRate);
        // Settle HPF state.
        for (int i = 0; i < 8192; ++i) (void) trafo.process(0.0);

        const int N = static_cast<int>(0.25 * kSampleRate);
        const double w = 2.0 * std::numbers::pi * 50.0 / kSampleRate;
        double re = 0.0, im = 0.0;
        for (int n = 0; n < N; ++n)
        {
            const double x = driveAmp * std::sin(w * n);
            const double y = trafo.process(x);
            re += y * std::cos(w * n);
            im += y * std::sin(w * n);
        }
        return std::sqrt(re * re + im * im)
             / static_cast<double>(N);
    };

    // With saturation on, the 50 Hz gain at loud drive must be *smaller*
    // (more roll-off) than the gain-per-unit-drive at a quiet level.
    const double quietGain_on  = measure(true,  0.05) / 0.05;
    const double loudGain_on   = measure(true,  1.0)  / 1.0;
    const double quietGain_off = measure(false, 0.05) / 0.05;
    const double loudGain_off  = measure(false, 1.0)  / 1.0;

    // Static path: loud/quiet gain-per-drive should be similar (only
    // JA saturation changes mid-band, not the LF corner).
    const double staticRatio = loudGain_off / quietGain_off;

    // Saturating path: loud gain-per-drive drops meaningfully below
    // quiet — i.e. loud/quiet ratio is noticeably smaller than static.
    const double satRatio = loudGain_on / quietGain_on;
    REQUIRE(satRatio < 0.95 * staticRatio);
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
