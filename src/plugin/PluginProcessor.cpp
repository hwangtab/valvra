// ─────────────────────────────────────────────────────────────────────────────
// PluginProcessor.cpp — Valvra host-level audio processor
// ─────────────────────────────────────────────────────────────────────────────
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

#include <cstdlib>

namespace valvra {

#ifndef VALVRA_DEFAULT_EXPANSION_MODE
#define VALVRA_DEFAULT_EXPANSION_MODE 0
#endif

#ifndef VALVRA_DEFAULT_EXPANSION_AMOUNT
#define VALVRA_DEFAULT_EXPANSION_AMOUNT 0.0
#endif

#ifndef VALVRA_DEFAULT_EXPANSION_MIX
#define VALVRA_DEFAULT_EXPANSION_MIX 1.0
#endif

namespace {
constexpr int kDefaultExpansionMode =
    (VALVRA_DEFAULT_EXPANSION_MODE < 0 || VALVRA_DEFAULT_EXPANSION_MODE > 4)
        ? 0 : VALVRA_DEFAULT_EXPANSION_MODE;
constexpr float kDefaultExpansionAmount =
    (VALVRA_DEFAULT_EXPANSION_AMOUNT < 0.0
        || VALVRA_DEFAULT_EXPANSION_AMOUNT > 1.0)
            ? 0.0f
            : static_cast<float>(VALVRA_DEFAULT_EXPANSION_AMOUNT);
constexpr float kDefaultExpansionMix =
    (VALVRA_DEFAULT_EXPANSION_MIX < 0.0
        || VALVRA_DEFAULT_EXPANSION_MIX > 1.0)
            ? 1.0f
            : static_cast<float>(VALVRA_DEFAULT_EXPANSION_MIX);
}

namespace {

dsp::TubeAmpChainConfig buildPresetConfig(int presetIndex, std::uint64_t seed)
{
    dsp::TubeAmpChainConfig cfg;
    switch (presetIndex)
    {
        case 0: cfg = dsp::chain_presets::V72Preamp();          break;
        case 1: cfg = dsp::chain_presets::MarshallMode();       break;
        case 2: cfg = dsp::chain_presets::CultureVultureMode(); break;
        case 3: cfg = dsp::chain_presets::RNDIMode();           break;
        case 4: cfg = dsp::chain_presets::HiFi300BMode();       break;
        default: cfg = dsp::chain_presets::V72Preamp();         break;
    }
    cfg.variationSeed = seed;
    return cfg;
}

float modeDriveScaleForPresetIndex(int presetIndex) noexcept
{
    // Keep V72 as-is (legacy behavior/reference anchor).
    // Lift lower-output modes so users do not need excessive Drive for
    // practical color density.
    switch (presetIndex)
    {
        case 1: return 1.6f; // Console Output
        case 2: return 1.9f; // Culture Vulture
        case 3: return 1.8f; // RNDI
        case 4: return 1.3f; // HiFi 300B
        case 0:
        default: return 1.0f; // V72
    }
}

float modeOutputTrimDbForPresetIndex(int presetIndex) noexcept
{
    // V72 remains the reference anchor.  The other modes are post-chain
    // trims, so they reduce loudness bias without changing how hard the
    // non-linear stages are driven.
    switch (presetIndex)
    {
        case 1: return 6.0f;   // Console Output
        case 2: return 12.0f;  // Culture Vulture
        case 3: return 9.0f;   // RNDI
        case 4: return 6.0f;   // HiFi 300B
        case 0:
        default: return 0.0f;  // V72
    }
}

float dbFromRms(double value) noexcept
{
    return static_cast<float>(
        20.0 * std::log10(std::max(value, 1.0e-9)));
}

float mapRealismControl(float userValue) noexcept
{
    const float x = juce::jlimit(0.0f, 1.0f, userValue);
    if (x <= 0.30f)
    {
        const float t = x / 0.30f;
        return 0.20f * t * t;
    }
    if (x <= 0.65f)
    {
        const float t = (x - 0.30f) / 0.35f;
        return 0.20f + 0.55f * std::pow(t, 0.85f);
    }
    const float t = (x - 0.65f) / 0.35f;
    return 0.75f + 0.25f * std::pow(t, 1.35f);
}

struct TargetProfileSpec
{
    float driveMin;
    float driveMax;
    float feedbackAmount;
    float transformerLoading;
    float interstageDA;
    float crosstalkDb;
    float noiseFloorDbfs;
    float recommendedRealism;
};

constexpr std::array<const char*, 5> kProfileVersionStateKeys {
    "valvra_profile_version_v72",
    "valvra_profile_version_console",
    "valvra_profile_version_cv",
    "valvra_profile_version_rndi",
    "valvra_profile_version_hifi"
};
constexpr const char* kLegacyProfileVersion = "legacy";
constexpr const char* kFittedProfileVersion = "fitted_v1";
constexpr const char* kLegacyV72OnlyKey = "valvra_v72_profile_version";

TargetProfileSpec legacyProfileSpec(int profileIndex) noexcept
{
    switch (profileIndex)
    {
        case 2: return { 1.2f, 2.6f, 0.10f, 0.55f, 0.040f, -70.0f, -78.0f, 0.30f }; // Console Output
        case 3: return { 2.0f, 3.0f, 0.03f, 0.70f, 0.085f, -66.0f, -72.0f, 0.45f }; // Culture Vulture
        case 4: return { 1.6f, 2.8f, 0.02f, 0.42f, 0.025f, -76.0f, -82.0f, 0.25f }; // RNDI
        case 5: return { 1.8f, 3.0f, 0.14f, 0.30f, 0.020f, -78.0f, -84.0f, 0.20f }; // HiFi 300B
        case 1:
        default: return { 0.55f, 1.45f, 0.12f, 0.48f, 0.055f, -72.0f, -80.0f, 0.35f }; // V72
    }
}

TargetProfileSpec fittedProfileSpecV1(int profileIndex) noexcept
{
    // 1차 측정 기반 피팅값 (문서/내부 데이터 앵커). 실측 리그 데이터가
    // 확보되면 피팅 스크립트 결과로 갱신한다.
    switch (profileIndex)
    {
        case 2: return { 1.15f, 2.55f, 0.105f, 0.58f, 0.045f, -69.0f, -77.0f, 0.31f }; // Console Output
        case 3: return { 1.95f, 2.95f, 0.035f, 0.73f, 0.090f, -65.0f, -71.0f, 0.46f }; // Culture Vulture
        case 4: return { 1.55f, 2.75f, 0.022f, 0.44f, 0.028f, -75.0f, -81.0f, 0.26f }; // RNDI
        case 5: return { 1.75f, 2.95f, 0.145f, 0.33f, 0.022f, -77.0f, -83.0f, 0.22f }; // HiFi 300B
        case 1:
        default: return { 0.58f, 1.42f, 0.115f, 0.52f, 0.060f, -71.0f, -79.0f, 0.36f }; // V72
    }
}

TargetProfileSpec targetProfileSpec(int profileIndex,
                                    bool useFittedProfile) noexcept
{
    const int clamped = juce::jlimit(1, 5, profileIndex);
    return useFittedProfile
        ? fittedProfileSpecV1(clamped)
        : legacyProfileSpec(clamped);
}

int effectiveTargetProfile(int selectedProfile, int presetIndex) noexcept
{
    if (selectedProfile > 0)
        return juce::jlimit(1, 5, selectedProfile);
    return juce::jlimit(1, 5, presetIndex + 1);
}

struct TargetMatchResult
{
    float score;
    int state; // 0 underdriven, 1 in range, 2 overdriven
};

TargetMatchResult targetMatchFor(float inputRmsDb,
                                 float drive,
                                 float outputPeakDb,
                                 float sagPercent,
                                 float correlation,
                                 int profileIndex,
                                 bool useFittedProfile) noexcept
{
    constexpr float sweetLow = -21.0f;
    constexpr float sweetHigh = -15.0f;
    const auto spec = targetProfileSpec(profileIndex, useFittedProfile);

    float score = 100.0f;
    int state = 1;
    if (inputRmsDb < sweetLow)
    {
        score -= (sweetLow - inputRmsDb) * 8.0f;
        state = 0;
    }
    else if (inputRmsDb > sweetHigh)
    {
        score -= (inputRmsDb - sweetHigh) * 8.0f;
        state = 2;
    }

    if (drive < spec.driveMin)
    {
        score -= (spec.driveMin - drive) * 28.0f;
        state = 0;
    }
    else if (drive > spec.driveMax)
    {
        score -= (drive - spec.driveMax) * 28.0f;
        state = 2;
    }

    if (outputPeakDb > -0.5f)
    {
        score -= (outputPeakDb + 0.5f) * 12.0f;
        state = 2;
    }
    else if (outputPeakDb < -36.0f)
    {
        score -= (-36.0f - outputPeakDb) * 1.8f;
        if (state == 1) state = 0;
    }

    const float sagTarget = (profileIndex == 4 || profileIndex == 5) ? 0.8f : 2.0f;
    if (sagPercent > sagTarget + 5.0f)
    {
        score -= (sagPercent - sagTarget - 5.0f) * 2.5f;
        state = 2;
    }

    if (correlation < -0.2f)
        score -= (-0.2f - correlation) * 20.0f;

    return { juce::jlimit(0.0f, 100.0f, score), state };
}

int targetReasonCodeFor(float inputRmsDb,
                        float drive,
                        float outputPeakDb,
                        float sagPercent,
                        int profileIndex,
                        bool useFittedProfile) noexcept
{
    const auto spec = targetProfileSpec(profileIndex, useFittedProfile);
    if (inputRmsDb < -21.0f) return 1; // input low
    if (inputRmsDb > -15.0f) return 2; // input hot
    if (drive < spec.driveMin) return 3;
    if (drive > spec.driveMax) return 4;
    if (outputPeakDb > -0.5f) return 5;
    if (sagPercent > 7.0f) return 6;
    return 0;
}

// Output safety soft-clip used only when the true-peak limiter is not
// engaged (bypass / null-test / TP Off-or-Soft).  Identity below ±T,
// smooth compression above it asymptoting to ±K, C1-continuous at the
// knee (value and unit slope match), so it is bit-transparent for any
// musical level yet bounds a pathological transient without a step.
inline float softSafetyClip(float x) noexcept
{
    // Linear (bit-identity) below ±T = ±1.0 (full scale) and a smooth,
    // C1-continuous compression above it toward the ±K (+6 dBFS)
    // asymptote.  This is only ever called inside the transition-gated
    // hold window in processBlock() — never in steady state — so the
    // tight knee bounds a transition transient (limiter ring-drain, A/B
    // rebuild spike) hard enough to keep the step small, while leaving
    // the steady limiter-off reference path completely untouched (the
    // gate keeps it off there) and a true-bypass dry term (≤ full scale)
    // bit-exact (identity below the knee).
    constexpr float T = 1.0f;
    constexpr float K = 2.0f;
    const float a = std::abs(x);
    if (a <= T) return x;
    const float over = (a - T) / (K - T);
    return std::copysign(T + (K - T) * std::tanh(over), x);
}

dsp::TransformerStageConfig transformerConfigForChoice(int choice)
{
    switch (choice)
    {
        case 2: return dsp::transformer_presets::Marinair();
        case 3: return dsp::transformer_presets::UTC_A12();
        case 4: return dsp::transformer_presets::JensenJT11();
        case 5: return dsp::transformer_presets::Lundahl();
        case 0:
        case 1:
        default: return dsp::transformer_presets::Marinair();
    }
}

// Swap a chain transformer's IRON (core material, bandwidth, presence
// peak) while keeping the CHAIN's calibration: drive/H_scale/outputGain
// set the core excursion and insertion trim for the node level at this
// position in this preset, and H_dc carries the standing magnetization
// of the surrounding circuit (SE idle current, PP imbalance).  Wholesale
// replacement with the stock preset (drive = 1.0, outputGain = 1.0,
// H_dc = 0) was measured to jump the HiFi output by +13 dB and push
// cores ~8x past their calibrated excursion.
dsp::TransformerStageConfig transformerOverrideKeepingCalibration(
    int choice, const dsp::TransformerStageConfig& calibrated)
{
    auto cfg = transformerConfigForChoice(choice);
    cfg.drive      = calibrated.drive;
    cfg.H_scale    = calibrated.H_scale;
    cfg.outputGain = calibrated.outputGain;
    cfg.H_dc       = calibrated.H_dc;
    return cfg;
}

// ─────────────────────────────────────────────────────────────────────────────
// Per-stage editor helpers (docs/26 §6)
//
// Tube model selector → DempwolfParams (use kRSD_1 / kRSD_2 / kEHX_1 /
// kECC82_Koren).  Index 0 = "Preset" → return nullopt to leave the preset
// stage's tube alone.  We don't surface a "12AU7-as-12AX7" warning UI yet;
// the four presets are well-documented in the params namespace.
// ─────────────────────────────────────────────────────────────────────────────
struct TubeChoiceLookup
{
    bool override_;
    dsp::DempwolfParams params;
};

TubeChoiceLookup tubeChoiceLookup(int choice)
{
    using namespace valvra::dsp;
    switch (choice)
    {
        case 1: return { true, params::kRSD_1 };             // 12AX7 RSD-1 (canonical)
        case 2: return { true, params::kRSD_2 };             // 12AX7 RSD-2 (sibling)
        case 3: return { true, params::kEHX_1 };             // 12AX7 EHX  (warm)
        case 4: return { true, params::kECC82_Koren };       // 12AU7      (low μ)
        case 5: return { true, params::k6SN7 };              // 6SN7       (HiFi smooth)
        case 6: return { true, params::k300B };              // 300B       (HiFi power triode)
        case 7: return { true, params::kEF86_TriodeStrapped };// EF86 (triode-strap, Vox)
        case 8: return { true, params::kEL34_TriodeStrapped };// EL34       (British power tube)
        case 9: return { true, params::k6L6GC_TriodeStrapped };// 6L6GC     (American power tube)
        case 0:
        default: return { false, params::kRSD_1 };
    }
}

// Topology selector.  Five user-visible implementations are exposed:
// CommonCathode, CathodeFollower, SRPP, LongTailedPair, and Cascode.  SRPP
// and Cascode solve a junction-
// voltage Newton-Raphson per sample (TubeStage::solveStackPair) so the
// "two stacked tubes share Ip" physics is honoured rather than
// approximated.  LongTailedPair uses TubeStage's phase-inverter operating
// point and is also present in the dedicated push-pull output stage.
struct TopologyChoiceLookup
{
    bool override_;
    dsp::TubeTopology topology;
};

TopologyChoiceLookup topologyChoiceLookup(int choice)
{
    using namespace valvra::dsp;
    switch (choice)
    {
        case 1: return { true,  TubeTopology::CommonCathode };
        case 2: return { true,  TubeTopology::CathodeFollower };
        case 3: return { true,  TubeTopology::SRPP };
        case 4: return { true,  TubeTopology::LongTailedPair };
        case 5: return { true,  TubeTopology::Cascode };
        case 0:
        default: return { false, TubeTopology::CommonCathode };
    }
}

bool treeContainsParamId(const juce::ValueTree& tree, const juce::String& paramId)
{
    static const juce::Identifier kIdProp { "id" };
    if (tree.hasProperty(kIdProp)
        && tree.getProperty(kIdProp).toString() == paramId)
    {
        return true;
    }

    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        if (treeContainsParamId(tree.getChild(i), paramId))
            return true;
    }
    return false;
}

constexpr const char* kABStateNodeType = "ab_state";
constexpr const char* kABSlotNodeType  = "ab_slot";
constexpr const char* kABSlotNameA = "A";
constexpr const char* kABSlotNameB = "B";
constexpr const char* kABSlotNameC = "C";
constexpr const char* kABSlotNameD = "D";
constexpr const char* kABSlotNameE = "E";

void stripABStateNode(juce::ValueTree& tree)
{
    const juce::Identifier abStateType { kABStateNodeType };
    for (int i = tree.getNumChildren(); --i >= 0; )
    {
        if (tree.getChild(i).hasType(abStateType))
            tree.removeChild(i, nullptr);
    }
}

} // namespace

void ValvraProcessor::IntegratedLufsMeter::prepare(double sampleRate) noexcept
{
    const double sr = std::max(sampleRate, 1000.0);

    auto setupHighShelf = [sr](Biquad& b, double freq, double gainDb)
    {
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * juce::MathConstants<double>::pi * freq / sr;
        const double cs = std::cos(w0);
        const double sn = std::sin(w0);
        const double alpha = sn / 2.0 * std::sqrt(2.0);
        const double beta = 2.0 * std::sqrt(A) * alpha;

        const double a0 = (A + 1.0) - (A - 1.0) * cs + beta;
        b.b0 =  A * ((A + 1.0) + (A - 1.0) * cs + beta) / a0;
        b.b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cs) / a0;
        b.b2 =  A * ((A + 1.0) + (A - 1.0) * cs - beta) / a0;
        b.a1 =  2.0 * ((A - 1.0) - (A + 1.0) * cs) / a0;
        b.a2 = ((A + 1.0) - (A - 1.0) * cs - beta) / a0;
        b.reset();
    };

    auto setupHighpass = [sr](Biquad& b, double freq, double q)
    {
        const double w0 = 2.0 * juce::MathConstants<double>::pi * freq / sr;
        const double cs = std::cos(w0);
        const double sn = std::sin(w0);
        const double alpha = sn / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b.b0 =  (1.0 + cs) * 0.5 / a0;
        b.b1 = -(1.0 + cs) / a0;
        b.b2 =  (1.0 + cs) * 0.5 / a0;
        b.a1 = -2.0 * cs / a0;
        b.a2 =  (1.0 - alpha) / a0;
        b.reset();
    };

    for (int ch = 0; ch < 2; ++ch)
    {
        setupHighShelf(shelf[static_cast<std::size_t>(ch)], 1681.974450955533, 4.0);
        setupHighpass(highpass[static_cast<std::size_t>(ch)], 38.13547087602444, 0.5);
    }
    energy = 0.0;
    samples = 0;
}

