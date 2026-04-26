// ─────────────────────────────────────────────────────────────────────────────
// PluginProcessor.h — JUCE AudioProcessor wrapping Valvra's DSP engine
//
// Exposes the TubeAmpChain through a clean parameter surface so DAW
// automation, preset management, and state save/load all work.
//
// References:
//   docs/20 §4.4 (chain builder)
//   docs/24 §A–D (preset modes)
//   docs/16 (UI/UX spec for parameter layout)
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "TubeAmpChain.h"
#include "PolyphaseOversampler.h"
#include "HarmonicAnalyzer.h"

#include <atomic>

namespace valvra {

class ValvraProcessor final
    : public juce::AudioProcessor
    , private juce::AudioProcessorValueTreeState::Listener
{
public:
    ValvraProcessor();
    ~ValvraProcessor() override;

    // ─── AudioProcessor interface ──────────────────────────────────────────
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;

    void processBlock(juce::AudioBuffer<float>& buffer,
                      juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    /// DAW-integrated bypass.  Returning this parameter tells the host to
    /// respect the plugin's own bypass state (the DAW crossfades smoothly
    /// and compensates PDC as if the plugin were bypassed externally).
    juce::AudioProcessorParameter* getBypassParameter() const override
    {
        return bypassParam_;
    }

    const juce::String getName() const override { return "Valvra"; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.2; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ─── Public accessors (used by editor) ─────────────────────────────────
    juce::AudioProcessorValueTreeState& parameters() noexcept { return params_; }

    /// Trigger a Monte Carlo re-roll (generates new seed, hot-updates chain)
    void reroll();

    /// Current variation seed — for state save/load and UI display.
    /// Reads an atomic so the UI thread never races against the audio
    /// thread's reroll consumption.
    std::uint64_t currentSeed() const noexcept
    {
        return currentSeed_.load(std::memory_order_relaxed);
    }

    // ─── Tier-2 diagnostics accessors (UI reads these on the message thread) ──
    /// Latest FFT snapshot (H1..H7) from the left channel
    HarmonicSnapshot latestHarmonics() const noexcept
    {
        return analyzer_.readSnapshot();
    }

    /// Run the rolling FFT once (call from UI Timer callback).
    void refreshHarmonicSnapshot() noexcept { analyzer_.updateSnapshot(); }

    /// Output transformer JA state (for B-H loop visualization).
    struct BHSnapshot { float H, M, Ms; };
    BHSnapshot readBHState() const noexcept
    {
        return BHSnapshot {
            bhState_[0].load(std::memory_order_relaxed),
            bhState_[1].load(std::memory_order_relaxed),
            bhState_[2].load(std::memory_order_relaxed)
        };
    }

    /// Null-test mode: when true, output = (active - bypass), i.e. only
    /// the change introduced by Valvra.  This is the quantitative audible
    /// proof of what the plugin is doing.
    void setNullTestMode(bool enabled) noexcept { nullTestMode_ = enabled; }
    bool nullTestMode() const noexcept { return nullTestMode_; }

    /// Load a factory preset by index (see FactoryPresets.h).  Sets all
    /// parameters + seed atomically; the audio thread picks it up at the
    /// start of the next processBlock() via rebuildRequested_.
    void loadFactoryPreset(int index);

private:
    // AudioProcessorValueTreeState::Listener
    void parameterChanged(const juce::String& paramID, float newValue) override;

    // Parameter IDs
    static constexpr const char* kParamPreset    = "preset";
    static constexpr const char* kParamDrive     = "drive";
    static constexpr const char* kParamOutputDb  = "outputDb";
    static constexpr const char* kParamMix       = "mix";
    static constexpr const char* kParamOversample = "oversample";
    static constexpr const char* kParamBypass    = "bypass";

    // Rebuild the chain from the currently-selected preset + seed.
    // Called on prepareToPlay and whenever preset changes.
    void rebuildChain();

    /// Map the Quality combobox index to a numeric OS factor (1/2/4/8).
    int currentOversampleFactor() const noexcept;

    juce::AudioProcessorValueTreeState params_;
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Cached pointer into params_ for fast reads in processBlock + for
    // returning via getBypassParameter().  Owned by the ValueTreeState.
    juce::AudioParameterBool* bypassParam_ { nullptr };

    // DSP engine — stereo (two independent chains with DIFFERENT seeds)
    dsp::TubeAmpChain chainL_;
    dsp::TubeAmpChain chainR_;

    // Shared-PSU stereo coupling.  In real two-channel tube gear, both
    // channels share the same B+ rail: a heavy transient on L pulls the
    // supply voltage down, biasing R's tubes slightly differently on
    // subsequent samples.  This creates the "glue" and program-dependent
    // stereo image modulation that's almost universally missing from
    // competing plugins.  We enable it whenever the preset uses PSU sag
    // (V72, Culture Vulture); solid-state-rectifier presets (Marshall,
    // RNDI) keep their stiff, per-channel-independent rails.
    dsp::PowerSupplySag sharedPSU_ {};
    bool                sharedPSUActive_ { false };

    // Oversamplers for each supported factor.  We keep all four alive so
    // the active path is selected per-block by the current Quality param
    // without re-allocating filter tables in the audio thread.
    dsp::PolyphaseOversampler<2> os2L_, os2R_;
    dsp::PolyphaseOversampler<4> os4L_, os4R_;
    dsp::PolyphaseOversampler<8> os8L_, os8R_;

    double sampleRate_  { 48000.0 };
    int    preparedBlockSize_ { 0 };
    std::atomic<std::uint64_t> currentSeed_ { 0 };
    int    currentPresetIndex_ { 0 };
    int    lastOsFactor_ { 4 };  // detect Quality-combo changes

    // Parameter smoothers — prevent zipper noise when the user moves knobs.
    // 20 ms smoothing is long enough to be inaudible but short enough to
    // remain responsive for fast automation curves.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>       driveSmooth_;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> outGainSmooth_;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>       mixSmooth_;

    // Message-thread → audio-thread hand-off flags.  ALL DSP-graph mutations
    // must route through one of these; direct calls into chainL_/chainR_
    // from non-audio threads are UB.
    std::atomic<bool>           rebuildRequested_  { false };
    std::atomic<bool>           rerollRequested_   { false };
    std::atomic<std::uint64_t>  pendingSeed_       { 0 };

    // Tier-2 additions
    HarmonicAnalyzer analyzer_ {};
    std::atomic<bool> nullTestMode_ { false };

    // B-H state snapshot (H, M, Ms) — written by audio thread, read by UI.
    std::array<std::atomic<float>, 3> bhState_ {};

    // Dry-path delay compensation.  The polyphase oversamplers introduce
    // 64/Factor samples of linear-phase group delay on the wet path.  For
    // wet/dry crossfade and null-test subtraction to remain phase-aligned,
    // the dry signal must be delayed by the same amount *inside the plugin*.
    // The DAW separately compensates the overall plugin output via the
    // setLatencySamples() report, so downstream tracks stay tight too.
    struct DryDelay
    {
        static constexpr std::size_t kSize = 128;      // > max latency (32)
        static constexpr std::size_t kMask = kSize - 1;
        std::array<float, kSize> buf {};
        std::size_t write { 0 };
        int latency { 0 };

        void clear() noexcept { buf.fill(0.0f); write = 0; }
        void setLatency(int n) noexcept
        {
            latency = (n < 0) ? 0
                    : (n >= static_cast<int>(kSize))
                        ? static_cast<int>(kSize) - 1 : n;
            clear();
        }
        float process(float x) noexcept
        {
            buf[write & kMask] = x;
            const std::size_t r =
                (write - static_cast<std::size_t>(latency)) & kMask;
            ++write;
            return buf[r];
        }
    };
    DryDelay dryDelayL_ {};
    DryDelay dryDelayR_ {};

    // Map the current oversample factor to the expected base-rate latency.
    int currentLatencyInSamples() const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ValvraProcessor)
};

} // namespace valvra
