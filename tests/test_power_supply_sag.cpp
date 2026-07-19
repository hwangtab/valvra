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

TEST_CASE("PSU DC level is always <= Vb_nominal", "[psu][invariant]")
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

TEST_CASE("PSU rail carries load-dependent 120 Hz ripple (reservoir physics)",
          "[psu][ripple]")
{
    // The reservoir-capacitor model makes ripple EMERGENT: charge pulses
    // near each |sin| crest, discharge into the load between them.  Three
    // physical facts the legacy fixed-amplitude sine could not show:
    //   1. no load → (almost) no ripple — the cap rides the crest
    //   2. ripple peak-to-peak grows ≈ I/(f·C) with load current
    //   3. the loaded rail average sags below nominal
    //
    // Uses the 6X4 preset: a bare reservoir with no smoothing choke, so the
    // ripple appears directly at the output node.  (The GZ34 preset now adds
    // a choke — see the separate "smoothing choke reduces output ripple"
    // case — which deliberately filters this ripple away, exactly as a real
    // choke-filtered supply does.)
    auto p = psu_presets::k6X4_Pultec;
    p.sampleRate = 48000.0;

    auto measure = [&](double iLoad) {
        PowerSupplySag psu { p };
        for (int i = 0; i < 48000; ++i) psu.process(iLoad);   // settle 1 s
        psu.process(iLoad);
        double vbMin = psu.currentVb();
        double vbMax = vbMin;
        double vbSum = 0.0;
        const int N = 8000;   // several ripple cycles
        for (int i = 0; i < N; ++i)
        {
            psu.process(iLoad);
            const double vb = psu.currentVb();
            if (vb < vbMin) vbMin = vb;
            if (vb > vbMax) vbMax = vb;
            vbSum += vb;
        }
        struct R { double pp, avg; };
        return R { vbMax - vbMin, vbSum / N };
    };

    const auto idle   = measure(0.0);
    const auto light  = measure(0.01);   // 10 mA
    const auto heavy  = measure(0.04);   // 40 mA

    // 1. Unloaded rail is essentially ripple-free and at nominal.
    REQUIRE(idle.pp < 0.1);
    REQUIRE(idle.avg == Approx(p.Vb_nominal).margin(1.0));

    // 2. Ripple grows with load current (roughly proportionally).
    REQUIRE(light.pp > 0.5);
    REQUIRE(heavy.pp > 2.0 * light.pp);

    // 3. The loaded rail sags below nominal, more so at heavy load.
    REQUIRE(light.avg < p.Vb_nominal - 0.5);
    REQUIRE(heavy.avg < light.avg);
}

TEST_CASE("PSU smoothing choke reduces output ripple (docs/34 S3.4)",
          "[psu][choke]")
{
    // A choke + second cap (π filter) is a second LC smoothing stage, so
    // the output ripple should be far lower than the same rectifier feeding
    // a bare reservoir.  Compare the GZ34 preset (choke on) against a copy
    // with the choke defeated but every other value identical.
    auto measurePP = [](PSUSagParams p) {
        p.sampleRate = 48000.0;
        PowerSupplySag psu { p };
        for (int i = 0; i < 48000; ++i) psu.process(0.04);   // settle 1 s
        double vMin = psu.currentVb(), vMax = vMin;
        for (int i = 0; i < 8000; ++i)
        {
            psu.process(0.04);
            const double vb = psu.currentVb();
            vMin = std::min(vMin, vb);
            vMax = std::max(vMax, vb);
        }
        return vMax - vMin;
    };

    auto withChoke = psu_presets::kGZ34;      // choke enabled in the preset
    auto noChoke   = psu_presets::kGZ34;
    noChoke.enableChoke = false;              // bare reservoir, same rectifier

    const double ppChoke   = measurePP(withChoke);
    const double ppNoChoke = measurePP(noChoke);

    // The choke stage should knock the ripple down by a large factor.
    REQUIRE(ppNoChoke > 0.3);
    REQUIRE(ppChoke < 0.5 * ppNoChoke);
    // And the output must never exceed the unloaded crest.
    REQUIRE(withChoke.Vb_nominal + 1.0 > measurePP(withChoke) + 0.0);
}

TEST_CASE("PSU vacuum rectifier sags with sub-linear (space-charge) depth",
          "[psu][spacecharge]")
{
    // A Child-Langmuir (exponent 1.5) rectifier drops as ΔV ∝ I^{2/3}, so
    // doubling the load current less-than-doubles the sag — the vacuum
    // "firm up" behaviour.  A silicon (ohmic, exponent 1.0) diode would
    // sag in proportion.  Compare the incremental sag ratio.
    auto sagAt = [](PSUSagParams p, double iLoad) {
        p.sampleRate = 48000.0;
        p.enableChoke = false;   // isolate the rectifier curvature
        PowerSupplySag psu { p };
        for (int i = 0; i < 96000; ++i) psu.process(iLoad);   // settle 2 s
        return p.Vb_nominal - psu.sagPercent() / 100.0 * p.Vb_nominal;
    };

    auto vac = psu_presets::kGZ34;   // exponent 1.5
    const double sagLoVac = vac.Vb_nominal - sagAt(vac, 0.02);
    const double sagHiVac = vac.Vb_nominal - sagAt(vac, 0.04);

    // Sub-linear: doubling current raises the sag by LESS than 2× for the
    // space-charge rectifier (a purely ohmic supply would give exactly 2×).
    REQUIRE(sagHiVac > sagLoVac);              // heavier load still sags more
    REQUIRE(sagHiVac < 2.0 * sagLoVac);        // but sub-linearly (firm-up)
}
