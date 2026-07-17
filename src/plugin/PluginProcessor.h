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
#include "TruePeakLimiter.h"
#include "HarmonicAnalyzer.h"
#include "NeuralResidualLayer.h"
#include "ExpansionRack.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <vector>

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

    /// True when both channels are using an RTNeural JSON model.
    bool neuralModelLoaded() const noexcept
    {
        return neuralModelLoadedState_.load(std::memory_order_relaxed);
    }

    /// Load an RTNeural JSON model file for both channels.
    /// Returns false if RTNeural is disabled or parsing fails.
    bool loadNeuralModelFile(const juce::String& path);
    void unloadNeuralModel() { loadNeuralModelFile({}); }
    juce::String neuralModelPath() const { return neuralModelPath_; }

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

    /// Long-running diagnostic state, sampled at the end of each processBlock
    /// for the Drift Recorder and Warmup HUD.  Written by the audio thread,
    /// read by UI Timer callbacks — atomics with relaxed ordering are safe
    /// because each scalar is independently meaningful.
    struct DriftSnapshot
    {
        float sagPercent;       ///< 0–15 typical (PSU sag amount)
        float warmup;           ///< 0.85–1.0 (gm fraction from cold)
        float thermalBiasV;     ///< abs of slow grid-bias drift in volts
    };
    DriftSnapshot readDriftState() const noexcept
    {
        return DriftSnapshot {
            sagPctState_     .load(std::memory_order_relaxed),
            warmupState_     .load(std::memory_order_relaxed),
            thermalDriftState_.load(std::memory_order_relaxed)
        };
    }

    /// Re-arm the warmup envelope of every stage to ~85% gm so the user can
    /// audibly hear the rack "come up" again.  Hand-off is via a flag picked
    /// up by the audio thread.
    void triggerWarmup() noexcept
    {
        warmupRequested_.store(true, std::memory_order_relaxed);
    }

    /// Last-N seed history for the Reroll Timeline panel.  Most-recent entry
    /// last.  Written when reroll() runs; read on the message thread.
    static constexpr int kSeedHistorySize = 10;
    std::array<std::uint64_t, kSeedHistorySize> seedHistory() const noexcept
    {
        std::array<std::uint64_t, kSeedHistorySize> snapshot {};
        for (int i = 0; i < kSeedHistorySize; ++i)
            snapshot[static_cast<std::size_t>(i)] =
                seedHistory_[static_cast<std::size_t>(i)]
                    .load(std::memory_order_relaxed);
        return snapshot;
    }
    int seedHistoryCount() const noexcept
    {
        return seedHistoryCount_.load(std::memory_order_relaxed);
    }

    /// Restore a seed previously listed in seedHistory().  Same path as
    /// reroll() but with a fixed seed instead of a freshly-rolled one.
    void recallSeed(std::uint64_t seed) noexcept;

    // ─── Mastering: True Peak limiter gain reduction (UI meter) ────────────
    /// Latest gain reduction in dB.  0 = limiter not engaged; negative when
    /// the limiter is pulling peaks down.  Atomically published by the
    /// audio thread at the end of each processBlock.
    float gainReductionDb() const noexcept
    {
        return gainReductionDbState_.load(std::memory_order_relaxed);
    }

    struct MasteringSnapshot
    {
        float momentaryLufs;
        float shortTermLufs;
        float integratedLufs;
        float truePeakDbtp;
        float peakDbfs;
        float gainReductionDb;
        float correlation;
        float inputRmsDbfs;
        float inputPeakDbfs;
        float inputTrimNeededDb;
        float levelMatchAppliedDb;
        float targetMatchScore;
        int targetMatchState;
        float realismApplied;
        float crosstalkDb;
        float noiseFloorDbfs;
        int targetReasonCode;
        float textureRecovery;
        float microMotion;
        float lowLevelHarmonicSlope;
        float interactionDrive;
    };
    MasteringSnapshot readMasteringState() const noexcept
    {
        return MasteringSnapshot {
            momentaryLufsState_.load(std::memory_order_relaxed),
            shortTermLufsState_.load(std::memory_order_relaxed),
            integratedLufsState_.load(std::memory_order_relaxed),
            truePeakDbtpState_.load(std::memory_order_relaxed),
            peakDbfsState_.load(std::memory_order_relaxed),
            gainReductionDbState_.load(std::memory_order_relaxed),
            correlationState_.load(std::memory_order_relaxed),
            inputRmsDbfsState_.load(std::memory_order_relaxed),
            inputPeakDbfsState_.load(std::memory_order_relaxed),
            inputTrimNeededDbState_.load(std::memory_order_relaxed),
            levelMatchAppliedDbState_.load(std::memory_order_relaxed),
            targetMatchScoreState_.load(std::memory_order_relaxed),
            targetMatchState_.load(std::memory_order_relaxed),
            realismAppliedState_.load(std::memory_order_relaxed),
            crosstalkDbState_.load(std::memory_order_relaxed),
            noiseFloorDbfsState_.load(std::memory_order_relaxed),
            targetReasonCodeState_.load(std::memory_order_relaxed),
            textureRecoveryState_.load(std::memory_order_relaxed),
            microMotionState_.load(std::memory_order_relaxed),
            lowLevelHarmonicSlopeState_.load(std::memory_order_relaxed),
            interactionDriveState_.load(std::memory_order_relaxed)
        };
    }

    /// Set Input Trim so the measured pre-drive level lands around -18 dBFS RMS.
    void calibrateInputToMinus18();

    /// Snapshot recent input/output loudness and write Analyze Match trim.
    void analyzeLevelMatch();

    // ─── A/B compare ───────────────────────────────────────────────────────
    /// Toggle the A/B slot.  Stores the live parameter state into the
    /// currently-active slot and loads the *other* slot's state into live.
    /// First call populates the destination from the current state, so
    /// subsequent toggles meaningfully compare two distinct settings.
    void toggleAB();

    /// Copy the live state into the inactive slot (so a subsequent toggle
    /// can compare the "modified" version back to this snapshot).
    void copyToInactiveSlot();
    void copyAToB();
    void copyBToA();
    void resetAB();
    void toggleABForCompare();
    void setABBlindModeWithHistory(bool enabled);
    void setABBlindMode(bool enabled) noexcept;
    bool abBlindMode() const noexcept { return abBlindMode_; }
    bool canUndoAB() const noexcept;
    bool canRedoAB() const noexcept;
    void undoAB();
    void redoAB();

    enum class SnapshotSlot : int { C = 0, D = 1, E = 2 };
    void storeSnapshot(SnapshotSlot slot);
    bool loadSnapshot(SnapshotSlot slot);
    bool hasSnapshot(SnapshotSlot slot) const noexcept;

    /// True when the active slot is "B" (used by the UI to label the toggle).
    bool isOnSlotB() const noexcept { return slotIsB_; }

    /// Null-test mode: when true, output = (active - bypass), i.e. only
    /// the change introduced by Valvra.  This is the quantitative audible
    /// proof of what the plugin is doing.
    void setNullTestMode(bool enabled) noexcept { nullTestMode_ = enabled; }
    bool nullTestMode() const noexcept { return nullTestMode_; }

    float uiScale() const noexcept
    {
        return uiScaleState_.load(std::memory_order_relaxed);
    }
    void setUiScale(float scale) noexcept
    {
        uiScaleState_.store(
            juce::jlimit(1.0f, 2.0f, scale),
            std::memory_order_relaxed);
    }

    /// Load a factory preset by index (see FactoryPresets.h).  Sets all
    /// parameters + seed atomically; the audio thread picks it up at the
    /// start of the next processBlock() via rebuildRequested_.
    void loadFactoryPreset(int index);

    // Per-stage editor parameter IDs.  Each of 1..4 stages exposes a tube
    // model, topology, drive trim, and bias offset.  All default to "Preset"
    // (or 0 for floats), meaning "leave the preset's stage configuration
    // alone" — so dialing them only kicks in when the user actually moves a
    // control.  This is the "per-stage 편집 최소판" called out in docs/26 §6.
    // Public so the StageEditorPanel can wire its widgets to the right
    // parameter IDs at runtime.
    struct PerStageParamIds
    {
        const char* tube;
        const char* topology;
        const char* drive;     ///< [-12, +12] dB trim on inputVoltageSwing
        const char* bias;      ///< [-0.8, +0.8] V offset on Vg_bias
    };
    static constexpr std::array<PerStageParamIds, 4> kStageParams {{
        { "stage1_tube", "stage1_topology", "stage1_drive", "stage1_bias" },
        { "stage2_tube", "stage2_topology", "stage2_drive", "stage2_bias" },
        { "stage3_tube", "stage3_topology", "stage3_drive", "stage3_bias" },
        { "stage4_tube", "stage4_topology", "stage4_drive", "stage4_bias" }
    }};

