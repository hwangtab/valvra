// Unit tests for ComponentVariation
// Validation: reproducibility (same seed → same result), spread matches σ

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <vector>
#include "ComponentVariation.h"

using Catch::Approx;
using namespace valvra::dsp;

TEST_CASE("Same seed produces identical variation", "[variation][reproducibility]")
{
    const uint64_t seed = 0x12345678abcdef01ULL;
    auto v1 = makeVariation(seed);
    auto v2 = makeVariation(seed);

    REQUIRE(v1.tube_G_scale == v2.tube_G_scale);
    REQUIRE(v1.tube_mu_scale == v2.tube_mu_scale);
    REQUIRE(v1.trafo_Ms_scale == v2.trafo_Ms_scale);
    REQUIRE(v1.Ck_scale == v2.Ck_scale);
}

TEST_CASE("Different seeds produce different variations", "[variation][diversity]")
{
    auto v1 = makeVariation(1);
    auto v2 = makeVariation(2);

    // At least one dimension must differ
    bool any_diff = false;
    any_diff |= (v1.tube_G_scale  != v2.tube_G_scale);
    any_diff |= (v1.tube_mu_scale != v2.tube_mu_scale);
    any_diff |= (v1.trafo_Ms_scale != v2.trafo_Ms_scale);
    any_diff |= (v1.Ck_scale != v2.Ck_scale);

    REQUIRE(any_diff);
}

TEST_CASE("Population mu spread approximates target sigma", "[variation][statistics]")
{
    // Dempwolf academic spread: μ 17% over 3 tubes → σ ≈ 8%
    // Sample 1000 instances and confirm empirical σ is in [5%, 12%]
    constexpr int N = 1000;
    double sum = 0.0;
    double sum_sq = 0.0;

    for (int i = 0; i < N; ++i)
    {
        auto v = makeVariation(static_cast<uint64_t>(i) * 0x9E3779B97F4A7C15ULL);
        sum    += v.tube_mu_scale;
        sum_sq += v.tube_mu_scale * v.tube_mu_scale;
    }

    const double mean = sum / N;
    const double var  = sum_sq / N - mean * mean;
    const double std  = std::sqrt(var);

    REQUIRE(mean == Approx(1.0).epsilon(0.03));  // centered near 1.0
    REQUIRE(std > 0.05);   // > 5%
    REQUIRE(std < 0.12);   // < 12%
}

TEST_CASE("Distribution presets widen Monte Carlo spread predictably",
          "[variation][distribution]")
{
    constexpr int N = 1000;
    auto measureStd = [](VariationDistribution d)
    {
        double sum = 0.0;
        double sumSq = 0.0;
        for (int i = 0; i < N; ++i)
        {
            const auto v = makeVariation(
                static_cast<uint64_t>(i) * 0x9E3779B97F4A7C15ULL, d);
            sum += v.tube_mu_scale;
            sumSq += v.tube_mu_scale * v.tube_mu_scale;
        }
        const double mean = sum / N;
        return std::sqrt(sumSq / N - mean * mean);
    };

    const double vintage = measureStd(VariationDistribution::Vintage);
    const double modern  = measureStd(VariationDistribution::Modern);
    const double worn    = measureStd(VariationDistribution::Worn);
    const double wild    = measureStd(VariationDistribution::Wild);

    REQUIRE(vintage < modern);
    REQUIRE(modern < worn);
    REQUIRE(worn < wild);
}

TEST_CASE("Applied variation changes Koren params", "[variation][integration]")
{
    const auto base = params::kRSD_1;
    const auto v    = makeVariation(42);
    const auto p    = applyVariation(base, v);

    REQUIRE(p.G != base.G);    // scaled
    REQUIRE(p.mu != base.mu);

    // But stays within reasonable bounds (no blowup)
    REQUIRE(p.mu  > base.mu  * 0.7);
    REQUIRE(p.mu  < base.mu  * 1.3);
}