void ValvraProcessor::IntegratedLufsMeter::reset() noexcept
{
    for (auto& f : shelf) f.reset();
    for (auto& f : highpass) f.reset();
    energy = 0.0;
    samples = 0;
}

void ValvraProcessor::IntegratedLufsMeter::process(float left,
                                                   float right,
                                                   bool stereo) noexcept
{
    auto processChannel = [this](float x, int ch)
    {
        const auto idx = static_cast<std::size_t>(ch);
        const double k = highpass[idx].process(shelf[idx].process(
            std::isfinite(x) ? static_cast<double>(x) : 0.0));
        return k * k;
    };

    double e = processChannel(left, 0);
    if (stereo)
        e += processChannel(right, 1);
    energy += e;
    ++samples;
}

float ValvraProcessor::IntegratedLufsMeter::lufs(float fallbackDb) const noexcept
{
    if (samples < 1024 || energy <= 0.0)
        return fallbackDb;
    const double meanSquare = energy / static_cast<double>(samples);
    return static_cast<float>(
        -0.691 + 10.0 * std::log10(std::max(meanSquare, 1.0e-18)));
}

void ValvraProcessor::MasterLoudnessMeter::prepare(double sr)
{
    sampleRate = (sr > 1000.0) ? sr : 48000.0;

    auto setupHighShelf = [](IntegratedLufsMeter::Biquad& b, double srIn,
                             double freq, double gainDb)
    {
        const double A  = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * juce::MathConstants<double>::pi * freq / srIn;
        const double cs = std::cos(w0);
        const double sn = std::sin(w0);
        const double alpha = sn / 2.0 * std::sqrt(2.0);
        const double beta = 2.0 * std::sqrt(A) * alpha;

        const double a0 = (A + 1.0) - (A - 1.0) * cs + beta;
        b.b0 =  A * ((A + 1.0) + (A - 1.0) * cs + beta) / a0;
        b.b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cs) / a0;
        b.b2 =  A * ((A + 1.0) + (A - 1.0) * cs - beta) / a0;
        b.a1 =  2.0 * ((A - 1.0) - (A + 1.0) * cs) / a0;
        b.a2 = ((A + 1.0) - (A - 1.0) * cs - beta) / a0;
        b.reset();
    };

    auto setupHighpass = [](IntegratedLufsMeter::Biquad& b, double srIn,
                            double freq, double q)
    {
        const double w0 = 2.0 * juce::MathConstants<double>::pi * freq / srIn;
        const double cs = std::cos(w0);
        const double sn = std::sin(w0);
        const double alpha = sn / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b.b0 =  (1.0 + cs) * 0.5 / a0;
        b.b1 = -(1.0 + cs) / a0;
        b.b2 =  (1.0 + cs) * 0.5 / a0;
        b.a1 = -2.0 * cs / a0;
        b.a2 =  (1.0 - alpha) / a0;
        b.reset();
    };

    for (int ch = 0; ch < 2; ++ch)
    {
        setupHighShelf(shelf[static_cast<std::size_t>(ch)], sampleRate,
                       1681.974450955533, 4.0);
        setupHighpass(highpass[static_cast<std::size_t>(ch)], sampleRate,
                      38.13547087602444, 0.5);
    }

    const auto momentarySamples =
        static_cast<std::size_t>(std::max(1.0, std::round(sampleRate * 0.400)));
    const auto shortTermSamples =
        static_cast<std::size_t>(std::max(1.0, std::round(sampleRate * 3.000)));
    momentaryRing.assign(momentarySamples, 0.0);
    shortTermRing.assign(shortTermSamples, 0.0);
    gatedBlockEnergies.assign(static_cast<std::size_t>(60.0 * 30.0 / 0.100),
                              0.0);
    blockSamples = static_cast<int>(std::max(1.0, std::round(sampleRate * 0.100)));
    reset();
}

void ValvraProcessor::MasterLoudnessMeter::reset() noexcept
{
    for (auto& f : shelf) f.reset();
    for (auto& f : highpass) f.reset();
    std::fill(momentaryRing.begin(), momentaryRing.end(), 0.0);
    std::fill(shortTermRing.begin(), shortTermRing.end(), 0.0);
    std::fill(gatedBlockEnergies.begin(), gatedBlockEnergies.end(), 0.0);
    momentarySum = 0.0;
    shortTermSum = 0.0;
    blockEnergy = 0.0;
    lastIntegrated = -100.0;
    momentaryWrite = 0;
    shortTermWrite = 0;
    gatedWrite = 0;
    gatedCount = 0;
    momentaryCount = 0;
    shortTermCount = 0;
    samplesInBlock = 0;
}

void ValvraProcessor::MasterLoudnessMeter::process(float left,
                                                   float right,
                                                   bool stereo) noexcept
{
    auto processChannel = [this](float x, int ch)
    {
        const auto idx = static_cast<std::size_t>(ch);
        const double k = highpass[idx].process(shelf[idx].process(
            std::isfinite(x) ? static_cast<double>(x) : 0.0));
        return k * k;
    };

    double e = processChannel(left, 0);
    if (stereo)
        e += processChannel(right, 1);

    if (! momentaryRing.empty())
    {
        momentarySum -= momentaryRing[momentaryWrite];
        momentaryRing[momentaryWrite] = e;
        momentarySum += e;
        momentaryWrite = (momentaryWrite + 1) % momentaryRing.size();
        if (momentaryCount < momentaryRing.size()) ++momentaryCount;
    }

    if (! shortTermRing.empty())
    {
        shortTermSum -= shortTermRing[shortTermWrite];
        shortTermRing[shortTermWrite] = e;
        shortTermSum += e;
        shortTermWrite = (shortTermWrite + 1) % shortTermRing.size();
        if (shortTermCount < shortTermRing.size()) ++shortTermCount;
    }

    blockEnergy += e;
    ++samplesInBlock;
    if (samplesInBlock < blockSamples || blockSamples <= 0)
        return;

    if (momentaryCount >= momentaryRing.size() && ! gatedBlockEnergies.empty())
    {
        const double mean = momentarySum / static_cast<double>(momentaryCount);
        const double lufs = -0.691 + 10.0 * std::log10(std::max(mean, 1.0e-18));
        if (lufs > -70.0)
        {
            gatedBlockEnergies[gatedWrite] = mean;
            gatedWrite = (gatedWrite + 1) % gatedBlockEnergies.size();
            if (gatedCount < gatedBlockEnergies.size()) ++gatedCount;

            double absSum = 0.0;
            for (std::size_t i = 0; i < gatedCount; ++i)
                absSum += gatedBlockEnergies[i];
            const double absMean = absSum / static_cast<double>(std::max<std::size_t>(1, gatedCount));
            const double relGate =
                -0.691 + 10.0 * std::log10(std::max(absMean, 1.0e-18)) - 10.0;

            double gatedSum = 0.0;
            std::size_t gatedN = 0;
            for (std::size_t i = 0; i < gatedCount; ++i)
            {
                const double ge = gatedBlockEnergies[i];
                const double gl = -0.691 + 10.0 * std::log10(std::max(ge, 1.0e-18));
                if (gl >= relGate)
                {
                    gatedSum += ge;
                    ++gatedN;
                }
            }
            if (gatedN > 0)
            {
                const double gatedMean = gatedSum / static_cast<double>(gatedN);
                lastIntegrated =
                    -0.691 + 10.0 * std::log10(std::max(gatedMean, 1.0e-18));
            }
        }
    }

    blockEnergy = 0.0;
    samplesInBlock = 0;
}

float ValvraProcessor::MasterLoudnessMeter::momentaryLufs() const noexcept
{
    if (momentaryCount < momentaryRing.size() / 2 || momentarySum <= 0.0)
        return -100.0f;
    const double mean = momentarySum / static_cast<double>(momentaryCount);
    return static_cast<float>(
        -0.691 + 10.0 * std::log10(std::max(mean, 1.0e-18)));
}

float ValvraProcessor::MasterLoudnessMeter::shortTermLufs() const noexcept
{
    if (shortTermCount < shortTermRing.size() / 2 || shortTermSum <= 0.0)
        return -100.0f;
    const double mean = shortTermSum / static_cast<double>(shortTermCount);
    return static_cast<float>(
        -0.691 + 10.0 * std::log10(std::max(mean, 1.0e-18)));
}

