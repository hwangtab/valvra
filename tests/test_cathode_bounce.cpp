// Unit tests for CathodeBounce
// Validation: τ = Rk·Ck, audibility > 5 mV threshold, recovery is monotonic

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "CathodeBounce.h"

using Catch::Approx;
using namespace valvra::dsp;

TEST_CASE("Cathode bounce tracks steady current to Ip*Rk", "[bounce][static]")
{
    CathodeBounceParams p { .Rk = 1500.0, .Ck = 25.0e-6, .sampleRate = 48000.0 };
    CathodeBounce cb { p };

    // DC plate current 1 mA → steady Vk should approach 1 mA × 1.5 kΩ = 1.5 V
    const double Ip_dc = 1.0e-3;
    for (int i = 0; i < 48000 * 2; ++i)  // 2 seconds (τ=37.5 ms, 50τ+)
        cb.process(Ip_dc);

    REQUIRE(cb.currentBias() == Approx(1.5).epsilon(0.01));
}

TEST_CASE("Cathode bounce time constant matches tau = Rk*Ck", "[bounce][time-constant]")
{
    // This test verifies the PURE single-pole RC response, so the soakage
    // network is disabled — soakage is a deliberate second mechanism tested
    // separately below.
    CathodeBounceParams p { .Rk = 1500.0, .Ck = 25.0e-6, .sampleRate = 48000.0 };
    p.enableSoakage = false;
    CathodeBounce cb { p };

    // Expected: at t = τ, Vk = 63.2% of final
    const double Ip = 1.0e-3;  // 1 mA
    const double target_V = Ip * p.Rk;                 // 1.5 V
    const double tau_sec = p.Rk * p.Ck;                // 37.5 ms
    const int tau_samples = static_cast<int>(tau_sec * p.sampleRate);

    for (int i = 0; i < tau_samples; ++i)
        cb.process(Ip);

    const double Vk_at_tau = cb.currentBias();
    const double expected = target_V * (1.0 - std::exp(-1.0));   // 0.632·V
    REQUIRE(Vk_at_tau == Approx(expected).epsilon(0.05));
}

TEST_CASE("Cathode bounce: soakage creates a slow post-burst tail",
          "[bounce][soakage][dielectric-absorption]")
{
    // Drive a burst long enough to charge the main cap (many τ = 37.5 ms),
    // then zero the current.  In the no-soakage path Vk decays along the
    // main τ alone; with soakage on, the dielectric network holds a
    // fraction of the voltage much longer, producing a slow tail.
    auto run = [](bool soakOn) -> std::vector<double>
    {
        CathodeBounceParams p { .Rk = 1500.0, .Ck = 25.0e-6, .sampleRate = 48000.0 };
        p.enableSoakage = soakOn;
        p.soakageAmount = 0.10;
        p.soakageTau    = 0.15;
        CathodeBounce cb { p };

        std::vector<double> out;
        // 0.3 s DC burst at 1 mA → Vk saturates to ~1.5 V
        for (int i = 0; i < 48000 * 3 / 10; ++i) cb.process(1.0e-3);
        // 0.6 s recovery at zero current → Vk decays back toward 0
        const int N = static_cast<int>(48000 * 0.6);
        out.reserve(N);
        for (int i = 0; i < N; ++i) out.push_back(cb.process(0.0));
        return out;
    };

    const auto yNoSoak = run(false);
    const auto ySoak   = run(true);

    // Check at a point 7 · τ_main into the recovery (≈ 260 ms).  Without
    // soakage the single pole has decayed to ~0.1% of peak; soakage must
    // still hold a measurable fraction of the original voltage here.
    const std::size_t probe = static_cast<std::size_t>(0.260 * 48000.0);
    REQUIRE(probe < yNoSoak.size());
    REQUIRE(ySoak[probe]  > 5.0 * std::abs(yNoSoak[probe]));
    REQUIRE(ySoak[probe]  > 0.02);   // > 20 mV — easily audible bias shift
    REQUIRE(ySoak[probe]  < 0.3);    // sanity: nowhere near full charge
}

TEST_CASE("Cathode bounce bias shift exceeds audibility threshold", "[bounce][audibility]")
{
    // Audibility threshold: 5-15 mV bias shift (Jones 2011)
    CathodeBounce cb;

    // Strong plate current spike (3 mA is realistic for 12AX7 overdrive)
    const double Ip_burst = 3.0e-3;
    for (int i = 0; i < 4800; ++i)   // 100 ms burst
        cb.process(Ip_burst);

    const double shift = cb.currentBias();
    REQUIRE(shift > 0.015);   // ≥ 15 mV, clearly above audibility
}

TEST_CASE("Biased grid subtracts Vk from input Vg", "[bounce][semantics]")
{
    CathodeBounce cb;

    for (int i = 0; i < 10000; ++i)
        cb.process(1.0e-3);

    // cathode above ground → grid effectively more negative
    const double Vg_input     = -1.5;
    const double Vg_effective = cb.biasedGrid(Vg_input);
    REQUIRE(Vg_effective < Vg_input);   // more negative bias after bounce
}

TEST_CASE("Per-instance Ck scale affects recovery speed", "[bounce][variation]")
{
    CathodeBounce cb_low;
    CathodeBounce cb_high;
    cb_low.setCkScale(0.8);   // −20% (aged electrolytic)
    cb_high.setCkScale(1.2);  // +20% (fresh electrolytic)

    // Apply step, then measure how fast each recovers
    for (int i = 0; i < 10000; ++i)
    {
        cb_low.process(1.0e-3);
        cb_high.process(1.0e-3);
    }

    // Now step to zero current; higher Ck should decay slower
    for (int i = 0; i < 1000; ++i)
    {
        cb_low.process(0.0);
        cb_high.process(0.0);
    }

    REQUIRE(cb_high.currentBias() > cb_low.currentBias());
}
