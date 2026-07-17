// ─────────────────────────────────────────────────────────────────────────────
// valvra_process — headless command-line audio processor for validation
//
// Reads raw 32-bit float samples from stdin, applies the configured Valvra
// chain, writes processed samples to stdout. Sample rate is passed by flag.
//
// Usage:
//   valvra_process [--preset=v72|rndi] [--drive=1.0] [--seed=0]
//                  [--sr=48000] [--os=4] [--warmup-sec=0.1]
//                  [--transformer=marinair|utc|jensen|lundahl]
//
// Example (Python validation):
//   python -c "import numpy as np; t=np.arange(0,2,1/48000); \
//              x=(0.1*np.sin(2*np.pi*1000*t)).astype(np.float32); \
//              import sys; sys.stdout.buffer.write(x.tobytes())" \
//     | ./valvra_process --preset=v72 --drive=1.0 > out.raw
//
// Reference: docs/22 §A (Dempwolf harmonic targets we aim to reproduce)
// ─────────────────────────────────────────────────────────────────────────────

#include "TubeAmpChain.h"
#include "PolyphaseOversampler.h"
#include "ExpansionRack.h"

// Single-header WAV reader/writer (public domain / MIT-0)
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <iostream>
#include <vector>
#include <cmath>
#include <climits>
#include <array>
#include <algorithm>
#include <limits>

#ifdef _WIN32
  #include <io.h>
  #include <fcntl.h>
#endif

using namespace valvra::dsp;

