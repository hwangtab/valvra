// ─────────────────────────────────────────────────────────────────────────────
// PluginProcessor.cpp — Valvra host-level audio processor
// ─────────────────────────────────────────────────────────────────────────────
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

namespace valvra {

namespace {

dsp::TubeAmpChainConfig buildPresetConfig(int presetIndex, std::uint64_t seed)
{
    dsp::TubeAmpChainConfig cfg;
    switch (presetIndex)
    {
        case 0: cfg = dsp::chain_presets::V72Preamp();       break;
        case 1: cfg = dsp::chain_presets::MarshallMode();    break;
        case 2: cfg = dsp::chain_presets::CultureVultureMode(); break;
        case 3: cfg = dsp::chain_presets::RNDIMode();        break;
        default: cfg = dsp::chain_presets::V72Preamp();      break;
    }
    cfg.variationSeed = seed;
    return cfg;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// ValvraProcessor — construction / layout
// ─────────────────────────────────────────────────────────────────────────────
ValvraProcessor::ValvraProcessor()
    : juce::AudioProcessor(
          BusesProperties()
              .withInput("Input",   juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , params_(*this, nullptr, "Valvra", createLayout())
{
    // Seed generator: mix high-resolution time with an atomic counter so
    // simultaneously-created instances get distinct seeds even if the
    // ticks counter has insufficient granularity on this platform.
    static std::atomic<std::uint64_t> sInstanceCounter { 0 };
    const std::uint64_t ticks =
        static_cast<std::uint64_t>(juce::Time::getHighResolutionTicks());
    const std::uint64_t counter =
        sInstanceCounter.fetch_add(1, std::memory_order_relaxed);
    const std::uint64_t initialSeed =
        (ticks * 0x9E3779B97F4A7C15ULL)
        ^ (counter * 0xBB67AE8584CAA73BULL)
        ^ 0xA5A5A5A5A5A5A5A5ULL;
    currentSeed_.store(initialSeed, std::memory_order_relaxed);

    // Wire the preset parameter to rebuild on change
    params_.addParameterListener(kParamPreset, this);

    // Cache the bypass pointer for getBypassParameter() + fast audio-thread
    // reads.  The AudioProcessorValueTreeState owns the AudioParameterBool;
    // we only hold a borrowed reference.
    bypassParam_ = dynamic_cast<juce::AudioParameterBool*>(
        params_.getParameter(kParamBypass));
}

ValvraProcessor::~ValvraProcessor()
{
    // Symmetric with the addParameterListener in the ctor — required to
    // avoid a dangling-listener warning when the processor is torn down
    // (e.g. when the user swaps plugins on a track).
    params_.removeParameterListener(kParamPreset, this);
}

void ValvraProcessor::parameterChanged(const juce::String& paramID,
                                       float /*newValue*/)
{
    // parameterChanged runs on the MESSAGE thread.  We must NOT call
    // rebuildChain() directly here because it mutates the DSP graph that
    // the audio thread is actively iterating over — doing so has at best
    // produced glitches and at worst crashed the DAW in practice.
    //
    // Instead we raise a flag; the audio thread consumes it at the top of
    // the next processBlock() invocation (single-producer single-consumer,
    // relaxed ordering is sufficient for a plain bool).
    if (paramID == kParamPreset)
        rebuildRequested_.store(true, std::memory_order_relaxed);
}

juce::AudioProcessorValueTreeState::ParameterLayout
ValvraProcessor::createLayout()
{
    using namespace juce;

    return {
        // Preset selector — 4 signature modes
        std::make_unique<AudioParameterChoice>(
            ParameterID { kParamPreset, 1 },
            "Mode",
            StringArray { "V72 Preamp", "Marshall", "Culture Vulture", "RNDI DI" },
            0),

        // Drive — input gain staging into the tube stages (0–100%)
        std::make_unique<AudioParameterFloat>(
            ParameterID { kParamDrive, 1 },
            "Drive",
            NormalisableRange<float> { 0.0f, 3.0f, 0.0f, 0.5f },  // log-like
            1.0f),

        // Output — makeup gain in dB
        std::make_unique<AudioParameterFloat>(
            ParameterID { kParamOutputDb, 1 },
            "Output",
            NormalisableRange<float> { -24.0f, 24.0f, 0.1f },
            0.0f),

        // Mix — dry/wet for parallel processing
        std::make_unique<AudioParameterFloat>(
            ParameterID { kParamMix, 1 },
            "Mix",
            NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
            1.0f),

        // Oversampling — 1×/2×/4×/8×
        std::make_unique<AudioParameterChoice>(
            ParameterID { kParamOversample, 1 },
            "Quality",
            StringArray { "Low (1x)", "Medium (2x)", "High (4x)", "Ultra (8x)" },
            2),

        // Host-visible bypass.  Declared last so existing preset XML (which
        // does not know about this param) still round-trips cleanly.
        std::make_unique<AudioParameterBool>(
            ParameterID { kParamBypass, 1 },
            "Bypass",
            false)
    };
}

// ─────────────────────────────────────────────────────────────────────────────
// Prepare / rebuild
// ─────────────────────────────────────────────────────────────────────────────
void ValvraProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    sampleRate_         = sampleRate;
    preparedBlockSize_  = samplesPerBlock;
    lastOsFactor_       = currentOversampleFactor();
    rebuildChain();
    os2L_.reset(); os2R_.reset();
    os4L_.reset(); os4R_.reset();
    os8L_.reset(); os8R_.reset();
    analyzer_.prepare(sampleRate);

    // Configure PDC (plugin delay compensation) + internal dry-path alignment.
    // Both must stay in sync for null-test subtraction to remain pristine.
    const int lat = currentLatencyInSamples();
    setLatencySamples(lat);
    dryDelayL_.setLatency(lat);
    dryDelayR_.setLatency(lat);

    // Parameter smoothers: 20 ms ramp time for inaudible knob changes.
    constexpr double kSmoothingSec = 0.020;
    driveSmooth_  .reset(sampleRate, kSmoothingSec);
    outGainSmooth_.reset(sampleRate, kSmoothingSec);
    mixSmooth_    .reset(sampleRate, kSmoothingSec);

    // Initialize to current raw parameter values so there is no startup ramp
    // from 0 → target on first playback.
    driveSmooth_  .setCurrentAndTargetValue(
        params_.getRawParameterValue(kParamDrive)->load());
    outGainSmooth_.setCurrentAndTargetValue(
        juce::Decibels::decibelsToGain(
            params_.getRawParameterValue(kParamOutputDb)->load()));
    mixSmooth_    .setCurrentAndTargetValue(
        params_.getRawParameterValue(kParamMix)->load());
}

void ValvraProcessor::rebuildChain()
{
    currentPresetIndex_ =
        static_cast<int>(*params_.getRawParameterValue(kParamPreset));

    const int osFactor = currentOversampleFactor();
    const double internalSR = sampleRate_ * static_cast<double>(osFactor);

    // ★ Stereo Monte Carlo: each channel must use a DIFFERENT seed so that
    // real analog-rack "two slightly different units" feel is preserved.
    constexpr std::uint64_t kStereoSalt = 0x123456789ABCDEFULL;
    const std::uint64_t seed = currentSeed_.load(std::memory_order_relaxed);
    auto cfgL = buildPresetConfig(currentPresetIndex_, seed);
    auto cfgR = buildPresetConfig(currentPresetIndex_, seed ^ kStereoSalt);

    chainL_.setup(cfgL, internalSR);
    chainR_.setup(cfgR, internalSR);

    // Shared-rail coupling: one PowerSupplySag per processor drives BOTH
    // chains.  Enable it only when the preset itself has sag on (solid-
    // state rectifier presets keep independent stiff rails).  Running the
    // envelope follower at the INTERNAL (upsampled) rate keeps the sag
    // time constants identical to what the individual chain's psu_ would
    // have seen in legacy independent-PSU mode.
    sharedPSUActive_ = cfgL.enablePSUSag;
    if (sharedPSUActive_)
    {
        auto psu = cfgL.psu;
        psu.sampleRate = internalSR;
        sharedPSU_.setParams(psu);
        sharedPSU_.reset();

        // Scatter the shared rail's ripple phase off the current seed so
        // two Valvra instances on different tracks don't sum their 120 Hz
        // content coherently.
        const std::uint64_t rippleBits =
            currentSeed_.load(std::memory_order_relaxed)
            ^ 0xF39E91E1A7E7A8B1ULL;
        const double ripplePhase =
            (static_cast<double>(rippleBits & 0xFFFFFFFFULL)
             / static_cast<double>(0xFFFFFFFFULL))
            * 2.0 * 3.14159265358979323846;
        sharedPSU_.setRipplePhase(ripplePhase);

        chainL_.setExternalPSUMode(true);
        chainR_.setExternalPSUMode(true);
        chainL_.setExternalVb(cfgL.psu.Vb_nominal);
        chainR_.setExternalVb(cfgL.psu.Vb_nominal);
    }
    else
    {
        chainL_.setExternalPSUMode(false);
        chainR_.setExternalPSUMode(false);
    }
}

int ValvraProcessor::currentOversampleFactor() const noexcept
{
    const int idx =
        static_cast<int>(*params_.getRawParameterValue(kParamOversample));
    switch (idx)
    {
        case 0: return 1;
        case 1: return 2;
        case 2: return 4;
        case 3: return 8;
        default: return 4;
    }
}

int ValvraProcessor::currentLatencyInSamples() const noexcept
{
    // Only the oversampling path adds latency (linear-phase FIRs).
    // At 1× the chain is sample-for-sample, so latency is zero.
    switch (currentOversampleFactor())
    {
        case 1: return 0;
        case 2: return dsp::PolyphaseOversampler<2>::latencyInBaseSamples();
        case 4: return dsp::PolyphaseOversampler<4>::latencyInBaseSamples();
        case 8: return dsp::PolyphaseOversampler<8>::latencyInBaseSamples();
        default: return 0;
    }
}

bool ValvraProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Support mono, stereo in == out
    const auto mainOut = layouts.getMainOutputChannelSet();
    const auto mainIn  = layouts.getMainInputChannelSet();
    if (mainIn != mainOut) return false;
    return mainOut == juce::AudioChannelSet::mono()
        || mainOut == juce::AudioChannelSet::stereo();
}

// ─────────────────────────────────────────────────────────────────────────────
// Core audio callback
// ─────────────────────────────────────────────────────────────────────────────
void ValvraProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                   juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numInCh     = getTotalNumInputChannels();
    const int numOutCh    = getTotalNumOutputChannels();

    // Defensive guard: some hosts can briefly hand us a buffer with no
    // output channels during bus reconfiguration.  Skip processing rather
    // than dereferencing a null write pointer.
    if (numOutCh <= 0 || numSamples <= 0) return;

    // Consume any pending rebuild request from the message thread BEFORE
    // touching the DSP graph.  This is the safe point to mutate chain state.
    if (rebuildRequested_.exchange(false, std::memory_order_relaxed))
        rebuildChain();

    // Apply a deferred reroll (seed change hot-updated into both chains).
    // Uses the same stereo salt as rebuildChain() for left/right differentiation.
    if (rerollRequested_.exchange(false, std::memory_order_relaxed))
    {
        constexpr std::uint64_t kStereoSalt = 0x123456789ABCDEFULL;
        const std::uint64_t newSeed =
            pendingSeed_.load(std::memory_order_relaxed);
        currentSeed_.store(newSeed, std::memory_order_relaxed);
        chainL_.setVariationSeed(newSeed);
        chainR_.setVariationSeed(newSeed ^ kStereoSalt);
    }

    // Clear any extra output channels that have no matching input (safety)
    for (int ch = numInCh; ch < numOutCh; ++ch)
        buffer.clear(ch, 0, numSamples);

    // Update smoother TARGETS once per block.  Actual ramped values are
    // pulled per-sample inside the inner loop, so a user knob move never
    // produces a discontinuity — the signal glides smoothly to the new
    // value over ~20 ms.
    driveSmooth_.setTargetValue(
        params_.getRawParameterValue(kParamDrive)->load());
    outGainSmooth_.setTargetValue(
        juce::Decibels::decibelsToGain(
            params_.getRawParameterValue(kParamOutputDb)->load()));
    mixSmooth_.setTargetValue(
        params_.getRawParameterValue(kParamMix)->load());

    const bool stereo = (numOutCh >= 2);

    auto* L = buffer.getWritePointer(0);
    auto* R = stereo ? buffer.getWritePointer(1) : nullptr;

    const bool nullTest = nullTestMode_.load(std::memory_order_relaxed);
    const bool bypass   = bypassParam_ != nullptr && bypassParam_->get();

    // Resolve oversample factor once per block; reconfigure the chain at
    // the correct internal rate if the user just changed the Quality combo.
    const int osFactor = currentOversampleFactor();
    if (osFactor != lastOsFactor_)
    {
        lastOsFactor_ = osFactor;
        rebuildChain();        // re-seed chain at the new internal sample rate
        // Flush the FIR delay lines of every oversampler so the newly-active
        // path does not see stale samples left over from the previous factor.
        os2L_.reset(); os2R_.reset();
        os4L_.reset(); os4R_.reset();
        os8L_.reset(); os8R_.reset();

        // Latency changed: update PDC report and re-align the internal dry
        // delay so null-test and wet/dry crossfade stay phase-correct.
        const int lat = currentLatencyInSamples();
        setLatencySamples(lat);
        dryDelayL_.setLatency(lat);
        dryDelayR_.setLatency(lat);
    }

    // Per-upsampled-sample chain step (stereo).  When sharedPSUActive_ is
    // set, both chains read the same Vb and we sum their plate currents into
    // the shared sag envelope so the NEXT upsampled step sees the updated rail.
    auto stepChainsStereo = [&](double& uL, double& uR) noexcept
    {
        if (sharedPSUActive_)
        {
            const double Vb = sharedPSU_.currentVb();
            chainL_.setExternalVb(Vb);
            chainR_.setExternalVb(Vb);
            uL = chainL_.process(uL);
            uR = chainR_.process(uR);
            sharedPSU_.process(chainL_.lastTotalIp() + chainR_.lastTotalIp());
        }
        else
        {
            uL = chainL_.process(uL);
            uR = chainR_.process(uR);
        }
    };

    // Per-upsampled-sample chain step (mono).  Only L is processed and only
    // L's plate current feeds the shared PSU.
    auto stepChainMono = [&](double& uL) noexcept
    {
        if (sharedPSUActive_)
        {
            const double Vb = sharedPSU_.currentVb();
            chainL_.setExternalVb(Vb);
            uL = chainL_.process(uL);
            sharedPSU_.process(chainL_.lastTotalIp());
        }
        else
        {
            uL = chainL_.process(uL);
        }
    };

    // Upsample → chain → downsample at the active factor.  In mono mode we
    // deliberately skip chainR_ so tone and CPU reflect a true single-channel
    // signal path.
    auto processSamplePair = [&](double xL, double xR,
                                 double& outL, double& outR) noexcept
    {
        if (! stereo)
        {
            switch (osFactor)
            {
                case 1:
                {
                    outL = xL;
                    outR = 0.0;
                    stepChainMono(outL);
                    return;
                }
                case 2:
                {
                    auto upL = os2L_.upsample(xL);
                    for (auto& v : upL) stepChainMono(v);
                    outL = os2L_.downsample(upL);
                    outR = 0.0;
                    return;
                }
                case 8:
                {
                    auto upL = os8L_.upsample(xL);
                    for (auto& v : upL) stepChainMono(v);
                    outL = os8L_.downsample(upL);
                    outR = 0.0;
                    return;
                }
                case 4:
                default:
                {
                    auto upL = os4L_.upsample(xL);
                    for (auto& v : upL) stepChainMono(v);
                    outL = os4L_.downsample(upL);
                    outR = 0.0;
                    return;
                }
            }
        }

        switch (osFactor)
        {
            case 1:
            {
                outL = xL; outR = xR;
                stepChainsStereo(outL, outR);
                return;
            }
            case 2:
            {
                auto upL = os2L_.upsample(xL);
                auto upR = os2R_.upsample(xR);
                for (std::size_t i = 0; i < upL.size(); ++i)
                    stepChainsStereo(upL[i], upR[i]);
                outL = os2L_.downsample(upL);
                outR = os2R_.downsample(upR);
                return;
            }
            case 8:
            {
                auto upL = os8L_.upsample(xL);
                auto upR = os8R_.upsample(xR);
                for (std::size_t i = 0; i < upL.size(); ++i)
                    stepChainsStereo(upL[i], upR[i]);
                outL = os8L_.downsample(upL);
                outR = os8R_.downsample(upR);
                return;
            }
            case 4:
            default:
            {
                auto upL = os4L_.upsample(xL);
                auto upR = os4R_.upsample(xR);
                for (std::size_t i = 0; i < upL.size(); ++i)
                    stepChainsStereo(upL[i], upR[i]);
                outL = os4L_.downsample(upL);
                outR = os4R_.downsample(upR);
                return;
            }
        }
    };

    for (int n = 0; n < numSamples; ++n)
    {
        // Sample-accurate parameter ramps — one getNextValue() pulls per sample.
        const float drive   = driveSmooth_  .getNextValue();
        const float outGain = outGainSmooth_.getNextValue();
        const float mix     = mixSmooth_    .getNextValue();

        const float dryL = L[n];
        const float dryR = stereo ? R[n] : dryL;

        // Bypass path: skip drive, OS, and the entire tube chain, but keep
        // the dry-delay running so the plugin's reported latency still
        // matches what comes out.  The DAW's PDC compensator then lines the
        // track up with other bus material cleanly.  We also keep feeding
        // the harmonic analyzer so its view quiets to the input spectrum
        // instead of freezing on the last active frame.
        if (bypass)
        {
            const float bL = dryDelayL_.process(dryL);
            const float bR = stereo ? dryDelayR_.process(dryR) : bL;
            L[n] = bL;
            if (stereo) R[n] = bR;
            analyzer_.push(bL);
            continue;
        }

        // Apply drive before oversampling
        const double xL = static_cast<double>(dryL) * drive;
        const double xR = static_cast<double>(dryR) * drive;

        double wetL = 0.0, wetR = 0.0;
        processSamplePair(xL, xR, wetL, wetR);

        // NaN / Inf guard: if the DSP graph briefly diverges (e.g. a
        // pathological parameter transient) we substitute zeros rather than
        // letting a single corrupted sample propagate into the DAW mixer.
        if (! std::isfinite(wetL)) wetL = 0.0;
        if (! std::isfinite(wetR)) wetR = 0.0;

        // Delay the dry signal by the same amount the wet path is offset by
        // the oversampling FIRs.  Without this, wet/dry mix and null-test
        // subtraction lose their phase relationship at OS > 1.
        const float dryLd = dryDelayL_.process(dryL);
        const float dryRd = stereo ? dryDelayR_.process(dryR) : dryLd;

        float outL, outR;
        if (nullTest)
        {
            // Null test: play the exact contribution of the tube chain
            // without any makeup gain. Using the raw (chain − dry) result
            // lets the listener hear purely "what Valvra adds", independent
            // of the Output knob position.
            outL = static_cast<float>(wetL - dryLd);
            outR = stereo ? static_cast<float>(wetR - dryRd) : 0.0f;
        }
        else
        {
            // Wet/dry crossfade, then a single makeup gain applied to the
            // mix. This makes Output behave intuitively regardless of Mix.
            outL = static_cast<float>(
                        (((1.0 - mix) * dryLd) + (mix * wetL)) * outGain);
            outR = stereo
                 ? static_cast<float>(
                        (((1.0 - mix) * dryRd) + (mix * wetR)) * outGain)
                 : 0.0f;
        }

        L[n] = outL;
        if (stereo) R[n] = outR;

        // Feed the harmonic analyzer (left channel only for efficiency)
        analyzer_.push(outL);
    }

    // Snapshot the output transformer's JA state (for B-H loop view).
    bhState_[0].store(static_cast<float>(chainL_.outputTrafoH()),
                      std::memory_order_relaxed);
    bhState_[1].store(static_cast<float>(chainL_.outputTrafoM()),
                      std::memory_order_relaxed);
    bhState_[2].store(static_cast<float>(chainL_.outputTrafoMs()),
                      std::memory_order_relaxed);
}

// ─────────────────────────────────────────────────────────────────────────────
// State save / load
// ─────────────────────────────────────────────────────────────────────────────
void ValvraProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = params_.copyState();
    const std::uint64_t seed =
        currentSeed_.load(std::memory_order_relaxed);
    state.setProperty("valvra_seed",
                      juce::var(static_cast<juce::int64>(seed)),
                      nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void ValvraProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // Runs on the message thread.  Do NOT touch chainL_/chainR_ here —
    // delegate to the audio thread via the rebuild flag so reconfiguration
    // happens at a safe boundary.
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        if (xml->hasTagName(params_.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml(*xml);
            params_.replaceState(tree);
            if (tree.hasProperty("valvra_seed"))
                currentSeed_.store(
                    static_cast<std::uint64_t>(
                        static_cast<juce::int64>(tree["valvra_seed"])),
                    std::memory_order_relaxed);
            rebuildRequested_.store(true, std::memory_order_relaxed);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Monte Carlo re-roll.  Must be callable from the UI thread without touching
// the live DSP graph directly — hand-off to the audio thread via a flag.
// ─────────────────────────────────────────────────────────────────────────────
void ValvraProcessor::reroll()
{
    const std::uint64_t newSeed =
        static_cast<std::uint64_t>(juce::Time::getHighResolutionTicks())
        ^ 0xC3C3C3C3C3C3C3C3ULL;
    pendingSeed_.store(newSeed, std::memory_order_relaxed);
    rerollRequested_.store(true, std::memory_order_relaxed);
    // Reflect immediately in the seed label; the audio thread will actually
    // apply the new value on its next processBlock() tick.
    currentSeed_.store(newSeed, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* ValvraProcessor::createEditor()
{
    return new ValvraEditor(*this);
}

// ─────────────────────────────────────────────────────────────────────────────
// Factory preset loader.  Called from the UI; writes to ValueTreeState
// atomically and defers the actual DSP graph rebuild to the audio thread.
// ─────────────────────────────────────────────────────────────────────────────
void ValvraProcessor::loadFactoryPreset(int index)
{
    const auto& presets = factoryPresets();
    if (index < 0 || index >= static_cast<int>(presets.size())) return;
    const auto& p = presets[static_cast<std::size_t>(index)];

    // Use setValueNotifyingHost so DAW automation lanes pick up the change.
    if (auto* m = dynamic_cast<juce::AudioParameterChoice*>(
            params_.getParameter(kParamPreset)))
        *m = static_cast<int>(p.mode);
    if (auto* d = dynamic_cast<juce::AudioParameterFloat*>(
            params_.getParameter(kParamDrive)))
        *d = p.drive;
    if (auto* o = dynamic_cast<juce::AudioParameterFloat*>(
            params_.getParameter(kParamOutputDb)))
        *o = p.outputDb;
    if (auto* mx = dynamic_cast<juce::AudioParameterFloat*>(
            params_.getParameter(kParamMix)))
        *mx = p.mix;
    if (auto* os = dynamic_cast<juce::AudioParameterChoice*>(
            params_.getParameter(kParamOversample)))
        *os = p.oversampleIdx;

    currentSeed_.store(p.seed, std::memory_order_relaxed);
    rebuildRequested_.store(true, std::memory_order_relaxed);
}

} // namespace valvra

// ─── JUCE entry point ────────────────────────────────────────────────────────
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new valvra::ValvraProcessor();
}