float ValvraProcessor::MasterLoudnessMeter::integratedLufs() const noexcept
{
    return static_cast<float>(lastIntegrated);
}

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
    // First entry of the Reroll Timeline is the seed the plugin opened with.
    pushSeedHistory(initialSeed);

    // Wire chain-shape parameters to rebuild on change
    params_.addParameterListener(kParamPreset, this);
    params_.addParameterListener(kParamStageCount, this);
    params_.addParameterListener(kParamInputTrafo, this);
    params_.addParameterListener(kParamOutputTrafo, this);
    params_.addParameterListener(kParamMcDistribution, this);
    params_.addParameterListener(kParamCvMode, this);
    params_.addParameterListener(kParamMains, this);
    params_.addParameterListener(kParamCvBias, this);
    // M/S mode swap requires both chains to be re-seeded (shared seed in
    // Mid/Side mode vs XOR-salted in Stereo mode).
    params_.addParameterListener(kParamMSMode, this);

    // Each per-stage editor control also triggers a chain rebuild.
    for (const auto& s : kStageParams)
    {
        params_.addParameterListener(s.tube,     this);
        params_.addParameterListener(s.topology, this);
        params_.addParameterListener(s.drive,    this);
        params_.addParameterListener(s.bias,     this);
    }

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
    params_.removeParameterListener(kParamStageCount, this);
    params_.removeParameterListener(kParamInputTrafo, this);
    params_.removeParameterListener(kParamOutputTrafo, this);
    params_.removeParameterListener(kParamMcDistribution, this);
    params_.removeParameterListener(kParamCvMode, this);
    params_.removeParameterListener(kParamMains, this);
    params_.removeParameterListener(kParamCvBias, this);
    params_.removeParameterListener(kParamMSMode, this);

    for (const auto& s : kStageParams)
    {
        params_.removeParameterListener(s.tube,     this);
        params_.removeParameterListener(s.topology, this);
        params_.removeParameterListener(s.drive,    this);
        params_.removeParameterListener(s.bias,     this);
    }

    if (auto* req = neuralSwapRequest_.exchange(nullptr, std::memory_order_acq_rel))
        delete req;
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
    if (paramID == kParamPreset
        || paramID == kParamStageCount
        || paramID == kParamInputTrafo
        || paramID == kParamOutputTrafo
        || paramID == kParamMcDistribution
        || paramID == kParamCvMode
        || paramID == kParamMains
        || paramID == kParamCvBias
        || paramID == kParamMSMode
        || paramID == kParamTargetProfile
        || paramID == kParamRealismAmount)
    {
        rebuildRequested_.store(true, std::memory_order_relaxed);
        return;
    }

    // Any per-stage editor change also rebuilds the chain.  This is a coarse
    // hammer (the alternative would be per-stage hot-update à la setVariationSeed),
    // but rebuild is fast enough at audio block boundaries that it's the
    // safer first iteration — and the user-facing changes are typically
    // discrete topology / tube swaps, not continuous automation.
    for (const auto& s : kStageParams)
    {
        if (paramID == s.tube || paramID == s.topology
            || paramID == s.drive || paramID == s.bias)
        {
            rebuildRequested_.store(true, std::memory_order_relaxed);
            return;
        }
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout
ValvraProcessor::createLayout()
{
    using namespace juce;
    const auto unitToPercentText = [](float value, int) -> juce::String
    {
        return juce::String(static_cast<int>(std::round(value * 100.0f)));
    };
    const auto percentTextToUnit = [](const juce::String& text) -> float
    {
        auto t = text.trim();
        t = t.upToFirstOccurrenceOf("%", false, false).trim();
        return juce::jlimit(0.0f, 1.0f, t.getFloatValue() / 100.0f);
    };

    return {
        // Preset selector — 4 signature modes
        std::make_unique<AudioParameterChoice>(
            ParameterID { kParamPreset, 1 },
            "Mode",
            StringArray { "V72 Preamp", "Console Output", "Culture Vulture", "RNDI DI", "HiFi 300B" },
            0),

        // Drive — input gain staging into the tube stages (0–100%)
        std::make_unique<AudioParameterFloat>(
            ParameterID { kParamDrive, 1 },
            "Drive",
            NormalisableRange<float> { 0.0f, 3.0f, 0.0f, 0.5f },  // log-like
            1.0f),

        std::make_unique<AudioParameterFloat>(
            ParameterID { kParamInputTrimDb, 1 },
            "Input Trim",
            NormalisableRange<float> { -24.0f, 24.0f, 0.1f },
            0.0f),

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

        std::make_unique<AudioParameterChoice>(
            ParameterID { kParamLevelMatchMode, 1 },
            "Level Match",
            StringArray { "Off", "Mode Trim", "Analyze Match" },
            1),
        std::make_unique<AudioParameterFloat>(
            ParameterID { kParamAnalyzedOutputTrimDb, 1 },
            "Analyzed Output Trim",
            NormalisableRange<float> { -24.0f, 24.0f, 0.1f },
            0.0f),
        std::make_unique<AudioParameterChoice>(
            ParameterID { kParamTargetProfile, 1 },
            "Target Profile",
            StringArray { "Auto", "V72", "Console Output", "Culture Vulture", "RNDI", "HiFi 300B" },
            0),
        std::make_unique<AudioParameterFloat>(
            ParameterID { kParamRealismAmount, 1 },
            "Realism",
            NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
            0.35f,
            "%",
            AudioProcessorParameter::genericParameter,
            unitToPercentText,
            percentTextToUnit),

        // Oversampling — 1×/2×/4×/8×/16×
        std::make_unique<AudioParameterChoice>(
            ParameterID { kParamOversample, 1 },
            "Quality",
            StringArray { "Low (1x)", "Medium (2x)", "High (4x)", "Ultra (8x)", "Insane (16x)" },
            2),

        std::make_unique<AudioParameterBool>(
            ParameterID { kParamMcLock, 1 },
            "Monte Carlo Lock",
            false),
        std::make_unique<AudioParameterChoice>(
            ParameterID { kParamMcDistribution, 1 },
            "Monte Carlo Distribution",
            StringArray { "Modern", "Vintage", "Warm", "Wild" },
            0),
        std::make_unique<AudioParameterChoice>(
            ParameterID { kParamCvMode, 1 },
            "Culture Vulture Mode",
            StringArray { "T", "P1", "P2" },
            1),
        // One switch drives every line-frequency mechanism coherently —
        // heater hum, PSU ripple (2× line) and the processor's leakage
        // hum all read the same chain field (docs/34 §1.5, docs/35 C1).
        std::make_unique<AudioParameterChoice>(
            ParameterID { kParamMains, 1 },
            "Mains Frequency",
            StringArray { "60 Hz (US/JP)", "50 Hz (EU)" },
            0),
        // CV-style BIAS: DC shift on the free suppressor grids [V].
        // 0 = documented operating point; negative chokes the plate
        // stream toward gnarl, positive opens it up (docs/35 C10).
        std::make_unique<AudioParameterFloat>(
            ParameterID { kParamCvBias, 1 },
            "Bias (g3)",
            NormalisableRange<float> { -2.0f, 1.5f, 0.01f },
            0.0f),
        std::make_unique<AudioParameterFloat>(
            ParameterID { kParamNeuralBlend, 1 },
            "Neural Blend",
            NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
            0.0f,
            "%",
            AudioProcessorParameter::genericParameter,
            unitToPercentText,
            percentTextToUnit),
        std::make_unique<AudioParameterChoice>(
            ParameterID { kParamExpansionMode, 1 },
            "Analog Engine",
            StringArray { "Off", "Opto Glue", "FET Punch", "Tape Print", "Synth FX" },
            kDefaultExpansionMode),
        std::make_unique<AudioParameterFloat>(
            ParameterID { kParamExpansionAmount, 1 },
            "Expansion Amount",
            NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
            kDefaultExpansionAmount,
            "%",
            AudioProcessorParameter::genericParameter,
            unitToPercentText,
            percentTextToUnit),
        std::make_unique<AudioParameterFloat>(
            ParameterID { kParamExpansionMix, 1 },
            "Expansion Mix",
            NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
            kDefaultExpansionMix,
            "%",
            AudioProcessorParameter::genericParameter,
            unitToPercentText,
            percentTextToUnit),

        // Chain Builder minimum editable controls. Stage count truncates
        // or extends the selected preset chain; transformer choices override
        // the preset's input/output transformer selection.
        std::make_unique<AudioParameterChoice>(
            ParameterID { kParamStageCount, 1 },
            "Stage Count",
            StringArray { "Preset", "1 Stage", "2 Stages", "3 Stages", "4 Stages" },
            0),

        std::make_unique<AudioParameterChoice>(
            ParameterID { kParamInputTrafo, 1 },
            "Input Transformer",
            StringArray { "Preset", "Off", "Marinair", "UTC A-12", "Jensen", "Lundahl" },
            0),

        std::make_unique<AudioParameterChoice>(
            ParameterID { kParamOutputTrafo, 1 },
            "Output Transformer",
            StringArray { "Preset", "Off", "Marinair", "UTC A-12", "Jensen", "Lundahl" },
            0),

        // Host-visible bypass.  Declared last so existing preset XML (which
        // does not know about this param) still round-trips cleanly.
        std::make_unique<AudioParameterBool>(
            ParameterID { kParamBypass, 1 },
            "Bypass",
            false),

        // ── Per-stage editor (docs/26 §6 minimum acceptance) ──────────────
        // Each stage gets four discrete controls: tube, topology, drive
        // trim (dB), bias offset (V).  All four default to "Preset" / 0,
        // meaning the chain runs untouched until the user dials a control.
        // We declare them at the end so older preset XML (which lacks them)
        // round-trips cleanly through setStateInformation.
        std::make_unique<AudioParameterChoice>(
            ParameterID { kStageParams[0].tube, 1 }, "Stage 1 Tube",
            StringArray { "Preset", "12AX7 RSD-1", "12AX7 RSD-2", "12AX7 EHX", "12AU7", "6SN7", "300B", "EF86 (triode)", "EL34", "6L6GC" }, 0),
        std::make_unique<AudioParameterChoice>(
            ParameterID { kStageParams[0].topology, 1 }, "Stage 1 Topology",
            StringArray { "Preset", "Common Cathode", "Cathode Follower", "SRPP", "Long-Tailed Pair", "Cascode" }, 0),
        std::make_unique<AudioParameterFloat>(
            ParameterID { kStageParams[0].drive, 1 }, "Stage 1 Drive",
            NormalisableRange<float> { -12.0f, 12.0f, 0.1f }, 0.0f),
        std::make_unique<AudioParameterFloat>(
            ParameterID { kStageParams[0].bias, 1 }, "Stage 1 Bias",
            NormalisableRange<float> { -0.8f, 0.8f, 0.005f }, 0.0f),

        std::make_unique<AudioParameterChoice>(
            ParameterID { kStageParams[1].tube, 1 }, "Stage 2 Tube",
            StringArray { "Preset", "12AX7 RSD-1", "12AX7 RSD-2", "12AX7 EHX", "12AU7", "6SN7", "300B", "EF86 (triode)", "EL34", "6L6GC" }, 0),
        std::make_unique<AudioParameterChoice>(
            ParameterID { kStageParams[1].topology, 1 }, "Stage 2 Topology",
            StringArray { "Preset", "Common Cathode", "Cathode Follower", "SRPP", "Long-Tailed Pair", "Cascode" }, 0),
        std::make_unique<AudioParameterFloat>(
            ParameterID { kStageParams[1].drive, 1 }, "Stage 2 Drive",
            NormalisableRange<float> { -12.0f, 12.0f, 0.1f }, 0.0f),
        std::make_unique<AudioParameterFloat>(
            ParameterID { kStageParams[1].bias, 1 }, "Stage 2 Bias",
            NormalisableRange<float> { -0.8f, 0.8f, 0.005f }, 0.0f),

        std::make_unique<AudioParameterChoice>(
            ParameterID { kStageParams[2].tube, 1 }, "Stage 3 Tube",
            StringArray { "Preset", "12AX7 RSD-1", "12AX7 RSD-2", "12AX7 EHX", "12AU7", "6SN7", "300B", "EF86 (triode)", "EL34", "6L6GC" }, 0),
        std::make_unique<AudioParameterChoice>(
            ParameterID { kStageParams[2].topology, 1 }, "Stage 3 Topology",
            StringArray { "Preset", "Common Cathode", "Cathode Follower", "SRPP", "Long-Tailed Pair", "Cascode" }, 0),
        std::make_unique<AudioParameterFloat>(
            ParameterID { kStageParams[2].drive, 1 }, "Stage 3 Drive",
            NormalisableRange<float> { -12.0f, 12.0f, 0.1f }, 0.0f),
        std::make_unique<AudioParameterFloat>(
            ParameterID { kStageParams[2].bias, 1 }, "Stage 3 Bias",
            NormalisableRange<float> { -0.8f, 0.8f, 0.005f }, 0.0f),

        std::make_unique<AudioParameterChoice>(
            ParameterID { kStageParams[3].tube, 1 }, "Stage 4 Tube",
            StringArray { "Preset", "12AX7 RSD-1", "12AX7 RSD-2", "12AX7 EHX", "12AU7", "6SN7", "300B", "EF86 (triode)", "EL34", "6L6GC" }, 0),
        std::make_unique<AudioParameterChoice>(
            ParameterID { kStageParams[3].topology, 1 }, "Stage 4 Topology",
            StringArray { "Preset", "Common Cathode", "Cathode Follower", "SRPP", "Long-Tailed Pair", "Cascode" }, 0),
        std::make_unique<AudioParameterFloat>(
            ParameterID { kStageParams[3].drive, 1 }, "Stage 4 Drive",
            NormalisableRange<float> { -12.0f, 12.0f, 0.1f }, 0.0f),
        std::make_unique<AudioParameterFloat>(
            ParameterID { kStageParams[3].bias, 1 }, "Stage 4 Bias",
            NormalisableRange<float> { -0.8f, 0.8f, 0.005f }, 0.0f),

        // ── Mastering section (docs/20 §4.8) ──────────────────────────────
        // True Peak brickwall limiter.  Off by default to keep the plugin's
        // 1:1 character behaviour for users who only want colour; flip on
        // when this is the last plugin in the chain to guarantee no DAC
        // overshoot on the final master.
        std::make_unique<AudioParameterBool>(
            ParameterID { kParamTpEnabled, 1 }, "True Peak Limiter", false),
        std::make_unique<AudioParameterChoice>(
            ParameterID { kParamTpMode, 1 }, "TP Safety Mode",
            StringArray { "Off", "Soft", "Brick-wall" }, 0),
        std::make_unique<AudioParameterFloat>(
            ParameterID { kParamTpCeilingDb, 1 }, "TP Ceiling",
            NormalisableRange<float> { -3.0f, -0.1f, 0.1f }, -1.0f),
        std::make_unique<AudioParameterFloat>(
            ParameterID { kParamTpLookaheadMs, 1 }, "TP Lookahead",
            NormalisableRange<float> { 1.0f, 10.0f, 0.1f }, 1.3f),

        // TPDF dither at the very output.  Quantization-aware noise shaping
        // is a v2 feature; flat triangular dither is enough for transparent
        // bit-depth reduction at the host's export stage.
        std::make_unique<AudioParameterBool>(
            ParameterID { kParamDitherEnable, 1 }, "TPDF Dither", false),
        std::make_unique<AudioParameterChoice>(
            ParameterID { kParamDitherDepth, 1 }, "Dither Depth",
            StringArray { "16-bit", "20-bit", "24-bit" }, 2),

        // Mid/Side processing.  When on, the L+R input is decoded into
        // Mid (centre/sum) and Side (difference/stereo width) signals,
        // each runs through one of the two chains independently, then
        // re-encoded back to L/R.  Because the chain is non-linear, the
        // centre energy gets shaped differently from the sides — useful
        // on master buses where you want tube colour on the centre image
        // but a cleaner stereo wash on the sides.  Mono compatibility:
        // a mono down-mix of the M/S-processed output equals the M chain
        // output alone (the S chain content cancels in the sum).  We
        // give both chains the SAME seed in M/S mode so the centre and
        // sides have identical character (the standard mastering
        // convention), versus the per-chain XOR salt used in plain stereo.
        std::make_unique<AudioParameterChoice>(
            ParameterID { kParamMSMode, 1 }, "Stereo Routing",
            StringArray { "Stereo", "Mid/Side" }, 0)
    };
}

// ─────────────────────────────────────────────────────────────────────────────
// Prepare / rebuild
// ─────────────────────────────────────────────────────────────────────────────
void ValvraProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    sampleRate_         = sampleRate;
    // Keep this zero until prepare completes so env-driven neural model load
    // applies synchronously (not queued to audio-thread fade logic).
    preparedBlockSize_  = 0;
    neuralSwapRequested_.store(false, std::memory_order_relaxed);
    if (auto* staleReq = neuralSwapRequest_.exchange(nullptr, std::memory_order_acq_rel))
        delete staleReq;
    neuralSwapPhase_ = NeuralSwapPhase::Idle;
    neuralSwapPos_ = 0;
    neuralSwapPendingApply_ = false;
    lastOsFactor_       = currentOversampleFactor();
    pendingOsFactor_    = 0;
    activeMsMode_ =
        (static_cast<int>(*params_.getRawParameterValue(kParamMSMode)) == 1);
    pendingMsMode_ = activeMsMode_;
    pendingMsModeValid_ = false;
    rebuildChain();
    os2L_.reset(); os2R_.reset();
    os4L_.reset(); os4R_.reset();
    os8L_.reset(); os8R_.reset();
    os16L_.reset(); os16R_.reset();
    analyzer_.prepare(sampleRate);
    neuralL_.prepare(sampleRate);
    neuralR_.prepare(sampleRate);
    expansionRack_.prepare(sampleRate);
    neuralModelLoadedState_.store(false, std::memory_order_relaxed);
    if (const char* modelPath = std::getenv("VALVRA_NEURAL_MODEL"))
    {
        if (modelPath[0] != '\0')
            loadNeuralModelFile(juce::String(modelPath));
    }
    slotA_.lufsMeter.prepare(sampleRate);
    slotB_.lufsMeter.prepare(sampleRate);
    slotC_.lufsMeter.prepare(sampleRate);
    slotD_.lufsMeter.prepare(sampleRate);
    slotE_.lufsMeter.prepare(sampleRate);
    masterLoudnessMeter_.prepare(sampleRate);

    // True Peak limiter prepares its own internal 4× detection oversampler.
    tpLimiter_.prepare(sampleRate);
    masterTpMeterL_.reset();
    masterTpMeterR_.reset();
    tpLimiter_.setCeilingDb(*params_.getRawParameterValue(kParamTpCeilingDb));
    tpLimiter_.setLookaheadMs(*params_.getRawParameterValue(kParamTpLookaheadMs));
    lastTpLatency_ = tpLimiter_.currentLatencyInSamples();
    const int tpModeAtPrepare = static_cast<int>(
        *params_.getRawParameterValue(kParamTpMode));
    tpLimiter_.setBypass(tpModeAtPrepare != 2);

    // Seed the safety-clip gate to the initial limiter-bypass state so the
    // first processed block is NOT seen as a transition (which would arm
    // the clip and perturb a steady limiter-off reference render).
    lastLimiterBypassed_ =
        (tpModeAtPrepare != 2)
        || nullTestMode_.load(std::memory_order_relaxed)
        || (bypassParam_ != nullptr && bypassParam_->get());
    safetyHoldSamples_ = 0;

    // Seed dither RNG once at prepare so independent instances do not emit
    // identical noise streams (would otherwise sum coherently in a mix bus).
    ditherRng_ ^=
        static_cast<std::uint64_t>(juce::Time::getHighResolutionTicks())
        * 0x9E3779B97F4A7C15ULL;
    if (ditherRng_ == 0) ditherRng_ = 0xDEADBEEFCAFEBABEULL;

    // Configure PDC (plugin delay compensation) + internal dry-path alignment.
    // Two distinct numbers:
    //   • setLatencySamples reports the TOTAL plugin delay to the host
    //     (OS lookahead + TP limiter lookahead).  Bus partners get realigned.
    //   • dryDelay only needs to absorb the chain's OS latency, because the
    //     TP limiter sits AFTER the wet/dry mix and adds its lookahead to
    //     both paths equally — once they're already combined.  Over-delaying
    //     the dry path here would break null-test cancellation.
    const int totalLat   = currentLatencyInSamples();
    const int dryAlignLat = totalLat - tpLimiter_.currentLatencyInSamples();
    setLatencySamples(totalLat);
    dryDelayL_.setLatency(dryAlignLat);
    dryDelayR_.setLatency(dryAlignLat);

    // Parameter smoothers: 20 ms ramp time for inaudible knob changes.
    constexpr double kSmoothingSec = 0.020;
    driveSmooth_  .reset(sampleRate, kSmoothingSec);
    inputTrimSmooth_.reset(sampleRate, kSmoothingSec);
    outGainSmooth_.reset(sampleRate, kSmoothingSec);
    mixSmooth_    .reset(sampleRate, kSmoothingSec);
    neuralBlendSmooth_.reset(sampleRate, kSmoothingSec);
    expansionAmountSmooth_.reset(sampleRate, kSmoothingSec);
    expansionMixSmooth_.reset(sampleRate, kSmoothingSec);
    bypassBlendSmooth_.reset(sampleRate, 0.005);
    nullTestBlendSmooth_.reset(sampleRate, 0.005);
    abMatchSmooth_.reset(sampleRate, 0.010);
    levelMatchSmooth_.reset(sampleRate, 0.020);
    realismSmooth_.reset(sampleRate, kSmoothingSec);
    graphFadeSamples_ = std::max(1, static_cast<int>(0.010 * sampleRate));
    neuralSwapSamples_ = std::max(1, static_cast<int>(0.010 * sampleRate));

    // Initialize to current raw parameter values so there is no startup ramp
    // from 0 → target on first playback.
    driveSmooth_  .setCurrentAndTargetValue(
        params_.getRawParameterValue(kParamDrive)->load());
    inputTrimSmooth_.setCurrentAndTargetValue(
        juce::Decibels::decibelsToGain(
            params_.getRawParameterValue(kParamInputTrimDb)->load()));
    outGainSmooth_.setCurrentAndTargetValue(
        juce::Decibels::decibelsToGain(
            params_.getRawParameterValue(kParamOutputDb)->load()));
    mixSmooth_    .setCurrentAndTargetValue(
        params_.getRawParameterValue(kParamMix)->load());
    neuralBlendSmooth_.setCurrentAndTargetValue(
        params_.getRawParameterValue(kParamNeuralBlend)->load());
    expansionAmountSmooth_.setCurrentAndTargetValue(
        params_.getRawParameterValue(kParamExpansionAmount)->load());
    expansionMixSmooth_.setCurrentAndTargetValue(
        params_.getRawParameterValue(kParamExpansionMix)->load());

    expansionRack_.setMode(static_cast<dsp::ExpansionMode>(
        static_cast<int>(*params_.getRawParameterValue(kParamExpansionMode))));
    expansionRack_.setAmount(
        params_.getRawParameterValue(kParamExpansionAmount)->load());
    expansionRack_.setMix(
        params_.getRawParameterValue(kParamExpansionMix)->load());
    bypassBlendSmooth_.setCurrentAndTargetValue(
        (bypassParam_ != nullptr && bypassParam_->get()) ? 1.0f : 0.0f);
    nullTestBlendSmooth_.setCurrentAndTargetValue(
        nullTestMode_.load(std::memory_order_relaxed) ? 1.0f : 0.0f);
    abMatchSmooth_.setCurrentAndTargetValue(1.0f);
    const float levelMatchDb = currentLevelMatchTrimDb();
    levelMatchAppliedDbState_.store(levelMatchDb, std::memory_order_relaxed);
    levelMatchSmooth_.setCurrentAndTargetValue(
        juce::Decibels::decibelsToGain(levelMatchDb));
    realismSmooth_.setCurrentAndTargetValue(
        mapRealismControl(params_.getRawParameterValue(kParamRealismAmount)->load()));
    const std::uint64_t seed = currentSeed_.load(std::memory_order_relaxed);
    analogLeakageRng_ = seed ^ 0x91E10DA5C79E7B1DULL;
    if (analogLeakageRng_ == 0) analogLeakageRng_ = 0xA5A55A5A12345678ULL;
    realismHumPhaseL_ = static_cast<double>(seed & 0xFFFFu)
                      / 65535.0 * juce::MathConstants<double>::twoPi;
    realismHumPhaseR_ = static_cast<double>((seed >> 16) & 0xFFFFu)
                      / 65535.0 * juce::MathConstants<double>::twoPi;
    realismNoiseEnergy_ = 0.0;
    realismNoiseCount_ = 0;
    const double srSafe = std::max(sampleRate, 1000.0);
    feelFastCoeff_ = 1.0 - std::exp(-1.0 / (0.012 * srSafe));
    feelSlowCoeff_ = 1.0 - std::exp(-1.0 / (0.350 * srSafe));
    feelMotionLpCoeff_ = 1.0 - std::exp(-1.0 / (0.060 * srSafe));
    feelFastEnv_ = 0.0;
    feelSlowEnv_ = 0.0;
    feelMotionLp_ = 0.0;
    feelPrevOut_ = 0.0;
    preparedBlockSize_ = samplesPerBlock;
}

