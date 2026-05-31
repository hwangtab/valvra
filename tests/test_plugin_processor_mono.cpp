#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "TubeAmpChain.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <array>
#include <cstdlib>
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

void setBoolParam(valvra::ValvraProcessor& proc,
                  const char* id,
                  bool value)
{
    auto* p = dynamic_cast<juce::AudioParameterBool*>(
        proc.parameters().getParameter(id));
    REQUIRE(p != nullptr);
    *p = value;
}

int choiceParamValue(valvra::ValvraProcessor& proc, const char* id)
{
    auto* p = dynamic_cast<juce::AudioParameterChoice*>(
        proc.parameters().getParameter(id));
    REQUIRE(p != nullptr);
    return p->getIndex();
}

float floatParamValue(valvra::ValvraProcessor& proc, const char* id)
{
    auto* p = dynamic_cast<juce::AudioParameterFloat*>(
        proc.parameters().getParameter(id));
    REQUIRE(p != nullptr);
    return p->get();
}

bool boolParamValue(valvra::ValvraProcessor& proc, const char* id)
{
    auto* p = dynamic_cast<juce::AudioParameterBool*>(
        proc.parameters().getParameter(id));
    REQUIRE(p != nullptr);
    return p->get();
}

double rmsBuffer(const juce::AudioBuffer<float>& buffer, int startSample = 0)
{
    double e = 0.0;
    int count = 0;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        for (int n = startSample; n < buffer.getNumSamples(); ++n)
        {
            const float v = buffer.getSample(ch, n);
            e += static_cast<double>(v) * static_cast<double>(v);
            ++count;
        }
    }
    return std::sqrt(e / std::max(count, 1));
}

void fillSine(juce::AudioBuffer<float>& buffer,
              double sampleRate,
              int offsetSamples,
              float amp = 0.25f)
{
    for (int n = 0; n < buffer.getNumSamples(); ++n)
    {
        const double t = static_cast<double>(offsetSamples + n) / sampleRate;
        const float x = static_cast<float>(
            amp * std::sin(2.0 * std::numbers::pi * 997.0 * t));
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.setSample(ch, n, x);
    }
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

template <typename T>
bool rectInside(const juce::Rectangle<T>& outer, const juce::Rectangle<T>& inner)
{
    return inner.getX() >= outer.getX()
        && inner.getY() >= outer.getY()
        && inner.getRight() <= outer.getRight()
        && inner.getBottom() <= outer.getBottom();
}

void requireValidBounds(const juce::Rectangle<int>& bounds,
                        const juce::Rectangle<int>& root,
                        const char* name,
                        float scale,
                        const char* tab)
{
    INFO("tab=" << tab << " scale=" << scale << " name=" << name
                << " bounds=" << bounds.toString().toStdString());
    REQUIRE(bounds.getWidth() > 0);
    REQUIRE(bounds.getHeight() > 0);
    REQUIRE(rectInside(root, bounds));
}

void requireNoOverlap(const juce::Rectangle<int>& a,
                      const juce::Rectangle<int>& b,
                      const char* aName,
                      const char* bName,
                      float scale,
                      const char* tab)
{
    INFO("tab=" << tab << " scale=" << scale
                << " overlap-check=" << aName << " vs " << bName
                << " a=" << a.toString().toStdString()
                << " b=" << b.toString().toStdString());
    REQUIRE_FALSE(a.intersects(b));
}

struct ScopedEnvOverride
{
    explicit ScopedEnvOverride(const char* key, const char* value)
        : key_ { key }
    {
        const char* prev = std::getenv(key_);
        if (prev != nullptr)
        {
            hadPrev_ = true;
            prevValue_ = prev;
        }
#if defined(_WIN32)
        _putenv_s(key_, value);
#else
        setenv(key_, value, 1);
#endif
    }

    ~ScopedEnvOverride()
    {
#if defined(_WIN32)
        if (hadPrev_)
            _putenv_s(key_, prevValue_.c_str());
        else
            _putenv_s(key_, "");
#else
        if (hadPrev_)
            setenv(key_, prevValue_.c_str(), 1);
        else
            unsetenv(key_);
#endif
    }

private:
    const char* key_ { nullptr };
    bool hadPrev_ { false };
    std::string prevValue_;
};

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
    setFloatParam(proc, "realismAmount", 0.0f);

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

    // The processor's reported latency includes the True Peak limiter's
    // lookahead window (always present, even when the limiter is bypassed,
    // so PDC stays consistent across toggles).  The reference renderer
    // does not include the limiter, so the processor's output is the
    // reference shifted right by `lat` samples; the first `lat` samples
    // are the limiter ring-buffer warm-up (zeros).  Compare accordingly.
    auto* out = buffer.getReadPointer(0);
    const int lat = proc.getLatencySamples();
    REQUIRE(lat < kBlockSize);
    for (int n = 0; n < lat; ++n)
        REQUIRE(out[n] == Approx(0.0f).margin(1e-6f));
    for (int n = lat; n < kBlockSize; ++n)
        REQUIRE(out[n] == Approx(expected[static_cast<std::size_t>(n - lat)])
                              .margin(1e-5f));
}

TEST_CASE("ValvraProcessor: chain builder parameters rebuild safely",
          "[plugin][chain-builder]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 128;

    valvra::ValvraProcessor proc;
    setChoiceParam(proc, "preset", 2);       // Culture Vulture native 3-stage
    setChoiceParam(proc, "oversample", 0);   // latency-free test path
    setChoiceParam(proc, "stageCount", 4);   // 4 stages
    setChoiceParam(proc, "inputTrafo", 1);   // input transformer off
    setChoiceParam(proc, "outputTrafo", 5);  // Lundahl output override
    setFloatParam(proc, "drive", 0.7f);
    setFloatParam(proc, "outputDb", 0.0f);
    setFloatParam(proc, "mix", 1.0f);

    juce::AudioProcessor::BusesLayout monoLayout;
    monoLayout.inputBuses .add(juce::AudioChannelSet::mono());
    monoLayout.outputBuses.add(juce::AudioChannelSet::mono());
    REQUIRE(proc.setBusesLayout(monoLayout));

    proc.prepareToPlay(kSampleRate, kBlockSize);

    juce::AudioBuffer<float> buffer(1, kBlockSize);
    for (int n = 0; n < kBlockSize; ++n)
    {
        const double t = static_cast<double>(n) / kSampleRate;
        buffer.setSample(0, n, static_cast<float>(
            0.12 * std::sin(2.0 * std::numbers::pi * 523.25 * t)));
    }

    juce::MidiBuffer midi;
    proc.processBlock(buffer, midi);

    double energy = 0.0;
    for (int n = 0; n < kBlockSize; ++n)
    {
        const float y = buffer.getSample(0, n);
        REQUIRE(std::isfinite(y));
        energy += static_cast<double>(y) * static_cast<double>(y);
    }
    REQUIRE(energy > 0.0);
    REQUIRE(proc.readBHState().Ms > 0.0f);
}

TEST_CASE("ValvraProcessor: Tier4 expansion engine changes wet output",
          "[plugin][expansion]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 512;

    auto runBlock = [&](int expansionMode, float amount, float mix)
    {
        valvra::ValvraProcessor proc;
        setChoiceParam(proc, "preset", 0);      // V72
        setChoiceParam(proc, "oversample", 0);  // 1x
        setFloatParam(proc, "drive", 1.0f);
        setFloatParam(proc, "outputDb", 0.0f);
        setFloatParam(proc, "mix", 1.0f);
        setChoiceParam(proc, "expansionMode", expansionMode);
        setFloatParam(proc, "expansionAmount", amount);
        setFloatParam(proc, "expansionMix", mix);

        juce::AudioProcessor::BusesLayout monoLayout;
        monoLayout.inputBuses .add(juce::AudioChannelSet::mono());
        monoLayout.outputBuses.add(juce::AudioChannelSet::mono());
        REQUIRE(proc.setBusesLayout(monoLayout));
        proc.prepareToPlay(kSampleRate, kBlockSize);

        // Force deterministic Monte Carlo character across helper invocations.
        proc.recallSeed(0x12345678ULL);
        juce::AudioBuffer<float> settle(1, kBlockSize);
        settle.clear();
        juce::MidiBuffer settleMidi;
        for (int i = 0; i < 8; ++i)
            proc.processBlock(settle, settleMidi);

        juce::AudioBuffer<float> buffer(1, kBlockSize);
        fillSine(buffer, kSampleRate, 0, 0.27f);
        juce::MidiBuffer midi;
        proc.processBlock(buffer, midi);

        std::vector<float> out(static_cast<std::size_t>(kBlockSize));
        for (int i = 0; i < kBlockSize; ++i)
            out[static_cast<std::size_t>(i)] = buffer.getSample(0, i);
        return out;
    };

    const auto base = runBlock(0, 0.0f, 1.0f);      // Off
    const auto tapeWet = runBlock(3, 1.0f, 1.0f);   // Tape with 100% wet
    REQUIRE(base.size() == tapeWet.size());

    double diffWet = 0.0;
    for (std::size_t i = 0; i < base.size(); ++i)
    {
        const double dWet =
            static_cast<double>(tapeWet[i]) - static_cast<double>(base[i]);
        diffWet += dWet * dWet;
    }
    const double rmsDiffWet = std::sqrt(diffWet / static_cast<double>(base.size()));
    REQUIRE(rmsDiffWet > 1.0e-4);
}

TEST_CASE("ValvraProcessor: bypass remains transparent when dither is enabled",
          "[plugin][bypass][dither][mastering]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 512;

    valvra::ValvraProcessor proc;
    setChoiceParam(proc, "oversample", 0);  // keep only the TP lookahead delay
    setBoolParam(proc, "ditherEnabled", true);
    setChoiceParam(proc, "ditherDepth", 0); // 16-bit would expose leaks clearly
    setBoolParam(proc, "bypass", true);

    juce::AudioProcessor::BusesLayout monoLayout;
    monoLayout.inputBuses .add(juce::AudioChannelSet::mono());
    monoLayout.outputBuses.add(juce::AudioChannelSet::mono());
    REQUIRE(proc.setBusesLayout(monoLayout));

    proc.prepareToPlay(kSampleRate, kBlockSize);

    std::vector<float> input(kBlockSize);
    juce::AudioBuffer<float> buffer(1, kBlockSize);
    for (int n = 0; n < kBlockSize; ++n)
    {
        const double t = static_cast<double>(n) / kSampleRate;
        input[static_cast<std::size_t>(n)] = static_cast<float>(
            0.23 * std::sin(2.0 * std::numbers::pi * 997.0 * t)
            + 0.07 * std::sin(2.0 * std::numbers::pi * 211.0 * t));
        buffer.setSample(0, n, input[static_cast<std::size_t>(n)]);
    }

    juce::MidiBuffer midi;
    proc.processBlock(buffer, midi);

    const int lat = proc.getLatencySamples();
    REQUIRE(lat > 0);
    REQUIRE(lat < kBlockSize);

    auto* out = buffer.getReadPointer(0);
    for (int n = 0; n < lat; ++n)
        REQUIRE(out[n] == Approx(0.0f).margin(1.0e-7f));
    for (int n = lat; n < kBlockSize; ++n)
        REQUIRE(out[n] == Approx(input[static_cast<std::size_t>(n - lat)])
                              .margin(1.0e-7f));
}

TEST_CASE("ValvraProcessor: neural blend alters output when enabled",
          "[plugin][neural]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 512;

    auto runBlock = [&](float neuralBlend)
    {
        valvra::ValvraProcessor proc;
        setChoiceParam(proc, "preset", 0);
        setChoiceParam(proc, "oversample", 0); // deterministic path
        setFloatParam(proc, "drive", 1.0f);
        setFloatParam(proc, "outputDb", 0.0f);
        setFloatParam(proc, "mix", 1.0f);
        setFloatParam(proc, "neuralBlend", neuralBlend);
        setBoolParam(proc, "tpEnabled", false);
        setChoiceParam(proc, "tpMode", 0);
        setBoolParam(proc, "bypass", false);

        juce::AudioProcessor::BusesLayout monoLayout;
        monoLayout.inputBuses .add(juce::AudioChannelSet::mono());
        monoLayout.outputBuses.add(juce::AudioChannelSet::mono());
        REQUIRE(proc.setBusesLayout(monoLayout));
        proc.prepareToPlay(kSampleRate, kBlockSize);

        juce::MidiBuffer midi;
        juce::AudioBuffer<float> buffer(1, kBlockSize);

        fillSine(buffer, kSampleRate, 0, 0.22f);          // settle
        proc.processBlock(buffer, midi);
        fillSine(buffer, kSampleRate, kBlockSize, 0.22f); // measure
        proc.processBlock(buffer, midi);

        std::vector<float> out(static_cast<std::size_t>(kBlockSize));
        auto* rd = buffer.getReadPointer(0);
        for (int n = 0; n < kBlockSize; ++n)
            out[static_cast<std::size_t>(n)] = rd[n];
        return out;
    };

    const auto physicsOnly = runBlock(0.0f);
    const auto blended     = runBlock(1.0f);
    REQUIRE(physicsOnly.size() == blended.size());

    std::size_t start = 0;
    while (start < physicsOnly.size()
           && std::abs(physicsOnly[start]) < 1.0e-9f
           && std::abs(blended[start]) < 1.0e-9f)
        ++start;

    double diffEnergy = 0.0;
    int count = 0;
    for (std::size_t i = start; i < physicsOnly.size(); ++i)
    {
        const double d = static_cast<double>(blended[i] - physicsOnly[i]);
        diffEnergy += d * d;
        ++count;
    }
    const double diffRms = std::sqrt(diffEnergy / std::max(count, 1));

    INFO("neural diff rms = " << diffRms);
    REQUIRE(diffRms > 1.0e-5);
}

