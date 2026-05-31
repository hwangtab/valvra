// ─────────────────────────────────────────────────────────────────────────────
// valvra_bench — wall-time benchmark for Valvra's full plugin signal path
//
// Measures real-time-factor (RTF) for each preset × OS factor × routing
// combination by running synthetic audio through ValvraProcessor with the
// same code path a DAW host would exercise.  Reports a markdown table of
// scenarios sorted by CPU cost so regressions and hotspots are obvious.
//
// Why this matters: the plugin has accumulated significant per-sample
// complexity (push-pull tail solver, compound topology Newton-Raphson,
// 4× wet OS, separate 4× TP detection OS, M/S encode/decode, hidden-
// physics envelope followers).  Without a baseline, any optimisation /
// new feature could silently regress real-time performance.
//
// Run: enable VALVRA_BUILD_BENCHES=ON, build target `valvra_bench`,
// run binary.  No arguments — outputs markdown to stdout.
// ─────────────────────────────────────────────────────────────────────────────

#include "PluginProcessor.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

struct Scenario
{
    int  preset;        // 0..4 — V72/Marshall/CV/RNDI/HiFi300B
    int  osIndex;       // 0..3 — 1×/2×/4×/8×
    bool stereo;        // false = mono, true = stereo
    bool msMode;        // Mid/Side routing
    bool tpEnabled;     // True Peak limiter on
    std::string label;
};

struct Result
{
    std::string label;
    double      rtf;        // real-time factor (audio_sec / wall_sec)
    double      cpuPercent; // 100 / rtf, lower is better
    double      blockMsP50; // median per-block wall time (ms)
    double      blockMsP95; // 95-percentile per-block wall time
};

const char* presetName(int idx)
{
    switch (idx)
    {
        case 0: return "V72";
        case 1: return "Marshall";
        case 2: return "CultureVulture";
        case 3: return "RNDI";
        case 4: return "HiFi300B";
        default: return "?";
    }
}

const char* osLabel(int idx)
{
    switch (idx)
    {
        case 0: return "1x";
        case 1: return "2x";
        case 2: return "4x";
        case 3: return "8x";
        default: return "?";
    }
}

void setChoice(valvra::ValvraProcessor& proc, const char* id, int idx)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(
            proc.parameters().getParameter(id)))
        *p = idx;
}

void setBool(valvra::ValvraProcessor& proc, const char* id, bool on)
{
    if (auto* p = dynamic_cast<juce::AudioParameterBool*>(
            proc.parameters().getParameter(id)))
        *p = on;
}

Result runScenario(const Scenario& s,
                   double sampleRate,
                   int    blockSize,
                   double durationSec)
{
    valvra::ValvraProcessor proc;

    setChoice(proc, "preset",     s.preset);
    setChoice(proc, "oversample", s.osIndex);
    setChoice(proc, "msMode",     s.msMode ? 1 : 0);
    setBool  (proc, "tpEnabled",  s.tpEnabled);

    juce::AudioProcessor::BusesLayout layout;
    const auto chSet = s.stereo
        ? juce::AudioChannelSet::stereo()
        : juce::AudioChannelSet::mono();
    layout.inputBuses .add(chSet);
    layout.outputBuses.add(chSet);
    if (! proc.setBusesLayout(layout))
    {
        return { s.label, 0.0, 0.0, 0.0, 0.0 };
    }

    proc.prepareToPlay(sampleRate, blockSize);

    const int numCh =
        s.stereo ? 2 : 1;
    const int totalSamples =
        static_cast<int>(durationSec * sampleRate);
    const int numBlocks =
        std::max(1, totalSamples / blockSize);

    juce::AudioBuffer<float> buffer(numCh, blockSize);
    juce::MidiBuffer midi;

    // Pseudo-random audio: fixed seed for reproducible runs.
    std::mt19937 rng(0xC0FFEEu);
    std::uniform_real_distribution<float> dist(-0.3f, 0.3f);

    auto fillBlock = [&]
    {
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* p = buffer.getWritePointer(ch);
            for (int n = 0; n < blockSize; ++n) p[n] = dist(rng);
        }
    };

    // Warm-up — let the chain's slow envelopes (DC tracker τ ≈ 200 ms,
    // thermal drift τ ≈ 8 s) settle before timing.  A few hundred ms
    // is enough to clear the dominant transient without blowing the
    // benchmark wall-time budget on warm-up.
    constexpr double kWarmupSec = 0.5;
    const int warmupBlocks =
        std::max(1, static_cast<int>(kWarmupSec * sampleRate / blockSize));
    for (int b = 0; b < warmupBlocks; ++b)
    {
        fillBlock();
        proc.processBlock(buffer, midi);
    }

    // Measurement run — record per-block wall time so we can report
    // p50/p95 in addition to the aggregate RTF.  p95 catches occasional
    // worst-case excursions (e.g. a transient pushing the push-pull
    // solver harder) that a mean would smooth over.
    std::vector<double> blockMs;
    blockMs.reserve(static_cast<std::size_t>(numBlocks));

    const auto t0 = std::chrono::high_resolution_clock::now();
    for (int b = 0; b < numBlocks; ++b)
    {
        fillBlock();
        const auto bt0 = std::chrono::high_resolution_clock::now();
        proc.processBlock(buffer, midi);
        const auto bt1 = std::chrono::high_resolution_clock::now();
        blockMs.push_back(
            std::chrono::duration<double, std::milli>(bt1 - bt0).count());
    }
    const auto t1 = std::chrono::high_resolution_clock::now();

    const double elapsedSec =
        std::chrono::duration<double>(t1 - t0).count();
    const double audioSec =
        static_cast<double>(numBlocks * blockSize) / sampleRate;
    const double rtf = (elapsedSec > 0.0) ? audioSec / elapsedSec : 0.0;
    const double cpuPct = (rtf > 0.0) ? 100.0 / rtf : 100.0;

    // p50, p95 of per-block wall time.
    auto sorted = blockMs;
    std::sort(sorted.begin(), sorted.end());
    const auto p50 = sorted.empty()
        ? 0.0
        : sorted[static_cast<std::size_t>(sorted.size() * 0.50)];
    const auto p95 = sorted.empty()
        ? 0.0
        : sorted[std::min(sorted.size() - 1,
                          static_cast<std::size_t>(sorted.size() * 0.95))];

    return { s.label, rtf, cpuPct, p50, p95 };
}

} // namespace

