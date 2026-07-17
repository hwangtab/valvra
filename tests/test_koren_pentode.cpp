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

TEST_CASE("KorenPentode: analytic dIp/dVa matches finite difference",
          "[KorenPentode][deriv]")
{
    // The plate-node Newton in TubeStage relies on the analytic ∂Ip/∂Va.
    // Validate it against a central finite difference across the operating
    // range, including the below-knee region and the secondary-emission
    // region (Vg2 > Va) where the derivative has an extra term.
    for (const auto& params : { pentode_params::k6AS6, pentode_params::kEF86 })
    {
        KorenPentode p { params };
        const double Vg1 = -1.0;
        const double Vg2 = 150.0;
        const double Vg3 = -0.4;
        const double h = 1.0e-2;

        for (double Va : { 20.0, 60.0, 120.0, 200.0, 300.0 })
        {
            const auto c  = p.evaluate(Va, Vg1, Vg2, Vg3);
            const double ipHi = p.evaluate(Va + h, Vg1, Vg2, Vg3).Ip;
            const double ipLo = p.evaluate(Va - h, Vg1, Vg2, Vg3).Ip;
            const double fd = (ipHi - ipLo) / (2.0 * h);

            INFO("Va=" << Va << " analytic=" << c.dIpdVa << " fd=" << fd);
            // Allow a small absolute floor plus a relative tolerance — the
            // finite difference itself carries O(h²) error and the clamps
            // create kinks the central difference smears.
            const double tol = 5.0e-6 + 0.06 * std::abs(fd);
            REQUIRE(std::abs(c.dIpdVa - fd) < tol);
        }
    }
}
