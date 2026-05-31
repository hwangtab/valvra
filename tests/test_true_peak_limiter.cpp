// ─────────────────────────────────────────────────────────────────────────────
// test_true_peak_limiter.cpp — TruePeakLimiter validation
//
// Three properties anyone using a brickwall master limiter cares about:
//   1) When bypassed, the output equals the input shifted by the reported
//      lookahead.  No gain reduction.  No nonlinearity.
//   2) Below ceiling, the limiter is transparent (gain = 1) modulo the
//      lookahead delay.  This is the "doesn't touch quiet material" promise.
//   3) Above ceiling, the limiter holds the output below the ceiling with
//      a small over-shoot tolerance (≤ 0.5 dB on transient material per
//      ITU-R BS.1770-5 4× detection accuracy).
//
// Reference: docs/20 §4.8.
// ─────────────────────────────────────────────────────────────────────────────
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TruePeakLimiter.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

using namespace valvra::dsp;
using Catch::Approx;

namespace {

constexpr double kSampleRate = 48000.0;

std::vector<float> makeSine(double freq, double amp, int N)
{
    std::vector<float> v(static_cast<std::size_t>(N));
    for (int n = 0; n < N; ++n)
        v[static_cast<std::size_t>(n)] = static_cast<float>(
            amp * std::sin(2.0 * std::numbers::pi * freq * n / kSampleRate));
    return v;
}

float peakAbs(const std::vector<float>& v, int startN = 0)
{
    float p = 0.0f;
    for (std::size_t i = static_cast<std::size_t>(startN); i < v.size(); ++i)
        p = std::max(p, std::abs(v[i]));
    return p;
}

} // namespace

TEST_CASE("TruePeakLimiter: bypassed output equals delayed input",
          "[mastering][tp-limiter]")
{
    TruePeakLimiter lim;
    lim.prepare(kSampleRate);
    lim.setBypass(true);
    lim.setCeilingDb(-1.0f);

    constexpr int N = 4096;
    auto inL = makeSine(220.0, 0.4, N);
    auto inR = makeSine(220.0, 0.4, N);
    auto outL = inL;
    auto outR = inR;

    lim.process(outL.data(), outR.data(), N);

    const int lat = TruePeakLimiter::latencyInSamples();
    REQUIRE(lat > 0);

    // After lat samples, output[lat..] should equal input[0..N-lat] within
    // float epsilon.  Before lat, output is the warm-up zeros from the ring.
    float maxErr = 0.0f;
    for (int n = lat; n < N; ++n)
    {
        const float diff =
            outL[static_cast<std::size_t>(n)] - inL[static_cast<std::size_t>(n - lat)];
        maxErr = std::max(maxErr, std::abs(diff));
    }
    REQUIRE(maxErr == Approx(0.0f).margin(1e-6f));
}

TEST_CASE("TruePeakLimiter: below ceiling, no gain reduction",
          "[mastering][tp-limiter]")
{
    TruePeakLimiter lim;
    lim.prepare(kSampleRate);
    lim.setBypass(false);
    lim.setCeilingDb(-1.0f);  // ≈ 0.891 linear

    constexpr int N = 4096;
    // Sine well below ceiling — peak 0.5 ≈ −6 dBFS, ceiling is −1 dBFS.
    auto inL = makeSine(440.0, 0.5, N);
    auto inR = inL;
    auto outL = inL;
    auto outR = inR;

    lim.process(outL.data(), outR.data(), N);

    // The first kLookaheadSamples are tail-priming; ignore them.
    const int settle = TruePeakLimiter::latencyInSamples() + 64;

    // Gain reduction should remain at 0 dB (within 0.05 dB).
    REQUIRE(lim.gainReductionDb() == Approx(0.0f).margin(0.05f));

    // Energy should match the delayed input within float precision.
    float maxErr = 0.0f;
    for (int n = settle; n < N; ++n)
    {
        const float ref =
            inL[static_cast<std::size_t>(n - TruePeakLimiter::latencyInSamples())];
        maxErr = std::max(
            maxErr,
            std::abs(outL[static_cast<std::size_t>(n)] - ref));
    }
    // Below ceiling we should be bit-equivalent (subject to float rounding
    // in the multiply-by-1.0).  Allow generous epsilon for float ops.
    REQUIRE(maxErr < 1.0e-5f);
}

TEST_CASE("TruePeakLimiter: above ceiling, peaks held below threshold",
          "[mastering][tp-limiter]")
{
    TruePeakLimiter lim;
    lim.prepare(kSampleRate);
    lim.setBypass(false);
    lim.setCeilingDb(-1.0f);  // 0.8913 linear

    constexpr int N = 16384;       // ≈ 340 ms — plenty for envelope settle
    // Hot sine: peak 1.5 ≈ +3.5 dBFS, well above the −1 dBTP ceiling.
    auto inL = makeSine(440.0, 1.5, N);
    auto inR = inL;
    auto outL = inL;
    auto outR = inR;

    lim.process(outL.data(), outR.data(), N);

    // Skip the first ~50 ms — the smoother needs time to seat from gain=1
    // down to the steady-state limiting value.
    const int settle = static_cast<int>(0.05 * kSampleRate);
    const float ceilingLinear = 0.8913f;
    const float ceilingTolerance = 1.05f * ceilingLinear;  // +0.5 dB headroom

    const float p = peakAbs(outL, settle);
    INFO("Peak after settle: " << p << " (ceiling: " << ceilingLinear << ")");
    REQUIRE(p <= ceilingTolerance);

    // Engaged limiter must report some gain reduction.
    REQUIRE(lim.gainReductionDb() < -0.5f);
}