int main()
{
    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlockSize  = 256;       // typical DAW block size
    constexpr double kDuration   = 4.0;       // seconds of audio per scenario

    // The benchmark grid covers what users actually configure.  We do
    // NOT exhaustively test every combination (40+) because the report
    // becomes unreadable; instead we pick scenarios that exercise each
    // axis (preset × OS factor × routing) at least once.
    std::vector<Scenario> scenarios;

    // Each preset at default (4× OS, stereo, no M/S, no TP) — five
    // baselines that show inherent per-preset cost.
    for (int p = 0; p < 5; ++p)
    {
        std::string label =
            std::string(presetName(p)) + " · 4× · stereo · clean";
        scenarios.push_back({ p, /*os=*/2, /*stereo=*/true,
                              /*ms=*/false, /*tp=*/false, label });
    }

    // OS-factor sweep on a typical preset (V72) to expose OS overhead.
    for (int os = 0; os < 4; ++os)
    {
        std::string label =
            std::string("V72 · ") + osLabel(os) + " · stereo · clean";
        scenarios.push_back({ 0, os, true, false, false, label });
    }

    // Mastering surcharge: V72 with TP limiter on, with M/S on, both.
    scenarios.push_back({ 0, 2, true,  false, true,
                          "V72 · 4× · stereo · TP on" });
    scenarios.push_back({ 0, 2, true,  true,  false,
                          "V72 · 4× · M/S · clean" });
    scenarios.push_back({ 0, 2, true,  true,  true,
                          "V72 · 4× · M/S · TP on (full master chain)" });

    // Mono path — should be ~half of stereo cost.
    scenarios.push_back({ 0, 2, false, false, false,
                          "V72 · 4× · MONO · clean" });

    // Marshall (push-pull EL34 solver) — heaviest known DSP.
    scenarios.push_back({ 1, 2, true,  false, true,
                          "Marshall · 4× · stereo · TP on" });

    std::vector<Result> results;
    results.reserve(scenarios.size());

    std::cerr << "Running " << scenarios.size() << " benchmark scenarios at "
              << kSampleRate << " Hz, block=" << kBlockSize
              << ", duration=" << kDuration << "s each...\n";

    for (const auto& s : scenarios)
    {
        std::cerr << "  " << s.label << "...\n";
        results.push_back(runScenario(s, kSampleRate, kBlockSize, kDuration));
    }

    // Sort by CPU% descending — the heaviest scenarios appear at the top.
    std::sort(results.begin(), results.end(),
              [](const Result& a, const Result& b) {
                  return a.cpuPercent > b.cpuPercent;
              });

    // Markdown table to stdout.
    std::cout << "# Valvra benchmark results\n\n";
    std::cout << "Sample rate: " << kSampleRate
              << " Hz · Block size: " << kBlockSize
              << " · Duration: " << kDuration << " s per scenario\n\n";
    std::cout << "| Scenario | RTF | CPU % (1 core) | p50 ms/block | p95 ms/block |\n";
    std::cout << "|---|---:|---:|---:|---:|\n";
    for (const auto& r : results)
    {
        std::printf("| %s | %.2fx | %.2f%% | %.3f | %.3f |\n",
                    r.label.c_str(),
                    r.rtf, r.cpuPercent, r.blockMsP50, r.blockMsP95);
    }
    std::cout << "\n*Lower CPU%, higher RTF, lower p95 = better.*\n";

    return 0;
}