namespace {

struct CliOptions
{
    std::string preset          { "v72" };
    std::string transformer     { "auto" };
    std::string inputWav        {};   // if set → read WAV instead of stdin
    std::string outputWav       {};   // if set → write WAV instead of stdout
    double      drive           { 1.0 };
    std::uint64_t seed          { 0 };
    double      sampleRate      { 48000.0 };
    int         oversample      { 4 };
    double      warmupSec       { 0.1 };
    std::string expansion       { "off" };
    double      expansionAmount { 0.0 };
    double      expansionMix    { 1.0 };
    double      realism         { 0.0 };
    std::string profileVersion  { "fitted_v1" };
    double      fitDriveScale   { std::numeric_limits<double>::quiet_NaN() };
    double      fitFeedback     { std::numeric_limits<double>::quiet_NaN() };
    double      fitLoading      { std::numeric_limits<double>::quiet_NaN() };
    double      fitInterstageDA { std::numeric_limits<double>::quiet_NaN() };
    double      fitInterstageDATau { std::numeric_limits<double>::quiet_NaN() };
};

void printHelp()
{
    std::fprintf(stderr,
        "valvra_process: apply Valvra to audio, via WAV file or stdin/stdout.\n"
        "Flags:\n"
        "  --input=path.wav       Read WAV file (16/24/32-bit PCM, any sr/channels).\n"
        "                         If omitted: reads raw float32 from stdin.\n"
        "  --output=path.wav      Write WAV file (32-bit float, matches input format).\n"
        "                         If omitted: writes raw float32 to stdout.\n"
        "  --preset=v72|rndi|marshall|cv|hifi   Chain mode preset (default: v72)\n"
        "  --transformer=marinair|utc|jensen|lundahl (default: preset default)\n"
        "  --drive=1.0             Input gain multiplier\n"
        "  --expansion=off|opto|fet|tape|synth  Tier4+ expansion engine\n"
        "  --expansion-amount=0..1  Expansion intensity\n"
        "  --expansion-mix=0..1     Expansion wet/dry mix\n"
        "  --realism=0..1           Analog surrounding-circuit realism\n"
        "  --profile-version=legacy|fitted_v1   Per-preset profile table (default: fitted_v1)\n"
        "  --v72-profile=legacy|fitted_v1       Backward-compatible alias of --profile-version\n"
        "  --fit-drive-scale=X      Override drive scale (fit harness)\n"
        "  --fit-feedback=X         Override realism feedback coeff (fit harness)\n"
        "  --fit-loading=X          Override realism transformer loading (fit harness)\n"
        "  --fit-da=X               Override realism interstage DA amount (fit harness)\n"
        "  --fit-da-tau=X           Override realism interstage DA tau [s] (fit harness)\n"
        "  --seed=N                Monte Carlo seed (default 0)\n"
        "  --sr=48000              Sample rate (used only for stdin mode)\n"
        "  --os=4                  Oversampling factor (1, 2, 4, 8, 16)\n"
        "  --warmup-sec=0.1        Seconds of silence to prime before reading input\n");
}

bool parseDoubleStrict(const std::string& key, const std::string& val,
                       double& out, std::string& err)
{
    if (val.empty())
    {
        err = "missing value for " + key + " (use " + key + "=VALUE)";
        return false;
    }
    try
    {
        std::size_t idx = 0;
        const double parsed = std::stod(val, &idx);
        if (idx != val.size())
        {
            err = "invalid numeric value for " + key + ": '" + val + "'";
            return false;
        }
        out = parsed;
        return true;
    }
    catch (const std::exception&)
    {
        err = "invalid numeric value for " + key + ": '" + val + "'";
        return false;
    }
}

bool parseUInt64Strict(const std::string& key, const std::string& val,
                       std::uint64_t& out, std::string& err)
{
    if (val.empty())
    {
        err = "missing value for " + key + " (use " + key + "=VALUE)";
        return false;
    }
    try
    {
        std::size_t idx = 0;
        const unsigned long long parsed = std::stoull(val, &idx);
        if (idx != val.size())
        {
            err = "invalid integer value for " + key + ": '" + val + "'";
            return false;
        }
        out = static_cast<std::uint64_t>(parsed);
        return true;
    }
    catch (const std::exception&)
    {
        err = "invalid integer value for " + key + ": '" + val + "'";
        return false;
    }
}

bool parseIntStrict(const std::string& key, const std::string& val,
                    int& out, std::string& err)
{
    if (val.empty())
    {
        err = "missing value for " + key + " (use " + key + "=VALUE)";
        return false;
    }
    try
    {
        std::size_t idx = 0;
        const long parsed = std::stol(val, &idx);
        if (idx != val.size() || parsed < INT_MIN || parsed > INT_MAX)
        {
            err = "invalid integer value for " + key + ": '" + val + "'";
            return false;
        }
        out = static_cast<int>(parsed);
        return true;
    }
    catch (const std::exception&)
    {
        err = "invalid integer value for " + key + ": '" + val + "'";
        return false;
    }
}

bool isSupportedOversample(int os) noexcept
{
    return os == 1 || os == 2 || os == 4 || os == 8 || os == 16;
}

bool validateCommonOptions(const CliOptions& o, std::string& err)
{
    if (! std::isfinite(o.drive))
    {
        err = "--drive must be finite";
        return false;
    }
    if (! std::isfinite(o.warmupSec) || o.warmupSec < 0.0)
    {
        err = "--warmup-sec must be finite and >= 0";
        return false;
    }
    if (! isSupportedOversample(o.oversample))
    {
        err = "--os must be one of 1, 2, 4, 8, 16";
        return false;
    }
    if (! std::isfinite(o.expansionAmount)
        || o.expansionAmount < 0.0 || o.expansionAmount > 1.0)
    {
        err = "--expansion-amount must be finite and within [0, 1]";
        return false;
    }
    if (! std::isfinite(o.expansionMix)
        || o.expansionMix < 0.0 || o.expansionMix > 1.0)
    {
        err = "--expansion-mix must be finite and within [0, 1]";
        return false;
    }
    if (! std::isfinite(o.realism) || o.realism < 0.0 || o.realism > 1.0)
    {
        err = "--realism must be finite and within [0, 1]";
        return false;
    }
    if (std::isfinite(o.fitDriveScale) && o.fitDriveScale <= 0.0)
    {
        err = "--fit-drive-scale must be > 0";
        return false;
    }
    if (std::isfinite(o.fitFeedback)
        && (o.fitFeedback < 0.0 || o.fitFeedback > 0.5))
    {
        err = "--fit-feedback must be within [0, 0.5]";
        return false;
    }
    if (std::isfinite(o.fitLoading)
        && (o.fitLoading < 0.0 || o.fitLoading > 1.0))
    {
        err = "--fit-loading must be within [0, 1]";
        return false;
    }
    if (std::isfinite(o.fitInterstageDA)
        && (o.fitInterstageDA < 0.0 || o.fitInterstageDA > 0.2))
    {
        err = "--fit-da must be within [0, 0.2]";
        return false;
    }
    if (std::isfinite(o.fitInterstageDATau)
        && (o.fitInterstageDATau < 0.05 || o.fitInterstageDATau > 2.0))
    {
        err = "--fit-da-tau must be within [0.05, 2.0]";
        return false;
    }
    return true;
}

bool validateStdinSampleRate(const CliOptions& o, std::string& err)
{
    if (! std::isfinite(o.sampleRate) || o.sampleRate <= 0.0)
    {
        err = "--sr must be finite and > 0";
        return false;
    }
    const double internalSR =
        o.sampleRate * static_cast<double>(o.oversample);
    if (! std::isfinite(internalSR) || internalSR <= 0.0)
    {
        err = "internal sample rate is invalid (sr * os)";
        return false;
    }
    return true;
}

bool validateWavMetadata(unsigned int channels, unsigned int sampleRate,
                         int oversample, std::string& err)
{
    if (channels == 0)
    {
        err = "input WAV has zero channels";
        return false;
    }
    if (sampleRate == 0)
    {
        err = "input WAV has invalid sample rate 0";
        return false;
    }
    const double internalSR =
        static_cast<double>(sampleRate) * static_cast<double>(oversample);
    if (! std::isfinite(internalSR) || internalSR <= 0.0)
    {
        err = "internal sample rate is invalid (input_sr * os)";
        return false;
    }
    return true;
}

bool warmupSamplesFrom(double warmupSec, double sampleRate,
                       int& out, std::string& err)
{
    const double warmup = warmupSec * sampleRate;
    if (! std::isfinite(warmup) || warmup < 0.0 ||
        warmup > static_cast<double>(INT_MAX))
    {
        err = "warmup sample count is out of range";
        return false;
    }
    out = static_cast<int>(warmup);
    return true;
}

bool parseArgs(int argc, char** argv, CliOptions& o, int& exitCode)
{
    std::string err;
    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        auto eq = a.find('=');
        std::string key, val;
        if (eq != std::string::npos) { key = a.substr(0, eq); val = a.substr(eq + 1); }
        else                         { key = a; }

        if      (key == "--preset")       o.preset = val;
        else if (key == "--transformer")  o.transformer = val;
        else if (key == "--input")        o.inputWav  = val;
        else if (key == "--output")       o.outputWav = val;
        else if (key == "--drive")
        {
            if (! parseDoubleStrict(key, val, o.drive, err))
            {
                std::fprintf(stderr, "error: %s\n", err.c_str());
                exitCode = 1;
                return false;
            }
        }
        else if (key == "--expansion")
        {
            o.expansion = val;
        }
        else if (key == "--expansion-amount")
        {
            if (! parseDoubleStrict(key, val, o.expansionAmount, err))
            {
                std::fprintf(stderr, "error: %s\n", err.c_str());
                exitCode = 1;
                return false;
            }
        }
        else if (key == "--expansion-mix")
        {
            if (! parseDoubleStrict(key, val, o.expansionMix, err))
            {
                std::fprintf(stderr, "error: %s\n", err.c_str());
                exitCode = 1;
                return false;
            }
        }
        else if (key == "--realism")
        {
            if (! parseDoubleStrict(key, val, o.realism, err))
            {
                std::fprintf(stderr, "error: %s\n", err.c_str());
                exitCode = 1;
                return false;
            }
        }
        else if (key == "--profile-version" || key == "--v72-profile")
        {
            o.profileVersion = val;
        }
        else if (key == "--fit-drive-scale")
        {
            if (! parseDoubleStrict(key, val, o.fitDriveScale, err))
            {
                std::fprintf(stderr, "error: %s\n", err.c_str());
                exitCode = 1;
                return false;
            }
        }
        else if (key == "--fit-feedback")
        {
            if (! parseDoubleStrict(key, val, o.fitFeedback, err))
            {
                std::fprintf(stderr, "error: %s\n", err.c_str());
                exitCode = 1;
                return false;
            }
        }
        else if (key == "--fit-loading")
        {
            if (! parseDoubleStrict(key, val, o.fitLoading, err))
            {
                std::fprintf(stderr, "error: %s\n", err.c_str());
                exitCode = 1;
                return false;
            }
        }
        else if (key == "--fit-da")
        {
            if (! parseDoubleStrict(key, val, o.fitInterstageDA, err))
            {
                std::fprintf(stderr, "error: %s\n", err.c_str());
                exitCode = 1;
                return false;
            }
        }
        else if (key == "--fit-da-tau")
        {
            if (! parseDoubleStrict(key, val, o.fitInterstageDATau, err))
            {
                std::fprintf(stderr, "error: %s\n", err.c_str());
                exitCode = 1;
                return false;
            }
        }
        else if (key == "--seed")
        {
            if (! parseUInt64Strict(key, val, o.seed, err))
            {
                std::fprintf(stderr, "error: %s\n", err.c_str());
                exitCode = 1;
                return false;
            }
        }
        else if (key == "--sr")
        {
            if (! parseDoubleStrict(key, val, o.sampleRate, err))
            {
                std::fprintf(stderr, "error: %s\n", err.c_str());
                exitCode = 1;
                return false;
            }
        }
        else if (key == "--os")
        {
            if (! parseIntStrict(key, val, o.oversample, err))
            {
                std::fprintf(stderr, "error: %s\n", err.c_str());
                exitCode = 1;
                return false;
            }
        }
        else if (key == "--warmup-sec")
        {
            if (! parseDoubleStrict(key, val, o.warmupSec, err))
            {
                std::fprintf(stderr, "error: %s\n", err.c_str());
                exitCode = 1;
                return false;
            }
        }
        else if (key == "--help" || key == "-h")
        {
            printHelp();
            exitCode = 0;
            return false;
        }
        else
        {
            std::fprintf(stderr, "error: unknown option '%s'\n", key.c_str());
            exitCode = 1;
            return false;
        }
    }