TEST_CASE("ValvraProcessor: neural model loader fails cleanly on bad path",
          "[plugin][neural][loader]")
{
    valvra::ValvraProcessor proc;
    const bool ok = proc.loadNeuralModelFile("/path/that/does/not/exist.json");
    REQUIRE(ok == false);
    REQUIRE(proc.neuralModelLoaded() == false);
}

TEST_CASE("ValvraProcessor: neural model loader can unload cleanly",
          "[plugin][neural][loader]")
{
#if defined(VALVRA_USE_RTNEURAL) && VALVRA_USE_RTNEURAL
    static constexpr const char* kModelJson = R"json(
{
  "in_shape": [1, 5],
  "layers": [
    {
      "type": "dense",
      "shape": [1, 8],
      "activation": "tanh",
      "weights": [
        [
          [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
          [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
          [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
          [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
          [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
        ],
        [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
      ]
    },
    {
      "type": "dense",
      "shape": [1, 1],
      "weights": [
        [
          [0.0], [0.0], [0.0], [0.0],
          [0.0], [0.0], [0.0], [0.0]
        ],
        [0.0]
      ]
    }
  ]
}
)json";

    valvra::ValvraProcessor proc;
    const juce::File tempModel = juce::File::createTempFile(".json");
    REQUIRE(tempModel.replaceWithText(kModelJson));

    const bool loaded = proc.loadNeuralModelFile(tempModel.getFullPathName());
    REQUIRE(loaded == true);
    REQUIRE(proc.neuralModelLoaded() == true);

    const bool unloaded = proc.loadNeuralModelFile("");
    REQUIRE(unloaded == false);
    REQUIRE(proc.neuralModelLoaded() == false);

    const bool cleaned = tempModel.deleteFile();
    REQUIRE(cleaned);
#else
    SUCCEED("RTNeural is disabled in this build configuration");
#endif
}

TEST_CASE("ValvraProcessor: prepared neural load applies on audio thread boundary",
          "[plugin][neural][loader][realtime]")
{
#if defined(VALVRA_USE_RTNEURAL) && VALVRA_USE_RTNEURAL
    static constexpr const char* kModelJson = R"json(
{
  "in_shape": [1, 5],
  "layers": [
    {
      "type": "dense",
      "shape": [1, 8],
      "activation": "tanh",
      "weights": [
        [
          [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
          [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
          [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
          [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
          [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
        ],
        [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
      ]
    },
    {
      "type": "dense",
      "shape": [1, 1],
      "weights": [
        [
          [0.0], [0.0], [0.0], [0.0],
          [0.0], [0.0], [0.0], [0.0]
        ],
        [0.0]
      ]
    }
  ]
}
)json";

    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 256;

    valvra::ValvraProcessor proc;
    juce::AudioProcessor::BusesLayout monoLayout;
    monoLayout.inputBuses .add(juce::AudioChannelSet::mono());
    monoLayout.outputBuses.add(juce::AudioChannelSet::mono());
    REQUIRE(proc.setBusesLayout(monoLayout));
    proc.prepareToPlay(kSampleRate, kBlockSize);

    const juce::File tempModel = juce::File::createTempFile(".json");
    REQUIRE(tempModel.replaceWithText(kModelJson));

    const bool queuedLoad = proc.loadNeuralModelFile(tempModel.getFullPathName());
    REQUIRE(queuedLoad == true);

    juce::AudioBuffer<float> buffer(1, kBlockSize);
    buffer.clear();
    juce::MidiBuffer midi;
    for (int i = 0; i < 3; ++i)
        proc.processBlock(buffer, midi);
    REQUIRE(proc.neuralModelLoaded() == true);

    const bool queuedUnload = proc.loadNeuralModelFile("");
    REQUIRE(queuedUnload == false);
    for (int i = 0; i < 3; ++i)
        proc.processBlock(buffer, midi);
    REQUIRE(proc.neuralModelLoaded() == false);

    const bool cleaned = tempModel.deleteFile();
    REQUIRE(cleaned);
#else
    SUCCEED("RTNeural is disabled in this build configuration");
#endif
}

TEST_CASE("ValvraProcessor: neural model hot-swap keeps waveform step bounded",
          "[plugin][stress][continuity][neural-swap]")
{
#if defined(VALVRA_USE_RTNEURAL) && VALVRA_USE_RTNEURAL
    static constexpr const char* kModelJsonA = R"json(
{
  "in_shape": [1, 5],
  "layers": [
    {
      "type": "dense",
      "shape": [1, 8],
      "activation": "tanh",
      "weights": [
        [
          [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
          [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
          [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
          [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
          [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
        ],
        [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
      ]
    },
    {
      "type": "dense",
      "shape": [1, 1],
      "weights": [
        [
          [0.0], [0.0], [0.0], [0.0],
          [0.0], [0.0], [0.0], [0.0]
        ],
        [0.0]
      ]
    }
  ]
}
)json";

    static constexpr const char* kModelJsonB = R"json(
{
  "in_shape": [1, 5],
  "layers": [
    {
      "type": "dense",
      "shape": [1, 8],
      "activation": "tanh",
      "weights": [
        [
          [0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25],
          [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
          [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
          [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
          [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
        ],
        [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
      ]
    },
    {
      "type": "dense",
      "shape": [1, 1],
      "weights": [
        [
          [0.02], [0.02], [0.02], [0.02],
          [0.02], [0.02], [0.02], [0.02]
        ],
        [0.0]
      ]
    }
  ]
}
)json";

    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 256;
    constexpr int kBlocks = 220;

    const juce::File modelA = juce::File::createTempFile(".json");
    const juce::File modelB = juce::File::createTempFile(".json");
    REQUIRE(modelA.replaceWithText(kModelJsonA));
    REQUIRE(modelB.replaceWithText(kModelJsonB));

    auto runScenario = [&](bool mutate)
    {
        valvra::ValvraProcessor proc;
        setChoiceParam(proc, "preset", 1);
        setChoiceParam(proc, "oversample", 2); // 4x baseline
        setFloatParam(proc, "drive", 1.1f);
        setFloatParam(proc, "outputDb", 0.0f);
        setFloatParam(proc, "mix", 1.0f);
        setFloatParam(proc, "neuralBlend", 1.0f);
        setBoolParam(proc, "tpEnabled", false);
        setChoiceParam(proc, "tpMode", 0);
        setBoolParam(proc, "bypass", false);
        proc.setNullTestMode(false);

        juce::AudioProcessor::BusesLayout monoLayout;
        monoLayout.inputBuses .add(juce::AudioChannelSet::mono());
        monoLayout.outputBuses.add(juce::AudioChannelSet::mono());
        REQUIRE(proc.setBusesLayout(monoLayout));
        proc.prepareToPlay(kSampleRate, kBlockSize);

        REQUIRE(proc.loadNeuralModelFile(modelA.getFullPathName()) == true);
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> buffer(1, kBlockSize);
        buffer.clear();
        // Let the initial load swap settle.
        for (int i = 0; i < 4; ++i)
            proc.processBlock(buffer, midi);

        int offset = 0;
        bool hasPrev = false;
        float prev = 0.0f;
        double maxStep = 0.0;
        int swapCalls = 0;
        bool useA = false;

        for (int b = 0; b < kBlocks; ++b)
        {
            if (mutate)
            {
                if (b % 12 == 0)
                {
                    const bool ok = proc.loadNeuralModelFile(
                        useA ? modelA.getFullPathName() : modelB.getFullPathName());
                    REQUIRE(ok == true);
                    useA = ! useA;
                    ++swapCalls;
                }
            }

            for (int n = 0; n < kBlockSize; ++n)
            {
                const double t = static_cast<double>(offset + n) / kSampleRate;
                buffer.setSample(0, n, static_cast<float>(
                    0.34 * std::sin(2.0 * std::numbers::pi * 907.0 * t)
                  + 0.11 * std::sin(2.0 * std::numbers::pi * 1849.0 * t)));
            }
            offset += kBlockSize;

            proc.processBlock(buffer, midi);

            for (int n = 0; n < kBlockSize; ++n)
            {
                const float y = buffer.getSample(0, n);
                REQUIRE(std::isfinite(y));
                REQUIRE(std::abs(y) < 32.0f);
                if (hasPrev)
                    maxStep = std::max(maxStep, std::abs(static_cast<double>(y - prev)));
                prev = y;
                hasPrev = true;
            }
        }

        return std::pair<double, int> { maxStep, swapCalls };
    };

    const auto [baselineStep, baselineSwaps] = runScenario(false);
    const auto [mutatedStep, mutatedSwaps] = runScenario(true);
    INFO("neural-swap baseline step = " << baselineStep
         << ", mutated step = " << mutatedStep
         << ", swap calls = " << mutatedSwaps);

    REQUIRE(baselineStep > 1.0e-6);
    REQUIRE(baselineSwaps == 0);
    REQUIRE(mutatedSwaps > 0);
    REQUIRE(mutatedStep < baselineStep * 6.0 + 3.0e-4);

    REQUIRE(modelA.deleteFile());
    REQUIRE(modelB.deleteFile());
#else
    SUCCEED("RTNeural is disabled in this build configuration");
#endif
}

TEST_CASE("ValvraProcessor: bypass transition remains transparent after TP limiting",
          "[plugin][bypass][tp-limiter][transition]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 512;

    valvra::ValvraProcessor proc;
    setChoiceParam(proc, "oversample", 0);
    setFloatParam(proc, "drive", 0.0f);
    setFloatParam(proc, "outputDb", 0.0f);
    setFloatParam(proc, "mix", 0.0f);       // dry into TP limiter only
    setBoolParam(proc, "tpEnabled", true);
    setChoiceParam(proc, "tpMode", 2);      // brick-wall
    setFloatParam(proc, "tpCeilingDb", -3.0f);
    setBoolParam(proc, "bypass", false);

    juce::AudioProcessor::BusesLayout monoLayout;
    monoLayout.inputBuses .add(juce::AudioChannelSet::mono());
    monoLayout.outputBuses.add(juce::AudioChannelSet::mono());
    REQUIRE(proc.setBusesLayout(monoLayout));
    proc.prepareToPlay(kSampleRate, kBlockSize);

    juce::MidiBuffer midi;
    juce::AudioBuffer<float> hot(1, kBlockSize);
    for (int n = 0; n < kBlockSize; ++n)
        hot.setSample(0, n, 1.0f);
    proc.processBlock(hot, midi);
    REQUIRE(proc.gainReductionDb() < -1.0f);

    std::vector<float> input(static_cast<std::size_t>(kBlockSize), 0.25f);
    juce::AudioBuffer<float> bypassed(1, kBlockSize);
    bypassed.copyFrom(0, 0, input.data(), kBlockSize);
    setBoolParam(proc, "bypass", true);
    proc.processBlock(bypassed, midi);

    const int lat = proc.getLatencySamples();
    REQUIRE(lat > 0);
    REQUIRE(lat < kBlockSize);
    for (int n = lat; n < kBlockSize; ++n)
    {
        REQUIRE(bypassed.getSample(0, n)
                == Approx(input[static_cast<std::size_t>(n - lat)]).margin(1.0e-7f));
    }
    REQUIRE(proc.gainReductionDb() == Approx(0.0f).margin(0.001f));
}

TEST_CASE("ValvraProcessor: non-finite input does not leak through bypass",
          "[plugin][bypass][nan-guard]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 256;

    valvra::ValvraProcessor proc;
    setChoiceParam(proc, "oversample", 0);
    setBoolParam(proc, "bypass", true);

    juce::AudioProcessor::BusesLayout monoLayout;
    monoLayout.inputBuses .add(juce::AudioChannelSet::mono());
    monoLayout.outputBuses.add(juce::AudioChannelSet::mono());
    REQUIRE(proc.setBusesLayout(monoLayout));
    proc.prepareToPlay(kSampleRate, kBlockSize);

    juce::AudioBuffer<float> buffer(1, kBlockSize);
    buffer.clear();
    buffer.setSample(0, 0, std::numeric_limits<float>::quiet_NaN());
    buffer.setSample(0, 1, std::numeric_limits<float>::infinity());
    buffer.setSample(0, 2, -std::numeric_limits<float>::infinity());
    for (int n = 3; n < kBlockSize; ++n)
        buffer.setSample(0, n, 0.125f);

    juce::MidiBuffer midi;
    proc.processBlock(buffer, midi);

    for (int n = 0; n < kBlockSize; ++n)
        REQUIRE(std::isfinite(buffer.getSample(0, n)));

    const int lat = proc.getLatencySamples();
    REQUIRE(buffer.getSample(0, lat) == Approx(0.0f).margin(1.0e-7f));
    REQUIRE(buffer.getSample(0, lat + 3) == Approx(0.125f).margin(1.0e-7f));
}

TEST_CASE("ValvraProcessor: factory presets clear per-stage overrides",
          "[plugin][factory-presets][stage-editor]")
{
    valvra::ValvraProcessor proc;

    setChoiceParam(proc, "stage1_tube", 6);
    setChoiceParam(proc, "stage1_topology", 3);
    setFloatParam(proc, "stage1_drive", 7.5f);
    setFloatParam(proc, "stage1_bias", -0.35f);
    setChoiceParam(proc, "stage3_tube", 7);
    setChoiceParam(proc, "stage3_topology", 4);
    setFloatParam(proc, "stage3_drive", -4.0f);
    setFloatParam(proc, "stage3_bias", 0.25f);
    setChoiceParam(proc, "cvMode", 2);

    proc.loadFactoryPreset(0);

    for (const auto& s : valvra::ValvraProcessor::kStageParams)
    {
        REQUIRE(choiceParamValue(proc, s.tube) == 0);
        REQUIRE(choiceParamValue(proc, s.topology) == 0);
        REQUIRE(floatParamValue(proc, s.drive) == Approx(0.0f).margin(1e-6f));
        REQUIRE(floatParamValue(proc, s.bias) == Approx(0.0f).margin(1e-6f));
    }
    REQUIRE(choiceParamValue(proc, "cvMode") == 1);
}

TEST_CASE("ValvraProcessor: Chain Builder exposes v1 tube and topology set",
          "[plugin][chain-builder][docs20]")
{
    valvra::ValvraProcessor proc;

    auto* tube = dynamic_cast<juce::AudioParameterChoice*>(
        proc.parameters().getParameter("stage1_tube"));
    auto* topology = dynamic_cast<juce::AudioParameterChoice*>(
        proc.parameters().getParameter("stage1_topology"));
    auto* tpMode = dynamic_cast<juce::AudioParameterChoice*>(
        proc.parameters().getParameter("tpMode"));
    auto* tpCeiling = dynamic_cast<juce::AudioParameterFloat*>(
        proc.parameters().getParameter("tpCeilingDb"));
    auto* tpLookahead = dynamic_cast<juce::AudioParameterFloat*>(
        proc.parameters().getParameter("tpLookaheadMs"));
    auto* oversample = dynamic_cast<juce::AudioParameterChoice*>(
        proc.parameters().getParameter("oversample"));
    auto* mcDistribution = dynamic_cast<juce::AudioParameterChoice*>(
        proc.parameters().getParameter("mcDistribution"));
    auto* mcLock = dynamic_cast<juce::AudioParameterBool*>(
        proc.parameters().getParameter("mcLock"));
    auto* cvMode = dynamic_cast<juce::AudioParameterChoice*>(
        proc.parameters().getParameter("cvMode"));

    REQUIRE(tube != nullptr);
    REQUIRE(topology != nullptr);
    REQUIRE(tpMode != nullptr);
    REQUIRE(tpCeiling != nullptr);
    REQUIRE(tpLookahead != nullptr);
    REQUIRE(oversample != nullptr);
    REQUIRE(mcDistribution != nullptr);
    REQUIRE(mcLock != nullptr);
    REQUIRE(cvMode != nullptr);

    REQUIRE(tube->choices.contains("EL34"));
    REQUIRE(tube->choices.contains("6L6GC"));
    REQUIRE(topology->choices.contains("Long-Tailed Pair"));
    REQUIRE(tpMode->choices == juce::StringArray { "Off", "Soft", "Brick-wall" });
    REQUIRE(tpCeiling->range.start == Approx(-3.0f));
    REQUIRE(tpCeiling->range.end == Approx(-0.1f));
    REQUIRE(tpLookahead->range.start == Approx(1.0f));
    REQUIRE(tpLookahead->range.end == Approx(10.0f));
    REQUIRE(oversample->choices.contains("Insane (16x)"));
    REQUIRE(mcDistribution->choices
            == juce::StringArray { "Modern", "Vintage", "Warm", "Wild" });
    REQUIRE(cvMode->choices == juce::StringArray { "T", "P1", "P2" });
}

TEST_CASE("ValvraProcessor: legacy tpEnabled state migrates to tpMode",
          "[plugin][state][tp][migration]")
{
    auto stripParamId = [](juce::ValueTree& tree,
                           const juce::String& paramId,
                           const auto& self) -> void
    {
        static const juce::Identifier kIdProp { "id" };
        for (int i = tree.getNumChildren(); --i >= 0; )
        {
            auto child = tree.getChild(i);
            self(child, paramId, self);
            if (child.hasProperty(kIdProp)
                && child.getProperty(kIdProp).toString() == paramId)
            {
                tree.removeChild(i, nullptr);
            }
        }
    };

    juce::MemoryBlock modernState;
    {
        valvra::ValvraProcessor writer;
        setBoolParam(writer, "tpEnabled", true);
        setChoiceParam(writer, "tpMode", 0); // off + legacy bool on
        writer.getStateInformation(modernState);
    }

    auto xml = juce::AudioProcessor::getXmlFromBinary(
        modernState.getData(),
        static_cast<int>(modernState.getSize()));
    REQUIRE(xml != nullptr);
    auto tree = juce::ValueTree::fromXml(*xml);
    stripParamId(tree, "tpMode", stripParamId); // emulate old project state

    juce::MemoryBlock legacyState;
    if (auto legacyXml = tree.createXml())
        juce::AudioProcessor::copyXmlToBinary(*legacyXml, legacyState);
    REQUIRE(legacyState.getSize() > 0);

    valvra::ValvraProcessor reader;
    reader.setStateInformation(legacyState.getData(),
                               static_cast<int>(legacyState.getSize()));

    REQUIRE(choiceParamValue(reader, "tpMode") == 2);   // migrated to brick-wall
    REQUIRE(boolParamValue(reader, "tpEnabled") == false); // legacy latch cleared
}

TEST_CASE("ValvraProcessor: null-test mode persists across state save/load",
          "[plugin][state][null-test]")
{
    juce::MemoryBlock state;
    {
        valvra::ValvraProcessor writer;
        writer.setNullTestMode(true);
        writer.getStateInformation(state);
    }
    REQUIRE(state.getSize() > 0);

    valvra::ValvraProcessor reader;
    REQUIRE(! reader.nullTestMode());
    reader.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    REQUIRE(reader.nullTestMode());
}

TEST_CASE("ValvraProcessor: UI scale persists across state save/load",
          "[plugin][state][ui-scale]")
{
    juce::MemoryBlock state;
    {
        valvra::ValvraProcessor writer;
        writer.setUiScale(1.5f);
        writer.getStateInformation(state);
    }
    REQUIRE(state.getSize() > 0);

    valvra::ValvraProcessor reader;
    REQUIRE(reader.uiScale() == Approx(1.0f).margin(1.0e-6f));
    reader.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    REQUIRE(reader.uiScale() == Approx(1.5f).margin(1.0e-6f));
}

TEST_CASE("ValvraProcessor: legacy state without UI scale keeps default",
          "[plugin][state][ui-scale][migration]")
{
    juce::MemoryBlock modernState;
    {
        valvra::ValvraProcessor writer;
        writer.setUiScale(2.0f);
        writer.getStateInformation(modernState);
    }

    auto xml = juce::AudioProcessor::getXmlFromBinary(
        modernState.getData(),
        static_cast<int>(modernState.getSize()));
    REQUIRE(xml != nullptr);
    auto tree = juce::ValueTree::fromXml(*xml);
    tree.removeProperty("valvra_ui_scale", nullptr);

    juce::MemoryBlock legacyState;
    if (auto legacyXml = tree.createXml())
        juce::AudioProcessor::copyXmlToBinary(*legacyXml, legacyState);
    REQUIRE(legacyState.getSize() > 0);

    valvra::ValvraProcessor reader;
    REQUIRE(reader.uiScale() == Approx(1.0f).margin(1.0e-6f));
    reader.setStateInformation(legacyState.getData(),
                               static_cast<int>(legacyState.getSize()));
    REQUIRE(reader.uiScale() == Approx(1.0f).margin(1.0e-6f));
}

TEST_CASE("ValvraProcessor: practical calibration params default on but legacy loads off",
          "[plugin][state][calibration][migration]")
{
    valvra::ValvraProcessor fresh;
    REQUIRE(choiceParamValue(fresh, "levelMatchMode") == 1);
    REQUIRE(floatParamValue(fresh, "inputTrimDb") == Approx(0.0f).margin(1.0e-6f));
    REQUIRE(choiceParamValue(fresh, "targetProfile") == 0);
    REQUIRE(floatParamValue(fresh, "realismAmount") == Approx(0.35f).margin(1.0e-6f));

    auto stripParamId = [](juce::ValueTree& tree,
                           const juce::String& paramId,
                           const auto& self) -> void
    {
        static const juce::Identifier kIdProp { "id" };
        for (int i = tree.getNumChildren(); --i >= 0; )
        {
            auto child = tree.getChild(i);
            self(child, paramId, self);
            if (child.hasProperty(kIdProp)
                && child.getProperty(kIdProp).toString() == paramId)
            {
                tree.removeChild(i, nullptr);
            }
        }
    };

    juce::MemoryBlock modernState;
    {
        valvra::ValvraProcessor writer;
        setChoiceParam(writer, "levelMatchMode", 1);
        setFloatParam(writer, "inputTrimDb", 6.0f);
        writer.getStateInformation(modernState);
    }

    auto xml = juce::AudioProcessor::getXmlFromBinary(
        modernState.getData(),
        static_cast<int>(modernState.getSize()));
    REQUIRE(xml != nullptr);
    auto tree = juce::ValueTree::fromXml(*xml);
    stripParamId(tree, "levelMatchMode", stripParamId);
    stripParamId(tree, "inputTrimDb", stripParamId);
    stripParamId(tree, "analyzedOutputTrimDb", stripParamId);
    stripParamId(tree, "targetProfile", stripParamId);
    stripParamId(tree, "realismAmount", stripParamId);

    juce::MemoryBlock legacyState;
    if (auto legacyXml = tree.createXml())
        juce::AudioProcessor::copyXmlToBinary(*legacyXml, legacyState);
    REQUIRE(legacyState.getSize() > 0);

    valvra::ValvraProcessor reader;
    reader.setStateInformation(legacyState.getData(),
                               static_cast<int>(legacyState.getSize()));
    REQUIRE(choiceParamValue(reader, "levelMatchMode") == 0);
    REQUIRE(floatParamValue(reader, "inputTrimDb") == Approx(0.0f).margin(1.0e-6f));
    REQUIRE(choiceParamValue(reader, "targetProfile") == 0);
    REQUIRE(floatParamValue(reader, "realismAmount") == Approx(0.0f).margin(1.0e-6f));
}

TEST_CASE("ValvraProcessor: per-mode profile versions default fitted and migrate legacy",
          "[plugin][state][profile-version][migration]")
{
    juce::MemoryBlock modernState;
    {
        valvra::ValvraProcessor writer;
        writer.getStateInformation(modernState);
    }

    auto xml = juce::AudioProcessor::getXmlFromBinary(
        modernState.getData(),
        static_cast<int>(modernState.getSize()));
    REQUIRE(xml != nullptr);
    auto tree = juce::ValueTree::fromXml(*xml);
    static constexpr std::array<const char*, 5> kKeys {
        "valvra_profile_version_v72",
        "valvra_profile_version_console",
        "valvra_profile_version_cv",
        "valvra_profile_version_rndi",
        "valvra_profile_version_hifi",
    };
    for (const auto* key : kKeys)
    {
        REQUIRE(tree.hasProperty(key));
        REQUIRE(tree[key].toString() == "fitted_v1");
    }

    for (const auto* key : kKeys)
        tree.removeProperty(key, nullptr);
    juce::MemoryBlock legacyState;
    if (auto legacyXml = tree.createXml())
        juce::AudioProcessor::copyXmlToBinary(*legacyXml, legacyState);
    REQUIRE(legacyState.getSize() > 0);

    valvra::ValvraProcessor reader;
    reader.setStateInformation(legacyState.getData(),
                               static_cast<int>(legacyState.getSize()));

    juce::MemoryBlock roundTrip;
    reader.getStateInformation(roundTrip);
    auto roundTripXml = juce::AudioProcessor::getXmlFromBinary(
        roundTrip.getData(),
        static_cast<int>(roundTrip.getSize()));
    REQUIRE(roundTripXml != nullptr);
    auto roundTripTree = juce::ValueTree::fromXml(*roundTripXml);
    for (const auto* key : kKeys)
    {
        REQUIRE(roundTripTree.hasProperty(key));
        REQUIRE(roundTripTree[key].toString() == "legacy");
    }
}

TEST_CASE("ValvraProcessor: input calibration writes trim toward -18 dBFS",
          "[plugin][calibration][input-trim]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 2048;

    valvra::ValvraProcessor proc;
    juce::AudioProcessor::BusesLayout monoLayout;
    monoLayout.inputBuses .add(juce::AudioChannelSet::mono());
    monoLayout.outputBuses.add(juce::AudioChannelSet::mono());
    REQUIRE(proc.setBusesLayout(monoLayout));
    setChoiceParam(proc, "levelMatchMode", 0);
    setFloatParam(proc, "drive", 0.0f);
    setFloatParam(proc, "mix", 0.0f);
    proc.prepareToPlay(kSampleRate, kBlockSize);

    juce::AudioBuffer<float> buffer(1, kBlockSize);
    buffer.clear();
    const float minus24Rms = juce::Decibels::decibelsToGain(-24.0f);
    for (int n = 0; n < kBlockSize; ++n)
        buffer.setSample(0, n, minus24Rms);

    juce::MidiBuffer midi;
    proc.processBlock(buffer, midi);
    proc.calibrateInputToMinus18();

    REQUIRE(floatParamValue(proc, "inputTrimDb") == Approx(6.0f).margin(0.35f));
}

TEST_CASE("ValvraProcessor: mode trim is post-chain output gain",
          "[plugin][calibration][level-match]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 4096;

    valvra::ValvraProcessor proc;
    juce::AudioProcessor::BusesLayout monoLayout;
    monoLayout.inputBuses .add(juce::AudioChannelSet::mono());
    monoLayout.outputBuses.add(juce::AudioChannelSet::mono());
    REQUIRE(proc.setBusesLayout(monoLayout));
    setChoiceParam(proc, "preset", 3);       // RNDI mode trim = +9 dB
    setChoiceParam(proc, "oversample", 0);   // deterministic dry latency
    setChoiceParam(proc, "levelMatchMode", 1);
    setFloatParam(proc, "drive", 0.0f);
    setFloatParam(proc, "mix", 0.0f);
    setFloatParam(proc, "outputDb", 0.0f);
    proc.prepareToPlay(kSampleRate, kBlockSize);

    juce::AudioBuffer<float> buffer(1, kBlockSize);
    buffer.clear();
    for (int n = 0; n < kBlockSize; ++n)
        buffer.setSample(0, n, 0.01f);

    juce::MidiBuffer midi;
    proc.processBlock(buffer, midi);

    const double measured = rmsBuffer(buffer, proc.getLatencySamples() + 32);
    const double expected = 0.01 * juce::Decibels::decibelsToGain(9.0);
    REQUIRE(measured == Approx(expected).margin(expected * 0.12));
}

TEST_CASE("ValvraProcessor: analyze match writes finite clamped trim",
          "[plugin][calibration][analyze-match]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 1024;

    valvra::ValvraProcessor proc;
    juce::AudioProcessor::BusesLayout monoLayout;
    monoLayout.inputBuses .add(juce::AudioChannelSet::mono());
    monoLayout.outputBuses.add(juce::AudioChannelSet::mono());
    REQUIRE(proc.setBusesLayout(monoLayout));
    setChoiceParam(proc, "preset", 2);
    setChoiceParam(proc, "levelMatchMode", 0);
    setFloatParam(proc, "drive", 1.4f);
    setFloatParam(proc, "mix", 1.0f);
    proc.prepareToPlay(kSampleRate, kBlockSize);

    juce::MidiBuffer midi;
    for (int b = 0; b < 12; ++b)
    {
        juce::AudioBuffer<float> buffer(1, kBlockSize);
        fillSine(buffer, kSampleRate, b * kBlockSize, 0.08f);
        proc.processBlock(buffer, midi);
    }

    proc.analyzeLevelMatch();
    const float trim = floatParamValue(proc, "analyzedOutputTrimDb");
    REQUIRE(std::isfinite(trim));
    REQUIRE(trim >= -12.0f);
    REQUIRE(trim <= 12.0f);
    REQUIRE(choiceParamValue(proc, "levelMatchMode") == 2);
}

TEST_CASE("ValvraProcessor: analog realism is seed-deterministic and bounded",
          "[plugin][realism][determinism]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 1024;
    constexpr std::uint64_t kSeed = 0x123456789ABCDEFull;

    auto render = [&](std::uint64_t seed)
    {
        valvra::ValvraProcessor proc;
        proc.recallSeed(seed);
        setChoiceParam(proc, "preset", 0);
        setChoiceParam(proc, "oversample", 0);
        setChoiceParam(proc, "levelMatchMode", 0);
        setFloatParam(proc, "drive", 1.0f);
        setFloatParam(proc, "outputDb", 0.0f);
        setFloatParam(proc, "mix", 1.0f);
        setFloatParam(proc, "realismAmount", 0.70f);
        proc.prepareToPlay(kSampleRate, kBlockSize);

        juce::AudioBuffer<float> buffer(2, kBlockSize);
        fillSine(buffer, kSampleRate, 0, 0.08f);
        juce::MidiBuffer midi;
        proc.processBlock(buffer, midi);

        std::vector<float> out(static_cast<std::size_t>(kBlockSize * 2));
        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < kBlockSize; ++n)
                out[static_cast<std::size_t>(ch * kBlockSize + n)] =
                    buffer.getSample(ch, n);
        return out;
    };

    const auto a = render(kSeed);
    const auto b = render(kSeed);
    const auto c = render(kSeed ^ 0x5555ULL);
    REQUIRE(a.size() == b.size());
    REQUIRE(a.size() == c.size());

    double sameDiff = 0.0;
    double seedDiff = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        sameDiff += std::abs(static_cast<double>(a[i]) - b[i]);
        seedDiff += std::abs(static_cast<double>(a[i]) - c[i]);
        REQUIRE(std::isfinite(a[i]));
    }
    REQUIRE(sameDiff / static_cast<double>(a.size()) < 1.0e-7);
    REQUIRE(seedDiff / static_cast<double>(a.size()) > 1.0e-6);
}

TEST_CASE("ValvraProcessor: analog realism crosstalk and noise stay practical",
          "[plugin][realism][crosstalk][noise]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 4096;

    valvra::ValvraProcessor proc;
    setChoiceParam(proc, "preset", 0);
    setChoiceParam(proc, "oversample", 0);
    setChoiceParam(proc, "levelMatchMode", 0);
    setChoiceParam(proc, "targetProfile", 1); // V72 profile
    setFloatParam(proc, "drive", 0.0f);
    setFloatParam(proc, "mix", 0.0f);
    setFloatParam(proc, "outputDb", 0.0f);
    setFloatParam(proc, "realismAmount", 0.35f);
    proc.prepareToPlay(kSampleRate, kBlockSize);

    juce::AudioBuffer<float> buffer(2, kBlockSize);
    buffer.clear();
    for (int n = 0; n < kBlockSize; ++n)
    {
        const double t = static_cast<double>(n) / kSampleRate;
        buffer.setSample(0, n, static_cast<float>(
            0.25 * std::sin(2.0 * std::numbers::pi * 997.0 * t)));
    }

    juce::MidiBuffer midi;
    proc.processBlock(buffer, midi);
    const double left = rmsBuffer(buffer, proc.getLatencySamples() + 32);
    double rightEnergy = 0.0;
    int count = 0;
    for (int n = proc.getLatencySamples() + 32; n < kBlockSize; ++n)
    {
        const double r = buffer.getSample(1, n);
        rightEnergy += r * r;
        ++count;
    }
    const double right = std::sqrt(rightEnergy / std::max(count, 1));
    REQUIRE(left > 1.0e-3);
    REQUIRE(right > 1.0e-6);
    REQUIRE(right < left * 0.01);

    const auto snap = proc.readMasteringState();
    REQUIRE(snap.crosstalkDb < -60.0f);
    REQUIRE(snap.noiseFloorDbfs < -70.0f);
    REQUIRE(snap.noiseFloorDbfs > -120.0f);
}

TEST_CASE("ValvraProcessor: Monte Carlo Lock preserves seed across actions",
          "[plugin][monte-carlo][lock]")
{
    valvra::ValvraProcessor proc;
    const auto initial = proc.currentSeed();

    setBoolParam(proc, "mcLock", true);
    proc.reroll();
    REQUIRE(proc.currentSeed() == initial);

    proc.recallSeed(initial ^ 0xDEADBEEFULL);
    REQUIRE(proc.currentSeed() == initial);

    proc.loadFactoryPreset(0);
    REQUIRE(proc.currentSeed() == initial);
}

TEST_CASE("ValvraProcessor: bypassed blocks still consume reroll requests",
          "[plugin][bypass][monte-carlo][reroll]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 512;

    valvra::ValvraProcessor proc;
    juce::AudioProcessor::BusesLayout monoLayout;
    monoLayout.inputBuses .add(juce::AudioChannelSet::mono());
    monoLayout.outputBuses.add(juce::AudioChannelSet::mono());
    REQUIRE(proc.setBusesLayout(monoLayout));
    proc.prepareToPlay(kSampleRate, kBlockSize);

    setBoolParam(proc, "bypass", true);

    const auto targetSeed = proc.currentSeed() ^ 0x1234ABCDEFull;
    proc.recallSeed(targetSeed);

    juce::AudioBuffer<float> buffer(1, kBlockSize);
    buffer.clear();
    juce::MidiBuffer midi;
    proc.processBlock(buffer, midi);

    REQUIRE(proc.currentSeed() == targetSeed);
}

TEST_CASE("ValvraProcessor: rapid state toggles remain finite and recover",
          "[plugin][stress][state-toggle]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 256;
    constexpr int kBlocks = 220;

    valvra::ValvraProcessor proc;
    setChoiceParam(proc, "preset", 1);      // Marshall (nonlinear, stress-friendly)
    setChoiceParam(proc, "oversample", 2);  // 4x baseline
    setFloatParam(proc, "drive", 1.2f);
    setFloatParam(proc, "outputDb", 0.0f);
    setFloatParam(proc, "mix", 1.0f);
    setBoolParam(proc, "tpEnabled", true);
    setChoiceParam(proc, "tpMode", 2);      // brick-wall
    setFloatParam(proc, "tpCeilingDb", -1.0f);
    setFloatParam(proc, "tpLookaheadMs", 3.0f);
    setChoiceParam(proc, "msMode", 0);
    setBoolParam(proc, "bypass", false);

    juce::AudioProcessor::BusesLayout stereoLayout;
    stereoLayout.inputBuses .add(juce::AudioChannelSet::stereo());
    stereoLayout.outputBuses.add(juce::AudioChannelSet::stereo());
    REQUIRE(proc.setBusesLayout(stereoLayout));
    proc.prepareToPlay(kSampleRate, kBlockSize);

    std::uint64_t rng = 0xC001D00D12345678ULL;
    auto nextU32 = [&]() noexcept
    {
        rng = rng * 6364136223846793005ULL + 1ULL;
        return static_cast<std::uint32_t>(rng >> 32);
    };
    auto nextFloat = [&]() noexcept
    {
        return static_cast<float>(nextU32())
             / static_cast<float>(0xFFFFFFFFu);
    };

    const auto seedBefore = proc.currentSeed();
    bool sawSeedChange = false;

    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buffer(2, kBlockSize);
    int offset = 0;
    for (int b = 0; b < kBlocks; ++b)
    {
        if (b % 3 == 0)
            setBoolParam(proc, "bypass", (nextU32() & 1u) != 0u);
        if (b % 4 == 0)
            proc.toggleAB();
        if (b % 7 == 0)
            proc.copyToInactiveSlot();
        if (b % 5 == 0)
            proc.reroll();
        if (b % 6 == 0)
            setChoiceParam(proc, "msMode", static_cast<int>(nextU32() % 2u));
        if (b % 8 == 0)
            setChoiceParam(proc, "oversample", static_cast<int>(nextU32() % 5u));
        if (b % 9 == 0)
            setFloatParam(proc, "outputDb", -12.0f + 24.0f * nextFloat());

        for (int n = 0; n < kBlockSize; ++n)
        {
            const double t = static_cast<double>(offset + n) / kSampleRate;
            const float l = static_cast<float>(
                0.22 * std::sin(2.0 * std::numbers::pi * 733.0 * t)
              + 0.11 * std::sin(2.0 * std::numbers::pi * 1999.0 * t));
            const float r = static_cast<float>(
                0.19 * std::sin(2.0 * std::numbers::pi * 911.0 * t)
              - 0.09 * std::sin(2.0 * std::numbers::pi * 1553.0 * t));
            buffer.setSample(0, n, l);
            buffer.setSample(1, n, r);
        }
        offset += kBlockSize;

        proc.processBlock(buffer, midi);

        for (int ch = 0; ch < 2; ++ch)
        {
            for (int n = 0; n < kBlockSize; ++n)
            {
                const float y = buffer.getSample(ch, n);
                REQUIRE(std::isfinite(y));
                REQUIRE(std::abs(y) < 32.0f);
            }
        }

        if (proc.currentSeed() != seedBefore)
            sawSeedChange = true;
    }

    REQUIRE(sawSeedChange);

    // Recover to a deterministic active state and ensure audio remains alive.
    setBoolParam(proc, "bypass", false);
    setChoiceParam(proc, "oversample", 2);
    setChoiceParam(proc, "msMode", 0);
    setFloatParam(proc, "outputDb", 0.0f);

    double tailEnergy = 0.0;
    for (int b = 0; b < 6; ++b)
    {
        fillSine(buffer, kSampleRate, offset, 0.2f);
        offset += kBlockSize;
        proc.processBlock(buffer, midi);
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int n = 0; n < kBlockSize; ++n)
            {
                const float y = buffer.getSample(ch, n);
                REQUIRE(std::isfinite(y));
                tailEnergy += static_cast<double>(y) * y;
            }
        }
    }
    REQUIRE(tailEnergy > 1.0e-6);
}

TEST_CASE("ValvraProcessor: automation burst keeps waveform step bounded",
          "[plugin][stress][continuity]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 256;
    constexpr int kBlocks = 180;
    constexpr std::uint64_t kSeed = 0x5A17C9E2D44B128FULL;

    auto runScenario = [&](bool mutate)
    {
        valvra::ValvraProcessor proc;
        setChoiceParam(proc, "preset", 1);
        setChoiceParam(proc, "oversample", 2); // 4x
        setFloatParam(proc, "drive", 1.1f);
        setFloatParam(proc, "outputDb", 0.0f);
        setFloatParam(proc, "mix", 1.0f);
        setBoolParam(proc, "tpEnabled", true);
        setChoiceParam(proc, "tpMode", 2);
        setFloatParam(proc, "tpCeilingDb", -1.0f);
        setFloatParam(proc, "tpLookaheadMs", 3.0f);
        setChoiceParam(proc, "msMode", 0);
        setBoolParam(proc, "bypass", false);

        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses .add(juce::AudioChannelSet::stereo());
        layout.outputBuses.add(juce::AudioChannelSet::stereo());
        REQUIRE(proc.setBusesLayout(layout));
        proc.prepareToPlay(kSampleRate, kBlockSize);

        proc.recallSeed(kSeed);

        juce::MidiBuffer midi;
        juce::AudioBuffer<float> buffer(2, kBlockSize);
        int offset = 0;
        double maxStep = 0.0;
        bool hasPrev = false;
        float prev = 0.0f;

        for (int b = 0; b < kBlocks; ++b)
        {
            if (mutate)
            {
                if (b % 7 == 0)  proc.toggleAB();
                if (b % 9 == 0)  proc.reroll();
                if (b % 11 == 0) proc.copyToInactiveSlot();
                if (b % 13 == 0) setChoiceParam(proc, "msMode", (b / 13) % 2);
                if (b % 17 == 0) setChoiceParam(proc, "oversample", (b / 17) % 5);
                if (b % 19 == 0) setFloatParam(proc, "outputDb",
                                                ((b / 19) % 2 == 0) ? -3.0f : 2.5f);
            }

            for (int n = 0; n < kBlockSize; ++n)
            {
                const double t = static_cast<double>(offset + n) / kSampleRate;
                buffer.setSample(0, n, static_cast<float>(
                    0.24 * std::sin(2.0 * std::numbers::pi * 701.0 * t)
                  + 0.09 * std::sin(2.0 * std::numbers::pi * 1871.0 * t)));
                buffer.setSample(1, n, static_cast<float>(
                    0.21 * std::sin(2.0 * std::numbers::pi * 929.0 * t)
                  - 0.08 * std::sin(2.0 * std::numbers::pi * 1597.0 * t)));
            }
            offset += kBlockSize;

            proc.processBlock(buffer, midi);

            for (int n = 0; n < kBlockSize; ++n)
            {
                const float y = buffer.getSample(0, n);
                REQUIRE(std::isfinite(y));
                if (hasPrev)
                    maxStep = std::max(maxStep, std::abs(static_cast<double>(y - prev)));
                prev = y;
                hasPrev = true;
            }
        }
        return maxStep;
    };

    const double baselineStep = runScenario(false);
    const double mutatedStep  = runScenario(true);

    INFO("baseline step = " << baselineStep << ", mutated step = " << mutatedStep);
    REQUIRE(baselineStep > 1.0e-6);
    REQUIRE(mutatedStep < baselineStep * 6.0 + 1.0e-4);
}

TEST_CASE("ValvraProcessor: null-test continuity survives oversample switches",
          "[plugin][stress][continuity][null-test]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 256;
    constexpr int kBlocks = 150;

    auto runScenario = [&](bool mutateOversample)
    {
        valvra::ValvraProcessor proc;
        setChoiceParam(proc, "preset", 0);
        setChoiceParam(proc, "oversample", 2); // 4x baseline
        setFloatParam(proc, "drive", 1.0f);
        setFloatParam(proc, "outputDb", 0.0f);
        setFloatParam(proc, "mix", 1.0f);
        setBoolParam(proc, "tpEnabled", false);
        setChoiceParam(proc, "tpMode", 0);
        setBoolParam(proc, "bypass", false);

        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses .add(juce::AudioChannelSet::stereo());
        layout.outputBuses.add(juce::AudioChannelSet::stereo());
        REQUIRE(proc.setBusesLayout(layout));
        proc.prepareToPlay(kSampleRate, kBlockSize);
        proc.setNullTestMode(true);

        juce::MidiBuffer midi;
        juce::AudioBuffer<float> buffer(2, kBlockSize);

        bool hasPrev = false;
        float prevL = 0.0f;
        float prevR = 0.0f;
        double maxStep = 0.0;
        int offset = 0;

        for (int b = 0; b < kBlocks; ++b)
        {
            if (mutateOversample && (b % 11 == 0))
                setChoiceParam(proc, "oversample", (b / 11) % 5);

            for (int n = 0; n < kBlockSize; ++n)
            {
                const double t = static_cast<double>(offset + n) / kSampleRate;
                buffer.setSample(0, n, static_cast<float>(
                    0.20 * std::sin(2.0 * std::numbers::pi * 997.0 * t)
                  + 0.07 * std::sin(2.0 * std::numbers::pi * 2333.0 * t)));
                buffer.setSample(1, n, static_cast<float>(
                    0.18 * std::sin(2.0 * std::numbers::pi * 661.0 * t)
                  - 0.06 * std::sin(2.0 * std::numbers::pi * 1777.0 * t)));
            }
            offset += kBlockSize;

            proc.processBlock(buffer, midi);

            for (int n = 0; n < kBlockSize; ++n)
            {
                const float l = buffer.getSample(0, n);
                const float r = buffer.getSample(1, n);
                REQUIRE(std::isfinite(l));
                REQUIRE(std::isfinite(r));
                if (hasPrev)
                {
                    maxStep = std::max(maxStep,
                                       std::abs(static_cast<double>(l - prevL)));
                    maxStep = std::max(maxStep,
                                       std::abs(static_cast<double>(r - prevR)));
                }
                prevL = l;
                prevR = r;
                hasPrev = true;
            }
        }

        return maxStep;
    };

    const double baselineStep = runScenario(false);
    const double switchedStep = runScenario(true);

    INFO("null baseline step = " << baselineStep
         << ", null switched step = " << switchedStep);
    REQUIRE(baselineStep > 1.0e-6);
    REQUIRE(switchedStep < baselineStep * 8.0 + 2.0e-4);
}

TEST_CASE("ValvraProcessor: latency switch with bypass/null-test stays stable",
          "[plugin][stress][latency][bypass][null-test]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 256;
    constexpr int kBlocks = 170;

    auto runScenario = [&](bool mutate)
    {
        valvra::ValvraProcessor proc;
        setChoiceParam(proc, "preset", 1);
        setChoiceParam(proc, "oversample", 2); // 4x baseline
        setFloatParam(proc, "drive", 1.0f);
        setFloatParam(proc, "outputDb", 0.0f);
        setFloatParam(proc, "mix", 1.0f);
        setBoolParam(proc, "tpEnabled", false);
        setChoiceParam(proc, "tpMode", 0);
        setBoolParam(proc, "bypass", false);
        proc.setNullTestMode(false);

        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses .add(juce::AudioChannelSet::stereo());
        layout.outputBuses.add(juce::AudioChannelSet::stereo());
        REQUIRE(proc.setBusesLayout(layout));
        proc.prepareToPlay(kSampleRate, kBlockSize);

        juce::MidiBuffer midi;
        juce::AudioBuffer<float> buffer(2, kBlockSize);
        int offset = 0;

        bool hasPrev = false;
        float prevL = 0.0f;
        double maxStep = 0.0;
        int latencyChanges = 0;
        int lastLatency = proc.getLatencySamples();

        for (int b = 0; b < kBlocks; ++b)
        {
            if (mutate)
            {
                if (b % 10 == 0)
                    setChoiceParam(proc, "oversample", (b / 10) % 5);
                if (b % 14 == 0)
                    setBoolParam(proc, "bypass", ((b / 14) % 2) == 1);
                if (b % 12 == 0)
                    proc.setNullTestMode(((b / 12) % 2) == 1);
            }

            for (int n = 0; n < kBlockSize; ++n)
            {
                const double t = static_cast<double>(offset + n) / kSampleRate;
                buffer.setSample(0, n, static_cast<float>(
                    0.23 * std::sin(2.0 * std::numbers::pi * 887.0 * t)
                  + 0.08 * std::sin(2.0 * std::numbers::pi * 1559.0 * t)));
                buffer.setSample(1, n, static_cast<float>(
                    0.20 * std::sin(2.0 * std::numbers::pi * 613.0 * t)
                  - 0.07 * std::sin(2.0 * std::numbers::pi * 2017.0 * t)));
            }
            offset += kBlockSize;

            proc.processBlock(buffer, midi);

            const int lat = proc.getLatencySamples();
            if (lat != lastLatency)
            {
                ++latencyChanges;
                lastLatency = lat;
            }

            for (int n = 0; n < kBlockSize; ++n)
            {
                const float y = buffer.getSample(0, n);
                REQUIRE(std::isfinite(y));
                if (hasPrev)
                    maxStep = std::max(maxStep,
                                       std::abs(static_cast<double>(y - prevL)));
                prevL = y;
                hasPrev = true;
            }
        }

        return std::pair<double, int> { maxStep, latencyChanges };
    };

    const auto [baselineStep, baselineLatencyChanges] = runScenario(false);
    const auto [mutatedStep, mutatedLatencyChanges]   = runScenario(true);

    INFO("baseline step = " << baselineStep
         << ", mutated step = " << mutatedStep
         << ", baseline latency changes = " << baselineLatencyChanges
         << ", mutated latency changes = " << mutatedLatencyChanges);

    REQUIRE(baselineStep > 1.0e-6);
    REQUIRE(mutatedLatencyChanges > 0);
    REQUIRE(mutatedStep < baselineStep * 10.0 + 3.0e-4);
}

TEST_CASE("ValvraProcessor: TP mode switching keeps waveform continuity",
          "[plugin][stress][continuity][tp-mode]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 256;
    constexpr int kBlocks = 180;

    auto runScenario = [&](bool mutateTp)
    {
        valvra::ValvraProcessor proc;
        setChoiceParam(proc, "preset", 1);
        setChoiceParam(proc, "oversample", 2); // 4x baseline
        setFloatParam(proc, "drive", 1.0f);
        setFloatParam(proc, "outputDb", 0.0f);
        setFloatParam(proc, "mix", 1.0f);
        setBoolParam(proc, "tpEnabled", false);
        setChoiceParam(proc, "tpMode", 0); // Off
        setFloatParam(proc, "tpCeilingDb", -1.0f);
        setFloatParam(proc, "tpLookaheadMs", 2.5f);
        setBoolParam(proc, "bypass", false);
        proc.setNullTestMode(false);

        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses .add(juce::AudioChannelSet::stereo());
        layout.outputBuses.add(juce::AudioChannelSet::stereo());
        REQUIRE(proc.setBusesLayout(layout));
        proc.prepareToPlay(kSampleRate, kBlockSize);

        juce::MidiBuffer midi;
        juce::AudioBuffer<float> buffer(2, kBlockSize);
        int offset = 0;

        bool hasPrev = false;
        float prev = 0.0f;
        double maxStep = 0.0;

        for (int b = 0; b < kBlocks; ++b)
        {
            if (mutateTp)
            {
                if (b % 9 == 0)
                    setChoiceParam(proc, "tpMode", (b / 9) % 3); // Off/Soft/Brick-wall
                if (b % 11 == 0)
                    setBoolParam(proc, "tpEnabled", ((b / 11) % 2) == 1);
                if (b % 13 == 0)
                    setFloatParam(proc, "tpCeilingDb",
                                  ((b / 13) % 2 == 0) ? -2.7f : -0.3f);
                if (b % 15 == 0)
                    setFloatParam(proc, "tpLookaheadMs",
                                  ((b / 15) % 2 == 0) ? 1.2f : 8.5f);
            }

            for (int n = 0; n < kBlockSize; ++n)
            {
                const double t = static_cast<double>(offset + n) / kSampleRate;
                buffer.setSample(0, n, static_cast<float>(
                    0.42 * std::sin(2.0 * std::numbers::pi * 971.0 * t)
                  + 0.18 * std::sin(2.0 * std::numbers::pi * 2011.0 * t)));
                buffer.setSample(1, n, static_cast<float>(
                    0.39 * std::sin(2.0 * std::numbers::pi * 739.0 * t)
                  - 0.16 * std::sin(2.0 * std::numbers::pi * 1667.0 * t)));
            }
            offset += kBlockSize;

            proc.processBlock(buffer, midi);

            for (int ch = 0; ch < 2; ++ch)
            {
                for (int n = 0; n < kBlockSize; ++n)
                {
                    const float y = buffer.getSample(ch, n);
                    REQUIRE(std::isfinite(y));
                    REQUIRE(std::abs(y) < 32.0f);
                    if (ch == 0)
                    {
                        if (hasPrev)
                            maxStep = std::max(maxStep, std::abs(static_cast<double>(y - prev)));
                        prev = y;
                        hasPrev = true;
                    }
                }
            }
        }

        return maxStep;
    };

    const double baselineStep = runScenario(false);
    const double mutatedStep  = runScenario(true);
    INFO("TP baseline step = " << baselineStep
         << ", TP mutated step = " << mutatedStep);
    REQUIRE(baselineStep > 1.0e-6);
    REQUIRE(mutatedStep < baselineStep * 12.0 + 3.0e-4);
}

TEST_CASE("ValvraProcessor: AB toggle with TP mode switching stays continuous",
          "[plugin][stress][continuity][ab][tp-mode]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 256;
    constexpr int kBlocks = 190;

    auto runScenario = [&](bool mutate)
    {
        valvra::ValvraProcessor proc;
        setChoiceParam(proc, "preset", 1);
        setChoiceParam(proc, "oversample", 2);
        setFloatParam(proc, "drive", 1.0f);
        setFloatParam(proc, "outputDb", -2.0f);
        setFloatParam(proc, "mix", 1.0f);
        setBoolParam(proc, "tpEnabled", true);
        setChoiceParam(proc, "tpMode", 2); // brick-wall baseline
        setFloatParam(proc, "tpCeilingDb", -1.0f);
        setFloatParam(proc, "tpLookaheadMs", 2.5f);
        setBoolParam(proc, "bypass", false);
        proc.setNullTestMode(false);

        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses .add(juce::AudioChannelSet::stereo());
        layout.outputBuses.add(juce::AudioChannelSet::stereo());
        REQUIRE(proc.setBusesLayout(layout));
        proc.prepareToPlay(kSampleRate, kBlockSize);

        juce::MidiBuffer midi;
        juce::AudioBuffer<float> buffer(2, kBlockSize);
        int offset = 0;

        bool hasPrev = false;
        float prev = 0.0f;
        double maxStep = 0.0;
        bool toggledAB = false;

        for (int b = 0; b < kBlocks; ++b)
        {
            if (mutate)
            {
                if (b % 6 == 0)
                {
                    proc.toggleAB();
                    toggledAB = true;
                }
                if (b % 10 == 0)
                    proc.copyToInactiveSlot();
                if (b % 8 == 0)
                    setChoiceParam(proc, "tpMode", (b / 8) % 3);
                if (b % 11 == 0)
                    setBoolParam(proc, "tpEnabled", ((b / 11) % 2) == 1);
                if (b % 13 == 0)
                    setFloatParam(proc, "tpCeilingDb",
                                  ((b / 13) % 2 == 0) ? -2.4f : -0.4f);
                if (b % 15 == 0)
                    setFloatParam(proc, "tpLookaheadMs",
                                  ((b / 15) % 2 == 0) ? 1.1f : 9.2f);
            }

            for (int n = 0; n < kBlockSize; ++n)
            {
                const double t = static_cast<double>(offset + n) / kSampleRate;
                buffer.setSample(0, n, static_cast<float>(
                    0.40 * std::sin(2.0 * std::numbers::pi * 1009.0 * t)
                  + 0.14 * std::sin(2.0 * std::numbers::pi * 1847.0 * t)));
                buffer.setSample(1, n, static_cast<float>(
                    0.36 * std::sin(2.0 * std::numbers::pi * 751.0 * t)
                  - 0.12 * std::sin(2.0 * std::numbers::pi * 2141.0 * t)));
            }
            offset += kBlockSize;

            proc.processBlock(buffer, midi);

            for (int n = 0; n < kBlockSize; ++n)
            {
                const float y = buffer.getSample(0, n);
                REQUIRE(std::isfinite(y));
                REQUIRE(std::abs(y) < 32.0f);
                if (hasPrev)
                    maxStep = std::max(maxStep,
                                       std::abs(static_cast<double>(y - prev)));
                prev = y;
                hasPrev = true;
            }
        }

        return std::pair<double, bool> { maxStep, toggledAB };
    };

    const auto [baselineStep, baselineToggled] = runScenario(false);
    const auto [mutatedStep, mutatedToggled]   = runScenario(true);
    INFO("AB+TP baseline step = " << baselineStep
         << ", AB+TP mutated step = " << mutatedStep);

    REQUIRE(baselineStep > 1.0e-6);
    REQUIRE(! baselineToggled);
    REQUIRE(mutatedToggled);
    REQUIRE(mutatedStep < baselineStep * 14.0 + 4.0e-4);
}

TEST_CASE("ValvraProcessor: factory-load plus reroll stays continuous",
          "[plugin][stress][continuity][factory][reroll]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 256;
    constexpr int kBlocks = 200;

    auto runScenario = [&](bool mutate)
    {
        valvra::ValvraProcessor proc;
        setChoiceParam(proc, "preset", 0);
        setChoiceParam(proc, "oversample", 2);
        setFloatParam(proc, "drive", 1.0f);
        setFloatParam(proc, "outputDb", 0.0f);
        setFloatParam(proc, "mix", 1.0f);
        setBoolParam(proc, "tpEnabled", true);
        setChoiceParam(proc, "tpMode", 2);
        setFloatParam(proc, "tpCeilingDb", -1.0f);
        setFloatParam(proc, "tpLookaheadMs", 2.0f);
        setBoolParam(proc, "bypass", false);
        proc.setNullTestMode(false);

        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses .add(juce::AudioChannelSet::stereo());
        layout.outputBuses.add(juce::AudioChannelSet::stereo());
        REQUIRE(proc.setBusesLayout(layout));
        proc.prepareToPlay(kSampleRate, kBlockSize);

        juce::MidiBuffer midi;
        juce::AudioBuffer<float> buffer(2, kBlockSize);
        int offset = 0;

        bool hasPrev = false;
        float prev = 0.0f;
        double maxStep = 0.0;
        bool sawFactoryLoad = false;
        bool sawSeedChange = false;
        std::uint64_t lastSeed = proc.currentSeed();

        for (int b = 0; b < kBlocks; ++b)
        {
            if (mutate)
            {
                if (b % 16 == 0)
                {
                    proc.loadFactoryPreset((b / 16) % 5);
                    sawFactoryLoad = true;
                }
                if (b % 9 == 0)
                    proc.reroll();
                if (b % 12 == 0)
                    setChoiceParam(proc, "oversample", (b / 12) % 5);
                if (b % 14 == 0)
                    proc.toggleAB();
                if (b % 20 == 0)
                    proc.copyToInactiveSlot();
            }

            for (int n = 0; n < kBlockSize; ++n)
            {
                const double t = static_cast<double>(offset + n) / kSampleRate;
                buffer.setSample(0, n, static_cast<float>(
                    0.38 * std::sin(2.0 * std::numbers::pi * 941.0 * t)
                  + 0.13 * std::sin(2.0 * std::numbers::pi * 1987.0 * t)));
                buffer.setSample(1, n, static_cast<float>(
                    0.34 * std::sin(2.0 * std::numbers::pi * 683.0 * t)
                  - 0.11 * std::sin(2.0 * std::numbers::pi * 1723.0 * t)));
            }
            offset += kBlockSize;

            proc.processBlock(buffer, midi);

            const auto seedNow = proc.currentSeed();
            if (seedNow != lastSeed)
            {
                sawSeedChange = true;
                lastSeed = seedNow;
            }

            for (int n = 0; n < kBlockSize; ++n)
            {
                const float y = buffer.getSample(0, n);
                REQUIRE(std::isfinite(y));
                REQUIRE(std::abs(y) < 32.0f);
                if (hasPrev)
                    maxStep = std::max(maxStep, std::abs(static_cast<double>(y - prev)));
                prev = y;
                hasPrev = true;
            }
        }

        return std::tuple<double, bool, bool> { maxStep, sawFactoryLoad, sawSeedChange };
    };

    const auto [baselineStep, baselineFactory, baselineSeedChange] = runScenario(false);
    const auto [mutatedStep, mutatedFactory, mutatedSeedChange] = runScenario(true);
    INFO("factory+reroll baseline step = " << baselineStep
         << ", mutated step = " << mutatedStep);

    REQUIRE(baselineStep > 1.0e-6);
    REQUIRE(! baselineFactory);
    REQUIRE(! baselineSeedChange);
    REQUIRE(mutatedFactory);
    REQUIRE(mutatedSeedChange);
    REQUIRE(mutatedStep < baselineStep * 16.0 + 5.0e-4);
}

TEST_CASE("ValvraProcessor: A/B compare uses integrated loudness match",
          "[plugin][ab][lufs]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 512;

    valvra::ValvraProcessor proc;
    setChoiceParam(proc, "oversample", 0);
    setFloatParam(proc, "drive", 0.0f);
    setFloatParam(proc, "mix", 0.0f);       // dry-only isolates output/match gain
    setBoolParam(proc, "tpEnabled", false);
    setChoiceParam(proc, "tpMode", 0);

    juce::AudioProcessor::BusesLayout monoLayout;
    monoLayout.inputBuses .add(juce::AudioChannelSet::mono());
    monoLayout.outputBuses.add(juce::AudioChannelSet::mono());
    REQUIRE(proc.setBusesLayout(monoLayout));
    proc.prepareToPlay(kSampleRate, kBlockSize);

    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buffer(1, kBlockSize);

    setFloatParam(proc, "outputDb", -12.0f); // slot A
    int offset = 0;
    for (int i = 0; i < 80; ++i)
    {
        fillSine(buffer, kSampleRate, offset);
        proc.processBlock(buffer, midi);
        offset += kBlockSize;
    }

    proc.toggleAB();                         // first B visit starts as A
    setFloatParam(proc, "outputDb", 0.0f);   // slot B, 12 dB louder
    double bRms = 0.0;
    for (int i = 0; i < 80; ++i)
    {
        fillSine(buffer, kSampleRate, offset);
        proc.processBlock(buffer, midi);
        offset += kBlockSize;
        if (i > 70)
            bRms = rmsBuffer(buffer, proc.getLatencySamples());
    }

    proc.toggleAB();                         // restore A, matched up to B
    double matchedARms = 0.0;
    for (int i = 0; i < 40; ++i)
    {
        fillSine(buffer, kSampleRate, offset);
        proc.processBlock(buffer, midi);
        offset += kBlockSize;
        if (i > 30)
            matchedARms = rmsBuffer(buffer, proc.getLatencySamples());
    }

    REQUIRE(floatParamValue(proc, "outputDb") == Approx(-12.0f).margin(1.0e-3f));
    REQUIRE(bRms > 0.01);
    REQUIRE(matchedARms > 0.01);
    const double deltaDb = 20.0 * std::log10(matchedARms / bRms);
    INFO("matched A vs B delta = " << deltaDb << " dB");
    REQUIRE(std::abs(deltaDb) < 1.0);
}

TEST_CASE("ValvraProcessor: copyToInactiveSlot preserves loudness-match context",
          "[plugin][ab][copy]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 512;

    valvra::ValvraProcessor proc;
    setChoiceParam(proc, "oversample", 0);
    setFloatParam(proc, "drive", 0.0f);
    setFloatParam(proc, "mix", 0.0f);      // dry-only for deterministic level
    setBoolParam(proc, "tpEnabled", false);
    setChoiceParam(proc, "tpMode", 0);

    juce::AudioProcessor::BusesLayout monoLayout;
    monoLayout.inputBuses .add(juce::AudioChannelSet::mono());
    monoLayout.outputBuses.add(juce::AudioChannelSet::mono());
    REQUIRE(proc.setBusesLayout(monoLayout));
    proc.prepareToPlay(kSampleRate, kBlockSize);

    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buffer(1, kBlockSize);
    int offset = 0;

    // Slot A at -12 dB
    setFloatParam(proc, "outputDb", -12.0f);
    for (int i = 0; i < 80; ++i)
    {
        fillSine(buffer, kSampleRate, offset);
        proc.processBlock(buffer, midi);
        offset += kBlockSize;
    }

    // Slot B at +12 dB
    proc.toggleAB();
    setFloatParam(proc, "outputDb", 12.0f);

    double bRms = 0.0;
    for (int i = 0; i < 80; ++i)
    {
        fillSine(buffer, kSampleRate, offset);
        proc.processBlock(buffer, midi);
        offset += kBlockSize;
        if (i > 70)
            bRms = rmsBuffer(buffer, proc.getLatencySamples());
    }
    REQUIRE(bRms > 0.01);

    // Copy live B to inactive A and switch to A. Since both slots now carry
    // the same state+loudness context, toggling should not create a level jump.
    proc.copyToInactiveSlot();
    proc.toggleAB();

    double copiedARms = 0.0;
    for (int i = 0; i < 40; ++i)
    {
        fillSine(buffer, kSampleRate, offset);
        proc.processBlock(buffer, midi);
        offset += kBlockSize;
        if (i > 30)
            copiedARms = rmsBuffer(buffer, proc.getLatencySamples());
    }
    REQUIRE(copiedARms > 0.01);

    const double deltaDb = 20.0 * std::log10(copiedARms / bRms);
    INFO("copied A vs B delta = " << deltaDb << " dB");
    REQUIRE(std::abs(deltaDb) < 0.5);
}

TEST_CASE("ValvraProcessor: copyAToB/copyBToA support explicit directional copy",
          "[plugin][ab][copy][directional]")
{
    valvra::ValvraProcessor proc;
    setFloatParam(proc, "outputDb", -6.0f); // live A

    proc.copyAToB();        // B <- A
    proc.toggleAB();        // switch to B
    REQUIRE(floatParamValue(proc, "outputDb") == Approx(-6.0f).margin(1.0e-3f));
    REQUIRE(proc.isOnSlotB());

    setFloatParam(proc, "outputDb", 3.0f); // live B
    proc.copyBToA();        // A <- B
    proc.toggleAB();        // switch to A
    REQUIRE(floatParamValue(proc, "outputDb") == Approx(3.0f).margin(1.0e-3f));
    REQUIRE(! proc.isOnSlotB());

    setFloatParam(proc, "outputDb", -2.0f); // live A
    proc.copyAToB();         // B <- A
    proc.toggleAB();         // switch to B
    REQUIRE(floatParamValue(proc, "outputDb") == Approx(-2.0f).margin(1.0e-3f));
    REQUIRE(proc.isOnSlotB());

    setFloatParam(proc, "outputDb", 5.0f); // live B
    proc.copyAToB(); // active destination(B) should apply immediately
    REQUIRE(floatParamValue(proc, "outputDb") == Approx(-2.0f).margin(1.0e-3f));
}

TEST_CASE("ValvraProcessor: resetAB keeps live state and re-arms slot A baseline",
          "[plugin][ab][reset]")
{
    valvra::ValvraProcessor proc;
    setFloatParam(proc, "outputDb", -12.0f); // A
    proc.toggleAB();                          // B
    setFloatParam(proc, "outputDb", 6.0f);   // live B
    REQUIRE(proc.isOnSlotB());

    proc.resetAB();
    REQUIRE(! proc.isOnSlotB());
    REQUIRE(floatParamValue(proc, "outputDb") == Approx(6.0f).margin(1.0e-3f));

    proc.toggleAB(); // first B visit after reset should mirror A (same live)
    REQUIRE(proc.isOnSlotB());
    REQUIRE(floatParamValue(proc, "outputDb") == Approx(6.0f).margin(1.0e-3f));
}

TEST_CASE("ValvraProcessor: blind A/B mode randomizes compare target",
          "[plugin][ab][blind]")
{
    valvra::ValvraProcessor proc;

    setFloatParam(proc, "outputDb", -12.0f); // live A
    proc.toggleAB();
    setFloatParam(proc, "outputDb", 6.0f);   // live B
    proc.toggleAB();                          // back to A

    REQUIRE(! proc.abBlindMode());
    proc.setABBlindMode(true);
    REQUIRE(proc.abBlindMode());

    bool sawA = false;
    bool sawB = false;
    for (int i = 0; i < 64; ++i)
    {
        proc.toggleABForCompare();
        const float outDb = floatParamValue(proc, "outputDb");
        if (std::abs(outDb - (-12.0f)) < 1.0e-3f) sawA = true;
        if (std::abs(outDb - 6.0f) < 1.0e-3f) sawB = true;
    }

    REQUIRE(sawA);
    REQUIRE(sawB);

    proc.setABBlindMode(false);
    const bool slotBefore = proc.isOnSlotB();
    proc.toggleABForCompare();
    REQUIRE(proc.isOnSlotB() != slotBefore);
}

TEST_CASE("ValvraProcessor: snapshot C/D/E load-store and state persistence",
          "[plugin][ab][snapshot][state]")
{
    valvra::ValvraProcessor writer;

    setFloatParam(writer, "outputDb", -9.0f);
    writer.storeSnapshot(valvra::ValvraProcessor::SnapshotSlot::C);
    REQUIRE(writer.hasSnapshot(valvra::ValvraProcessor::SnapshotSlot::C));

    setFloatParam(writer, "outputDb", 4.0f);
    writer.storeSnapshot(valvra::ValvraProcessor::SnapshotSlot::D);
    REQUIRE(writer.hasSnapshot(valvra::ValvraProcessor::SnapshotSlot::D));
    REQUIRE(! writer.hasSnapshot(valvra::ValvraProcessor::SnapshotSlot::E));

    juce::MemoryBlock state;
    writer.getStateInformation(state);
    REQUIRE(state.getSize() > 0);

    valvra::ValvraProcessor reader;
    reader.setStateInformation(state.getData(), static_cast<int>(state.getSize()));

    REQUIRE(reader.hasSnapshot(valvra::ValvraProcessor::SnapshotSlot::C));
    REQUIRE(reader.hasSnapshot(valvra::ValvraProcessor::SnapshotSlot::D));
    REQUIRE(! reader.hasSnapshot(valvra::ValvraProcessor::SnapshotSlot::E));

    setFloatParam(reader, "outputDb", 0.0f);
    REQUIRE(reader.loadSnapshot(valvra::ValvraProcessor::SnapshotSlot::C));
    REQUIRE(floatParamValue(reader, "outputDb") == Approx(-9.0f).margin(1.0e-3f));

    setFloatParam(reader, "outputDb", 0.0f);
    REQUIRE(reader.loadSnapshot(valvra::ValvraProcessor::SnapshotSlot::D));
    REQUIRE(floatParamValue(reader, "outputDb") == Approx(4.0f).margin(1.0e-3f));

    REQUIRE(! reader.loadSnapshot(valvra::ValvraProcessor::SnapshotSlot::E));
}

TEST_CASE("ValvraProcessor: A/B workflow undo-redo restores live state",
          "[plugin][ab][undo][redo]")
{
    valvra::ValvraProcessor proc;

    setFloatParam(proc, "outputDb", -6.0f); // A
    proc.copyAToB();
    proc.toggleAB();                         // B
    setFloatParam(proc, "outputDb", 6.0f);   // live B
    proc.storeSnapshot(valvra::ValvraProcessor::SnapshotSlot::C);

    setFloatParam(proc, "outputDb", -2.0f);  // not in AB history (param change)
    REQUIRE(proc.loadSnapshot(valvra::ValvraProcessor::SnapshotSlot::C));
    REQUIRE(floatParamValue(proc, "outputDb") == Approx(6.0f).margin(1.0e-3f));

    REQUIRE(proc.canUndoAB());
    proc.undoAB();
    REQUIRE(floatParamValue(proc, "outputDb") == Approx(-2.0f).margin(1.0e-3f));
    REQUIRE(proc.canRedoAB());

    proc.redoAB();
    REQUIRE(floatParamValue(proc, "outputDb") == Approx(6.0f).margin(1.0e-3f));
}

TEST_CASE("ValvraProcessor: A/B workflow history is bounded to 32 steps",
          "[plugin][ab][undo][redo][history]")
{
    valvra::ValvraProcessor proc;
    setFloatParam(proc, "outputDb", -3.0f);

    for (int i = 0; i < 40; ++i)
        proc.toggleABForCompare();

    int undoCount = 0;
    while (proc.canUndoAB())
    {
        proc.undoAB();
        ++undoCount;
    }
    REQUIRE(undoCount == 32);

    int redoCount = 0;
    while (proc.canRedoAB())
    {
        proc.redoAB();
        ++redoCount;
    }
    REQUIRE(redoCount == 32);
}

TEST_CASE("ValvraProcessor: AB state serialization does not recurse across reloads",
          "[plugin][ab][state][serialization]")
{
    valvra::ValvraProcessor writer;
    setFloatParam(writer, "outputDb", -8.0f);
    writer.storeSnapshot(valvra::ValvraProcessor::SnapshotSlot::C);
    setFloatParam(writer, "outputDb", 2.5f);
    writer.storeSnapshot(valvra::ValvraProcessor::SnapshotSlot::D);

    juce::MemoryBlock state1;
    writer.getStateInformation(state1);
    REQUIRE(state1.getSize() > 0);

    valvra::ValvraProcessor reader;
    reader.setStateInformation(state1.getData(), static_cast<int>(state1.getSize()));

    juce::MemoryBlock state2;
    reader.getStateInformation(state2);
    REQUIRE(state2.getSize() > 0);

    reader.setStateInformation(state2.getData(), static_cast<int>(state2.getSize()));
    juce::MemoryBlock state3;
    reader.getStateInformation(state3);
    REQUIRE(state3.getSize() > 0);

    const auto sz2 = static_cast<double>(state2.getSize());
    const auto sz3 = static_cast<double>(state3.getSize());
    INFO("state2 size = " << sz2 << ", state3 size = " << sz3);
    REQUIRE(sz3 <= sz2 * 1.10 + 64.0);
}

TEST_CASE("ValvraProcessor: mastering snapshot publishes LUFS/TP/peak/correlation",
          "[plugin][mastering][meter]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 512;

    valvra::ValvraProcessor proc;
    setChoiceParam(proc, "oversample", 0);
    setFloatParam(proc, "drive", 0.0f);
    setFloatParam(proc, "mix", 0.0f);

    juce::AudioProcessor::BusesLayout stereoLayout;
    stereoLayout.inputBuses .add(juce::AudioChannelSet::stereo());
    stereoLayout.outputBuses.add(juce::AudioChannelSet::stereo());
    REQUIRE(proc.setBusesLayout(stereoLayout));
    proc.prepareToPlay(kSampleRate, kBlockSize);

    juce::AudioBuffer<float> buffer(2, kBlockSize);
    juce::MidiBuffer midi;
    for (int i = 0; i < 360; ++i)
    {
        for (int n = 0; n < kBlockSize; ++n)
        {
            const double t = static_cast<double>(i * kBlockSize + n) / kSampleRate;
            buffer.setSample(0, n, static_cast<float>(0.3 * std::sin(2.0 * std::numbers::pi * 440.0 * t)));
            buffer.setSample(1, n, static_cast<float>(0.25 * std::sin(2.0 * std::numbers::pi * 880.0 * t)));
        }
        proc.processBlock(buffer, midi);
    }

    const auto s = proc.readMasteringState();
    REQUIRE(std::isfinite(s.momentaryLufs));
    REQUIRE(std::isfinite(s.shortTermLufs));
    REQUIRE(std::isfinite(s.integratedLufs));
    REQUIRE(std::isfinite(s.truePeakDbtp));
    REQUIRE(std::isfinite(s.peakDbfs));
    REQUIRE(std::isfinite(s.gainReductionDb));
    REQUIRE(std::isfinite(s.correlation));
    REQUIRE(s.momentaryLufs > -80.0f);
    REQUIRE(s.shortTermLufs > -80.0f);
    REQUIRE(s.integratedLufs > -80.0f);
    REQUIRE(s.truePeakDbtp <= 3.0f);
    REQUIRE(s.peakDbfs <= 0.1f);
    REQUIRE(s.correlation >= -1.0f);
    REQUIRE(s.correlation <= 1.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Mid/Side mode — the centre and sides should be processed independently and
// the result must be an exact L/R pair (encode → process → decode is a
// linear, level-preserving operation when both chains are identical).
//
// The defining mono-compatibility property: in M/S mode with both chains
// identical, the L+R sum of the OUTPUT equals 2× the centre-only chain
// applied to the L+R sum of the INPUT.  That is, the side-chain content
// cancels in mono down-mix — *exactly* what mastering engineers expect.
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("ValvraProcessor: M/S mode mono-compat (pure side input nulls in sum)",
          "[plugin][m-s][mastering]")
{
    constexpr double kSampleRate = 48000.0;
    // Chain DC tracker has tau≈200 ms (alpha=0.9999 per sample).  We need
    // to feed enough samples for the operating point to settle before we
    // can assert "side cancels in the mono down-mix" — otherwise the
    // chain's still-converging baseline dominates whatever the side path
    // contributes.  Two ~16k blocks (~340 ms each) is plenty.
    constexpr int kSettleSamples = 16384;
    constexpr int kMeasureSamples = 16384;

    valvra::ValvraProcessor proc;
    setChoiceParam(proc, "preset", 0);       // V72 (well-behaved baseline)
    setChoiceParam(proc, "oversample", 0);   // latency-free for direct compare
    setFloatParam(proc, "drive", 1.0f);
    setFloatParam(proc, "outputDb", 0.0f);
    setFloatParam(proc, "mix", 1.0f);
    setChoiceParam(proc, "msMode", 1);       // Mid/Side ON

    juce::AudioProcessor::BusesLayout stereoLayout;
    stereoLayout.inputBuses .add(juce::AudioChannelSet::stereo());
    stereoLayout.outputBuses.add(juce::AudioChannelSet::stereo());
    REQUIRE(proc.setBusesLayout(stereoLayout));

    proc.prepareToPlay(kSampleRate, kMeasureSamples);

    // PURE SIDE INPUT: L = +0.30 sin(660), R = −0.30 sin(660).
    //   ⇒ M = 0,  S = 0.30 sin(660 Hz)
    //   ⇒ chain_L sees silence, chain_R sees the side signal.
    //   ⇒ L_out + R_out = 2·chain_L(0)  — only the M-chain's idle output.
    auto fillBuf = [&](juce::AudioBuffer<float>& buf, int offsetSamples)
    {
        const int N = buf.getNumSamples();
        for (int n = 0; n < N; ++n)
        {
            const double t =
                static_cast<double>(n + offsetSamples) / kSampleRate;
            const double side =
                0.30 * std::sin(2.0 * std::numbers::pi * 660.0 * t);
            buf.setSample(0, n, static_cast<float>( side));
            buf.setSample(1, n, static_cast<float>(-side));
        }
    };

    juce::MidiBuffer midi;

    // 1) Settle block — let the DC tracker converge.  Discard outputs.
    juce::AudioBuffer<float> settleBuf(2, kSettleSamples);
    fillBuf(settleBuf, 0);
    proc.processBlock(settleBuf, midi);

    // 2) Measurement block — assert mono-compat on STEADY-STATE output.
    juce::AudioBuffer<float> measBuf(2, kMeasureSamples);
    fillBuf(measBuf, kSettleSamples);
    proc.processBlock(measBuf, midi);

    double energyL = 0.0, energySum = 0.0;
    for (int n = 0; n < kMeasureSamples; ++n)
    {
        const float l = measBuf.getSample(0, n);
        const float r = measBuf.getSample(1, n);
        REQUIRE(std::isfinite(l));
        REQUIRE(std::isfinite(r));
        energyL   += static_cast<double>(l) * l;
        const float s = l + r;
        energySum += static_cast<double>(s) * s;
    }
    REQUIRE(energyL > 0.0);

    // After settle, L+R should be meaningfully below L alone.  The chain
    // is non-zero even on silent input — heater hum (~2e-4 V at the
    // grid) and shot noise both feed in continuously and get amplified
    // by the stage gain (~100×).  That sets a noise floor of around
    // −25 dB relative to a moderate signal, which is the residue we see
    // in the L+R sum.  We assert −18 dB (an >8× attenuation of the side
    // path in the mono down-mix) — strong evidence that M/S routing is
    // actually canceling the side channel, while leaving room for the
    // chain's own noise model.
    const double cancelDb =
        10.0 * std::log10(energySum / std::max(energyL, 1e-12));
    INFO("L+R energy / L energy = " << cancelDb << " dB (after settle)");
    REQUIRE(cancelDb < -18.0);
}

TEST_CASE("ValvraProcessor: M/S vs Stereo produces audibly different output",
          "[plugin][m-s]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 1024;

    auto runWith = [&](int msMode)
    {
        valvra::ValvraProcessor proc;
        setChoiceParam(proc, "preset", 1);       // Marshall (heavy non-linearity)
        setChoiceParam(proc, "oversample", 0);
        setFloatParam(proc, "drive", 1.5f);      // hot enough that PP cuts off
        setFloatParam(proc, "outputDb", 0.0f);
        setFloatParam(proc, "mix", 1.0f);
        setChoiceParam(proc, "msMode", msMode);

        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses .add(juce::AudioChannelSet::stereo());
        layout.outputBuses.add(juce::AudioChannelSet::stereo());
        REQUIRE(proc.setBusesLayout(layout));

        proc.prepareToPlay(kSampleRate, kBlockSize);

        juce::AudioBuffer<float> buf(2, kBlockSize);
        for (int n = 0; n < kBlockSize; ++n)
        {
            const double t = static_cast<double>(n) / kSampleRate;
            const double centre =
                0.20 * std::sin(2.0 * std::numbers::pi * 220.0 * t);
            const double side   =
                0.12 * std::sin(2.0 * std::numbers::pi * 880.0 * t);
            buf.setSample(0, n, static_cast<float>(centre + side));
            buf.setSample(1, n, static_cast<float>(centre - side));
        }

        juce::MidiBuffer midi;
        proc.processBlock(buf, midi);

        std::vector<float> out(2 * kBlockSize);
        for (int n = 0; n < kBlockSize; ++n)
        {
            out[static_cast<std::size_t>(2 * n)]     = buf.getSample(0, n);
            out[static_cast<std::size_t>(2 * n + 1)] = buf.getSample(1, n);
        }
        return out;
    };

    const auto stereo = runWith(0);
    const auto ms     = runWith(1);

    // The two outputs must differ by more than rounding noise — the
    // chain non-linearity gives the centre image different harmonic
    // shaping in M/S mode than in conventional stereo, so the buffers
    // diverge audibly.  Compute the RMS difference normalised by the
    // RMS of the stereo path; a ratio > 1% means the modes are clearly
    // distinguishable.
    double diffEnergy = 0.0, refEnergy = 0.0;
    for (std::size_t i = 0; i < stereo.size(); ++i)
    {
        const double d = stereo[i] - ms[i];
        diffEnergy += d * d;
        refEnergy  += stereo[i] * stereo[i];
    }
    const double ratio = std::sqrt(diffEnergy / std::max(refEnergy, 1e-9));
    INFO("RMS-diff / RMS-ref = " << ratio);
    REQUIRE(ratio > 0.01);   // > 1% RMS difference
}

TEST_CASE("ValvraProcessor: mastering path runs across documented sample rates",
          "[plugin][mastering][sample-rate]")
{
    constexpr int kBlockSize = 512;
    constexpr std::array<double, 6> kSampleRates {
        44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0
    };

    for (double sampleRate : kSampleRates)
    {
        INFO("sampleRate = " << sampleRate);

        valvra::ValvraProcessor proc;
        setChoiceParam(proc, "preset", 1);          // Console Output (nonlinear heavy path)
        setChoiceParam(proc, "oversample", 1);      // 2x
        setChoiceParam(proc, "tpMode", 2);          // brick-wall
        setFloatParam(proc, "tpCeilingDb", -1.0f);
        setFloatParam(proc, "tpLookaheadMs", 5.0f);
        setChoiceParam(proc, "msMode", 1);          // M/S processing path
        setChoiceParam(proc, "expansionMode", 3);   // Tape core
        setFloatParam(proc, "expansionAmount", 0.75f);
        setFloatParam(proc, "expansionMix", 1.0f);
        setFloatParam(proc, "drive", 1.3f);
        setFloatParam(proc, "mix", 1.0f);

        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses .add(juce::AudioChannelSet::stereo());
        layout.outputBuses.add(juce::AudioChannelSet::stereo());
        REQUIRE(proc.setBusesLayout(layout));
        proc.prepareToPlay(sampleRate, kBlockSize);

        juce::AudioBuffer<float> buffer(2, kBlockSize);
        juce::MidiBuffer midi;

        double energy = 0.0;
        constexpr int kBlocks = 32;
        for (int block = 0; block < kBlocks; ++block)
        {
            for (int n = 0; n < kBlockSize; ++n)
            {
                const double t = static_cast<double>(block * kBlockSize + n) / sampleRate;
                const double centre = 0.22 * std::sin(2.0 * std::numbers::pi * 220.0 * t);
                const double side   = 0.12 * std::sin(2.0 * std::numbers::pi * 1870.0 * t);
                buffer.setSample(0, n, static_cast<float>(centre + side));
                buffer.setSample(1, n, static_cast<float>(centre - side));
            }
            proc.processBlock(buffer, midi);

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                const float* data = buffer.getReadPointer(ch);
                for (int n = 0; n < kBlockSize; ++n)
                {
                    REQUIRE(std::isfinite(data[n]));
                    energy += static_cast<double>(data[n]) * static_cast<double>(data[n]);
                }
            }
        }

        REQUIRE(energy > 0.0);
        REQUIRE(proc.getLatencySamples() >= 0);
    }
}

#if !defined(VALVRA_ENABLE_UI_TEST_HOOKS) || !VALVRA_ENABLE_UI_TEST_HOOKS
TEST_CASE("ValvraEditor: UI layout invariants across scales and tabs",
          "[plugin][ui][layout]")
{
    SUCCEED("UI test hooks are disabled");
}
#else
TEST_CASE("ValvraEditor: UI layout invariants across scales and tabs",
          "[plugin][ui][layout]")
{
    ScopedEnvOverride noFit("VALVRA_UI_TEST_NO_FIT", "1");
    juce::ScopedJuceInitialiser_GUI gui;

    valvra::ValvraProcessor proc;
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses .add(juce::AudioChannelSet::stereo());
    layout.outputBuses.add(juce::AudioChannelSet::stereo());
    REQUIRE(proc.setBusesLayout(layout));
    proc.prepareToPlay(48000.0, 256);

    valvra::ValvraEditor editor { proc };
    editor.setVisible(true);

    const std::array<float, 4> scales { 1.0f, 1.25f, 1.5f, 2.0f };
    const std::array<valvra::ValvraEditor::DebugTab, 3> tabs {
        valvra::ValvraEditor::DebugTab::Sound,
        valvra::ValvraEditor::DebugTab::Analysis,
        valvra::ValvraEditor::DebugTab::Output
    };
    const std::array<const char*, 3> tabNames { "Sound", "Analysis", "Output" };

    std::size_t snapshots = 0;
    for (float scale : scales)
    {
        editor.debugSetUiScale(scale);
        REQUIRE(editor.getWidth() == static_cast<int>(std::round(980.0f * scale)));
        REQUIRE(editor.getHeight() == static_cast<int>(std::round(760.0f * scale)));
        for (std::size_t ti = 0; ti < tabs.size(); ++ti)
        {
            editor.debugSelectTab(tabs[ti]);
            editor.resized();
            const auto root = editor.getLocalBounds();
            const auto tabName = tabNames[ti];

            const auto ab = editor.debugBoundsForNamedComponent("AB Compare Toggle");
            const auto blind = editor.debugBoundsForNamedComponent("AB Blind Mode");
            const auto aToB = editor.debugBoundsForNamedComponent("Copy A To B");
            const auto bToA = editor.debugBoundsForNamedComponent("Copy B To A");
            const auto reset = editor.debugBoundsForNamedComponent("Reset AB");
            const auto undo = editor.debugBoundsForNamedComponent("AB Undo");
            const auto redo = editor.debugBoundsForNamedComponent("AB Redo");
            const auto seed = editor.debugBoundsForNamedComponent("Seed Status");
            const auto nullT = editor.debugBoundsForNamedComponent("Null Test Toggle");
            const auto snapC = editor.debugBoundsForNamedComponent("Snapshot C");
            const auto snapD = editor.debugBoundsForNamedComponent("Snapshot D");
            const auto snapE = editor.debugBoundsForNamedComponent("Snapshot E");

            requireValidBounds(ab, root, "AB Compare Toggle", scale, tabName);
            requireValidBounds(blind, root, "AB Blind Mode", scale, tabName);
            requireValidBounds(aToB, root, "Copy A To B", scale, tabName);
            requireValidBounds(bToA, root, "Copy B To A", scale, tabName);
            requireValidBounds(reset, root, "Reset AB", scale, tabName);
            requireValidBounds(undo, root, "AB Undo", scale, tabName);
            requireValidBounds(redo, root, "AB Redo", scale, tabName);
            requireValidBounds(seed, root, "Seed Status", scale, tabName);
            requireValidBounds(nullT, root, "Null Test Toggle", scale, tabName);
            requireValidBounds(snapC, root, "Snapshot C", scale, tabName);
            requireValidBounds(snapD, root, "Snapshot D", scale, tabName);
            requireValidBounds(snapE, root, "Snapshot E", scale, tabName);

            requireNoOverlap(ab, blind, "AB Compare Toggle", "AB Blind Mode", scale, tabName);
            requireNoOverlap(aToB, bToA, "Copy A To B", "Copy B To A", scale, tabName);
            requireNoOverlap(undo, redo, "AB Undo", "AB Redo", scale, tabName);
            requireNoOverlap(seed, nullT, "Seed Status", "Null Test Toggle", scale, tabName);
            requireNoOverlap(snapC, snapD, "Snapshot C", "Snapshot D", scale, tabName);
            requireNoOverlap(snapD, snapE, "Snapshot D", "Snapshot E", scale, tabName);

            const auto uiScaleBox = editor.debugBoundsForNamedComponent("UI Scale");
            requireValidBounds(uiScaleBox, root, "UI Scale", scale, tabName);
            REQUIRE(uiScaleBox.getHeight() >= 20);

            if (tabs[ti] == valvra::ValvraEditor::DebugTab::Sound)
            {
                const auto stagePanel = editor.debugBoundsForNamedComponent("Stage Editor Panel");
                const auto chainView = editor.debugBoundsForNamedComponent("Chain Builder View");
                requireValidBounds(stagePanel, root, "Stage Editor Panel", scale, tabName);
                requireValidBounds(chainView, root, "Chain Builder View", scale, tabName);
                requireNoOverlap(chainView, stagePanel, "Chain Builder View", "Stage Editor Panel", scale, tabName);

                const auto s = editor.debugStageEditorLayout();
                const auto stageLocal = stagePanel.withPosition(0, 0);
                requireValidBounds(s.stageSelector, stageLocal, "Stage Selector", scale, tabName);
                requireValidBounds(s.tubeBox, stageLocal, "Stage Tube", scale, tabName);
                requireValidBounds(s.topologyBox, stageLocal, "Stage Topology", scale, tabName);
                requireValidBounds(s.driveSlider, stageLocal, "Stage Drive", scale, tabName);
                requireValidBounds(s.biasSlider, stageLocal, "Stage Bias", scale, tabName);
                requireValidBounds(s.driveValue, stageLocal, "Stage Drive Value", scale, tabName);
                requireValidBounds(s.biasValue, stageLocal, "Stage Bias Value", scale, tabName);

                REQUIRE(s.tubeBox.getHeight() >= 20);
                REQUIRE(s.topologyBox.getHeight() >= 20);
                REQUIRE(s.driveValue.getHeight() >= 12);
                REQUIRE(s.biasValue.getHeight() >= 12);
                requireNoOverlap(s.tubeBox, s.topologyBox, "Stage Tube", "Stage Topology", scale, tabName);
                requireNoOverlap(s.driveSlider, s.biasSlider, "Stage Drive", "Stage Bias", scale, tabName);

                const auto nodeBands = editor.debugChainBuilderNodeBands();
                REQUIRE(nodeBands.size() >= 3);
                for (const auto& n : nodeBands)
                {
                    INFO("tab=" << tabName << " scale=" << scale
                                << " node=" << n.node.toString().toStdString()
                                << " title=" << n.titleBand.toString().toStdString()
                                << " detail=" << n.detailBand.toString().toStdString());
                    REQUIRE(n.node.getWidth() > 0.0f);
                    REQUIRE(n.node.getHeight() > 0.0f);
                    REQUIRE(n.titleBand.getHeight() > 0.0f);
                    REQUIRE(n.detailBand.getHeight() > 0.0f);
                    REQUIRE(n.titleBand.getBottom() <= n.detailBand.getY());
                }
            }
            else if (tabs[ti] == valvra::ValvraEditor::DebugTab::Output)
            {
                const auto outPanel = editor.debugBoundsForNamedComponent("Output Panel");
                requireValidBounds(outPanel, root, "Output Panel", scale, tabName);

                const auto m = editor.debugMasteringLayout();
                const auto outputLocal = outPanel.withPosition(0, 0);
                requireValidBounds(m.analogSection, outputLocal, "Master Analog Section", scale, tabName);
                requireValidBounds(m.safetySection, outputLocal, "Master Safety Section", scale, tabName);
                requireValidBounds(m.meterSection, outputLocal, "Master Meter Section", scale, tabName);
                requireNoOverlap(m.analogSection, m.safetySection, "Master Analog Section", "Master Safety Section", scale, tabName);
                requireNoOverlap(m.safetySection, m.meterSection, "Master Safety Section", "Master Meter Section", scale, tabName);

                requireValidBounds(m.lufsMeterBounds, m.meterSection, "LUFS Meter Band", scale, tabName);
                requireValidBounds(m.calibrationMeterBounds, m.meterSection, "Calibration Meter Band", scale, tabName);
                requireValidBounds(m.tpMeterBounds, m.meterSection, "TP Meter Band", scale, tabName);
                requireValidBounds(m.grHistoryBounds, m.meterSection, "GR History Band", scale, tabName);
                requireNoOverlap(m.lufsMeterBounds, m.calibrationMeterBounds, "LUFS Meter Band", "Calibration Meter Band", scale, tabName);
                requireNoOverlap(m.calibrationMeterBounds, m.tpMeterBounds, "Calibration Meter Band", "TP Meter Band", scale, tabName);
                requireNoOverlap(m.tpMeterBounds, m.grHistoryBounds, "TP Meter Band", "GR History Band", scale, tabName);
            }

            ++snapshots;
        }
    }

    REQUIRE(snapshots == scales.size() * tabs.size());
}
#endif