bool ValvraProcessor::loadNeuralModelFile(const juce::String& path)
{
    auto* request = new NeuralSwapRequest();
    const double srForModel = (sampleRate_ > 1000.0) ? sampleRate_ : 48000.0;
    request->left.prepare(srForModel);
    request->right.prepare(srForModel);

    const std::string p = path.toStdString();
    if (p.empty())
    {
        request->left.clearModel();
        request->right.clearModel();
        request->loaded = false;

        if (preparedBlockSize_ <= 0)
        {
            neuralL_ = std::move(request->left);
            neuralR_ = std::move(request->right);
            neuralModelLoadedState_.store(false, std::memory_order_relaxed);
            neuralModelPath_.clear();
            delete request;
            return false;
        }

        if (auto* old = neuralSwapRequest_.exchange(request, std::memory_order_acq_rel))
            delete old;
        neuralSwapRequested_.store(true, std::memory_order_release);
        neuralModelPath_.clear();
        return false;
    }

    const bool leftOk = request->left.loadModelJson(p);
    const bool rightOk = request->right.loadModelJson(p);
    const bool ok = leftOk && rightOk;
    if (! ok)
    {
        delete request;
        return false;
    }

    request->loaded = true;
    if (preparedBlockSize_ <= 0)
    {
        neuralL_ = std::move(request->left);
        neuralR_ = std::move(request->right);
        neuralModelLoadedState_.store(true, std::memory_order_relaxed);
        neuralModelPath_ = path;
        delete request;
        return true;
    }

    if (auto* old = neuralSwapRequest_.exchange(request, std::memory_order_acq_rel))
        delete old;
    neuralSwapRequested_.store(true, std::memory_order_release);
    neuralModelPath_ = path;
    return true;
}

void ValvraProcessor::rebuildChain()
{
    currentPresetIndex_ =
        static_cast<int>(*params_.getRawParameterValue(kParamPreset));

    const int osFactor = lastOsFactor_;
    const double internalSR = sampleRate_ * static_cast<double>(osFactor);

    // ★ Stereo Monte Carlo: each channel must use a DIFFERENT seed so that
    // real analog-rack "two slightly different units" feel is preserved.
    // EXCEPTION: in Mid/Side mode the two chains process M (centre) and S
    // (sides) — those should share identical character so a mono-compat
    // sum cancels out the side path cleanly.  Use the SAME seed for both
    // when M/S routing is active.
    constexpr std::uint64_t kStereoSalt = 0x123456789ABCDEFULL;
    const std::uint64_t seed = currentSeed_.load(std::memory_order_relaxed);
    const bool msMode = activeMsMode_;
    const std::uint64_t seedL = seed;
    const std::uint64_t seedR = msMode ? seed : (seed ^ kStereoSalt);
    auto cfgL = buildPresetConfig(currentPresetIndex_, seedL);
    auto cfgR = buildPresetConfig(currentPresetIndex_, seedR);
    const auto distribution = static_cast<dsp::VariationDistribution>(
        static_cast<int>(*params_.getRawParameterValue(kParamMcDistribution)));
    cfgL.variationDistribution = distribution;
    cfgR.variationDistribution = distribution;

    const auto cvVoicing = static_cast<dsp::CultureVultureVoicing>(
        static_cast<int>(*params_.getRawParameterValue(kParamCvMode)));
    if (currentPresetIndex_ == 2)
    {
        cfgL = dsp::chain_presets::CultureVultureMode(cvVoicing);
        cfgR = dsp::chain_presets::CultureVultureMode(cvVoicing);
        cfgL.variationSeed = seedL;
        cfgR.variationSeed = seedR;
        cfgL.variationDistribution = distribution;
        cfgR.variationDistribution = distribution;
    }

    // Mains region — applied after the CV block above may have replaced
    // the configs wholesale.
    {
        const bool eu = static_cast<int>(
            *params_.getRawParameterValue(kParamMains)) == 1;
        const double mainsHz = eu ? 50.0 : 60.0;
        cfgL.mainsFrequencyHz = mainsHz;
        cfgR.mainsFrequencyHz = mainsHz;
        const double g3Bias = static_cast<double>(
            params_.getRawParameterValue(kParamCvBias)->load());
        cfgL.suppressorBiasOffsetV = g3Bias;
        cfgR.suppressorBiasOffsetV = g3Bias;
    }

    auto applyChainBuilderParams = [this](dsp::TubeAmpChainConfig& cfg)
    {
        const int stageChoice = static_cast<int>(
            *params_.getRawParameterValue(kParamStageCount));
        if (stageChoice > 0)
        {
            const int requestedStages = stageChoice;
            const int clampedStages = juce::jlimit(
                1, dsp::TubeAmpChainConfig::kMaxStages, requestedStages);
            if (clampedStages > cfg.numStages && cfg.numStages > 0)
            {
                const auto lastConfigured =
                    cfg.stages[static_cast<std::size_t>(cfg.numStages - 1)];
                for (int i = cfg.numStages; i < clampedStages; ++i)
                    cfg.stages[static_cast<std::size_t>(i)] = lastConfigured;
            }
            cfg.numStages = clampedStages;
        }

        const int inputChoice = static_cast<int>(
            *params_.getRawParameterValue(kParamInputTrafo));
        if (inputChoice == 1)
            cfg.useInputTransformer = false;
        else if (inputChoice >= 2)
        {
            cfg.useInputTransformer = true;
            cfg.inputTrafoConfig = transformerOverrideKeepingCalibration(
                inputChoice, cfg.inputTrafoConfig);
        }

        const int outputChoice = static_cast<int>(
            *params_.getRawParameterValue(kParamOutputTrafo));
        if (outputChoice == 1)
            cfg.useOutputTransformer = false;
        else if (outputChoice >= 2)
        {
            cfg.useOutputTransformer = true;
            cfg.outputTrafoConfig = transformerOverrideKeepingCalibration(
                outputChoice, cfg.outputTrafoConfig);
        }
    };

    applyChainBuilderParams(cfgL);
    applyChainBuilderParams(cfgR);

    // Per-stage editor overrides — apply AFTER stage count has been settled
    // so that stages added by the chain-builder Stage Count combo also pick
    // up the user's tube/topology/drive/bias choices.
    auto applyStageEditOverrides = [this](dsp::TubeAmpChainConfig& cfg)
    {
        const int n = cfg.numStages;
        for (int i = 0; i < n && i < dsp::TubeAmpChainConfig::kMaxStages; ++i)
        {
            const auto& ids = kStageParams[static_cast<std::size_t>(i)];
            auto& s = cfg.stages[static_cast<std::size_t>(i)];

            const int tubeChoice = static_cast<int>(
                *params_.getRawParameterValue(ids.tube));
            const auto tube = tubeChoiceLookup(tubeChoice);
            if (tube.override_)
                s.tube = tube.params;

            const int topoChoice = static_cast<int>(
                *params_.getRawParameterValue(ids.topology));
            const auto topo = topologyChoiceLookup(topoChoice);
            if (topo.override_)
                s.topology = topo.topology;

            const float driveDb = *params_.getRawParameterValue(ids.drive);
            if (std::abs(driveDb) > 1.0e-3f)
            {
                const double mult = std::pow(10.0,
                    static_cast<double>(driveDb) / 20.0);
                s.inputVoltageSwing *= mult;
            }

            const float biasOffset = *params_.getRawParameterValue(ids.bias);
            if (std::abs(biasOffset) > 1.0e-4f)
                s.Vg_bias += static_cast<double>(biasOffset);
        }
    };
    applyStageEditOverrides(cfgL);
    applyStageEditOverrides(cfgR);

    const int selectedProfile = static_cast<int>(
        *params_.getRawParameterValue(kParamTargetProfile));
    const int profile = effectiveTargetProfile(selectedProfile,
                                               currentPresetIndex_);
    const auto realismProfile = targetProfileSpec(
        profile, useFittedProfileForTargetProfile(profile));
    const float realism = mapRealismControl(
        params_.getRawParameterValue(kParamRealismAmount)->load());
    auto applyRealism = [&](dsp::TubeAmpChainConfig& cfg)
    {
        cfg.realismAmount = realism;
        cfg.feedbackAmount =
            static_cast<double>(realism * realismProfile.feedbackAmount);
        cfg.transformerLoading =
            static_cast<double>(realism * realismProfile.transformerLoading);
        cfg.interstageDAAmount =
            static_cast<double>(realism * realismProfile.interstageDA);
        cfg.interstageDATau =
            (profile == 3) ? 0.55 : (profile == 5) ? 0.25 : 0.38;
        cfg.feedbackVoicing =
            (profile == 3) ? dsp::FeedbackVoicing::LowFeedback
          : (profile == 4) ? dsp::FeedbackVoicing::IronDamping
          :                 dsp::FeedbackVoicing::Controlled;
    };
    applyRealism(cfgL);
    applyRealism(cfgR);

    // Slow-state carry-over (docs/34 §4.3): if this rebuild is a pure
    // parameter edit — same preset, seed, internal rate and routing — the
    // running chains' warmup/thermal/magnetic/supply history is re-based
    // onto the new operating points instead of cold-starting.  Reroll,
    // preset, OS and M/S changes intentionally do NOT carry (a different
    // unit / a different circuit has no claim on the old state; reroll
    // continuity is handled by its own crossfade).  The pre-setup copies
    // are value snapshots, same practice as the reroll crossfade path.
    const bool canCarry = carryLastPreset_ == currentPresetIndex_
        && carryLastSeed_ == seed
        && carryLastSR_ == internalSR
        && carryLastMs_ == msMode;
    if (canCarry)
    {
        const dsp::TubeAmpChain prevL = chainL_;
        const dsp::TubeAmpChain prevR = chainR_;
        chainL_.setup(cfgL, internalSR);
        chainR_.setup(cfgR, internalSR);
        chainL_.carrySlowStateFrom(prevL);
        chainR_.carrySlowStateFrom(prevR);
    }
    else
    {
        chainL_.setup(cfgL, internalSR);
        chainR_.setup(cfgR, internalSR);
    }
    carryLastPreset_ = currentPresetIndex_;
    carryLastSeed_   = seed;
    carryLastSR_     = internalSR;
    carryLastMs_     = msMode;

    // Expansion engines roll their own per-instance lottery off the same
    // seed (tape oxide pinning, transport wow depth, T4 trap-level τ) —
    // without this every instance's tape/opto "unit" was identical.
    {
        const auto vx = dsp::makeVariation(
            cfgL.variationSeed ^ 0xD6E8FEB86659FD93ULL,
            cfgL.variationDistribution);
        expansionRack_.setVariation(vx.tapeCoreK_scale,
                                    vx.tapeWow_scale,
                                    vx.optoMemoryTau_scale);
    }

    // Shared-rail coupling: one PowerSupplySag per processor drives BOTH
    // chains.  Enable it only when the preset itself has sag on (solid-
    // state rectifier presets keep independent stiff rails) AND we're in
    // plain Stereo routing — in Mid/Side mode the two chains process the
    // centre and the side independently, and a shared rail would let the
    // side's current draw modulate the centre's gain (breaking mono-
    // compatibility, since a side-only transient would leave a residue
    // in L+R after recombination).  Running the envelope follower at
    // the INTERNAL (upsampled) rate keeps the sag time constants
    // identical to what the individual chain's psu_ would have seen in
    // legacy independent-PSU mode.
    sharedPSUActive_ = cfgL.enablePSUSag && (! msMode);
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
        case 4: return 16;
        default: return 4;
    }
}

float ValvraProcessor::currentLevelMatchTrimDb() const noexcept
{
    const int mode =
        static_cast<int>(*params_.getRawParameterValue(kParamLevelMatchMode));
    if (mode == 1)
        return modeOutputTrimDbForPresetIndex(currentPresetIndex_);
    if (mode == 2)
    {
        const float analyzed =
            params_.getRawParameterValue(kParamAnalyzedOutputTrimDb)->load();
        return juce::jlimit(-24.0f, 24.0f, analyzed);
    }
    return 0.0f;
}

float ValvraProcessor::recommendedRealismForCurrentPreset() const noexcept
{
    const int selectedProfile = static_cast<int>(
        *params_.getRawParameterValue(kParamTargetProfile));
    const int profile = effectiveTargetProfile(selectedProfile,
                                               currentPresetIndex_);
    return targetProfileSpec(
        profile, useFittedProfileForTargetProfile(profile)).recommendedRealism;
}

bool ValvraProcessor::useFittedProfileForPreset(int presetIndex) const noexcept
{
    const int idx = juce::jlimit(
        0,
        static_cast<int>(useFittedProfileByMode_.size()) - 1,
        presetIndex);
    return useFittedProfileByMode_[static_cast<std::size_t>(idx)];
}

bool ValvraProcessor::useFittedProfileForTargetProfile(int profileIndex) const noexcept
{
    const int clamped = juce::jlimit(1, 5, profileIndex);
    return useFittedProfileForPreset(clamped - 1);
}