    if (! validateCommonOptions(o, err))
    {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        exitCode = 1;
        return false;
    }

    const auto isOneOf = [](const std::string& v, const auto& allowed) noexcept
    {
        for (const auto* a : allowed)
            if (v == a) return true;
        return false;
    };

    static constexpr std::array<const char*, 7> kPresets {
        "v72", "rndi", "marshall", "cv", "vulture", "hifi", "300b"
    };
    if (! isOneOf(o.preset, kPresets))
    {
        std::fprintf(stderr,
                     "error: --preset must be one of v72|rndi|marshall|cv|hifi\n");
        exitCode = 1;
        return false;
    }

    static constexpr std::array<const char*, 5> kTransformers {
        "auto", "marinair", "utc", "jensen", "lundahl"
    };
    if (! isOneOf(o.transformer, kTransformers))
    {
        std::fprintf(stderr,
                     "error: --transformer must be one of auto|marinair|utc|jensen|lundahl\n");
        exitCode = 1;
        return false;
    }

    static constexpr std::array<const char*, 7> kExpansions {
        "off", "opto", "la2a", "fet", "1176", "tape", "synth"
    };
    if (! isOneOf(o.expansion, kExpansions) && o.expansion != "fx")
    {
        std::fprintf(stderr,
                     "error: --expansion must be one of off|opto|fet|tape|synth\n");
        exitCode = 1;
        return false;
    }

