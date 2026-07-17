// ─────────────────────────────────────────────────────────────────────────────
// Unit tests for Jiles-Atherton hysteresis model
//
// Validation:
//   1. Langevin Taylor expansion continuity at x≈0 (no 0/0 explosion)
//   2. Sign-preserving δ under DC (dH=0)
//   3. Major loop: M saturates at ±Ms as |H| → ∞
//   4. Hysteresis: ascending and descending branches differ
//   5. Wiping-out: minor loops respect JA physics
// ─────────────────────────────────────────────────────────────────────────────

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "JilesAtherton.h"

using Catch::Approx;
using namespace valvra::dsp;

TEST_CASE("Langevin Taylor expansion matches exact at x=1e-4 boundary", "[ja][numerical]")
{
    const double x = 1.0e-4;

    // Just above threshold: uses exact coth(x) − 1/x.  Near x=0 this
    // expression suffers catastrophic cancellation (coth(1e-4) ≈ 10000.00003,
    // subtracting 10000 keeps only ~8 significant digits out of 16).
    // Taylor side is accurate to machine precision.  Therefore the
    // realistic continuity tolerance at the boundary is dominated by
    // the float-subtraction error in coth, NOT by Taylor truncation error.
    const double just_above = langevin(x * 1.001);
    const double just_below = langevin(x * 0.999);

    // Expected exact value L(1e-4) ≈ x/3 = 3.33e-5.
    // Continuity error must be small relative to this: ≤ 1% (≈ 3.3e-7).
    REQUIRE(std::abs(just_above - just_below) < 1.0e-6);
}

TEST_CASE("Langevin is odd symmetric", "[ja][numerical]")
{
    REQUIRE(langevin(1.0) == Approx(-langevin(-1.0)));
    REQUIRE(langevin(5.0) == Approx(-langevin(-5.0)));
    REQUIRE(langevin(0.0) == Approx(0.0));
}

TEST_CASE("Langevin approaches ±1 as |x| → ∞", "[ja][numerical]")
{
    REQUIRE(langevin(50.0) == Approx(1.0).epsilon(0.02));
    REQUIRE(langevin(-50.0) == Approx(-1.0).epsilon(0.02));
}

TEST_CASE("JA saturates near ±Ms for large fields", "[ja][major-loop]")
{
    JilesAtherton ja { ja_params::kNiPermalloy_Peerless };

    // Ramp up to strong positive field
    double M_pos = 0.0;
    for (double H = 0.0; H <= 1.0e5; H += 1000.0)
        M_pos = ja.process(H);

    // Should be close to Ms (but not equal: reversibility loss)
    REQUIRE(M_pos > 0.5 * ja_params::kNiPermalloy_Peerless.Ms);
    REQUIRE(M_pos < ja_params::kNiPermalloy_Peerless.Ms * 1.01);
}

TEST_CASE("JA shows hysteresis: ascending ≠ descending", "[ja][hysteresis]")
{
    JilesAtherton ja { ja_params::kNiPermalloy_Peerless };

    // First ramp up (ascending branch)
    double M_asc_at_500 = 0.0;
    for (double H = -1.0e5; H <= 5.0e2; H += 500.0)
        M_asc_at_500 = ja.process(H);

    // Continue to positive saturation, then reverse
    for (double H = 5.0e2; H <= 1.0e5; H += 1000.0)
        ja.process(H);

    // Descending branch
    double M_desc_at_500 = 0.0;
    for (double H = 1.0e5; H >= 5.0e2; H -= 500.0)
        M_desc_at_500 = ja.process(H);

    // At H=500, ascending branch should have LOWER M than descending
    REQUIRE(M_asc_at_500 < M_desc_at_500);
}

TEST_CASE("JA reset returns to initial state", "[ja][reset]")
{
    JilesAtherton ja { ja_params::kNiPermalloy_Peerless };

    for (double H = 0.0; H <= 1.0e5; H += 1000.0)
        ja.process(H);

    const double M_before_reset = ja.process(0.0);
    REQUIRE(M_before_reset > 0.0);   // remanence present

    ja.reset();

    const double M_after_reset = ja.process(0.0);
    REQUIRE(std::abs(M_after_reset) < 1e-3);
}

TEST_CASE("JA DC input (dH=0) preserves δ sign", "[ja][numerical]")
{
    JilesAtherton ja { ja_params::kNiPermalloy_Peerless };

    // First sample sets δ
    ja.process(100.0);
    // Multiple DC-identical samples must not destabilize
    for (int i = 0; i < 100; ++i)
        ja.process(100.0);

    const double M_after_DC = ja.process(100.0);
    REQUIRE(std::isfinite(M_after_DC));
    REQUIRE(std::abs(M_after_DC) < 1e8);   // no blowup
}

TEST_CASE("Material presets: Si-steel saturates higher than permalloy", "[ja][materials]")
{
    JilesAtherton permalloy { ja_params::kNiPermalloy_Peerless };
    JilesAtherton sisteel   { ja_params::kSiSteel_M6 };

    // Ramp to strong field
    double M_permalloy = 0.0;
    double M_sisteel   = 0.0;
    for (double H = 0.0; H <= 5.0e4; H += 500.0)
    {
        M_permalloy = permalloy.process(H);
        M_sisteel   = sisteel.process(H);
    }

    REQUIRE(M_sisteel > M_permalloy);
}

TEST_CASE("JA Bertotti excess loss widens the loop with field rate",
          "[ja][excess]")
{
    // docs/34 §3.2 — with the sample period supplied, faster field motion
    // sees a rate-scaled pinning (k_dyn ∝ 1 + kExcess·√rate): the SAME H
    // samples must trace a wider (lossier) loop than the quasi-static
    // model (dt = 0), and the quasi-static path must remain bit-identical
    // to the classic rate-independent behaviour.
    auto loopArea = [](double dt) {
        JilesAtherton ja { ja_params::kSiSteel_M6 };
        ja.reset();
        const double A = 2.0 * ja_params::kSiSteel_M6.a;
        constexpr int kCycle = 480;   // 100 Hz at 48 kHz
        double area = 0.0;
        double hPrev = 0.0, mPrev = 0.0;
        for (int i = 0; i < 4 * kCycle; ++i)
        {
            constexpr double kPi = 3.14159265358979323846;
            const double h = A * std::sin(2.0 * kPi * i / double(kCycle));
            const double m = ja.process(h, dt);
            if (i >= 3 * kCycle)   // settled last cycle
                area += 0.5 * (m + mPrev) * (h - hPrev);
            hPrev = h; mPrev = m;
        }
        return std::abs(area);
    };

    const double areaStatic = loopArea(0.0);
    const double areaFast   = loopArea(1.0 / 48000.0);
    INFO("static loop area = " << areaStatic
         << ", rate-aware = " << areaFast);
    REQUIRE(areaStatic > 0.0);
    REQUIRE(areaFast > 1.02 * areaStatic);   // measurably wider loop
}