TEST_CASE("TruePeakLimiter: isolated transient is held below threshold",
          "[mastering][tp-limiter][transient]")
{
    TruePeakLimiter lim;
    lim.prepare(kSampleRate);
    lim.setBypass(false);
    lim.setCeilingDb(-1.0f);

    constexpr int N = 512;
    std::vector<float> outL(static_cast<std::size_t>(N), 0.0f);
    outL[0] = 1.5f;

    lim.process(outL.data(), nullptr, N);

    const float ceilingLinear = 0.8913f;
    const float p = peakAbs(outL);
    INFO("Impulse peak after limiting: " << p);
    REQUIRE(p <= ceilingLinear * 1.001f);
}

TEST_CASE("TruePeakLimiter: bypass reports no gain reduction",
          "[mastering][tp-limiter][bypass]")
{
    TruePeakLimiter lim;
    lim.prepare(kSampleRate);
    lim.setBypass(true);
    lim.setCeilingDb(-1.0f);

    constexpr int N = 4096;
    auto outL = makeSine(440.0, 1.5, N);

    lim.process(outL.data(), nullptr, N);

    REQUIRE(lim.gainReductionDb() == Approx(0.0f).margin(0.001f));
}

TEST_CASE("TruePeakLimiter: bypass transition ignores stale gain reduction",
          "[mastering][tp-limiter][bypass][transition]")
{
    TruePeakLimiter lim;
    lim.prepare(kSampleRate);
    lim.setCeilingDb(-6.0f);

    constexpr int N = 256;
    std::vector<float> hot(static_cast<std::size_t>(N), 1.5f);
    lim.setBypass(false);
    lim.process(hot.data(), nullptr, N);
    REQUIRE(lim.gainReductionDb() < -1.0f);

    std::vector<float> in(static_cast<std::size_t>(N), 0.25f);
    auto out = in;
    lim.setBypass(true);
    lim.process(out.data(), nullptr, N);

    const int lat = TruePeakLimiter::latencyInSamples();
    for (int n = lat; n < N; ++n)
        REQUIRE(out[static_cast<std::size_t>(n)]
                == Approx(in[static_cast<std::size_t>(n - lat)]).margin(1.0e-7f));
    REQUIRE(lim.gainReductionDb() == Approx(0.0f).margin(0.001f));
}

TEST_CASE("TruePeakLimiter: non-finite input is muted instead of propagated",
          "[mastering][tp-limiter][nan-guard]")
{
    TruePeakLimiter lim;
    lim.prepare(kSampleRate);
    lim.setBypass(true);

    constexpr int N = 256;
    std::vector<float> out(static_cast<std::size_t>(N), 0.25f);
    out[0] = std::numeric_limits<float>::quiet_NaN();
    out[1] = std::numeric_limits<float>::infinity();
    out[2] = -std::numeric_limits<float>::infinity();

    lim.process(out.data(), nullptr, N);

    for (float y : out)
        REQUIRE(std::isfinite(y));
    const int lat = TruePeakLimiter::latencyInSamples();
    REQUIRE(out[static_cast<std::size_t>(lat)] == Approx(0.0f).margin(1.0e-7f));
    REQUIRE(out[static_cast<std::size_t>(lat + 3)] == Approx(0.25f).margin(1.0e-7f));
}

TEST_CASE("TruePeakLimiter: stereo image preserved (single shared gain)",
          "[mastering][tp-limiter]")
{
    TruePeakLimiter lim;
    lim.prepare(kSampleRate);
    lim.setBypass(false);
    lim.setCeilingDb(-1.0f);

    constexpr int N = 8192;
    // L only is hot; R is quiet.  After limiting, L should still be larger
    // than R by the same ratio as the input — both channels share the same
    // gain so L:R ratio stays constant under reduction.
    auto inL = makeSine(330.0, 1.4, N);    // hot
    auto inR = makeSine(330.0, 0.2, N);    // quiet, well below ceiling
    auto outL = inL;
    auto outR = inR;

    lim.process(outL.data(), outR.data(), N);

    const int settle = static_cast<int>(0.05 * kSampleRate);
    const float pL = peakAbs(outL, settle);
    const float pR = peakAbs(outR, settle);

    // Input ratio is 1.4 / 0.2 = 7.  Output ratio should be the same
    // (within sampling phase tolerance) because the gain is shared.
    REQUIRE(pL / std::max(pR, 1.0e-6f) == Approx(7.0f).epsilon(0.05f));
}

TEST_CASE("TruePeakLimiter: setCeilingDb honours the new value",
          "[mastering][tp-limiter]")
{
    TruePeakLimiter lim;
    lim.prepare(kSampleRate);
    lim.setBypass(false);

    constexpr int N = 8192;
    auto inL = makeSine(440.0, 1.5, N);
    auto inR = inL;

    // Tighter ceiling = harder limiting
    {
        lim.setCeilingDb(-3.0f);  // 0.7079 linear
        auto outL = inL;
        auto outR = inR;
        lim.process(outL.data(), outR.data(), N);
        const float p = peakAbs(outL, static_cast<int>(0.05 * kSampleRate));
        REQUIRE(p <= 0.71f * 1.05f);  // ceiling + 0.5 dB tolerance
    }
    lim.reset();

    // Looser ceiling = lighter limiting
    {
        lim.setCeilingDb(-0.3f);  // 0.9661 linear
        auto outL = inL;
        auto outR = inR;
        lim.process(outL.data(), outR.data(), N);
        const float p = peakAbs(outL, static_cast<int>(0.05 * kSampleRate));
        REQUIRE(p <= 0.97f * 1.05f);
    }
}