    static constexpr std::array<const char*, 2> kProfileVersions {
        "legacy", "fitted_v1"
    };
    if (! isOneOf(o.profileVersion, kProfileVersions))
    {
        std::fprintf(stderr,
                     "error: --profile-version must be one of legacy|fitted_v1\n");
        exitCode = 1;
        return false;
    }

    return true;
}

double modeDriveScaleForPreset(const std::string& preset) noexcept
{
    if (preset == "marshall") return 1.6;
    if (preset == "cv" || preset == "vulture") return 1.9;
    if (preset == "rndi") return 1.8;
    if (preset == "hifi" || preset == "300b") return 1.3;
    return 1.0; // v72
}

double mapRealismControl(double userValue) noexcept
{
    const double x = std::clamp(userValue, 0.0, 1.0);
    if (x <= 0.30)
    {
        const double t = x / 0.30;
        return 0.20 * t * t;
    }
    if (x <= 0.65)
    {
        const double t = (x - 0.30) / 0.35;
        return 0.20 + 0.55 * std::pow(t, 0.85);
    }
    const double t = (x - 0.65) / 0.35;
    return 0.75 + 0.25 * std::pow(t, 1.35);
}

struct CliRealismProfile
{
    double feedbackAmount;
    double transformerLoading;
    double interstageDA;
    double interstageDATau;
    FeedbackVoicing feedbackVoicing;
};

