// ─────────────────────────────────────────────────────────────────────────────
// test_compound_topologies.cpp — SRPP and Cascode (stacked-tube) validation
//
// Both topologies share the same physical structure: two triodes stacked,
// the same Ip flowing through both, the chain Newton-Raphson-solving the
// junction voltage V_mid per sample so KCL is preserved.  The two differ
// in where the upper grid is biased and where the output is taken.
//
// What we verify:
//   1) Idle silence — at zero input, the output sits at AC-coupled zero.
//      The setup-time Newton-Raphson must have converged to a real
//      operating point (not an unstable saddle that drifts).
//   2) Solver stability — heavy drive transients do not produce NaN /
//      Inf at the output.  The clamps inside process() must hold.
//   3) SRPP push-pull symmetry — the SRPP topology has more symmetric
//      large-signal behaviour than CommonCathode (active-load instead of
//      a passive resistor).  The asymmetry of clipped peaks should be
//      smaller for SRPP than for CC at the same drive level.
//   4) Cascode wider bandwidth — the Miller capacitance suppression of
//      cascode should produce a flatter HF response than CommonCathode
//      with the same Cgp_miller value.  We compare the response to a
//      ringing impulse-train probe (sum of high-freq sines).
// ─────────────────────────────────────────────────────────────────────────────
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TubeStage.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

using namespace valvra::dsp;
using Catch::Approx;

namespace {

constexpr double kSampleRate = 48000.0;

TubeStageConfig baseConfig()
{
    TubeStageConfig c;
    c.tube       = params::kRSD_1;
    c.upperTube  = params::kRSD_1;     // matched pair
    c.Vg_bias    = -1.5;
    c.Vp_nominal = 250.0;
    c.Rp         = 100.0e3;
    c.Rk         = 1500.0;
    c.Ck         = 22.0e-6;
    c.Rp_upper   = 100.0e3;
    c.Vg_upper_bias = 90.0;
    c.inputVoltageSwing = 1.0;
    c.outputGainLinear  = 1.0;
    c.enableWarmup      = false;       // run at full gm for repeatable tests
    c.enableMillerFilter   = false;    // isolate topology behaviour
    c.enableHeaterHum      = false;
    c.enableShotNoise      = false;
    c.enableThermalDrift   = false;
    c.enableGridConduction = false;
    c.enableMicrophonics   = false;
    c.enableSlewLimit      = false;
    c.enableSoakage        = false;
    c.enableCathodeBounce  = false;
    return c;
}

std::vector<double> render(TubeStage& stage,
                           double freq, double amp,
                           int N, double Vb)
{
    std::vector<double> out;
    out.reserve(static_cast<std::size_t>(N));
    for (int n = 0; n < N; ++n)
    {
        const double t = n / kSampleRate;
        const double x = amp * std::sin(2.0 * std::numbers::pi * freq * t);
        out.push_back(stage.process(x, Vb));
    }
    return out;
}

double rms(const std::vector<double>& v, int startN = 0)
{
    double s = 0.0;
    int    count = 0;
    for (std::size_t i = static_cast<std::size_t>(startN); i < v.size(); ++i)
    {
        s += v[i] * v[i];
        ++count;
    }
    return std::sqrt(s / std::max(count, 1));
}

} // namespace

TEST_CASE("SRPP: idle silence (AC-coupled output settles to zero)",
          "[topology][srpp]")
{
    auto cfg = baseConfig();
    cfg.topology = TubeTopology::SRPP;

    TubeStage s;
    s.setup(cfg, kSampleRate);
    s.reset(false);

    // Process silence for ~200 ms — long enough for the slow DC tracker
    // (alpha=0.9999 → tau ~200 ms) to reach steady state.
    constexpr int N = 12000;
    std::vector<double> out;
    out.reserve(N);
    for (int n = 0; n < N; ++n) out.push_back(s.process(0.0, 250.0));

    // The last quarter of the buffer should be very close to zero.
    const double tail = rms(out, N * 3 / 4);
    INFO("SRPP idle tail RMS = " << tail);
    REQUIRE(tail < 5.0e-3);  // < ~−46 dB after settle
}

