#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ExpansionRack.h"

#include <cmath>
#include <numbers>

using Catch::Approx;
using valvra::dsp::ExpansionMode;
using valvra::dsp::ExpansionRack;

namespace {

constexpr double kSampleRate = 48000.0;

} // namespace

TEST_CASE("ExpansionRack: amount 0 keeps signal transparent", "[expansion]")
{
    ExpansionRack rack;
    rack.prepare(kSampleRate);
    rack.setMode(ExpansionMode::TapeSat);
    rack.setAmount(0.0);
    rack.setMix(0.0);

    for (int n = 0; n < 512; ++n)
    {
        const double x = 0.37 * std::sin(2.0 * std::numbers::pi * 997.0
                                        * static_cast<double>(n) / kSampleRate);
        double y = 0.0;
        rack.processMono(x, y);
        REQUIRE(y == Approx(x).margin(1.0e-12));
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

    for (int n = 0; n < 2048; ++n)
    {
        const double t = static_cast<double>(n) / kSampleRate;
        const double x = 0.41 * std::sin(2.0 * std::numbers::pi * 311.0 * t);
        double y = 0.0;
        rack.processMono(x, y);
        REQUIRE(y == Approx(x).margin(1.0e-12));
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