CliRealismProfile realismProfileForPreset(const CliOptions& o) noexcept
{
    const auto& preset = o.preset;
    const bool fitted = (o.profileVersion == "fitted_v1");
    if (preset == "marshall")
        return fitted ? CliRealismProfile { 0.105, 0.58, 0.045, 0.38, FeedbackVoicing::Controlled }
                      : CliRealismProfile { 0.10, 0.55, 0.040, 0.38, FeedbackVoicing::Controlled };
    if (preset == "cv" || preset == "vulture")
        return fitted ? CliRealismProfile { 0.035, 0.73, 0.090, 0.55, FeedbackVoicing::LowFeedback }
                      : CliRealismProfile { 0.03, 0.70, 0.085, 0.55, FeedbackVoicing::LowFeedback };
    if (preset == "rndi")
        return fitted ? CliRealismProfile { 0.022, 0.44, 0.028, 0.38, FeedbackVoicing::IronDamping }
                      : CliRealismProfile { 0.02, 0.42, 0.025, 0.38, FeedbackVoicing::IronDamping };
    if (preset == "hifi" || preset == "300b")
        return fitted ? CliRealismProfile { 0.145, 0.33, 0.022, 0.25, FeedbackVoicing::Controlled }
                      : CliRealismProfile { 0.14, 0.30, 0.020, 0.25, FeedbackVoicing::Controlled };
    return fitted ? CliRealismProfile { 0.115, 0.52, 0.060, 0.38, FeedbackVoicing::Controlled }
                  : CliRealismProfile { 0.12, 0.48, 0.055, 0.38, FeedbackVoicing::Controlled };
}

void applyRealism(TubeAmpChainConfig& cfg, const CliOptions& o) noexcept
{
    const double realism = mapRealismControl(o.realism);
    auto p = realismProfileForPreset(o);
    if (std::isfinite(o.fitFeedback))
        p.feedbackAmount = o.fitFeedback;
    if (std::isfinite(o.fitLoading))
        p.transformerLoading = o.fitLoading;
    if (std::isfinite(o.fitInterstageDA))
        p.interstageDA = o.fitInterstageDA;
    if (std::isfinite(o.fitInterstageDATau))
        p.interstageDATau = o.fitInterstageDATau;

    cfg.realismAmount = realism;
    cfg.feedbackAmount = realism * p.feedbackAmount;
    cfg.transformerLoading = realism * p.transformerLoading;
    cfg.interstageDAAmount = realism * p.interstageDA;
    cfg.interstageDATau = p.interstageDATau;
    cfg.feedbackVoicing = p.feedbackVoicing;
}

TubeAmpChainConfig buildConfig(const CliOptions& o)
{
    TubeAmpChainConfig cfg;
    if      (o.preset == "rndi")     cfg = chain_presets::RNDIMode();
    else if (o.preset == "marshall") cfg = chain_presets::MarshallMode();
    else if (o.preset == "hifi" || o.preset == "300b")
                                     cfg = chain_presets::HiFi300BMode();
    else if (o.preset == "cv" ||
             o.preset == "vulture")  cfg = chain_presets::CultureVultureMode();
    else                              cfg = chain_presets::V72Preamp();

    // Apply preset-aware drive mapping to all stages uniformly.
    const double driveScale = std::isfinite(o.fitDriveScale)
        ? o.fitDriveScale
        : modeDriveScaleForPreset(o.preset);
    const double effDrive = o.drive * driveScale;
    for (int i = 0; i < cfg.numStages; ++i)
        cfg.stages[i].inputVoltageSwing *= effDrive;

    // Transformer override — swap the IRON, keep the chain's calibration
    // (drive/H_scale/outputGain set the core excursion and insertion trim
    // for this node's level; H_dc carries the circuit's standing
    // magnetization).  Wholesale preset replacement was measured to jump
    // levels by >10 dB and over-excite the core ~8x.
    auto setTrafo = [](TransformerStageConfig& t, const std::string& name){
        auto stock = t;
        if      (name == "marinair") stock = transformer_presets::Marinair();
        else if (name == "utc")      stock = transformer_presets::UTC_A12();
        else if (name == "jensen")   stock = transformer_presets::JensenJT11();
        else if (name == "lundahl")  stock = transformer_presets::Lundahl();
        else return;
        stock.drive      = t.drive;
        stock.H_scale    = t.H_scale;
        stock.outputGain = t.outputGain;
        stock.H_dc       = t.H_dc;
        t = stock;
    };
    if (o.transformer != "auto")
    {
        if (cfg.useInputTransformer)  setTrafo(cfg.inputTrafoConfig,  o.transformer);
        if (cfg.useOutputTransformer) setTrafo(cfg.outputTrafoConfig, o.transformer);
    }

    cfg.variationSeed = o.seed;
    applyRealism(cfg, o);
    return cfg;
}