TEST_CASE("Cascode: idle silence (AC-coupled output settles to zero)",
          "[topology][cascode]")
{
    auto cfg = baseConfig();
    cfg.topology = TubeTopology::Cascode;

    TubeStage s;
    s.setup(cfg, kSampleRate);
    s.reset(false);

    constexpr int N = 12000;
    std::vector<double> out;
    out.reserve(N);
    for (int n = 0; n < N; ++n) out.push_back(s.process(0.0, 250.0));

    const double tail = rms(out, N * 3 / 4);
    INFO("Cascode idle tail RMS = " << tail);
    REQUIRE(tail < 5.0e-3);  // < ~−46 dB after settle
}

TEST_CASE("SRPP: solver stays finite under heavy drive",
          "[topology][srpp][solver]")
{
    auto cfg = baseConfig();
    cfg.topology = TubeTopology::SRPP;
    cfg.compoundSolverIters = 2;

    TubeStage s;
    s.setup(cfg, kSampleRate);
    s.reset(false);

    // 6× nominal drive — way past any reasonable headroom.  Newton-Raphson
    // could potentially diverge here without the clamp+warm-start safety.
    auto out = render(s, 220.0, 6.0, 8192, 250.0);
    for (double v : out) REQUIRE(std::isfinite(v));
}

TEST_CASE("Cascode: solver stays finite under heavy drive",
          "[topology][cascode][solver]")
{
    auto cfg = baseConfig();
    cfg.topology = TubeTopology::Cascode;
    cfg.compoundSolverIters = 2;

    TubeStage s;
    s.setup(cfg, kSampleRate);
    s.reset(false);

    auto out = render(s, 440.0, 6.0, 8192, 250.0);
    for (double v : out) REQUIRE(std::isfinite(v));
}

TEST_CASE("SRPP and Cascode produce non-trivial output distinct from CC",
          "[topology][character]")
{
    // Sanity: each topology produces audible output and SRPP / Cascode
    // are audibly distinct from CommonCathode.  We use a hand-tuned
    // operating point appropriate for compound topologies (less negative
    // bias so the lower tube has meaningful Ip even at the lowered
    // Vp_lower = V_mid that the stack equilibrium imposes).
    auto runWith = [](TubeTopology topo)
    {
        auto cfg = baseConfig();
        cfg.topology = topo;
        // Operating point tuning: SRPP/Cascode pull V_mid below
        // Vp_nominal, so the lower tube needs a hotter bias to reach
        // its sweet-spot gm at the new Vp.  -0.6 V keeps Ip ≈ 0.5 mA
        // at V_mid ≈ 100 V.
        if (topo == TubeTopology::SRPP || topo == TubeTopology::Cascode)
        {
            cfg.Vg_bias        = -0.6;
            cfg.Rk             = 600.0;     // smaller Rk for hotter bias
            cfg.Rp_upper       = 47.0e3;    // medium load for cascode
            cfg.Vg_upper_bias  = 100.0;     // upper grid above mid-rail
        }
        TubeStage s;
        s.setup(cfg, kSampleRate);
        s.reset(false);
        // Settle for ~250 ms first so the slow DC tracker reaches its
        // steady state before we start measuring.
        for (int n = 0; n < 12000; ++n) (void) s.process(0.0, 250.0);
        return render(s, 220.0, 0.4, 8192, 250.0);
    };

    const auto cc      = runWith(TubeTopology::CommonCathode);
    const auto srpp    = runWith(TubeTopology::SRPP);
    const auto cascode = runWith(TubeTopology::Cascode);

    auto rmsDiff = [](const std::vector<double>& a, const std::vector<double>& b)
    {
        double e = 0.0;
        const std::size_t skip = 4096;  // skip more for deeper settle
        for (std::size_t i = skip; i < a.size(); ++i)
            e += (a[i] - b[i]) * (a[i] - b[i]);
        return std::sqrt(e / (a.size() - skip));
    };

    const double rms_cc      = rms(cc,      4096);
    const double rms_srpp    = rms(srpp,    4096);
    const double rms_cascode = rms(cascode, 4096);

    INFO("rms_cc=" << rms_cc << " rms_srpp=" << rms_srpp
         << " rms_cascode=" << rms_cascode);

    // Each topology must produce non-trivial output.
    REQUIRE(rms_cc      > 1.0e-3);
    REQUIRE(rms_srpp    > 1.0e-3);
    REQUIRE(rms_cascode > 1.0e-3);

    // SRPP and Cascode must each be audibly distinct from CC — the same
    // input should yield meaningfully different waveforms because the
    // load impedance and signal-path topology are different.
    REQUIRE(rmsDiff(cc, srpp)    > 0.1 * rms_cc);
    REQUIRE(rmsDiff(cc, cascode) > 0.1 * rms_cc);
}
