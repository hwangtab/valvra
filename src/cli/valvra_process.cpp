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
        "  --preset=v72|rndi|marshall|cv   Chain mode preset (default: v72)\n"
        "  --transformer=marinair|utc|jensen|lundahl (default: preset default)\n"
        "  --drive=1.0             Input gain multiplier\n"
        "  --seed=N                Monte Carlo seed (default 0)\n"
        "  --sr=48000              Sample rate (used only for stdin mode)\n"
        "  --os=4                  Oversampling factor (1, 2, 4, 8)\n"
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
    return os == 1 || os == 2 || os == 4 || os == 8;
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
        err = "--os must be one of 1, 2, 4, 8";
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
    }

    if (! validateCommonOptions(o, err))
    {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        exitCode = 1;
        return false;
    }
    return true;
}

TubeAmpChainConfig buildConfig(const CliOptions& o)
{
    TubeAmpChainConfig cfg;
    if      (o.preset == "rndi")     cfg = chain_presets::RNDIMode();
    else if (o.preset == "marshall") cfg = chain_presets::MarshallMode();
    else if (o.preset == "cv" ||
             o.preset == "vulture")  cfg = chain_presets::CultureVultureMode();
    else                              cfg = chain_presets::V72Preamp();

    // Apply drive to all stages uniformly
    for (int i = 0; i < cfg.numStages; ++i)
        cfg.stages[i].inputVoltageSwing *= o.drive;

    // Transformer override
    auto setTrafo = [](TransformerStageConfig& t, const std::string& name){
        if      (name == "marinair") t = transformer_presets::Marinair();
        else if (name == "utc")      t = transformer_presets::UTC_A12();
        else if (name == "jensen")   t = transformer_presets::JensenJT11();
        else if (name == "lundahl")  t = transformer_presets::Lundahl();
    };
    if (o.transformer != "auto")
    {
        if (cfg.useInputTransformer)  setTrafo(cfg.inputTrafoConfig,  o.transformer);
        if (cfg.useOutputTransformer) setTrafo(cfg.outputTrafoConfig, o.transformer);
    }

    cfg.variationSeed = o.seed;
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

// ─────────────────────────────────────────────────────────────────────────────
// stdin → stdout loop (raw float32 mono).
// ─────────────────────────────────────────────────────────────────────────────
template <int Factor>
void runStdinStdoutOS(TubeAmpChain& chain, int warmupSamples)
{
    PolyphaseOversampler<Factor> os;
    for (int i = 0; i < warmupSamples; ++i)
        (void)processOneSampleOS(chain, os, 0.0f);

    float inSample = 0.0f;
    while (std::fread(&inSample, sizeof(float), 1, stdin) == 1)
    {
        const float out = processOneSampleOS(chain, os, inSample);
        std::fwrite(&out, sizeof(float), 1, stdout);
    }
    std::fflush(stdout);
}

void runStdinStdoutNoOS(TubeAmpChain& chain, int warmupSamples)
{
    for (int i = 0; i < warmupSamples; ++i)
        (void)processOneSampleNoOS(chain, 0.0f);

    float inSample = 0.0f;
    while (std::fread(&inSample, sizeof(float), 1, stdin) == 1)
    {
        const float out = processOneSampleNoOS(chain, inSample);
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
                    outBuf[f * ch + c] = processOneSampleNoOS(
                        ch_chain, interleaved[f * ch + c]);
                break;
            case 2: {
                PolyphaseOversampler<2> os;
                for (int i = 0; i < warmup; ++i)
                    (void)processOneSampleOS(ch_chain, os, 0.0f);
                for (drwav_uint64 f = 0; f < readFrames; ++f)
                    outBuf[f * ch + c] = processOneSampleOS(
                        ch_chain, os, interleaved[f * ch + c]);
                break;
            }
            case 8: {
                PolyphaseOversampler<8> os;
                for (int i = 0; i < warmup; ++i)
                    (void)processOneSampleOS(ch_chain, os, 0.0f);
                for (drwav_uint64 f = 0; f < readFrames; ++f)
                    outBuf[f * ch + c] = processOneSampleOS(
                        ch_chain, os, interleaved[f * ch + c]);
                break;
            }
            case 4:
            default: {
                PolyphaseOversampler<4> os;
                for (int i = 0; i < warmup; ++i)
                    (void)processOneSampleOS(ch_chain, os, 0.0f);
                for (drwav_uint64 f = 0; f < readFrames; ++f)
                    outBuf[f * ch + c] = processOneSampleOS(
                        ch_chain, os, interleaved[f * ch + c]);
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
        "valvra_process: preset=%s drive=%.2f seed=%llu sr=%.0f os=%dx\n",
        opts.preset.c_str(), opts.drive,
        static_cast<unsigned long long>(opts.seed),
        opts.sampleRate, opts.oversample);

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

    int warmupSamples = 0;
    if (! warmupSamplesFrom(opts.warmupSec, opts.sampleRate, warmupSamples, err))
    {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        return 1;
    }

    switch (opts.oversample)
    {
        case 1: runStdinStdoutNoOS(chain, warmupSamples); break;
        case 2: runStdinStdoutOS<2>(chain, warmupSamples); break;
        case 4: runStdinStdoutOS<4>(chain, warmupSamples); break;
        case 8: runStdinStdoutOS<8>(chain, warmupSamples); break;
        default:
            std::fprintf(stderr, "Unsupported oversample factor %d\n", opts.oversample);
            return 1;
    }

    return 0;
}