private:
    // AudioProcessorValueTreeState::Listener
    void parameterChanged(const juce::String& paramID, float newValue) override;

    // Parameter IDs
    static constexpr const char* kParamPreset    = "preset";
    static constexpr const char* kParamDrive     = "drive";
    static constexpr const char* kParamInputTrimDb = "inputTrimDb";
    static constexpr const char* kParamOutputDb  = "outputDb";
    static constexpr const char* kParamMix       = "mix";
    static constexpr const char* kParamLevelMatchMode = "levelMatchMode";
    static constexpr const char* kParamAnalyzedOutputTrimDb = "analyzedOutputTrimDb";
    static constexpr const char* kParamTargetProfile = "targetProfile";
    static constexpr const char* kParamRealismAmount = "realismAmount";
    static constexpr const char* kParamOversample = "oversample";
    static constexpr const char* kParamStageCount = "stageCount";
    static constexpr const char* kParamInputTrafo = "inputTrafo";
    static constexpr const char* kParamOutputTrafo = "outputTrafo";
    static constexpr const char* kParamBypass    = "bypass";
    static constexpr const char* kParamMcLock    = "mcLock";
    static constexpr const char* kParamMcDistribution = "mcDistribution";
    static constexpr const char* kParamCvMode    = "cvMode";
    static constexpr const char* kParamNeuralBlend = "neuralBlend";
    static constexpr const char* kParamExpansionMode = "expansionMode";
    static constexpr const char* kParamExpansionAmount = "expansionAmount";
    static constexpr const char* kParamExpansionMix = "expansionMix";

    // Mastering section
    static constexpr const char* kParamTpEnabled    = "tpEnabled";
    static constexpr const char* kParamTpMode       = "tpMode";
    static constexpr const char* kParamTpCeilingDb  = "tpCeilingDb";
    static constexpr const char* kParamTpLookaheadMs = "tpLookaheadMs";
    static constexpr const char* kParamDitherEnable = "ditherEnabled";
    static constexpr const char* kParamDitherDepth  = "ditherDepth";
    static constexpr const char* kParamMSMode       = "msMode";

    struct ABSlot;
    struct ABHistoryState;

    // Rebuild the chain from the currently-selected preset + seed.
    // Called on prepareToPlay and whenever preset changes.
    void rebuildChain();
    void switchToABSlot(bool toB);
    bool nextBlindSlotIsB() noexcept;
    void performABAction(const std::function<void()>& action);
    void captureLiveIntoABSlot(struct ABSlot& slot);
    void copySlot(struct ABSlot& dst, const struct ABSlot& src);
    void clearSlot(struct ABSlot& slot);
    ABSlot& slotFromSnapshot(SnapshotSlot slot);
    const ABSlot& slotFromSnapshot(SnapshotSlot slot) const;

    ABHistoryState captureABHistoryState();
    void applyABHistoryState(const ABHistoryState& s);
    bool abHistoryStatesEqual(const ABHistoryState& a,
                              const ABHistoryState& b) const;
    void pushUndoAB(const ABHistoryState& s);
    void pushRedoAB(const ABHistoryState& s);
    ABHistoryState popUndoAB();
    ABHistoryState popRedoAB();
    void clearRedoAB() noexcept;
    void clearHistoryAB() noexcept;
    void writeABStateToTree(juce::ValueTree& tree) const;
    void restoreABStateFromTree(const juce::ValueTree& tree);

    /// Map the Quality combobox index to a numeric OS factor (1/2/4/8/16).
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
    dsp::PolyphaseOversampler<16> os16L_, os16R_;

    double sampleRate_  { 48000.0 };
    int    preparedBlockSize_ { 0 };
    std::atomic<std::uint64_t> currentSeed_ { 0 };
    int    currentPresetIndex_ { 0 };
    int    lastOsFactor_ { 4 };  // detect Quality-combo changes
    int    pendingOsFactor_ { 0 };
    bool   activeMsMode_ { false };
    bool   pendingMsMode_ { false };
    bool   pendingMsModeValid_ { false };

    // Slow-state carry-over across parameter-edit rebuilds (docs/34 §4.3):
    // fingerprint of the last built graph.  A rebuild with an unchanged
    // fingerprint (same preset / seed / internal rate / routing) re-bases
    // the previous chains' warmup, thermal, magnetic and supply history
    // onto the new operating points instead of cold-starting them.
    int           carryLastPreset_ { -1 };
    std::uint64_t carryLastSeed_   { 0 };
    double        carryLastSR_     { 0.0 };
    bool          carryLastMs_     { false };

    // Parameter smoothers — prevent zipper noise when the user moves knobs.
    // 20 ms smoothing is long enough to be inaudible but short enough to
    // remain responsive for fast automation curves.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>       driveSmooth_;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> inputTrimSmooth_;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> outGainSmooth_;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>       mixSmooth_;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>       neuralBlendSmooth_;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>       expansionAmountSmooth_;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>       expansionMixSmooth_;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>       bypassBlendSmooth_;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>       nullTestBlendSmooth_;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> abMatchSmooth_;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> levelMatchSmooth_;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>       realismSmooth_;

    // Message-thread → audio-thread hand-off flags.  ALL DSP-graph mutations
    // must route through one of these; direct calls into chainL_/chainR_
    // from non-audio threads are UB.
    std::atomic<bool>           rebuildRequested_  { false };
    std::atomic<bool>           rerollRequested_   { false };
    std::atomic<std::uint64_t>  pendingSeed_       { 0 };

    // Click-free graph swaps.  Tube/seed/topology rebuilds reset stateful
    // analog models, so the audio thread fades the active graph down, applies
    // the rebuild at silence, then fades back up over 10 ms.
    enum class GraphFadePhase { Idle, FadeOut, FadeIn };
    GraphFadePhase graphFadePhase_ { GraphFadePhase::Idle };
    int            graphFadePos_ { 0 };
    int            graphFadeSamples_ { 480 };
    bool           graphFadePendingRebuild_ { false };
    bool           graphFadePendingReroll_ { false };
    std::uint64_t  graphFadePendingSeed_ { 0 };

    // Neural model hot-swap fade.  We fade only the neural contribution
    // (not the full signal path), apply the queued model at zero neural gain,
    // then fade neural contribution back in.
    enum class NeuralSwapPhase { Idle, FadeOut, FadeIn };
    NeuralSwapPhase neuralSwapPhase_ { NeuralSwapPhase::Idle };
    int             neuralSwapPos_ { 0 };
    int             neuralSwapSamples_ { 240 };
    bool            neuralSwapPendingApply_ { false };

    // Tier-2 additions
    HarmonicAnalyzer analyzer_ {};
    std::atomic<bool> nullTestMode_ { false };
    std::atomic<float> uiScaleState_ { 1.0f };

    // Tier-3 bootstrap: lightweight neural residual layer.  Default blend=0
    // so the signal path remains pure physics unless the user opts in.
    struct NeuralSwapRequest
    {
        dsp::NeuralResidualLayer left {};
        dsp::NeuralResidualLayer right {};
        bool loaded { false };
    };
    dsp::NeuralResidualLayer neuralL_ {};
    dsp::NeuralResidualLayer neuralR_ {};
    dsp::ExpansionRack expansionRack_ {};
    std::atomic<bool> neuralModelLoadedState_ { false };
    std::atomic<bool> neuralSwapRequested_ { false };
    std::atomic<NeuralSwapRequest*> neuralSwapRequest_ { nullptr };
    juce::String neuralModelPath_ {};

    // B-H state snapshot (H, M, Ms) — written by audio thread, read by UI.
    std::array<std::atomic<float>, 3> bhState_ {};

    // Drift / warmup state — written by audio thread at end of processBlock.
    std::atomic<float> sagPctState_      { 0.0f };
    std::atomic<float> warmupState_      { 1.0f };
    std::atomic<float> thermalDriftState_{ 0.0f };

    // Warmup trigger — UI flag, audio thread re-arms each stage.
    std::atomic<bool>  warmupRequested_ { false };

    // Seed history for the Reroll Timeline.  Fixed-size ring; older entries
    // sit at lower indices, newest at seedHistoryCount_ − 1 (clamped).
    std::array<std::atomic<std::uint64_t>, kSeedHistorySize> seedHistory_ {};
    std::atomic<int>   seedHistoryCount_ { 0 };
    void pushSeedHistory(std::uint64_t seed) noexcept;

    // ─── Mastering ─────────────────────────────────────────────────────────
    // True Peak limiter — single instance handles the (already mixed) stereo
    // master output.  Always runs the lookahead delay so PDC stays consistent
    // even when the user toggles TP off; the gain multiplier is the only
    // thing that flips between identity and active.
    dsp::TruePeakLimiter tpLimiter_ {};
    std::atomic<float>   gainReductionDbState_ { 0.0f };
    std::atomic<float>   momentaryLufsState_ { -100.0f };
    std::atomic<float>   shortTermLufsState_ { -100.0f };
    std::atomic<float>   integratedLufsState_ { -100.0f };
    std::atomic<float>   truePeakDbtpState_ { -100.0f };
    std::atomic<float>   peakDbfsState_ { -100.0f };
    std::atomic<float>   correlationState_ { 1.0f };
    std::atomic<float>   inputRmsDbfsState_ { -100.0f };
    std::atomic<float>   inputPeakDbfsState_ { -100.0f };
    std::atomic<float>   inputTrimNeededDbState_ { 0.0f };
    std::atomic<float>   levelMatchAppliedDbState_ { 0.0f };
    std::atomic<float>   targetMatchScoreState_ { 0.0f };
    std::atomic<int>     targetMatchState_ { 0 };
    std::atomic<float>   realismAppliedState_ { 0.0f };
    std::atomic<float>   crosstalkDbState_ { -120.0f };
    std::atomic<float>   noiseFloorDbfsState_ { -120.0f };
    std::atomic<int>     targetReasonCodeState_ { 0 };
    std::atomic<float>   textureRecoveryState_ { 0.0f };
    std::atomic<float>   microMotionState_ { 0.0f };
    std::atomic<float>   lowLevelHarmonicSlopeState_ { 0.0f };
    std::atomic<float>   interactionDriveState_ { 0.0f };
    int                  lastTpLatency_ { dsp::TruePeakLimiter::latencyInSamples() };
    // Output-safety soft-clip gating (see processBlock): the clip runs
    // only across a transition window, never in steady state.
    bool                 lastLimiterBypassed_ { false };
    int                  safetyHoldSamples_ { 0 };
    float                recentInputRmsDb_ { -100.0f };
    float                recentMatchOutputRmsDb_ { -100.0f };

    float currentLevelMatchTrimDb() const noexcept;
    float recommendedRealismForCurrentPreset() const noexcept;
    bool useFittedProfileForPreset(int presetIndex) const noexcept;
    bool useFittedProfileForTargetProfile(int profileIndex) const noexcept;
    std::uint64_t analogLeakageRng_ { 0x91E10DA5C79E7B1DULL };
    double realismHumPhaseL_ { 0.0 };
    double realismHumPhaseR_ { 1.7 };
    double realismNoiseEnergy_ { 0.0 };
    int realismNoiseCount_ { 0 };
    double feelFastEnv_ { 0.0 };
    double feelSlowEnv_ { 0.0 };
    double feelMotionLp_ { 0.0 };
    double feelPrevOut_ { 0.0 };
    double feelFastCoeff_ { 0.0 };
    double feelSlowCoeff_ { 0.0 };
    double feelMotionLpCoeff_ { 0.0 };
    std::array<bool, 5> useFittedProfileByMode_ { true, true, true, true, true };

    // TPDF dither RNG — xorshift64, seeded once per prepareToPlay so two
    // instances on adjacent tracks don't broadcast correlated dither noise.
    std::uint64_t ditherRng_ { 0xA5A5A5A5DEADBEEFULL };

    // ─── A/B compare ───────────────────────────────────────────────────────
    struct IntegratedLufsMeter
    {
        struct Biquad
        {
            double b0 {1.0}, b1 {0.0}, b2 {0.0}, a1 {0.0}, a2 {0.0};
            double z1 {0.0}, z2 {0.0};

            void reset() noexcept { z1 = 0.0; z2 = 0.0; }
            double process(double x) noexcept
            {
                const double y = b0 * x + z1;
                z1 = b1 * x - a1 * y + z2;
                z2 = b2 * x - a2 * y;
                return y;
            }
        };

        std::array<Biquad, 2> shelf {};
        std::array<Biquad, 2> highpass {};
        double energy { 0.0 };
        std::uint64_t samples { 0 };

        void prepare(double sampleRate) noexcept;
        void reset() noexcept;
        void process(float left, float right, bool stereo) noexcept;
        float lufs(float fallbackDb = -100.0f) const noexcept;
    };

    struct MasterLoudnessMeter
    {
        std::array<IntegratedLufsMeter::Biquad, 2> shelf {};
        std::array<IntegratedLufsMeter::Biquad, 2> highpass {};
        std::vector<double> momentaryRing {};
        std::vector<double> shortTermRing {};
        std::vector<double> gatedBlockEnergies {};
        double momentarySum { 0.0 };
        double shortTermSum { 0.0 };
        double blockEnergy { 0.0 };
        double lastIntegrated { -100.0 };
        double sampleRate { 48000.0 };
        std::size_t momentaryWrite { 0 };
        std::size_t shortTermWrite { 0 };
        std::size_t gatedWrite { 0 };
        std::size_t gatedCount { 0 };
        std::size_t momentaryCount { 0 };
        std::size_t shortTermCount { 0 };
        int blockSamples { 0 };
        int samplesInBlock { 0 };

        void prepare(double sr);
        void reset() noexcept;
        void process(float left, float right, bool stereo) noexcept;
        float momentaryLufs() const noexcept;
        float shortTermLufs() const noexcept;
        float integratedLufs() const noexcept;
    };

    // Two parameter snapshots.  We deliberately store full ValueTree copies
    // (not just a flat numeric vector) so that future params added to the
    // layout don't need a corresponding A/B serialization patch.
    struct ABSlot
    {
        juce::ValueTree state;
        std::uint64_t   seed { 0 };
        float           loudnessDb { -100.0f };
        IntegratedLufsMeter lufsMeter {};
        bool            populated { false };
    };
    ABSlot slotA_ {};
    ABSlot slotB_ {};
    ABSlot slotC_ {};
    ABSlot slotD_ {};
    ABSlot slotE_ {};
    bool   slotIsB_ { false };
    bool   abBlindMode_ { false };
    std::uint64_t abBlindRng_ { 0x8B5AD4CE9F1A2C67ULL };
    float  recentOutputLoudnessDb_ { -100.0f };
    MasterLoudnessMeter masterLoudnessMeter_ {};
    dsp::PolyphaseOversampler<4> masterTpMeterL_ {};
    dsp::PolyphaseOversampler<4> masterTpMeterR_ {};

    struct ABHistoryState
    {
        ABSlot slotA;
        ABSlot slotB;
        ABSlot slotC;
        ABSlot slotD;
        ABSlot slotE;
        juce::ValueTree liveState;
        std::uint64_t liveSeed { 0 };
        std::uint64_t blindRng { 0 };
        bool slotIsB { false };
        bool blindMode { false };
    };
    static constexpr int kABHistoryDepth = 32;
    std::array<ABHistoryState, kABHistoryDepth> abUndoHistory_ {};
    std::array<ABHistoryState, kABHistoryDepth> abRedoHistory_ {};
    int abUndoCount_ { 0 };
    int abRedoCount_ { 0 };

    // Dry-path delay compensation.  The polyphase oversamplers introduce
    // 64/Factor samples of linear-phase group delay on the wet path.  For
    // wet/dry crossfade and null-test subtraction to remain phase-aligned,
    // the dry signal must be delayed by the same amount *inside the plugin*.
    // The DAW separately compensates the overall plugin output via the
    // setLatencySamples() report, so downstream tracks stay tight too.
    struct DryDelay
    {
        // Must exceed the maximum OS round-trip latency it absorbs —
        // PolyphaseOversampler::latencyInBaseSamples() is 63 at every
        // factor since the factor-scaled FIR redesign.  256 keeps 4x
        // headroom; setLatency() clamps silently, so an undersized ring
        // would mis-align the dry path (comb filtering, broken null
        // test) with no error.
        static constexpr std::size_t kSize = 256;
        static constexpr std::size_t kMask = kSize - 1;
        static constexpr int kLatencyCrossfadeSamples = 64;
        std::array<float, kSize> buf {};
        std::size_t write { 0 };
        int latencyCurrent { 0 };
        int latencyTarget  { 0 };
        int latencyFadePos { 0 };
        bool latencyFadeActive { false };

        void clear() noexcept
        {
            buf.fill(0.0f);
            write = 0;
            latencyCurrent = 0;
            latencyTarget = 0;
            latencyFadePos = 0;
            latencyFadeActive = false;
        }
        void setLatency(int n) noexcept
        {
            const int clamped = (n < 0) ? 0
                : (n >= static_cast<int>(kSize))
                    ? static_cast<int>(kSize) - 1 : n;
            if (clamped == latencyTarget
                && (latencyFadeActive || clamped == latencyCurrent))
                return;

            latencyTarget = clamped;
            if (latencyTarget == latencyCurrent)
            {
                latencyFadeActive = false;
                latencyFadePos = 0;
                return;
            }

            if (write == 0)
            {
                latencyCurrent = latencyTarget;
                latencyFadeActive = false;
                latencyFadePos = 0;
                return;
            }

            latencyFadeActive = true;
            latencyFadePos = 0;
        }
        float process(float x) noexcept
        {
            buf[write & kMask] = x;
            const std::size_t rCur =
                (write - static_cast<std::size_t>(latencyCurrent)) & kMask;
            const float yCur = buf[rCur];

            float y = yCur;
            if (latencyFadeActive)
            {
                const std::size_t rNew =
                    (write - static_cast<std::size_t>(latencyTarget)) & kMask;
                const float yNew = buf[rNew];
                const float t = static_cast<float>(latencyFadePos + 1)
                    / static_cast<float>(kLatencyCrossfadeSamples);
                y = yCur * (1.0f - t) + yNew * t;

                ++latencyFadePos;
                if (latencyFadePos >= kLatencyCrossfadeSamples)
                {
                    latencyCurrent = latencyTarget;
                    latencyFadePos = 0;
                    latencyFadeActive = false;
                }
            }

            ++write;
            return y;
        }
    };
    DryDelay dryDelayL_ {};
    DryDelay dryDelayR_ {};

    // Map the current oversample factor to the expected base-rate latency.
    int currentLatencyInSamples() const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ValvraProcessor)
};

} // namespace valvra
