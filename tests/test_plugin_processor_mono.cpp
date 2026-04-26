#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "PluginProcessor.h"
#include "TubeAmpChain.h"

#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

using Catch::Approx;

namespace {

void setChoiceParam(valvra::ValvraProcessor& proc,
                    const char* id,
                    int index)
{
    auto* p = dynamic_cast<juce::AudioParameterChoice*>(
        proc.parameters().getParameter(id));
    REQUIRE(p != nullptr);
    *p = index;
}

void setFloatParam(valvra::ValvraProcessor& proc,
                   const char* id,
                   float value)
{
    auto* p = dynamic_cast<juce::AudioParameterFloat*>(
        proc.parameters().getParameter(id));
    REQUIRE(p != nullptr);
    *p = value;
}

std::vector<float> runReferenceMono(const std::vector<float>& input,
                                    std::uint64_t seed,
                                    double sampleRate)
{
    using namespace valvra::dsp;

    auto cfg = chain_presets::V72Preamp();
    cfg.variationSeed = seed;

    TubeAmpChain chain;
    chain.setup(cfg, sampleRate);
    chain.setExternalPSUMode(true);
    chain.setExternalVb(cfg.psu.Vb_nominal);

    auto psuParams = cfg.psu;
    psuParams.sampleRate = sampleRate;
    PowerSupplySag sharedPSU { psuParams };
    sharedPSU.reset();

    const std::uint64_t rippleBits = seed ^ 0xF39E91E1A7E7A8B1ULL;
    const double ripplePhase =
        (static_cast<double>(rippleBits & 0xFFFFFFFFULL)
         / static_cast<double>(0xFFFFFFFFULL))
        * 2.0 * std::numbers::pi;
    sharedPSU.setRipplePhase(ripplePhase);

    std::vector<float> out;
    out.reserve(input.size());
    for (float x : input)
    {
        const double vb = sharedPSU.currentVb();
        chain.setExternalVb(vb);
        const double y = chain.process(static_cast<double>(x));
        sharedPSU.process(chain.lastTotalIp());
        out.push_back(static_cast<float>(y));
    }
    return out;
}

} // namespace

TEST_CASE("ValvraProcessor: mono path matches single-chain shared-PSU reference",
          "[plugin][mono][psu]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 256;

    valvra::ValvraProcessor proc;

    // Force deterministic, latency-free path for a direct sample-by-sample
    // reference comparison.
    setChoiceParam(proc, "preset", 0);      // V72
    setChoiceParam(proc, "oversample", 0);  // 1x
    setFloatParam(proc, "drive", 1.0f);
    setFloatParam(proc, "outputDb", 0.0f);
    setFloatParam(proc, "mix", 1.0f);

    juce::AudioProcessor::BusesLayout monoLayout;
    monoLayout.inputBuses .add(juce::AudioChannelSet::mono());
    monoLayout.outputBuses.add(juce::AudioChannelSet::mono());
    REQUIRE(proc.setBusesLayout(monoLayout));

    proc.prepareToPlay(kSampleRate, kBlockSize);

    std::vector<float> input(kBlockSize);
    for (int n = 0; n < kBlockSize; ++n)
    {
        const double t = static_cast<double>(n) / kSampleRate;
        input[static_cast<std::size_t>(n)] =
            static_cast<float>(0.18 * std::sin(2.0 * std::numbers::pi * 997.0 * t));
    }

    juce::AudioBuffer<float> buffer(1, kBlockSize);
    buffer.copyFrom(0, 0, input.data(), kBlockSize);
    juce::MidiBuffer midi;
    proc.processBlock(buffer, midi);

    const auto expected = runReferenceMono(input, proc.currentSeed(), kSampleRate);
    REQUIRE(expected.size() == input.size());

    auto* out = buffer.getReadPointer(0);
    for (int n = 0; n < kBlockSize; ++n)
    {
        REQUIRE(out[n] == Approx(expected[static_cast<std::size_t>(n)]).margin(1e-6f));
    }
}
