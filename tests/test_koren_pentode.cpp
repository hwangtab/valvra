#include <catch2/catch_test_macros.hpp>

#include "KorenPentode.h"

#include <cmath>

using namespace valvra::dsp;

TEST_CASE("KorenPentode: screen voltage increases plate current", "[KorenPentode]")
{
    KorenPentode p { pentode_params::k6AS6 };
    const double Va = 180.0;
    const double Vg1 = -1.2;
    const double Vg3 = -0.8;

    const auto low = p.evaluate(Va, Vg1, 90.0, Vg3);
    const auto high = p.evaluate(Va, Vg1, 170.0, Vg3);

    REQUIRE(std::isfinite(low.Ip));
    REQUIRE(std::isfinite(high.Ip));
    REQUIRE(high.Ip > low.Ip);
}

TEST_CASE("KorenPentode: suppressor bias reshapes Ip/Ig2 split", "[KorenPentode]")
{
    KorenPentode p { pentode_params::k6AS6 };
    const double Va = 70.0;
    const double Vg1 = -0.6;
    const double Vg2 = 150.0;

    const auto mild = p.evaluate(Va, Vg1, Vg2, -0.2);
    const auto hard = p.evaluate(Va, Vg1, Vg2, -3.0);

    REQUIRE(std::isfinite(mild.Ip));
    REQUIRE(std::isfinite(hard.Ip));
    REQUIRE(std::isfinite(mild.Ig2));
    REQUIRE(std::isfinite(hard.Ig2));

    // More-negative suppressor should reduce plate conduction and
    // increase screen-current capture under the same operating point.
    REQUIRE(hard.Ip < mild.Ip);
    REQUIRE(hard.Ig2 > mild.Ig2);
}