// ─────────────────────────────────────────────────────────────────────────────
// Per-sample processing through an optional oversampler.  Templated on the
// factor so the inner loop inlines fully.
// ─────────────────────────────────────────────────────────────────────────────
template <int Factor>
static float processOneSampleOS(TubeAmpChain& chain,
                                PolyphaseOversampler<Factor>& os,
                                float inSample) noexcept
{
    auto up = os.upsample(static_cast<double>(inSample));
    for (auto& v : up) v = chain.process(v);
    return static_cast<float>(os.downsample(up));
}

static float processOneSampleNoOS(TubeAmpChain& chain, float inSample) noexcept
{
    return static_cast<float>(chain.process(static_cast<double>(inSample)));
}

static ExpansionMode parseExpansionMode(const std::string& m) noexcept
{
    if (m == "opto" || m == "la2a") return ExpansionMode::OptoComp;
    if (m == "fet" || m == "1176")  return ExpansionMode::FetComp;
    if (m == "tape")                return ExpansionMode::TapeSat;
    if (m == "synth" || m == "fx")  return ExpansionMode::SynthFx;
    return ExpansionMode::Off;
}

// ─────────────────────────────────────────────────────────────────────────────
// stdin → stdout loop (raw float32 mono).
// ─────────────────────────────────────────────────────────────────────────────
template <int Factor>
void runStdinStdoutOS(TubeAmpChain& chain,
                      ExpansionRack& expansion,
                      int warmupSamples)
{
    PolyphaseOversampler<Factor> os;
    for (int i = 0; i < warmupSamples; ++i)
        (void)processOneSampleOS(chain, os, 0.0f);

    float inSample = 0.0f;
    while (std::fread(&inSample, sizeof(float), 1, stdin) == 1)
    {
        double wet = processOneSampleOS(chain, os, inSample);
        expansion.processMono(wet, wet);
        const float out = static_cast<float>(wet);
        std::fwrite(&out, sizeof(float), 1, stdout);
    }
    std::fflush(stdout);
}

void runStdinStdoutNoOS(TubeAmpChain& chain,
                        ExpansionRack& expansion,
                        int warmupSamples)
{
    for (int i = 0; i < warmupSamples; ++i)
        (void)processOneSampleNoOS(chain, 0.0f);

    float inSample = 0.0f;
    while (std::fread(&inSample, sizeof(float), 1, stdin) == 1)
    {
        double wet = processOneSampleNoOS(chain, inSample);
        expansion.processMono(wet, wet);
        const float out = static_cast<float>(wet);
        std::fwrite(&out, sizeof(float), 1, stdout);
    }
    std::fflush(stdout);
}

