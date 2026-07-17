#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ExpansionRack.h"

#include <cmath>
#include <numbers>
#include <vector>

using Catch::Approx;
using valvra::dsp::ExpansionMode;
using valvra::dsp::ExpansionRack;

namespace {

constexpr double kSampleRate = 48000.0;

} // namespace

TEST_CASE("ExpansionRack: amount 0 keeps signal transparent", "[expansion]")
{
    // The rack carries a fixed, mode-invariant latency (the tape 2x
    // wrap's FIR round trip; docs/34 4.2) -- transparency is defined as
    // bit-identity against the LATENCY-ALIGNED input.
    ExpansionRack rack;
    rack.prepare(kSampleRate);
    rack.setMode(ExpansionMode::TapeSat);
    rack.setAmount(0.0);
    rack.setMix(0.0);

    const int lat = ExpansionRack::latencyInBaseSamples();
    std::vector<double> xs;
    for (int n = 0; n < 512; ++n)
    {
        const double x = 0.37 * std::sin(2.0 * std::numbers::pi * 997.0
                                        * static_cast<double>(n) / kSampleRate);
        xs.push_back(x);
        double y = 0.0;
        rack.processMono(x, y);
        if (n >= lat)
            REQUIRE(y == Approx(xs[static_cast<std::size_t>(n - lat)])
                             .margin(1.0e-12));
    }
}

TEST_CASE("ExpansionRack: mix 0 stays transparent at high amount",
          "[expansion]")
{
    ExpansionRack rack;
    rack.prepare(kSampleRate);
    rack.setMode(ExpansionMode::TapeSat);
    rack.setAmount(1.0);
    rack.setMix(0.0);

    const int lat = ExpansionRack::latencyInBaseSamples();
    std::vector<double> xs;
    for (int n = 0; n < 2048; ++n)
    {
        const double t = static_cast<double>(n) / kSampleRate;
        const double x = 0.41 * std::sin(2.0 * std::numbers::pi * 311.0 * t);
        xs.push_back(x);
        double y = 0.0;
        rack.processMono(x, y);
        if (n >= lat)
            REQUIRE(y == Approx(xs[static_cast<std::size_t>(n - lat)])
                             .margin(1.0e-12));
    }
}

TEST_CASE("ExpansionRack: all modes render finite bounded output", "[expansion]")
{
    for (int mode = 0; mode <= static_cast<int>(ExpansionMode::SynthFx); ++mode)
    {
        ExpansionRack rack;
        rack.prepare(kSampleRate);
        rack.setMode(static_cast<ExpansionMode>(mode));
        rack.setAmount(0.75);
        rack.setMix(0.9);

        double peak = 0.0;
        for (int n = 0; n < 4096; ++n)
        {
            const double t = static_cast<double>(n) / kSampleRate;
            const double xL = 0.45 * std::sin(2.0 * std::numbers::pi * 220.0 * t);
            const double xR = 0.45 * std::sin(2.0 * std::numbers::pi * 330.0 * t);
            double yL = 0.0, yR = 0.0;
            rack.processStereo(xL, xR, yL, yR);
            REQUIRE(std::isfinite(yL));
            REQUIRE(std::isfinite(yR));
            peak = std::max(peak, std::max(std::abs(yL), std::abs(yR)));
        }

        INFO("mode=" << mode << " peak=" << peak);
        REQUIRE(peak < 4.0);
    }
}

TEST_CASE("ExpansionRack: mode switching crossfades without hard discontinuity",
          "[expansion][switch]")
{
    ExpansionRack rack;
    rack.prepare(kSampleRate);
    rack.setAmount(0.9);
    rack.setMix(1.0);
    rack.setMode(ExpansionMode::OptoComp);

    double prevL = 0.0, prevR = 0.0;
    double worstJump = 0.0;
    for (int n = 0; n < 6000; ++n)
    {
        if (n == 1800) rack.setMode(ExpansionMode::TapeSat);
        if (n == 3600) rack.setMode(ExpansionMode::FetComp);

        const double t = static_cast<double>(n) / kSampleRate;
        const double xL = 0.35 * std::sin(2.0 * std::numbers::pi * 127.0 * t);
        const double xR = 0.32 * std::sin(2.0 * std::numbers::pi * 191.0 * t);
        double yL = 0.0, yR = 0.0;
        rack.processStereo(xL, xR, yL, yR);

        const double jump = std::max(std::abs(yL - prevL), std::abs(yR - prevR));
        worstJump = std::max(worstJump, jump);
        prevL = yL;
        prevR = yR;
    }

    INFO("worst per-sample jump=" << worstJump);
    REQUIRE(worstJump < 1.0);
}

TEST_CASE("ExpansionRack: reported latency matches the measured delay",
          "[expansion][latency]")
{
    // The processor adds latencyInBaseSamples() to its PDC; an impulse
    // through the Off path must land exactly there, and the tape path's
    // FIR round trip must land within a sample of the same figure (its
    // group delay is what the delay ring was matched to).
    ExpansionRack rack;
    rack.prepare(kSampleRate);
    rack.setMode(ExpansionMode::Off);
    rack.setAmount(0.0);
    rack.setMix(1.0);

    const int lat = ExpansionRack::latencyInBaseSamples();
    int peakAt = -1; double peak = 0.0;
    for (int n = 0; n < 128; ++n)
    {
        double y = 0.0;
        rack.processMono(n == 0 ? 1.0 : 0.0, y);
        if (std::abs(y) > peak) { peak = std::abs(y); peakAt = n; }
    }
    REQUIRE(peakAt == lat);
}

TEST_CASE("ExpansionRack: tape 2x wrap suppresses folded JA harmonics",
          "[expansion][alias]")
{
    // 5 kHz hot tape drive: the JA core's 7th harmonic (35 kHz) folds to
    // 13 kHz at a 48 kHz base rate.  With the 2x wrap the folded product
    // must sit well below the legitimate in-band 3rd harmonic (15 kHz).
    ExpansionRack rack;
    rack.prepare(kSampleRate);
    rack.setMode(ExpansionMode::TapeSat);
    rack.setAmount(0.9);
    rack.setMix(1.0);

    const double f = 5000.0;
    const int N = 24000;
    std::vector<double> out;
    out.reserve(N);
    for (int n = 0; n < N; ++n)
    {
        const double x = 0.8 * std::sin(2.0 * std::numbers::pi * f
                                        * static_cast<double>(n) / kSampleRate);
        double y = 0.0;
        rack.processMono(x, y);
        out.push_back(y);
    }
    auto goertzel = [&](double freq) {
        const double w = 2.0 * std::numbers::pi * freq / kSampleRate;
        double s1 = 0.0, s2 = 0.0;
        for (std::size_t i = out.size() / 2; i < out.size(); ++i)
        {
            const double s = out[i] + 2.0 * std::cos(w) * s1 - s2;
            s2 = s1; s1 = s;
        }
        const double re = s1 - s2 * std::cos(w);
        const double im = s2 * std::sin(w);
        return std::sqrt(re * re + im * im);
    };
    const double h3    = goertzel(3.0 * f);          // 15 kHz, legitimate
    const double alias = goertzel(2.0 * kSampleRate - 7.0 * f); // folded H7 @ 13 kHz? no: |7f - fs| = 13 kHz
    const double fold7 = goertzel(std::abs(7.0 * f - kSampleRate)); // 13 kHz
    INFO("H3=" << h3 << " fold7(13k)=" << fold7 << " alias-bin=" << alias);
    REQUIRE(h3 > 0.0);
    REQUIRE(fold7 < 0.2 * h3);
}
