// ─────────────────────────────────────────────────────────────────────────────
// NeuralResidualLayer — neural-style residual post-processor
//
// Two execution paths under one API:
//   1) Bootstrap residual MLP (always available, no external deps)
//   2) RTNeural runtime model (optional, loaded from JSON)
//
//   y_final = (1 - blend) * y_physics + blend * y_hybrid
//   y_hybrid = y_physics + residual(features)
//
// Design goals:
//   - Real-time safe in process() (no dynamic allocation / no locks)
//   - Stable under all finite inputs
//   - Deterministic fallback when RTNeural is disabled or model missing
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <string>

#if defined(VALVRA_USE_RTNEURAL) && VALVRA_USE_RTNEURAL
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include <RTNeural/RTNeural.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#endif

namespace valvra::dsp {

class NeuralResidualLayer
{
public:
    void prepare(double sampleRate) noexcept
    {
        const double fc = 7000.0; // tame HF fizz on residual branch
        residualAlpha_ = 1.0 - std::exp(-2.0 * 3.14159265358979323846
                                        * fc / std::max(sampleRate, 1.0));
        reset();
    }

    bool loadModelJson(const std::string& path) noexcept
    {
#if defined(VALVRA_USE_RTNEURAL) && VALVRA_USE_RTNEURAL
        try
        {
            std::ifstream jsonStream(path, std::ifstream::binary);
            if (! jsonStream.good())
            {
                clearModel();
                return false;
            }

            rtModel_.parseJson(jsonStream);
            rtModel_.reset();
            rtModelLoaded_ = true;
            return true;
        }
        catch (...)
        {
            clearModel();
            return false;
        }
#else
        (void) path;
        return false;
#endif
    }

    void clearModel() noexcept
    {
#if defined(VALVRA_USE_RTNEURAL) && VALVRA_USE_RTNEURAL
        rtModelLoaded_ = false;
        rtModel_.reset();
#endif
    }

    bool usingRtModel() const noexcept
    {
#if defined(VALVRA_USE_RTNEURAL) && VALVRA_USE_RTNEURAL
        return rtModelLoaded_;
#else
        return false;
#endif
    }

    void reset() noexcept
    {
        prevIn_ = 0.0;
        prevPhys_ = 0.0;
        prevOut_ = 0.0;
        residualLP_ = 0.0;
#if defined(VALVRA_USE_RTNEURAL) && VALVRA_USE_RTNEURAL
        if (rtModelLoaded_)
            rtModel_.reset();
#endif
    }

    double process(double input, double physicsOut, double blend) noexcept
    {
        if (! std::isfinite(input) || ! std::isfinite(physicsOut))
        {
            reset();
            return 0.0;
        }

        const double b = std::clamp(blend, 0.0, 1.0);
        if (b <= 1.0e-6)
        {
            prevIn_ = input;
            prevPhys_ = physicsOut;
            prevOut_ = physicsOut;
            residualLP_ = 0.0;
            return physicsOut;
        }

        // Five features: current input/output, first-order deltas, and a
        // mild nonlinearity that helps the residual react to saturation depth.
        const double dIn = input - prevIn_;
        const double dPhys = physicsOut - prevPhys_;
        const double sat = std::tanh(0.75 * physicsOut + 0.25 * input);
        const std::array<double, 5> f {
            input, physicsOut, dIn, dPhys, sat
        };

        double residual = 0.0;
#if defined(VALVRA_USE_RTNEURAL) && VALVRA_USE_RTNEURAL
        if (rtModelLoaded_)
        {
            try
            {
                residual = rtModel_.forward(f.data());
            }
            catch (...)
            {
                rtModelLoaded_ = false;
                residual = 0.0;
            }
        }
#endif

        if (! usingRtModel())
        {
            // Tiny 5->4->1 MLP with tanh activations and fixed bootstrap
            // weights. Conservative constants keep the residual subtle.
            std::array<double, 4> h {};
            for (int i = 0; i < 4; ++i)
            {
                double acc = b1_[static_cast<std::size_t>(i)];
                for (int j = 0; j < 5; ++j)
                    acc += w1_[static_cast<std::size_t>(i)]
                              [static_cast<std::size_t>(j)]
                           * f[static_cast<std::size_t>(j)];
                h[static_cast<std::size_t>(i)] = std::tanh(acc);
            }

            residual = b2_;
            for (int i = 0; i < 4; ++i)
                residual += w2_[static_cast<std::size_t>(i)]
                          * h[static_cast<std::size_t>(i)];
        }

        // Program dependence using prior output (light recurrence).
        residual += 0.035 * std::tanh(prevOut_ * 1.2);

        // LP on residual branch keeps transition continuity and avoids sharp
        // HF alias-like accents when blend is automated.
        residualLP_ += residualAlpha_ * (residual - residualLP_);

        // Cap residual contribution to keep headroom behavior predictable.
        const double shapedResidual = std::clamp(residualLP_, -0.35, 0.35);
        const double hybrid = physicsOut + shapedResidual;
        const double out = physicsOut + b * (hybrid - physicsOut);

        prevIn_ = input;
        prevPhys_ = physicsOut;
        prevOut_ = out;
        return out;
    }

private:
    std::array<std::array<double, 5>, 4> w1_ {{
        {{ 0.38,  0.47, -0.06,  0.12,  0.31 }},
        {{-0.22,  0.41,  0.14, -0.19,  0.27 }},
        {{ 0.11, -0.29,  0.36,  0.24, -0.08 }},
        {{ 0.19,  0.08, -0.22,  0.33,  0.29 }}
    }};
    std::array<double, 4> b1_ {{ 0.01, -0.02, 0.00, 0.03 }};
    std::array<double, 4> w2_ {{ 0.07, -0.05, 0.06, 0.04 }};
    double b2_ { 0.0 };

    double residualAlpha_ { 0.4 };
    double prevIn_ { 0.0 };
    double prevPhys_ { 0.0 };
    double prevOut_ { 0.0 };
    double residualLP_ { 0.0 };

#if defined(VALVRA_USE_RTNEURAL) && VALVRA_USE_RTNEURAL
    using RTModel = RTNeural::ModelT<double, 5, 1,
        RTNeural::DenseT<double, 5, 8>,
        RTNeural::TanhActivationT<double, 8>,
        RTNeural::DenseT<double, 8, 1>>;
    RTModel rtModel_ {};
    bool rtModelLoaded_ { false };
#endif
};

} // namespace valvra::dsp