// ─────────────────────────────────────────────────────────────────────────────
// WAV file → WAV file processing (arbitrary input format, 32-bit float output).
// Preserves sample rate.  Each input channel is processed independently through
// its own chain instance so stereo Monte Carlo remains intact.
// ─────────────────────────────────────────────────────────────────────────────
int runWavFile(const CliOptions& opts)
{
    drwav inWav;
    if (! drwav_init_file(&inWav, opts.inputWav.c_str(), nullptr))
    {
        std::fprintf(stderr, "error: cannot open %s\n", opts.inputWav.c_str());
        return 1;
    }
    const unsigned int ch      = inWav.channels;
    const unsigned int inSR    = inWav.sampleRate;
    const drwav_uint64 nFrames = inWav.totalPCMFrameCount;
    std::string err;
    if (! validateWavMetadata(ch, inSR, opts.oversample, err))
    {
        drwav_uninit(&inWav);
        std::fprintf(stderr, "error: %s\n", err.c_str());
        return 1;
    }

    std::fprintf(stderr, "input: %s (%u ch, %u Hz, %llu frames)\n",
                 opts.inputWav.c_str(), ch, inSR,
                 static_cast<unsigned long long>(nFrames));

    // Read all frames as interleaved float32
    std::vector<float> interleaved(
        static_cast<std::size_t>(nFrames) * ch, 0.0f);
    const drwav_uint64 readFrames =
        drwav_read_pcm_frames_f32(&inWav, nFrames, interleaved.data());
    drwav_uninit(&inWav);
    if (readFrames == 0)
    {
        std::fprintf(stderr, "error: no frames read from WAV\n");
        return 1;
    }

    // Build one chain per channel with different seeds (stereo Monte Carlo)
    constexpr std::uint64_t kStereoSalt = 0x123456789ABCDEFULL;
    std::vector<TubeAmpChain> chains(ch);
    const double internalSR =
        static_cast<double>(inSR) * static_cast<double>(opts.oversample);
    for (unsigned int c = 0; c < ch; ++c)
    {
        auto cfg = buildConfig(opts);
        cfg.variationSeed =
            opts.seed ^ (c == 0 ? 0ULL : kStereoSalt);
        chains[c].setup(cfg, internalSR);
    }
    std::vector<ExpansionRack> expansions(ch);
    for (auto& e : expansions)
    {
        e.prepare(static_cast<double>(inSR));
        e.setMode(parseExpansionMode(opts.expansion));
        e.setAmount(opts.expansionAmount);
        e.setMix(opts.expansionMix);
        // Same per-instance lottery as the plugin (PluginProcessor.cpp)
        // so a CLI render with --seed=N matches the plugin instance, and
        // the readiness/feel artifacts certify varied units, not just
        // the nominal one.
        const auto vx = makeVariation(
            opts.seed ^ 0xD6E8FEB86659FD93ULL,
            VariationDistribution::Modern);
        e.setVariation(vx.tapeCoreK_scale, vx.tapeWow_scale,
                       vx.optoMemoryTau_scale);
    }

    int warmup = 0;
    if (! warmupSamplesFrom(opts.warmupSec, static_cast<double>(inSR), warmup, err))
    {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        return 1;
    }

    // Process each channel independently through its own chain + oversampler.
    std::vector<float> outBuf(
        static_cast<std::size_t>(nFrames) * ch, 0.0f);
    for (unsigned int c = 0; c < ch; ++c)
    {
        auto& ch_chain = chains[c];
        switch (opts.oversample)
        {
            case 1:
                for (int i = 0; i < warmup; ++i)
                    (void)processOneSampleNoOS(ch_chain, 0.0f);
                for (drwav_uint64 f = 0; f < readFrames; ++f)
                {
                    double wet = processOneSampleNoOS(
                        ch_chain, interleaved[f * ch + c]);
                    expansions[c].processMono(wet, wet);
                    outBuf[f * ch + c] = static_cast<float>(wet);
                }
                break;
            case 2: {
                PolyphaseOversampler<2> os;
                for (int i = 0; i < warmup; ++i)
                    (void)processOneSampleOS(ch_chain, os, 0.0f);
                for (drwav_uint64 f = 0; f < readFrames; ++f)
                {
                    double wet = processOneSampleOS(
                        ch_chain, os, interleaved[f * ch + c]);
                    expansions[c].processMono(wet, wet);
                    outBuf[f * ch + c] = static_cast<float>(wet);
                }
                break;
            }
            case 8: {
                PolyphaseOversampler<8> os;
                for (int i = 0; i < warmup; ++i)
                    (void)processOneSampleOS(ch_chain, os, 0.0f);
                for (drwav_uint64 f = 0; f < readFrames; ++f)
                {
                    double wet = processOneSampleOS(
                        ch_chain, os, interleaved[f * ch + c]);
                    expansions[c].processMono(wet, wet);
                    outBuf[f * ch + c] = static_cast<float>(wet);
                }
                break;
            }
            case 16: {
                PolyphaseOversampler<16> os;
                for (int i = 0; i < warmup; ++i)
                    (void)processOneSampleOS(ch_chain, os, 0.0f);
                for (drwav_uint64 f = 0; f < readFrames; ++f)
                {
                    double wet = processOneSampleOS(
                        ch_chain, os, interleaved[f * ch + c]);
                    expansions[c].processMono(wet, wet);
                    outBuf[f * ch + c] = static_cast<float>(wet);
                }
                break;
            }
            case 4:
            default: {
                PolyphaseOversampler<4> os;
                for (int i = 0; i < warmup; ++i)
                    (void)processOneSampleOS(ch_chain, os, 0.0f);
                for (drwav_uint64 f = 0; f < readFrames; ++f)
                {
                    double wet = processOneSampleOS(
                        ch_chain, os, interleaved[f * ch + c]);
                    expansions[c].processMono(wet, wet);
                    outBuf[f * ch + c] = static_cast<float>(wet);
                }
                break;
            }
        }
    }

    // Write output: 32-bit float WAV, same sr/ch as input
    drwav_data_format fmt;
    fmt.container     = drwav_container_riff;
    fmt.format        = DR_WAVE_FORMAT_IEEE_FLOAT;
    fmt.channels      = ch;
    fmt.sampleRate    = inSR;
    fmt.bitsPerSample = 32;

    drwav outWav;
    if (! drwav_init_file_write(&outWav, opts.outputWav.c_str(), &fmt, nullptr))
    {
        std::fprintf(stderr, "error: cannot write %s\n", opts.outputWav.c_str());
        return 1;
    }
    drwav_write_pcm_frames(&outWav, readFrames, outBuf.data());
    drwav_uninit(&outWav);

    std::fprintf(stderr, "wrote: %s (%llu frames)\n", opts.outputWav.c_str(),
                 static_cast<unsigned long long>(readFrames));
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
#ifdef _WIN32
    // Switch stdio to binary mode on Windows so \r\n doesn't mangle the stream
    _setmode(_fileno(stdin),  _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    CliOptions opts;
    int parseExitCode = 0;
    if (! parseArgs(argc, argv, opts, parseExitCode))
        return parseExitCode;

    // Inform the user on stderr so stdout remains pure audio
    std::fprintf(stderr,
        "valvra_process: preset=%s drive=%.2f seed=%llu sr=%.0f os=%dx exp=%s amt=%.2f mix=%.2f realism=%.2f\n",
        opts.preset.c_str(), opts.drive,
        static_cast<unsigned long long>(opts.seed),
        opts.sampleRate, opts.oversample,
        opts.expansion.c_str(), opts.expansionAmount, opts.expansionMix,
        opts.realism);

    // WAV file I/O takes precedence over stdin/stdout.
    if (! opts.inputWav.empty() && ! opts.outputWav.empty())
        return runWavFile(opts);
    if (! opts.inputWav.empty() || ! opts.outputWav.empty())
    {
        std::fprintf(stderr,
                     "error: --input and --output must both be specified for "
                     "WAV mode.\n");
        return 1;
    }

    // Fall-through: stdin/stdout raw float32 mode.
    std::string err;
    if (! validateStdinSampleRate(opts, err))
    {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        return 1;
    }

    TubeAmpChain chain;
    const auto cfg = buildConfig(opts);
    const double internalSR = opts.sampleRate * static_cast<double>(opts.oversample);
    chain.setup(cfg, internalSR);
    ExpansionRack expansion;
    expansion.prepare(opts.sampleRate);
    expansion.setMode(parseExpansionMode(opts.expansion));
    expansion.setAmount(opts.expansionAmount);
    expansion.setMix(opts.expansionMix);
    {
        // Per-instance lottery, matching the plugin (see WAV path above).
        const auto vx = makeVariation(
            opts.seed ^ 0xD6E8FEB86659FD93ULL,
            VariationDistribution::Modern);
        expansion.setVariation(vx.tapeCoreK_scale, vx.tapeWow_scale,
                               vx.optoMemoryTau_scale);
    }

    int warmupSamples = 0;
    if (! warmupSamplesFrom(opts.warmupSec, opts.sampleRate, warmupSamples, err))
    {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        return 1;
    }

    switch (opts.oversample)
    {
        case 1: runStdinStdoutNoOS(chain, expansion, warmupSamples); break;
        case 2: runStdinStdoutOS<2>(chain, expansion, warmupSamples); break;
        case 4: runStdinStdoutOS<4>(chain, expansion, warmupSamples); break;
        case 8: runStdinStdoutOS<8>(chain, expansion, warmupSamples); break;
        case 16: runStdinStdoutOS<16>(chain, expansion, warmupSamples); break;
        default:
            std::fprintf(stderr, "Unsupported oversample factor %d\n", opts.oversample);
            return 1;
    }

    return 0;
}