int ValvraProcessor::currentLatencyInSamples() const noexcept
{
    // OS path latency (linear-phase FIRs).  The True Peak limiter ALWAYS
    // contributes its lookahead even when bypassed, because we don't want
    // PDC to flip when the user toggles TP off — that would re-time the
    // track relative to its bus partners.
    int osLat = 0;
    switch (lastOsFactor_)
    {
        case 1: osLat = 0; break;
        case 2: osLat = dsp::PolyphaseOversampler<2>::latencyInBaseSamples(); break;
        case 4: osLat = dsp::PolyphaseOversampler<4>::latencyInBaseSamples(); break;
        case 8: osLat = dsp::PolyphaseOversampler<8>::latencyInBaseSamples(); break;
        case 16: osLat = dsp::PolyphaseOversampler<16>::latencyInBaseSamples(); break;
        default: osLat = 0; break;
    }
    // The expansion rack has a fixed, mode-invariant latency (its tape
    // path's 2× FIR round trip; the other modes ride a matching delay
    // ring — docs/34 §4.2).  It sits in the wet path BEFORE the mix, so
    // like the OS latency it is absorbed by the dry-alignment delay and
    // never flips PDC on a mode change.
    return osLat + dsp::ExpansionRack::latencyInBaseSamples()
         + tpLimiter_.currentLatencyInSamples();
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

    // Consume pending graph mutations from the message thread.  Rebuilds
    // reset warmup/filter/transformer/PSU state, so we do not mutate the
    // live graph at block start.  Instead the sample loop below fades the
    // current graph to silence, applies the rebuild at zero gain, then fades
    // back in over 10 ms.
    const bool wantRebuild =
        rebuildRequested_.exchange(false, std::memory_order_relaxed);
    const bool wantReroll =
        rerollRequested_.exchange(false, std::memory_order_relaxed);
    const bool wantNeuralSwap =
        neuralSwapRequested_.exchange(false, std::memory_order_acquire);
    const bool bypass   = bypassParam_ != nullptr && bypassParam_->get();
    bypassBlendSmooth_.setTargetValue(bypass ? 1.0f : 0.0f);

    auto applyQueuedNeuralSwap = [this]() noexcept
    {
        if (auto* req = neuralSwapRequest_.exchange(nullptr, std::memory_order_acq_rel))
        {
            neuralL_ = std::move(req->left);
            neuralR_ = std::move(req->right);
            neuralModelLoadedState_.store(req->loaded, std::memory_order_relaxed);
            delete req;
        }
    };

    auto stageFadeMutationIfNeeded = [this]() noexcept
    {
        if (graphFadePhase_ == GraphFadePhase::Idle)
        {
            graphFadePhase_ = GraphFadePhase::FadeOut;
            graphFadePos_ = 0;
        }
    };

    if (wantNeuralSwap)
    {
        neuralSwapPendingApply_ = true;
        neuralSwapPhase_ = NeuralSwapPhase::FadeOut;
        neuralSwapPos_ = 0;
    }

    if (wantRebuild || wantReroll)
    {
        graphFadePendingRebuild_ = graphFadePendingRebuild_ || wantRebuild;
        graphFadePendingReroll_  = graphFadePendingReroll_  || wantReroll;
        if (wantReroll)
            graphFadePendingSeed_ =
                pendingSeed_.load(std::memory_order_relaxed);
        stageFadeMutationIfNeeded();
    }

    // NOTE: while bypass is active we intentionally KEEP graph mutations
    // queued instead of hard-applying them here.  Immediate rebuild under
    // bypass can still jump because latency-alignment structures (dryDelay /
    // oversampler phase) reset abruptly on the dry path.  By deferring until
    // bypass release, we reuse the existing fade-out/rebuild/fade-in path.

    // When host bypass is active, we can apply neural swaps immediately:
    // no wet path is audible and this avoids queuing stale model requests.
    if (bypass && neuralSwapPendingApply_)
    {
        applyQueuedNeuralSwap();
        neuralSwapPendingApply_ = false;
        neuralSwapPhase_ = NeuralSwapPhase::Idle;
        neuralSwapPos_ = 0;
    }

    // Re-arm the warmup envelope on every stage when the user clicks the
    // Warmup button.  Cheap — just nudges warmupCurrent_ back to 0.85 for
    // each stage, so the next ~30 seconds of audio settle gradually back
    // toward unity gain.  Done here on the audio thread so we don't race
    // the inner loop's read of warmupCurrent_.
    if (warmupRequested_.exchange(false, std::memory_order_relaxed))
    {
        // Single chain helper covers preamp stages AND the push-pull
        // output section, so the user clicking Warmup re-arms the entire
        // rack — not just the preamp valves.
        chainL_.warmupAllStages();
        chainR_.warmupAllStages();
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
    inputTrimSmooth_.setTargetValue(
        juce::Decibels::decibelsToGain(
            params_.getRawParameterValue(kParamInputTrimDb)->load()));
    outGainSmooth_.setTargetValue(
        juce::Decibels::decibelsToGain(
            params_.getRawParameterValue(kParamOutputDb)->load()));
    mixSmooth_.setTargetValue(
        params_.getRawParameterValue(kParamMix)->load());
    neuralBlendSmooth_.setTargetValue(
        params_.getRawParameterValue(kParamNeuralBlend)->load());
    expansionAmountSmooth_.setTargetValue(
        params_.getRawParameterValue(kParamExpansionAmount)->load());
    expansionMixSmooth_.setTargetValue(
        params_.getRawParameterValue(kParamExpansionMix)->load());
    realismSmooth_.setTargetValue(
        mapRealismControl(params_.getRawParameterValue(kParamRealismAmount)->load()));
    const float levelMatchDb = currentLevelMatchTrimDb();
    levelMatchAppliedDbState_.store(levelMatchDb, std::memory_order_relaxed);
    levelMatchSmooth_.setTargetValue(
        juce::Decibels::decibelsToGain(levelMatchDb));

    expansionRack_.setMode(static_cast<dsp::ExpansionMode>(
        static_cast<int>(*params_.getRawParameterValue(kParamExpansionMode))));

    const bool stereo = (numOutCh >= 2);

    auto* L = buffer.getWritePointer(0);
    auto* R = stereo ? buffer.getWritePointer(1) : nullptr;

    const bool nullTest = nullTestMode_.load(std::memory_order_relaxed);
    nullTestBlendSmooth_.setTargetValue(nullTest ? 1.0f : 0.0f);
    // Mid/Side routing changes are graph mutations (same class as oversample):
    // switching L/R <-> M/S remaps persistent non-linear chain states, so we
    // stage it through fade-out/rebuild/fade-in instead of hard-switching.
    const bool desiredMsMode = (numOutCh >= 2) && (static_cast<int>(
        *params_.getRawParameterValue(kParamMSMode)) == 1);
    if (desiredMsMode != activeMsMode_)
    {
        pendingMsMode_ = desiredMsMode;
        pendingMsModeValid_ = true;
        graphFadePendingRebuild_ = true;
        stageFadeMutationIfNeeded();
    }

    // Process with the currently-active routing while a switch is staged.
    const bool msMode = (numOutCh >= 2) && activeMsMode_;

    // Oversample-factor changes are graph-shape mutations: stage them through
    // the same 10 ms fade-out/rebuild/fade-in path as reroll/rebuild, rather
    // than hard-swapping FIR state at block boundaries (which can click).
    const int desiredOsFactor = currentOversampleFactor();
    if (desiredOsFactor != lastOsFactor_)
    {
        pendingOsFactor_ = desiredOsFactor;
        graphFadePendingRebuild_ = true;
        stageFadeMutationIfNeeded();
    }
    const int osFactor = lastOsFactor_;

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
                case 16:
                {
                    auto upL = os16L_.upsample(xL);
                    for (auto& v : upL) stepChainMono(v);
                    outL = os16L_.downsample(upL);
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
            case 16:
            {
                auto upL = os16L_.upsample(xL);
                auto upR = os16R_.upsample(xR);
                for (std::size_t i = 0; i < upL.size(); ++i)
                    stepChainsStereo(upL[i], upR[i]);
                outL = os16L_.downsample(upL);
                outR = os16R_.downsample(upR);
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

    double blockEnergy = 0.0;
    int blockEnergyCount = 0;
    double inputEnergy = 0.0;
    double inputPeakAbs = 0.0;
    int inputEnergyCount = 0;
    double matchOutputEnergy = 0.0;
    int matchOutputEnergyCount = 0;
    double feelRecoveryEnergy = 0.0;
    double feelMotionEnergy = 0.0;
    double feelMotionDeltaEnergy = 0.0;
    int feelCount = 0;
    realismNoiseEnergy_ = 0.0;
    realismNoiseCount_ = 0;
    const int selectedRealismProfile = static_cast<int>(
        *params_.getRawParameterValue(kParamTargetProfile));
    const int realismProfileIndex = effectiveTargetProfile(
        selectedRealismProfile, currentPresetIndex_);
    const auto realismProfile = targetProfileSpec(
        realismProfileIndex,
        useFittedProfileForTargetProfile(realismProfileIndex));
    const float realismRecommended =
        std::max(0.05f, mapRealismControl(realismProfile.recommendedRealism));

    auto nextLeakageNoise = [this]() noexcept
    {
        std::uint64_t x = analogLeakageRng_;
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        analogLeakageRng_ = (x != 0) ? x : 0xA5A55A5A12345678ULL;
        const double u = static_cast<double>(x & 0xFFFFFFULL) / 8388608.0;
        return u - 1.0;
    };

    for (int n = 0; n < numSamples; ++n)
    {
        // Sample-accurate parameter ramps — one getNextValue() pulls per sample.
        const float driveRaw = driveSmooth_.getNextValue();
        const float drive = driveRaw * modeDriveScaleForPresetIndex(currentPresetIndex_);
        const float inputTrimGain = inputTrimSmooth_.getNextValue();
        const float outGain = outGainSmooth_.getNextValue();
        const float levelMatchGain = levelMatchSmooth_.getNextValue();
        const float realism = juce::jlimit(0.0f, 1.0f, realismSmooth_.getNextValue());
        const float mix     = mixSmooth_    .getNextValue();
        const float expansionAmount = expansionAmountSmooth_.getNextValue();
        const float expansionMix = expansionMixSmooth_.getNextValue();
        expansionRack_.setAmount(expansionAmount);
        expansionRack_.setMix(expansionMix);

        const float rawL = L[n];
        const float rawR = stereo ? R[n] : rawL;
        const float dryL = std::isfinite(rawL) ? rawL : 0.0f;
        const float dryR = std::isfinite(rawR) ? rawR : 0.0f;
        const float dryLd = dryDelayL_.process(dryL);
        const float dryRd = stereo ? dryDelayR_.process(dryR) : dryLd;
        const float bypassBlend = bypassBlendSmooth_.getNextValue();

        // Fast path: once the bypass crossfade has fully reached dry, skip
        // all wet processing work but keep dry-delay latency alignment.
        if (bypass && bypassBlend >= 0.9999f)
        {
            L[n] = dryLd;
            if (stereo) R[n] = dryRd;
            analyzer_.push(dryLd);
            continue;
        }


        const float trimL = dryL * inputTrimGain;
        const float trimR = dryR * inputTrimGain;
        inputEnergy += static_cast<double>(trimL) * trimL;
        inputPeakAbs = std::max(inputPeakAbs, std::abs(static_cast<double>(trimL)));
        ++inputEnergyCount;
        if (stereo)
        {
            inputEnergy += static_cast<double>(trimR) * trimR;
            inputPeakAbs = std::max(inputPeakAbs, std::abs(static_cast<double>(trimR)));
            ++inputEnergyCount;
        }

        // Input Trim is the hardware-lineup trim; Drive then pushes the
        // calibrated signal into the non-linear stages.
        double xL = static_cast<double>(trimL) * drive;
        double xR = static_cast<double>(trimR) * drive;
        const double neuralInL = xL;
        const double neuralInR = xR;

        // M/S encode: at this point xL/xR represent the (post-drive)
        // input to the chains.  In M/S mode we substitute (M, S) for
        // (L, R) so chainL_ processes the centre and chainR_ processes
        // the sides.  Decode happens symmetrically right after the
        // chain runs, so downstream code (dry-delay, null-test mix,
        // limiter) operates in L/R space as before.
        if (msMode)
        {
            constexpr double kInvSqrt2 = 0.70710678118654752440;
            const double M = kInvSqrt2 * (xL + xR);
            const double S = kInvSqrt2 * (xL - xR);
            xL = M;
            xR = S;
        }

        double wetL = 0.0, wetR = 0.0;
        processSamplePair(xL, xR, wetL, wetR);

        if (msMode)
        {
            // Decode (M_processed, S_processed) → (L, R).
            //
            // L = (M + S)/sqrt(2), R = (M − S)/sqrt(2) is the inverse
            // of the energy-preserving encode above.  When the two
            // chains share the same seed and identical character, a
            // mono down-mix leaves the side chain output entirely cancelled.
            constexpr double kInvSqrt2 = 0.70710678118654752440;
            const double M = wetL;
            const double S = wetR;
            wetL = kInvSqrt2 * (M + S);
            wetR = kInvSqrt2 * (M - S);
        }

        // NaN / Inf guard: if the DSP graph briefly diverges (e.g. a
        // pathological parameter transient) we substitute zeros rather than
        // letting a single corrupted sample propagate into the DAW mixer.
        if (! std::isfinite(wetL)) wetL = 0.0;
        if (! std::isfinite(wetR)) wetR = 0.0;

        float neuralSwapGain = 1.0f;
        if (neuralSwapPhase_ == NeuralSwapPhase::FadeOut)
        {
            const float t = static_cast<float>(neuralSwapPos_)
                          / static_cast<float>(std::max(1, neuralSwapSamples_));
            neuralSwapGain = 1.0f - t;
            ++neuralSwapPos_;
            if (neuralSwapPos_ >= neuralSwapSamples_)
            {
                if (neuralSwapPendingApply_)
                {
                    applyQueuedNeuralSwap();
                    neuralSwapPendingApply_ = false;
                }
                neuralSwapPhase_ = NeuralSwapPhase::FadeIn;
                neuralSwapPos_ = 0;
                neuralSwapGain = 0.0f;
            }
        }
        else if (neuralSwapPhase_ == NeuralSwapPhase::FadeIn)
        {
            const float t = static_cast<float>(neuralSwapPos_)
                          / static_cast<float>(std::max(1, neuralSwapSamples_));
            neuralSwapGain = t;
            ++neuralSwapPos_;
            if (neuralSwapPos_ >= neuralSwapSamples_)
            {
                neuralSwapPhase_ = NeuralSwapPhase::Idle;
                neuralSwapPos_ = 0;
                neuralSwapGain = 1.0f;
            }
        }

        const float neuralBlend = neuralBlendSmooth_.getNextValue();
        const float effectiveNeuralBlend = neuralBlend * neuralSwapGain;
        if (effectiveNeuralBlend > 1.0e-6f)
        {
            wetL = neuralL_.process(neuralInL, wetL, effectiveNeuralBlend);
            if (stereo)
                wetR = neuralR_.process(neuralInR, wetR, effectiveNeuralBlend);
            else
                wetR = 0.0;
        }

        expansionRack_.processStereo(wetL, stereo ? wetR : wetL, wetL, wetR);
        if (! stereo)
            wetR = 0.0;

        // Null-test toggle is crossfaded (like bypass) to avoid a hard jump
        // between "wet mix" and "wet-dry difference" monitoring modes.
        const float nullBlend = nullTestBlendSmooth_.getNextValue();

        const float dryMixL = dryLd * inputTrimGain;
        const float dryMixR = dryRd * inputTrimGain;
        const double mixOutL = ((1.0 - mix) * dryMixL) + (mix * wetL);
        const double mixOutR = stereo
            ? (((1.0 - mix) * dryMixR) + (mix * wetR))
            : 0.0;
        matchOutputEnergy += mixOutL * mixOutL;
        ++matchOutputEnergyCount;
        if (stereo)
        {
            matchOutputEnergy += mixOutR * mixOutR;
            ++matchOutputEnergyCount;
        }

        const float normalL = static_cast<float>(
            mixOutL * levelMatchGain * outGain);
        const float normalR = stereo
            ? static_cast<float>(mixOutR * levelMatchGain * outGain)
            : 0.0f;

        const float nullL = static_cast<float>(wetL - dryMixL);
        const float nullR = stereo ? static_cast<float>(wetR - dryMixR) : 0.0f;

        float normalGain = 1.0f;
        if (! bypass)
            normalGain = abMatchSmooth_.getNextValue();
        else
            abMatchSmooth_.skip(1);

        float outL = normalL * normalGain;
        float outR = normalR * normalGain;
        if (nullBlend > 1.0e-6f)
        {
            outL = outL * (1.0f - nullBlend) + nullL * nullBlend;
            outR = outR * (1.0f - nullBlend) + nullR * nullBlend;
        }

        if (realism > 1.0e-6f && ! bypass)
        {
            const float profileScale = juce::jlimit(
                0.0f, 1.0f, realism / realismRecommended);
            const float interactionDrive = juce::jlimit(
                0.0f,
                1.0f,
                static_cast<float>(0.5 * (chainL_.currentInteractionDrive()
                    + (stereo ? chainR_.currentInteractionDrive()
                              : chainL_.currentInteractionDrive()))));
            const float crossDb =
                realismProfile.crosstalkDb
                - (1.0f - profileScale) * 18.0f
                - (1.0f - interactionDrive) * 6.0f;
            const float noiseCapDb = (realismProfileIndex >= 4) ? -82.0f : -70.0f;
            const float noiseDb = std::min(
                noiseCapDb,
                realismProfile.noiseFloorDbfs
                    - (1.0f - profileScale) * 18.0f
                    - (1.0f - interactionDrive) * 7.0f);
            const float crossGain = juce::Decibels::decibelsToGain(crossDb);
            const float noiseGain = juce::Decibels::decibelsToGain(noiseDb)
                                  * (0.35f + 0.65f * interactionDrive);

            if (stereo)
            {
                const float srcL = outL;
                const float srcR = outR;
                outL += srcR * crossGain;
                outR += srcL * crossGain;
            }

            // Output leakage hum runs at the same line frequency as the
            // chain's heater / rectifier textures (docs/34 §1.5) — one
            // 50/60 Hz source for the whole instance instead of a hardcoded
            // 60 Hz that contradicted a 50 Hz-configured chain.
            const double mainsHz = std::max(1.0, chainL_.mainsFrequencyHz());
            const double humInc = juce::MathConstants<double>::twoPi
                                * mainsHz / std::max(sampleRate_, 1.0);
            realismHumPhaseL_ += humInc;
            realismHumPhaseR_ += humInc * 1.003;
            if (realismHumPhaseL_ >= juce::MathConstants<double>::twoPi)
                realismHumPhaseL_ -= juce::MathConstants<double>::twoPi;
            if (realismHumPhaseR_ >= juce::MathConstants<double>::twoPi)
                realismHumPhaseR_ -= juce::MathConstants<double>::twoPi;

            const float nL = static_cast<float>(
                noiseGain * (0.65 * nextLeakageNoise()
                    + 0.35 * std::sin(realismHumPhaseL_)));
            outL += nL;
            realismNoiseEnergy_ += static_cast<double>(nL) * nL;
            ++realismNoiseCount_;
            if (stereo)
            {
                const float nR = static_cast<float>(
                    noiseGain * (0.65 * nextLeakageNoise()
                        + 0.35 * std::sin(realismHumPhaseR_)));
                outR += nR;
                realismNoiseEnergy_ += static_cast<double>(nR) * nR;
                ++realismNoiseCount_;
            }
        }

        if (graphFadePhase_ != GraphFadePhase::Idle && ! bypass)
        {
            const float t = static_cast<float>(graphFadePos_)
                          / static_cast<float>(std::max(1, graphFadeSamples_));
            const float fadeGain = (graphFadePhase_ == GraphFadePhase::FadeOut)
                ? (1.0f - t)
                : t;
            outL *= fadeGain;
            outR *= fadeGain;

            ++graphFadePos_;
            if (graphFadePos_ >= graphFadeSamples_)
            {
                if (graphFadePhase_ == GraphFadePhase::FadeOut)
                {
                    const bool applyReroll = graphFadePendingReroll_;
                    const bool applyRebuild = graphFadePendingRebuild_;
                    if (applyReroll)
                    {
                        currentSeed_.store(graphFadePendingSeed_,
                                           std::memory_order_relaxed);
                        graphFadePendingReroll_ = false;
                    }
                    if (applyRebuild || applyReroll)
                    {
                        if (pendingOsFactor_ != 0)
                        {
                            lastOsFactor_ = pendingOsFactor_;
                            pendingOsFactor_ = 0;
                        }
                        if (pendingMsModeValid_)
                        {
                            activeMsMode_ = pendingMsMode_;
                            pendingMsModeValid_ = false;
                        }
                        rebuildChain();
                        os2L_.reset(); os2R_.reset();
                        os4L_.reset(); os4R_.reset();
                        os8L_.reset(); os8R_.reset();
                        os16L_.reset(); os16R_.reset();
                        const int totalLat = currentLatencyInSamples();
                        const int dryAlignLat =
                            totalLat - tpLimiter_.currentLatencyInSamples();
                        setLatencySamples(totalLat);
                        dryDelayL_.setLatency(dryAlignLat);
                        dryDelayR_.setLatency(dryAlignLat);
                        graphFadePendingRebuild_ = false;
                        graphFadePendingSeed_ = 0;
                    }
                    graphFadePhase_ = GraphFadePhase::FadeIn;
                    graphFadePos_ = 0;
                }
                else
                {
                    if (graphFadePendingRebuild_ || graphFadePendingReroll_)
                    {
                        graphFadePhase_ = GraphFadePhase::FadeOut;
                        graphFadePos_ = 0;
                    }
                    else
                    {
                        graphFadePhase_ = GraphFadePhase::Idle;
                        graphFadePos_ = 0;
                    }
                }
            }
        }

        if (bypassBlend > 0.0001f)
        {
            const float wetKeep = 1.0f - bypassBlend;
            outL = outL * wetKeep + dryLd * bypassBlend;
            outR = outR * wetKeep + dryRd * bypassBlend;
        }

        const double monoOut = stereo
            ? 0.5 * (static_cast<double>(outL) + static_cast<double>(outR))
            : static_cast<double>(outL);
        const double absOut = std::abs(monoOut);
        feelFastEnv_ += feelFastCoeff_ * (absOut - feelFastEnv_);
        feelSlowEnv_ += feelSlowCoeff_ * (absOut - feelSlowEnv_);
        const double recovery = std::max(0.0, feelFastEnv_ - feelSlowEnv_);
        feelRecoveryEnergy += recovery * recovery;

        feelMotionLp_ += feelMotionLpCoeff_ * (monoOut - feelMotionLp_);
        const double motion = monoOut - feelMotionLp_;
        feelMotionEnergy += motion * motion;
        const double motionDelta = monoOut - feelPrevOut_;
        feelPrevOut_ = monoOut;
        feelMotionDeltaEnergy += motionDelta * motionDelta;
        ++feelCount;

        L[n] = outL;
        if (stereo) R[n] = outR;

        if (! bypass && nullBlend < 0.5f)
        {
            auto& activeSlot = slotIsB_ ? slotB_ : slotA_;
            activeSlot.lufsMeter.process(outL, outR, stereo);

            blockEnergy += static_cast<double>(outL) * outL;
            if (stereo)
                blockEnergy += static_cast<double>(outR) * outR;
            blockEnergyCount += stereo ? 2 : 1;
        }

        // Feed the harmonic analyzer (left channel only for efficiency)
        analyzer_.push(outL);
    }

    if (inputEnergyCount > 0)
    {
        const double inputRms = std::sqrt(inputEnergy
            / static_cast<double>(inputEnergyCount));
        const float inputDb = dbFromRms(inputRms);
        recentInputRmsDb_ = (recentInputRmsDb_ <= -99.0f)
            ? inputDb
            : (0.96f * recentInputRmsDb_ + 0.04f * inputDb);
        inputRmsDbfsState_.store(recentInputRmsDb_,
                                 std::memory_order_relaxed);
        inputPeakDbfsState_.store(dbFromRms(inputPeakAbs),
                                  std::memory_order_relaxed);
        inputTrimNeededDbState_.store(
            juce::jlimit(-24.0f, 24.0f, -18.0f - recentInputRmsDb_),
            std::memory_order_relaxed);

        const int selectedProfile = static_cast<int>(
            *params_.getRawParameterValue(kParamTargetProfile));
        const int profile = effectiveTargetProfile(selectedProfile,
                                                   currentPresetIndex_);
        const float driveRaw =
            params_.getRawParameterValue(kParamDrive)->load();
        const float prevPeak = peakDbfsState_.load(std::memory_order_relaxed);
        const float prevCorr = correlationState_.load(std::memory_order_relaxed);
        const float sag = sharedPSUActive_
            ? static_cast<float>(sharedPSU_.sagPercent())
            : static_cast<float>(chainL_.currentSagPercent());
        const auto match = targetMatchFor(recentInputRmsDb_,
                                          driveRaw,
                                          prevPeak,
                                          sag,
                                          prevCorr,
                                          profile,
                                          useFittedProfileForTargetProfile(profile));
        targetMatchScoreState_.store(match.score, std::memory_order_relaxed);
        targetMatchState_.store(match.state, std::memory_order_relaxed);
        targetReasonCodeState_.store(
            targetReasonCodeFor(recentInputRmsDb_,
                                driveRaw,
                                prevPeak,
                                sag,
                                profile,
                                useFittedProfileForTargetProfile(profile)),
            std::memory_order_relaxed);
    }

    const float currentRealism = mapRealismControl(
        params_.getRawParameterValue(kParamRealismAmount)->load());
    realismAppliedState_.store(currentRealism, std::memory_order_relaxed);
    const float profileScaleForState = juce::jlimit(
        0.0f, 1.0f, currentRealism / realismRecommended);
    const float interactionDriveForState = juce::jlimit(
        0.0f,
        1.0f,
        static_cast<float>(0.5 * (chainL_.currentInteractionDrive()
            + (stereo ? chainR_.currentInteractionDrive()
                      : chainL_.currentInteractionDrive()))));
    crosstalkDbState_.store(
        (currentRealism > 1.0e-6f)
            ? realismProfile.crosstalkDb
                - (1.0f - profileScaleForState) * 18.0f
                - (1.0f - interactionDriveForState) * 6.0f
            : -120.0f,
        std::memory_order_relaxed);
    if (realismNoiseCount_ > 0)
    {
        noiseFloorDbfsState_.store(
            dbFromRms(std::sqrt(realismNoiseEnergy_
                / static_cast<double>(realismNoiseCount_))),
            std::memory_order_relaxed);
    }
    else
    {
        noiseFloorDbfsState_.store(-120.0f, std::memory_order_relaxed);
    }

    float textureRecovery = 0.0f;
    float microMotion = 0.0f;
    if (feelCount > 0)
    {
        const double outRmsRef = (blockEnergyCount > 0)
            ? std::sqrt(blockEnergy / static_cast<double>(blockEnergyCount))
            : std::max(1.0e-6, std::sqrt(matchOutputEnergy
                  / static_cast<double>(std::max(1, matchOutputEnergyCount))));
        const double recoveryRms =
            std::sqrt(feelRecoveryEnergy / static_cast<double>(feelCount));
        const double motionRms = std::sqrt(
            (feelMotionEnergy + 0.25 * feelMotionDeltaEnergy)
            / static_cast<double>(feelCount));
        textureRecovery = juce::jlimit(
            0.0f, 1.0f,
            static_cast<float>(recoveryRms / (outRmsRef * 0.55 + 1.0e-6)));
        microMotion = juce::jlimit(
            0.0f, 1.0f,
            static_cast<float>(motionRms / (outRmsRef * 0.22 + 1.0e-6)));
    }

    float lowLevelHarmonicSlope = 0.0f;
    const auto harmonicSnap = analyzer_.readSnapshot();
    if (harmonicSnap.valid)
    {
        double harmonicEnergy = 0.0;
        for (float dbc : harmonicSnap.harmonicsDbc)
            harmonicEnergy += std::pow(10.0, static_cast<double>(dbc) / 10.0);
        const float harmonicDb = static_cast<float>(
            10.0 * std::log10(std::max(harmonicEnergy, 1.0e-12)));
        const float harmonicNorm = juce::jlimit(
            0.0f, 1.0f, (harmonicDb + 70.0f) / 45.0f);
        const float levelNorm = juce::jlimit(
            0.0f, 1.0f, (recentInputRmsDb_ + 30.0f) / 12.0f);
        lowLevelHarmonicSlope = juce::jlimit(
            0.0f, 1.0f, 0.35f * levelNorm + 0.65f * harmonicNorm);
    }
    textureRecoveryState_.store(textureRecovery, std::memory_order_relaxed);
    microMotionState_.store(microMotion, std::memory_order_relaxed);
    lowLevelHarmonicSlopeState_.store(
        lowLevelHarmonicSlope, std::memory_order_relaxed);
    interactionDriveState_.store(
        juce::jlimit(
            0.0f,
            1.0f,
            static_cast<float>(0.5 * (chainL_.currentInteractionDrive()
                + (stereo ? chainR_.currentInteractionDrive()
                          : chainL_.currentInteractionDrive())))),
        std::memory_order_relaxed);

    if (matchOutputEnergyCount > 0 && ! bypass)
    {
        const double outRms = std::sqrt(matchOutputEnergy
            / static_cast<double>(matchOutputEnergyCount));
        const float outDb = dbFromRms(outRms);
        recentMatchOutputRmsDb_ = (recentMatchOutputRmsDb_ <= -99.0f)
            ? outDb
            : (0.96f * recentMatchOutputRmsDb_ + 0.04f * outDb);
    }

    if (blockEnergyCount > 0)
    {
        const double rms = std::sqrt(blockEnergy
            / static_cast<double>(blockEnergyCount));
        const float db = dbFromRms(rms);
        recentOutputLoudnessDb_ = 0.95f * recentOutputLoudnessDb_ + 0.05f * db;

        auto& activeSlot = slotIsB_ ? slotB_ : slotA_;
        integratedLufsState_.store(
            activeSlot.lufsMeter.lufs(recentOutputLoudnessDb_),
            std::memory_order_relaxed);
    }

    // ─── Mastering: True Peak limiter + TPDF dither ──────────────────────
    // Sync limiter state from params every block (cheap; the only
    // not-instant transition is the ceiling change which the limiter's own
    // smoother handles).  Bypass when TP off OR null-test on — the user
    // wants the limiter out of the way when they're proving the plugin's
    // raw contribution.
    const int tpMode = static_cast<int>(
        *params_.getRawParameterValue(kParamTpMode));
    const bool tpSoft = (tpMode == 1);
    const bool tpBrickwall = (tpMode == 2);
    tpLimiter_.setLookaheadMs(
        params_.getRawParameterValue(kParamTpLookaheadMs)->load());
    const int tpLatency = tpLimiter_.currentLatencyInSamples();
    if (tpLatency != lastTpLatency_)
    {
        lastTpLatency_ = tpLatency;
        const int totalLat = currentLatencyInSamples();
        setLatencySamples(totalLat);
    }
    tpLimiter_.setCeilingDb(
        *params_.getRawParameterValue(kParamTpCeilingDb));
    // The brick-wall limiter is OFF during host bypass: a true bypass
    // must be bit-exact, and a look-ahead limiter would otherwise
    // pre-duck the dry signal in anticipation of the active-period tail
    // still in its delay pipeline (breaking transparency).  The wet
    // path's own safety clamp (in the per-sample loop, applied only
    // while bypassing) bounds the crossfade leak instead.
    tpLimiter_.setBypass((! tpBrickwall) || nullTest || bypass);

    tpLimiter_.process(L, stereo ? R : nullptr, numSamples);

    if (tpSoft && ! nullTest && ! bypass)
    {
        const float ceiling = juce::Decibels::decibelsToGain(
            params_.getRawParameterValue(kParamTpCeilingDb)->load());
        const float safeCeiling = std::max(ceiling, 1.0e-4f);
        for (int n = 0; n < numSamples; ++n)
        {
            L[n] = safeCeiling * std::tanh(L[n] / safeCeiling);
            if (stereo)
                R[n] = safeCeiling * std::tanh(R[n] / safeCeiling);
        }
    }

    // Final output safety soft-clip — engaged ONLY across a transition,
    // never in steady state.  When the brick-wall limiter is toggled OFF
    // (host bypass / TP Off-or-Soft) its look-ahead delay ring drains the
    // PRE-limit mix it was still holding, and an A/B snapshot swap exposes
    // the rebuilding chain's settling spike — either of which, on a hot
    // chain, can momentarily exceed full scale and (worse) step the
    // output.  softSafetyClip bounds those smoothly.  It is gated to a
    // short hold window after any limiter-mode change / graph-rebuild
    // fade / bypass crossfade so steady processing (and the bit-exact
    // limiter-off reference path) is never touched: it is identity below
    // ±1 (full scale) anyway, and the gating then guarantees zero effect
    // on stable output even for a legitimate in-spec peak above 0 dBFS.
    const bool limiterBypassedThisBlock =
        (tpMode != 2) || nullTest || bypass;
    if (limiterBypassedThisBlock != lastLimiterBypassed_
        || graphFadePhase_ != GraphFadePhase::Idle
        || bypassBlendSmooth_.isSmoothing())
    {
        // Hold long enough to cover BOTH the limiter look-ahead ring
        // drain AND a graph-rebuild/A-B-swap settling transient.  Counted
        // in SAMPLES (not blocks) so a small host buffer can't leave part
        // of it unclipped — floored at the max possible lookahead so the
        // floor holds regardless of the current TP latency, plus two
        // blocks of settle margin.  Re-armed every block while a
        // fade/crossfade is still running.
        safetyHoldSamples_ =
            std::max(tpLimiter_.currentLatencyInSamples(),
                     dsp::TruePeakLimiter::kMaxLookaheadSamples)
            + 2 * numSamples;
    }
    lastLimiterBypassed_ = limiterBypassedThisBlock;
    if (safetyHoldSamples_ > 0)
    {
        safetyHoldSamples_ -= numSamples;
        for (int n = 0; n < numSamples; ++n)
        {
            L[n] = softSafetyClip(L[n]);
            if (stereo) R[n] = softSafetyClip(R[n]);
        }
    }

    // Publish gain reduction for the UI meter (0 dB when idle, negative when limiting).
    gainReductionDbState_.store(tpLimiter_.gainReductionDb(),
                                std::memory_order_relaxed);

    // TPDF dither at the absolute final stage.  Per-channel independent
    // noise streams decorrelate the noise floor (3 dB summing at the bus)
    // and avoid implying a phantom-centre image at very quiet passages.
    //
    // Host-visible bypass must remain bit-transparent apart from the reported
    // latency.  Null Test also needs the raw contribution without output-stage
    // noise.  In both modes the dither generator is kept out of the signal.
    const bool ditherEnabled =
        (! bypass)
        && (! nullTest)
        && static_cast<bool>(*params_.getRawParameterValue(kParamDitherEnable));
    if (ditherEnabled)
    {
        const int depthIdx = static_cast<int>(
            *params_.getRawParameterValue(kParamDitherDepth));
        const int targetDepth = (depthIdx == 0) ? 16
                              : (depthIdx == 1) ? 20 : 24;
        const float lsb = std::pow(2.0f, -(static_cast<float>(targetDepth) - 1.0f));

        auto unitNoise = [this]() noexcept
        {
            std::uint64_t x = ditherRng_;
            x ^= x << 13;
            x ^= x >> 7;
            x ^= x << 17;
            ditherRng_ = x;
            // 24 bits → [0, 1)
            return static_cast<float>(x & 0xFFFFFFULL) / 16777216.0f;
        };

        for (int n = 0; n < numSamples; ++n)
        {
            // Triangular = (uniform − uniform) ∈ (−1, +1) on average.  Two
            // independent draws per channel — twice the RNG cost, but the
            // arithmetic is trivial relative to the chain itself.
            const float tpdfL = lsb * (unitNoise() - unitNoise());
            L[n] += tpdfL;
            if (stereo)
            {
                const float tpdfR = lsb * (unitNoise() - unitNoise());
                R[n] += tpdfR;
            }
        }
    }

    // Publish post-master output stats for the mastering HUD.
    double peakAbs = 0.0;
    double truePeakAbs = 0.0;
    double sumL = 0.0, sumR = 0.0, sumLL = 0.0, sumRR = 0.0, sumLR = 0.0;
    for (int n = 0; n < numSamples; ++n)
    {
        const double l = std::isfinite(L[n]) ? static_cast<double>(L[n]) : 0.0;
        peakAbs = std::max(peakAbs, std::abs(l));
        auto upL = masterTpMeterL_.upsample(l);
        for (double v : upL)
            truePeakAbs = std::max(truePeakAbs, std::abs(v));
        if (stereo)
        {
            const double r = std::isfinite(R[n]) ? static_cast<double>(R[n]) : 0.0;
            peakAbs = std::max(peakAbs, std::abs(r));
            auto upR = masterTpMeterR_.upsample(r);
            for (double v : upR)
                truePeakAbs = std::max(truePeakAbs, std::abs(v));
            sumL += l; sumR += r;
            sumLL += l * l; sumRR += r * r;
            sumLR += l * r;
            masterLoudnessMeter_.process(static_cast<float>(l),
                                         static_cast<float>(r),
                                         true);
        }
        else
        {
            masterLoudnessMeter_.process(static_cast<float>(l), 0.0f, false);
        }
    }
    truePeakAbs = std::max(truePeakAbs, peakAbs);
    peakDbfsState_.store(static_cast<float>(
        20.0 * std::log10(std::max(peakAbs, 1.0e-9))),
        std::memory_order_relaxed);
    truePeakDbtpState_.store(static_cast<float>(
        20.0 * std::log10(std::max(truePeakAbs, 1.0e-9))),
        std::memory_order_relaxed);
    momentaryLufsState_.store(masterLoudnessMeter_.momentaryLufs(),
                              std::memory_order_relaxed);
    shortTermLufsState_.store(masterLoudnessMeter_.shortTermLufs(),
                              std::memory_order_relaxed);
    integratedLufsState_.store(masterLoudnessMeter_.integratedLufs(),
                               std::memory_order_relaxed);
    if (stereo)
    {
        const double n = static_cast<double>(numSamples);
        const double cov = sumLR - (sumL * sumR / n);
        const double varL = std::max(0.0, sumLL - (sumL * sumL / n));
        const double varR = std::max(0.0, sumRR - (sumR * sumR / n));
        const double denom = std::sqrt(varL * varR);
        const float corr = (denom > 1.0e-18)
            ? static_cast<float>(juce::jlimit(-1.0, 1.0, cov / denom))
            : 1.0f;
        correlationState_.store(corr, std::memory_order_relaxed);
    }
    else
    {
        correlationState_.store(1.0f, std::memory_order_relaxed);
    }

    // Snapshot the output transformer's JA state (for B-H loop view).
    bhState_[0].store(static_cast<float>(chainL_.outputTrafoH()),
                      std::memory_order_relaxed);
    bhState_[1].store(static_cast<float>(chainL_.outputTrafoM()),
                      std::memory_order_relaxed);
    bhState_[2].store(static_cast<float>(chainL_.outputTrafoMs()),
                      std::memory_order_relaxed);

    // Drift Recorder snapshots — published once per block (much slower than
    // audio rate) so the UI Timer can sample at 30 Hz without contention.
    // Sag is read from the shared rail when active, otherwise from the L
    // chain's internal sag follower; warmup and thermal-drift use the L
    // chain's stage 0 since that's the user-visible "front of the rack".
    const float sagPct = sharedPSUActive_
        ? static_cast<float>(sharedPSU_.sagPercent())
        : static_cast<float>(chainL_.currentSagPercent());
    sagPctState_.store(sagPct, std::memory_order_relaxed);

    if (chainL_.numStages() > 0)
    {
        const auto& s0 = chainL_.stage(0);
        warmupState_.store(
            static_cast<float>(s0.warmupProgress()),
            std::memory_order_relaxed);
        thermalDriftState_.store(
            static_cast<float>(std::abs(s0.thermalBiasShift())),
            std::memory_order_relaxed);
    }
}

void ValvraProcessor::calibrateInputToMinus18()
{
    const float measured = inputRmsDbfsState_.load(std::memory_order_relaxed);
    if (! std::isfinite(measured) || measured <= -99.0f)
        return;

    auto* trim = dynamic_cast<juce::AudioParameterFloat*>(
        params_.getParameter(kParamInputTrimDb));
    if (trim == nullptr)
        return;

    const float current = trim->get();
    const float target = juce::jlimit(-24.0f, 24.0f,
                                      current + (-18.0f - measured));
    *trim = target;
}

void ValvraProcessor::analyzeLevelMatch()
{
    const float inDb = recentInputRmsDb_;
    const float outDb = recentMatchOutputRmsDb_;
    if (! std::isfinite(inDb) || ! std::isfinite(outDb)
        || inDb <= -99.0f || outDb <= -99.0f)
    {
        return;
    }

    auto* analyzed = dynamic_cast<juce::AudioParameterFloat*>(
        params_.getParameter(kParamAnalyzedOutputTrimDb));
    if (analyzed == nullptr)
        return;

    const float trimDb = juce::jlimit(-12.0f, 12.0f, inDb - outDb);
    *analyzed = trimDb;
    if (auto* mode = dynamic_cast<juce::AudioParameterChoice*>(
            params_.getParameter(kParamLevelMatchMode)))
        *mode = 2;
}

// ─────────────────────────────────────────────────────────────────────────────
// State save / load
// ─────────────────────────────────────────────────────────────────────────────
void ValvraProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto& activeSlot = slotIsB_ ? slotB_ : slotA_;
    captureLiveIntoABSlot(activeSlot);

    auto state = params_.copyState();
    stripABStateNode(state);
    const std::uint64_t seed =
        currentSeed_.load(std::memory_order_relaxed);
    state.setProperty("valvra_seed",
                      juce::var(static_cast<juce::int64>(seed)),
                      nullptr);
    state.setProperty("valvra_null_test",
                      juce::var(static_cast<bool>(nullTestMode_.load(std::memory_order_relaxed))),
                      nullptr);
    state.setProperty("valvra_ui_scale",
                      juce::var(static_cast<double>(uiScaleState_.load(std::memory_order_relaxed))),
                      nullptr);
    state.setProperty("valvra_neural_model_path", neuralModelPath_, nullptr);
    for (int i = 0; i < static_cast<int>(kProfileVersionStateKeys.size()); ++i)
    {
        state.setProperty(
            kProfileVersionStateKeys[static_cast<std::size_t>(i)],
            useFittedProfileForPreset(i) ? kFittedProfileVersion
                                         : kLegacyProfileVersion,
            nullptr);
    }
    writeABStateToTree(state);
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
            const bool hasTpModeParam = treeContainsParamId(tree, kParamTpMode);
            const bool hasCalibrationParams =
                treeContainsParamId(tree, kParamLevelMatchMode)
                && treeContainsParamId(tree, kParamInputTrimDb)
                && treeContainsParamId(tree, kParamTargetProfile);
            const bool hasRealismParam =
                treeContainsParamId(tree, kParamRealismAmount);
            bool hasAnyProfileVersion = false;
            for (const auto* key : kProfileVersionStateKeys)
                hasAnyProfileVersion = hasAnyProfileVersion || tree.hasProperty(key);

            if (hasAnyProfileVersion)
            {
                for (int i = 0; i < static_cast<int>(kProfileVersionStateKeys.size()); ++i)
                {
                    const auto* key = kProfileVersionStateKeys[static_cast<std::size_t>(i)];
                    useFittedProfileByMode_[static_cast<std::size_t>(i)] =
                        tree.hasProperty(key)
                        && tree[key].toString() == kFittedProfileVersion;
                }
            }
            else
            {
                // Backward compatibility:
                // 1) very old state: no profile version key at all
                // 2) transition state: v72-only key exists
                const bool v72Fitted =
                    tree.hasProperty(kLegacyV72OnlyKey)
                    && tree[kLegacyV72OnlyKey].toString() == kFittedProfileVersion;
                useFittedProfileByMode_.fill(false);
                useFittedProfileByMode_[0] = v72Fitted;
            }
            params_.replaceState(tree);

            // Legacy migration: pre-tpMode states only had tpEnabled (bool).
            // Convert that one-shot intent into the new mode enum, then clear
            // the legacy bool so TP off/on follows tpMode deterministically.
            if (! hasTpModeParam)
            {
                const bool legacyTpEnabled = static_cast<bool>(
                    *params_.getRawParameterValue(kParamTpEnabled));
                if (auto* tpMode = dynamic_cast<juce::AudioParameterChoice*>(
                        params_.getParameter(kParamTpMode)))
                {
                    *tpMode = legacyTpEnabled ? 2 : 0; // Brick-wall / Off
                }
                if (auto* legacy = dynamic_cast<juce::AudioParameterBool*>(
                        params_.getParameter(kParamTpEnabled)))
                {
                    *legacy = false;
                }
            }

            // Legacy projects predate practical calibration.  Keep their
            // exact sound by loading the new correction layer disabled.
            if (! hasCalibrationParams)
            {
                if (auto* trim = dynamic_cast<juce::AudioParameterFloat*>(
                        params_.getParameter(kParamInputTrimDb)))
                    *trim = 0.0f;
                if (auto* mode = dynamic_cast<juce::AudioParameterChoice*>(
                        params_.getParameter(kParamLevelMatchMode)))
                    *mode = 0; // Off
                if (auto* analyzed = dynamic_cast<juce::AudioParameterFloat*>(
                        params_.getParameter(kParamAnalyzedOutputTrimDb)))
                    *analyzed = 0.0f;
                if (auto* profile = dynamic_cast<juce::AudioParameterChoice*>(
                        params_.getParameter(kParamTargetProfile)))
                    *profile = 0; // Auto
            }
            if (! hasRealismParam)
            {
                if (auto* realism = dynamic_cast<juce::AudioParameterFloat*>(
                        params_.getParameter(kParamRealismAmount)))
                    *realism = 0.0f;
            }

            if (tree.hasProperty("valvra_seed"))
                currentSeed_.store(
                    static_cast<std::uint64_t>(
                        static_cast<juce::int64>(tree["valvra_seed"])),
                    std::memory_order_relaxed);
            if (tree.hasProperty("valvra_null_test"))
            {
                setNullTestMode(static_cast<bool>(tree["valvra_null_test"]));
            }
            if (tree.hasProperty("valvra_ui_scale"))
            {
                setUiScale(static_cast<float>(
                    static_cast<double>(tree["valvra_ui_scale"])));
            }
            if (tree.hasProperty("valvra_neural_model_path"))
            {
                const auto path = tree["valvra_neural_model_path"].toString();
                if (path.isNotEmpty())
                    loadNeuralModelFile(path);
                else
                    unloadNeuralModel();
            }
            restoreABStateFromTree(tree);
            clearHistoryAB();
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
    if (static_cast<bool>(*params_.getRawParameterValue(kParamMcLock)))
        return;

    const std::uint64_t newSeed =
        static_cast<std::uint64_t>(juce::Time::getHighResolutionTicks())
        ^ 0xC3C3C3C3C3C3C3C3ULL;
    pendingSeed_.store(newSeed, std::memory_order_relaxed);
    rerollRequested_.store(true, std::memory_order_relaxed);
    // Reflect immediately in the seed label; the audio thread will actually
    // apply the new value on its next processBlock() tick.
    currentSeed_.store(newSeed, std::memory_order_relaxed);
    pushSeedHistory(newSeed);
}

void ValvraProcessor::recallSeed(std::uint64_t seed) noexcept
{
    if (static_cast<bool>(*params_.getRawParameterValue(kParamMcLock)))
        return;

    // Same handoff path as reroll() but with a caller-supplied seed — used
    // by the Reroll Timeline panel when the user clicks an old entry.  We
    // don't push back into the history (avoids duplicating an entry that's
    // already there); recall is a navigation action, not a new event.
    pendingSeed_.store(seed, std::memory_order_relaxed);
    rerollRequested_.store(true, std::memory_order_relaxed);
    currentSeed_.store(seed, std::memory_order_relaxed);
}

// ─────────────────────────────────────────────────────────────────────────────
// A/B compare — store/restore parameter snapshots between two slots.
//
// Semantics: live state is always held in params_.  toggleAB() saves live
// into the currently-active slot, then loads the OTHER slot into live.
// First toggle from a slot that has never been populated populates it from
// live (so the first A↔B comparison is meaningful: the user dialed in one
// setting on A, hits A/B, B starts identical, they tweak B, now both have
// distinct content for the next comparison).
// ─────────────────────────────────────────────────────────────────────────────
void ValvraProcessor::captureLiveIntoABSlot(ABSlot& slot)
{
    slot.state = params_.copyState();
    stripABStateNode(slot.state);
    slot.seed = currentSeed_.load(std::memory_order_relaxed);
    slot.loudnessDb = slot.lufsMeter.lufs(recentOutputLoudnessDb_);
    slot.populated = true;
}

void ValvraProcessor::copySlot(ABSlot& dst, const ABSlot& src)
{
    dst.state = src.state.isValid() ? src.state.createCopy() : juce::ValueTree();
    dst.seed = src.seed;
    dst.loudnessDb = src.loudnessDb;
    dst.lufsMeter = src.lufsMeter;
    dst.populated = src.populated;
}

void ValvraProcessor::clearSlot(ABSlot& slot)
{
    slot.state = juce::ValueTree();
    slot.seed = 0;
    slot.loudnessDb = -100.0f;
    slot.lufsMeter.reset();
    slot.populated = false;
}

ValvraProcessor::ABSlot& ValvraProcessor::slotFromSnapshot(SnapshotSlot slot)
{
    switch (slot)
    {
        case SnapshotSlot::C: return slotC_;
        case SnapshotSlot::D: return slotD_;
        case SnapshotSlot::E: return slotE_;
    }
    return slotC_;
}

const ValvraProcessor::ABSlot&
ValvraProcessor::slotFromSnapshot(SnapshotSlot slot) const
{
    switch (slot)
    {
        case SnapshotSlot::C: return slotC_;
        case SnapshotSlot::D: return slotD_;
        case SnapshotSlot::E: return slotE_;
    }
    return slotC_;
}

ValvraProcessor::ABHistoryState ValvraProcessor::captureABHistoryState()
{
    ABHistoryState s;
    s.slotA = slotA_;
    s.slotB = slotB_;
    s.slotC = slotC_;
    s.slotD = slotD_;
    s.slotE = slotE_;
    s.liveState = params_.copyState();
    stripABStateNode(s.liveState);
    s.liveSeed = currentSeed_.load(std::memory_order_relaxed);
    s.blindRng = abBlindRng_;
    s.slotIsB = slotIsB_;
    s.blindMode = abBlindMode_;
    return s;
}

bool ValvraProcessor::abHistoryStatesEqual(const ABHistoryState& a,
                                           const ABHistoryState& b) const
{
    auto slotsEqual = [](const ABSlot& x, const ABSlot& y)
    {
        const bool stateEqual =
            (! x.state.isValid() && ! y.state.isValid())
            || (x.state.isValid() && y.state.isValid()
                && x.state.isEquivalentTo(y.state));
        return stateEqual
            && x.seed == y.seed
            && x.populated == y.populated
            && x.loudnessDb == y.loudnessDb;
    };

    const bool liveEqual =
        a.liveState.isValid() && b.liveState.isValid()
            ? a.liveState.isEquivalentTo(b.liveState)
            : (! a.liveState.isValid() && ! b.liveState.isValid());

    return slotsEqual(a.slotA, b.slotA)
        && slotsEqual(a.slotB, b.slotB)
        && slotsEqual(a.slotC, b.slotC)
        && slotsEqual(a.slotD, b.slotD)
        && slotsEqual(a.slotE, b.slotE)
        && liveEqual
        && a.liveSeed == b.liveSeed
        && a.blindRng == b.blindRng
        && a.slotIsB == b.slotIsB
        && a.blindMode == b.blindMode;
}

void ValvraProcessor::applyABHistoryState(const ABHistoryState& s)
{
    copySlot(slotA_, s.slotA);
    copySlot(slotB_, s.slotB);
    copySlot(slotC_, s.slotC);
    copySlot(slotD_, s.slotD);
    copySlot(slotE_, s.slotE);
    slotIsB_ = s.slotIsB;
    abBlindMode_ = s.blindMode;
    abBlindRng_ = s.blindRng;

    if (s.liveState.isValid())
        params_.replaceState(s.liveState);
    currentSeed_.store(s.liveSeed, std::memory_order_relaxed);
    abMatchSmooth_.setTargetValue(1.0f);
    rebuildRequested_.store(true, std::memory_order_relaxed);
}

void ValvraProcessor::pushUndoAB(const ABHistoryState& s)
{
    if (abUndoCount_ < kABHistoryDepth)
    {
        abUndoHistory_[static_cast<std::size_t>(abUndoCount_)] = s;
        ++abUndoCount_;
        return;
    }

    for (int i = 1; i < kABHistoryDepth; ++i)
        abUndoHistory_[static_cast<std::size_t>(i - 1)]
            = abUndoHistory_[static_cast<std::size_t>(i)];
    abUndoHistory_[kABHistoryDepth - 1] = s;
}

void ValvraProcessor::pushRedoAB(const ABHistoryState& s)
{
    if (abRedoCount_ < kABHistoryDepth)
    {
        abRedoHistory_[static_cast<std::size_t>(abRedoCount_)] = s;
        ++abRedoCount_;
        return;
    }

    for (int i = 1; i < kABHistoryDepth; ++i)
        abRedoHistory_[static_cast<std::size_t>(i - 1)]
            = abRedoHistory_[static_cast<std::size_t>(i)];
    abRedoHistory_[kABHistoryDepth - 1] = s;
}

ValvraProcessor::ABHistoryState ValvraProcessor::popUndoAB()
{
    jassert(abUndoCount_ > 0);
    --abUndoCount_;
    return abUndoHistory_[static_cast<std::size_t>(abUndoCount_)];
}

ValvraProcessor::ABHistoryState ValvraProcessor::popRedoAB()
{
    jassert(abRedoCount_ > 0);
    --abRedoCount_;
    return abRedoHistory_[static_cast<std::size_t>(abRedoCount_)];
}

void ValvraProcessor::clearRedoAB() noexcept
{
    abRedoCount_ = 0;
}

void ValvraProcessor::clearHistoryAB() noexcept
{
    abUndoCount_ = 0;
    abRedoCount_ = 0;
}

void ValvraProcessor::performABAction(const std::function<void()>& action)
{
    const auto before = captureABHistoryState();
    action();
    const auto after = captureABHistoryState();

    if (! abHistoryStatesEqual(before, after))
    {
        pushUndoAB(before);
        clearRedoAB();
    }
}

bool ValvraProcessor::canUndoAB() const noexcept
{
    return abUndoCount_ > 0;
}

bool ValvraProcessor::canRedoAB() const noexcept
{
    return abRedoCount_ > 0;
}

void ValvraProcessor::undoAB()
{
    if (! canUndoAB())
        return;

    const auto current = captureABHistoryState();
    pushRedoAB(current);
    applyABHistoryState(popUndoAB());
}

void ValvraProcessor::redoAB()
{
    if (! canRedoAB())
        return;

    const auto current = captureABHistoryState();
    pushUndoAB(current);
    applyABHistoryState(popRedoAB());
}

void ValvraProcessor::writeABStateToTree(juce::ValueTree& tree) const
{
    const juce::Identifier abStateType { kABStateNodeType };
    const juce::Identifier slotType { kABSlotNodeType };
    const juce::Identifier slotNameKey { "slotName" };
    const juce::Identifier populatedKey { "populated" };
    const juce::Identifier seedKey { "seed" };
    const juce::Identifier loudnessKey { "loudnessDb" };
    const juce::Identifier activeKey { "activeSlotIsB" };
    const juce::Identifier blindKey { "blindMode" };
    const juce::Identifier blindRngKey { "blindRng" };

    for (int i = tree.getNumChildren(); --i >= 0; )
    {
        if (tree.getChild(i).hasType(abStateType))
            tree.removeChild(i, nullptr);
    }

    juce::ValueTree abState(abStateType);
    abState.setProperty(activeKey, slotIsB_, nullptr);
    abState.setProperty(blindKey, abBlindMode_, nullptr);
    abState.setProperty(blindRngKey,
                        juce::var(static_cast<juce::int64>(abBlindRng_)),
                        nullptr);

    auto appendSlot = [&](const char* slotName, const ABSlot& slot)
    {
        juce::ValueTree node(slotType);
        node.setProperty(slotNameKey, juce::String(slotName), nullptr);
        node.setProperty(populatedKey, slot.populated, nullptr);
        node.setProperty(seedKey,
                         juce::var(static_cast<juce::int64>(slot.seed)),
                         nullptr);
        node.setProperty(loudnessKey, slot.loudnessDb, nullptr);
        if (slot.populated && slot.state.isValid())
        {
            auto slotState = slot.state.createCopy();
            stripABStateNode(slotState);
            node.addChild(slotState, -1, nullptr);
        }
        abState.addChild(node, -1, nullptr);
    };

    appendSlot(kABSlotNameA, slotA_);
    appendSlot(kABSlotNameB, slotB_);
    appendSlot(kABSlotNameC, slotC_);
    appendSlot(kABSlotNameD, slotD_);
    appendSlot(kABSlotNameE, slotE_);

    tree.addChild(abState, -1, nullptr);
}

void ValvraProcessor::restoreABStateFromTree(const juce::ValueTree& tree)
{
    const juce::Identifier abStateType { kABStateNodeType };
    const juce::Identifier slotType { kABSlotNodeType };
    const juce::Identifier slotNameKey { "slotName" };
    const juce::Identifier populatedKey { "populated" };
    const juce::Identifier seedKey { "seed" };
    const juce::Identifier loudnessKey { "loudnessDb" };
    const juce::Identifier activeKey { "activeSlotIsB" };
    const juce::Identifier blindKey { "blindMode" };
    const juce::Identifier blindRngKey { "blindRng" };

    const auto abState = tree.getChildWithName(abStateType);
    if (! abState.isValid())
    {
        resetAB();
        clearSlot(slotC_);
        clearSlot(slotD_);
        clearSlot(slotE_);
        abBlindMode_ = false;
        return;
    }

    auto restoreSlot = [&](ABSlot& slot, const juce::String& slotName)
    {
        clearSlot(slot);
        for (int i = 0; i < abState.getNumChildren(); ++i)
        {
            auto node = abState.getChild(i);
            if (! node.hasType(slotType))
                continue;
            if (node.getProperty(slotNameKey).toString() != slotName)
                continue;

            const bool populated =
                static_cast<bool>(node.getProperty(populatedKey, false));
            if (! populated)
                return;

            slot.populated = true;
            slot.seed = static_cast<std::uint64_t>(
                static_cast<juce::int64>(node.getProperty(seedKey, 0)));
            slot.loudnessDb = static_cast<float>(
                static_cast<double>(node.getProperty(loudnessKey, -100.0)));
            if (node.getNumChildren() > 0)
            {
                slot.state = node.getChild(0).createCopy();
                stripABStateNode(slot.state);
            }
            if (! slot.state.isValid())
                clearSlot(slot);
            return;
        }
    };

    restoreSlot(slotA_, kABSlotNameA);
    restoreSlot(slotB_, kABSlotNameB);
    restoreSlot(slotC_, kABSlotNameC);
    restoreSlot(slotD_, kABSlotNameD);
    restoreSlot(slotE_, kABSlotNameE);

    if (! slotA_.populated)
        captureLiveIntoABSlot(slotA_);

    slotIsB_ = static_cast<bool>(abState.getProperty(activeKey, false));
    if (slotIsB_ && ! slotB_.populated)
        slotIsB_ = false;
    abBlindMode_ = static_cast<bool>(abState.getProperty(blindKey, false));
    abBlindRng_ = static_cast<std::uint64_t>(
        static_cast<juce::int64>(abState.getProperty(
            blindRngKey,
            juce::var(static_cast<juce::int64>(0x8B5AD4CE9F1A2C67ULL)))));
    if (abBlindRng_ == 0)
        abBlindRng_ = 0x8B5AD4CE9F1A2C67ULL;
}

void ValvraProcessor::copyToInactiveSlot()
{
    performABAction([this]
    {
        const auto& source = slotIsB_ ? slotB_ : slotA_;
        auto& target = slotIsB_ ? slotA_ : slotB_;
        target.state = params_.copyState();
        target.seed = currentSeed_.load(std::memory_order_relaxed);
        target.lufsMeter = source.lufsMeter;
        target.loudnessDb = source.lufsMeter.lufs(recentOutputLoudnessDb_);
        target.populated = true;
    });
}

void ValvraProcessor::setABBlindMode(bool enabled) noexcept
{
    abBlindMode_ = enabled;
    if (enabled)
    {
        const std::uint64_t mix =
            static_cast<std::uint64_t>(juce::Time::getHighResolutionTicks())
            ^ currentSeed_.load(std::memory_order_relaxed)
            ^ 0xD1B54A32D192ED03ULL;
        abBlindRng_ ^= (mix == 0 ? 0x9E3779B97F4A7C15ULL : mix);
        if (abBlindRng_ == 0) abBlindRng_ = 0x8B5AD4CE9F1A2C67ULL;
    }
}

void ValvraProcessor::setABBlindModeWithHistory(bool enabled)
{
    performABAction([this, enabled] { setABBlindMode(enabled); });
}

bool ValvraProcessor::nextBlindSlotIsB() noexcept
{
    std::uint64_t x = abBlindRng_;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    abBlindRng_ = (x == 0) ? 0xA0761D6478BD642FULL : x;
    return (abBlindRng_ & 1ULL) != 0ULL;
}

void ValvraProcessor::switchToABSlot(bool toB)
{
    // 1) Save current live state into the slot we are leaving.
    auto& fromSlot = slotIsB_ ? slotB_ : slotA_;
    fromSlot.state = params_.copyState();
    fromSlot.seed = currentSeed_.load(std::memory_order_relaxed);
    fromSlot.loudnessDb = fromSlot.lufsMeter.lufs(recentOutputLoudnessDb_);
    fromSlot.populated = true;

    // 2) If destination is current slot, nothing to swap.
    if (toB == slotIsB_)
    {
        abMatchSmooth_.setTargetValue(1.0f);
        return;
    }

    // 3) Switch active slot.
    slotIsB_ = toB;
    auto& toSlot = slotIsB_ ? slotB_ : slotA_;

    // 4) If destination was never populated, seed it from source state.
    if (! toSlot.populated)
    {
        toSlot.state = fromSlot.state.createCopy();
        toSlot.seed = fromSlot.seed;
        toSlot.loudnessDb = fromSlot.loudnessDb;
        toSlot.lufsMeter.reset();
        toSlot.populated = true;
        abMatchSmooth_.setTargetValue(1.0f);
        return;
    }

    // 5) Restore destination and loudness-match to source slot.
    toSlot.loudnessDb = toSlot.lufsMeter.lufs(toSlot.loudnessDb);
    const float matchDb = juce::jlimit(
        -12.0f, 12.0f, fromSlot.loudnessDb - toSlot.loudnessDb);
    abMatchSmooth_.setTargetValue(juce::Decibels::decibelsToGain(matchDb));

    params_.replaceState(toSlot.state);
    currentSeed_.store(toSlot.seed, std::memory_order_relaxed);
    rebuildRequested_.store(true, std::memory_order_relaxed);
}

void ValvraProcessor::copyAToB()
{
    performABAction([this]
    {
        auto& activeSlot = slotIsB_ ? slotB_ : slotA_;
        captureLiveIntoABSlot(activeSlot);

        if (! slotA_.populated)
            copySlot(slotA_, activeSlot);
        if (! slotA_.populated)
            return;

        copySlot(slotB_, slotA_);

        if (slotIsB_)
        {
            params_.replaceState(slotB_.state);
            currentSeed_.store(slotB_.seed, std::memory_order_relaxed);
            rebuildRequested_.store(true, std::memory_order_relaxed);
            abMatchSmooth_.setTargetValue(1.0f);
        }
    });
}

void ValvraProcessor::copyBToA()
{
    performABAction([this]
    {
        auto& activeSlot = slotIsB_ ? slotB_ : slotA_;
        captureLiveIntoABSlot(activeSlot);

        if (! slotB_.populated)
            copySlot(slotB_, activeSlot);
        if (! slotB_.populated)
            return;

        copySlot(slotA_, slotB_);

        if (! slotIsB_)
        {
            params_.replaceState(slotA_.state);
            currentSeed_.store(slotA_.seed, std::memory_order_relaxed);
            rebuildRequested_.store(true, std::memory_order_relaxed);
            abMatchSmooth_.setTargetValue(1.0f);
        }
    });
}

void ValvraProcessor::resetAB()
{
    performABAction([this]
    {
        // Keep the live state untouched, but normalize slot bookkeeping so A is
        // the current reference and B is empty.
        slotA_.state = params_.copyState();
        slotA_.seed = currentSeed_.load(std::memory_order_relaxed);
        slotA_.loudnessDb = recentOutputLoudnessDb_;
        slotA_.lufsMeter.reset();
        slotA_.populated = true;

        clearSlot(slotB_);
        clearSlot(slotC_);
        clearSlot(slotD_);
        clearSlot(slotE_);
        slotIsB_ = false;
        abBlindMode_ = false;
        abMatchSmooth_.setTargetValue(1.0f);
    });
}

void ValvraProcessor::toggleABForCompare()
{
    performABAction([this]
    {
        if (! abBlindMode_)
        {
            switchToABSlot(! slotIsB_);
            return;
        }

        bool targetIsB = nextBlindSlotIsB();
        // Ensure the user actually hears a switch on each click in blind mode.
        if (targetIsB == slotIsB_)
            targetIsB = ! targetIsB;
        switchToABSlot(targetIsB);
    });
}

void ValvraProcessor::toggleAB()
{
    performABAction([this] { switchToABSlot(! slotIsB_); });
}

void ValvraProcessor::storeSnapshot(SnapshotSlot slot)
{
    performABAction([this, slot]
    {
        auto& target = slotFromSnapshot(slot);
        captureLiveIntoABSlot(target);
    });
}

bool ValvraProcessor::loadSnapshot(SnapshotSlot slot)
{
    bool loaded = false;
    performABAction([this, slot, &loaded]
    {
        auto& source = slotFromSnapshot(slot);
        if (! source.populated || ! source.state.isValid())
            return;

        auto& activeSlot = slotIsB_ ? slotB_ : slotA_;
        captureLiveIntoABSlot(activeSlot);

        params_.replaceState(source.state);
        currentSeed_.store(source.seed, std::memory_order_relaxed);
        rebuildRequested_.store(true, std::memory_order_relaxed);
        abMatchSmooth_.setTargetValue(1.0f);
        loaded = true;
    });
    return loaded;
}

bool ValvraProcessor::hasSnapshot(SnapshotSlot slot) const noexcept
{
    const auto& s = slotFromSnapshot(slot);
    return s.populated && s.state.isValid();
}

// Append a seed to the bounded history ring.  Called only from the message
// thread (reroll() / loadFactoryPreset() / setStateInformation), so the
// non-atomic count update is sequentially consistent on the writer side.
// The reader (UI Timer in RerollTimelineView) tolerates a torn read because
// each entry is independent and out-of-order recall just shows yesterday's
// seed for one frame.
void ValvraProcessor::pushSeedHistory(std::uint64_t seed) noexcept
{
    int n = seedHistoryCount_.load(std::memory_order_relaxed);

    // Skip duplicates of the most-recent entry — common when factory
    // presets share a seed with the previous reroll, or when the user
    // clicks Reroll twice within one tick.
    if (n > 0 &&
        seedHistory_[static_cast<std::size_t>(n - 1)]
            .load(std::memory_order_relaxed) == seed)
        return;

    if (n < kSeedHistorySize)
    {
        seedHistory_[static_cast<std::size_t>(n)]
            .store(seed, std::memory_order_relaxed);
        seedHistoryCount_.store(n + 1, std::memory_order_relaxed);
        return;
    }

    // Buffer full: shift left by one and append.  At kSeedHistorySize=10
    // this is 9 atomic writes per reroll, well under any audible budget.
    for (int i = 1; i < kSeedHistorySize; ++i)
    {
        const auto v =
            seedHistory_[static_cast<std::size_t>(i)]
                .load(std::memory_order_relaxed);
        seedHistory_[static_cast<std::size_t>(i - 1)]
            .store(v, std::memory_order_relaxed);
    }
    seedHistory_[kSeedHistorySize - 1]
        .store(seed, std::memory_order_relaxed);
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
    if (auto* inputTrim = dynamic_cast<juce::AudioParameterFloat*>(
            params_.getParameter(kParamInputTrimDb)))
        *inputTrim = 0.0f;
    if (auto* match = dynamic_cast<juce::AudioParameterChoice*>(
            params_.getParameter(kParamLevelMatchMode)))
        *match = 1;
    if (auto* analyzed = dynamic_cast<juce::AudioParameterFloat*>(
            params_.getParameter(kParamAnalyzedOutputTrimDb)))
        *analyzed = 0.0f;
    if (auto* profile = dynamic_cast<juce::AudioParameterChoice*>(
            params_.getParameter(kParamTargetProfile)))
        *profile = 0;
    if (auto* realism = dynamic_cast<juce::AudioParameterFloat*>(
            params_.getParameter(kParamRealismAmount)))
    {
        const int mode = static_cast<int>(p.mode);
        const int profile = effectiveTargetProfile(0, mode);
        const auto spec = targetProfileSpec(
            profile,
            useFittedProfileForTargetProfile(profile));
        *realism = spec.recommendedRealism;
    }
    if (auto* nb = dynamic_cast<juce::AudioParameterFloat*>(
            params_.getParameter(kParamNeuralBlend)))
        *nb = 0.0f;
    if (auto* os = dynamic_cast<juce::AudioParameterChoice*>(
            params_.getParameter(kParamOversample)))
        *os = p.oversampleIdx;
    if (auto* cv = dynamic_cast<juce::AudioParameterChoice*>(
            params_.getParameter(kParamCvMode)))
        *cv = p.cvModeIdx;
    if (auto* sc = dynamic_cast<juce::AudioParameterChoice*>(
            params_.getParameter(kParamStageCount)))
        *sc = 0;
    if (auto* it = dynamic_cast<juce::AudioParameterChoice*>(
            params_.getParameter(kParamInputTrafo)))
        *it = 0;
    if (auto* ot = dynamic_cast<juce::AudioParameterChoice*>(
            params_.getParameter(kParamOutputTrafo)))
        *ot = 0;
    for (const auto& s : kStageParams)
    {
        if (auto* tube = dynamic_cast<juce::AudioParameterChoice*>(
                params_.getParameter(s.tube)))
            *tube = 0;
        if (auto* topology = dynamic_cast<juce::AudioParameterChoice*>(
                params_.getParameter(s.topology)))
            *topology = 0;
        if (auto* drive = dynamic_cast<juce::AudioParameterFloat*>(
                params_.getParameter(s.drive)))
            *drive = 0.0f;
        if (auto* bias = dynamic_cast<juce::AudioParameterFloat*>(
                params_.getParameter(s.bias)))
            *bias = 0.0f;
    }

    const bool lockSeed = static_cast<bool>(
        *params_.getRawParameterValue(kParamMcLock));
    if (! lockSeed)
    {
        currentSeed_.store(p.seed, std::memory_order_relaxed);
        pushSeedHistory(p.seed);
    }
    rebuildRequested_.store(true, std::memory_order_relaxed);
}

} // namespace valvra

// ─── JUCE entry point ────────────────────────────────────────────────────────
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new valvra::ValvraProcessor();
}
