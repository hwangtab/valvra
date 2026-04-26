// Unit tests for PowerSupplySag
// Validation: sag depth matches academic ranges per rectifier type

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "PowerSupplySag.h"

using Catch::Approx;
using namespace valvra::dsp;

TEST_CASE("PSU sag reaches expected depth with GZ34 at heavy load", "[psu][static]")
{
    // GZ34: Z ≈ 200 Ω, expected sag 3–5% at typical Ip swings
    PowerSupplySag psu { psu_presets::kGZ34 };

    // Sustained 5 mA draw (heavy for 12AX7-class preamp)
    const double Ip = 5.0e-3;
    for (int i = 0; i < 48000 * 2; ++i)  // 2 s
        psu.process(Ip);

    const double sag = psu.sagPercent();
    REQUIRE(sag > 0.2);   // > 0.2% at this load
    REQUIRE(sag < 10.0);  // < 10% (sanity bound for GZ34)
}

TEST_CASE("5U4GB sags more than GZ34 under identical load", "[psu][compare]")
{
    PowerSupplySag gz34  { psu_presets::kGZ34 };
    PowerSupplySag u5u4gb { psu_presets::k5U4GB };

    const double Ip = 5.0e-3;
    for (int i = 0; i < 48000 * 2; ++i)
    {
        gz34.process(Ip);
        u5u4gb.process(Ip);
    }

    REQUIRE(u5u4gb.sagPercent() > gz34.sagPercent());
}

TEST_CASE("Solid-state rectifier sags much less than vacuum", "[psu][compare]")
{
    PowerSupplySag gz34 { psu_presets::kGZ34 };
    PowerSupplySag ss   { psu_presets::kSolidState };

    const double Ip = 10.0e-3;  // heavy load
    for (int i = 0; i < 48000; ++i)
    {
        gz34.process(Ip);
        ss.process(Ip);
    }

    REQUIRE(ss.sagPercent() < gz34.sagPercent() / 5.0);
}

TEST_CASE("PSU envelope recovers when load is removed", "[psu][dynamic]")
{
    PowerSupplySag psu { psu_presets::k6X4_Pultec };

    // Apply heavy load
    for (int i = 0; i < 48000; ++i)
        psu.process(8.0e-3);
    const double vb_loaded = psu.currentVb();

    // Remove load
    for (int i = 0; i < 48000; ++i)
        psu.process(0.0);
    const double vb_unloaded = psu.currentVb();

    REQUIRE(vb_unloaded > vb_loaded);
    // Should be close to nominal after recovery
    REQUIRE(vb_unloaded == Approx(350.0).epsilon(0.01));
}

TEST_CASE("PSU DC level is always ≤ Vb_nominal", "[psu][invariant]")
{
    // This invariant applies to the sag component only — real rail also
    // carries a 120 Hz ripple that can push momentary Vb above nominal,
    // which is tested separately.  Disable ripple here to keep the check
    // focused on the sag envelope.
    auto p = psu_presets::kGZ34;
    p.ripple_amp = 0.0;
    PowerSupplySag psu { p };

    for (double ip = 0.0; ip <= 0.05; ip += 0.001)
    {
        for (int i = 0; i < 100; ++i)
            psu.process(ip);
        REQUIRE(psu.currentVb() <= 325.0);
    }
}

TEST_CASE("PSU rail carries 120 Hz ripple with expected magnitude",
          "[psu][ripple]")
{
    auto p = psu_presets::kGZ34;
    p.sampleRate = 48000.0;
    PowerSupplySag psu { p };

    // Quiescent current — sag envelope settles to zero, leaving only
    // ripple on top of Vb_nominal.
    for (int i = 0; i < 10000; ++i) psu.process(0.0);

    // Avoid numeric_limits::infinity() — the test suite is compiled with
    // -ffast-math, under which infinities are UB and min/max behave in
    // surprising ways.  Prime with the first measured value instead.
    psu.process(0.0);
    double vbMin = psu.currentVb();
    double vbMax = vbMin;
    for (int i = 0; i < 8000; ++i)  // several ripple cycles
    {
        psu.process(0.0);
        const double vb = psu.currentVb();
        if (vb < vbMin) vbMin = vb;
        if (vb > vbMax) vbMax = vb;
    }

    // Peak-to-peak ≈ 2 × ripple_amp, with wiggle room for sampling phase.
    const double pp = vbMax - vbMin;
    REQUIRE(pp == Approx(2.0 * p.ripple_amp).epsilon(0.05));
    // Ripple is centered on Vb_nominal (sag is zero at rest)
    REQUIRE((vbMin + vbMax) * 0.5 == Approx(p.Vb_nominal).margin(0.01));
}