TEST_CASE("Applied variation to JA params preserves bounds", "[variation][ja]")
{
    const auto base = ja_params::kNiPermalloy_Peerless;
    for (uint64_t seed = 1; seed <= 50; ++seed)
    {
        auto v = makeVariation(seed);
        auto p = applyVariation(base, v);

        REQUIRE(p.Ms > 0.0);
        REQUIRE(p.a  > 0.0);
        REQUIRE(p.k  > 0.0);
        REQUIRE(p.Ms < base.Ms * 2.0);
    }
}

TEST_CASE("Era pathologies are zero on Modern, graded on vintage eras",
          "[variation][era]")
{
    // docs/34 §3.6/§3.7 — coupling-cap leakage and carbon excess noise are
    // properties the component ERA has or hasn't: hard zero on Modern,
    // population means graded Vintage < Worn < Wild, hard caps respected.
    double sumLeak[4] = {}, sumNoise[4] = {};
    constexpr int N = 300;
    const VariationDistribution dists[4] = {
        VariationDistribution::Modern, VariationDistribution::Vintage,
        VariationDistribution::Worn,   VariationDistribution::Wild };

    for (int d = 0; d < 4; ++d)
        for (int s = 0; s < N; ++s)
        {
            const auto v = makeVariation(0xABC000ULL + s, dists[d]);
            REQUIRE(v.couplingLeak_ratio >= 0.0);
            REQUIRE(v.couplingLeak_ratio <= 0.05);
            REQUIRE(v.excessNoise_offset >= 0.0);
            REQUIRE(v.excessNoise_offset <= 0.45);
            sumLeak[d]  += v.couplingLeak_ratio;
            sumNoise[d] += v.excessNoise_offset;
        }

    REQUIRE(sumLeak[0] == 0.0);          // Modern: no leakage at all
    REQUIRE(sumNoise[0] == 0.0);         // Modern: no excess noise
    REQUIRE(sumLeak[1] > 0.0);
    REQUIRE(sumLeak[2] > sumLeak[1]);    // Worn leaks more than Vintage
    REQUIRE(sumLeak[3] > sumLeak[2]);    // Wild leaks most
    REQUIRE(sumNoise[2] > sumNoise[1]);
    REQUIRE(sumNoise[3] > sumNoise[2]);
}

TEST_CASE("Tube G and mu draws are weakly correlated, marginals preserved",
          "[variation][correlation]")
{
    // docs/34 §1.6 residual — perveance and amplification factor ride the
    // same cathode lottery (ρ ≈ 0.4).  The correlated construction must
    // keep each marginal σ at its documented value.
    constexpr int N = 600;
    double mG = 0.0, mMu = 0.0;
    std::vector<double> gs, mus;
    gs.reserve(N); mus.reserve(N);
    for (int s = 0; s < N; ++s)
    {
        const auto v = makeVariation(0x5EED0000ULL + s,
                                     VariationDistribution::Modern);
        gs.push_back(v.tube_G_scale);
        mus.push_back(v.tube_mu_scale);
        mG += v.tube_G_scale; mMu += v.tube_mu_scale;
    }
    mG /= N; mMu /= N;
    double sG = 0.0, sMu = 0.0, cov = 0.0;
    for (int i = 0; i < N; ++i)
    {
        sG  += (gs[i] - mG) * (gs[i] - mG);
        sMu += (mus[i] - mMu) * (mus[i] - mMu);
        cov += (gs[i] - mG) * (mus[i] - mMu);
    }
    sG = std::sqrt(sG / N); sMu = std::sqrt(sMu / N);
    const double rho = cov / N / (sG * sMu);

    INFO("sigma_G=" << sG << " sigma_mu=" << sMu << " rho=" << rho);
    REQUIRE(sMu == Approx(0.08).epsilon(0.25));    // Dempwolf μ spread kept
    REQUIRE(sG == Approx(0.12).epsilon(0.25));     // perveance spread kept
    REQUIRE(rho > 0.15);
    REQUIRE(rho < 0.65);
}
