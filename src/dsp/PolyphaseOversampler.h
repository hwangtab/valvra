// ─────────────────────────────────────────────────────────────────────────────
// PolyphaseOversampler — linear-phase FIR polyphase up/downsampler
//
// Each tube stage produces harmonics up to the hard-clipping edge; at 44.1 kHz
// the 7th harmonic of a 6 kHz input already aliases. Standard practice is to
// oversample N× around the nonlinearity, process in the upsampled domain, then
// decimate.  The polyphase half-band structure achieves this with only N−1
// multiplies per output sample (versus N² for the naive approach).
//
// This implementation is deliberately **dependency-free** — we do NOT pull in
// chowdsp_utils here because that library assumes JUCE is present, which
// conflicts with our headers-only core DSP library. A minimal windowed-sinc
// FIR with polyphase decomposition is sufficient for 4× / 8×.
//
// Reference:
//   docs/07 §5 (polyphase half-band rationale)
//   Vaidyanathan, "Multirate Systems and Filter Banks" (1993) ch. 4
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace valvra::dsp {

// ─────────────────────────────────────────────────────────────────────────────
// Generate an N-tap low-pass FIR with Kaiser window.
// Cutoff is expressed as a fraction of the output Nyquist (0.5 = half-band).
// ─────────────────────────────────────────────────────────────────────────────
inline std::vector<double> kaiserLowpass(int numTaps, double cutoff, double beta)
{
    std::vector<double> h(static_cast<std::size_t>(numTaps));
    const double M = numTaps - 1;

    // Compute I0(β) for Kaiser window
    auto besselI0 = [](double x) {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 50; ++k)
        {
            term *= (x * 0.5 / k) * (x * 0.5 / k);
            sum  += term;
            if (term < 1e-15 * sum) break;
        }
        return sum;
    };
    const double I0beta = besselI0(beta);

    for (int n = 0; n < numTaps; ++n)
    {
        const double x = 2.0 * cutoff * (n - M * 0.5);
        const double sinc = (std::abs(x) < 1e-12)
            ? 1.0
            : std::sin(M_PI * x) / (M_PI * x);

        const double r  = 2.0 * n / M - 1.0;
        const double kw = besselI0(beta * std::sqrt(1.0 - r * r)) / I0beta;

        h[static_cast<std::size_t>(n)] = 2.0 * cutoff * sinc * kw;
    }
    return h;
}

// ─────────────────────────────────────────────────────────────────────────────
// PolyphaseOversampler — N× upsampler and decimator pair
//
// Usage:
//   PolyphaseOversampler<4> os;
//   os.prepare();
//   for each input sample x:
//       auto up = os.upsample(x);           // array of 4 samples
//       for (int i = 0; i < 4; ++i)
//           up[i] = nonlinear(up[i]);
//       const double y = os.downsample(up); // single output sample
// ─────────────────────────────────────────────────────────────────────────────
template <int Factor>
class PolyphaseOversampler
{
public:
    static_assert(Factor >= 2, "Factor must be at least 2");

    PolyphaseOversampler()
    {
        prepare();
    }

    // Design filters sized for the requested factor.  The legacy design
    // reused one 65-tap filter for every factor; since a Kaiser FIR has a
    // FIXED transition width in cycles/sample, at 4×+ that transition
    // band swallowed the audio top octave (−0.95 dB @ 18 kHz at 4×,
    // −2.2 dB @ 10 kHz at 16×) and left content just above base Nyquist
    // attenuated only ~10 dB before folding.  Scaling taps ∝ Factor
    // keeps the transition width CONSTANT in absolute Hz: passband to
    // ~19.7 kHz within 0.1 dB and ≥90 dB at base Nyquist, every factor.
    void prepare()
    {
        constexpr double kBeta = 8.6;   // ≈ 90 dB Kaiser stop-band

        // Centre the transition band just below base Nyquist so the
        // stop edge lands AT base Nyquist (no aliasable gap).  The
        // Kaiser main-lobe transition width is ≈ 5.71/kTaps cycles/sample.
        const double dNu    = 5.71 / static_cast<double>(kTaps);
        const double cutoff = 0.5 / static_cast<double>(Factor) - 0.5 * dNu;

        // Up-sampling path: filter gain = Factor to compensate for zero-stuff loss
        auto h_up = kaiserLowpass(kTaps, cutoff, kBeta);
        for (auto& v : h_up) v *= static_cast<double>(Factor);

        // Down-sampling path: unity gain
        auto h_down = kaiserLowpass(kTaps, cutoff, kBeta);

        coeffs_up_   = std::move(h_up);
        coeffs_down_ = std::move(h_down);

        bufferUp_.fill(0.0);
        bufferDown_.fill(0.0);
        writeIdx_up_   = 0;
        writeIdx_down_ = 0;
    }

    void reset()
    {
        bufferUp_.fill(0.0);
        bufferDown_.fill(0.0);
        writeIdx_up_   = 0;
        writeIdx_down_ = 0;
    }

    // Upsample one input sample to `Factor` output samples.
    // TRUE polyphase decomposition: the zero-stuffed stream is nonzero
    // only every Factor-th tap, so output phase p needs only the taps
    // h[p + k·Factor] against the BASE-rate input history — kTaps
    // multiplies total per base sample instead of kTaps·Factor.  With
    // the factor-scaled filter lengths this keeps the upsampler's cost
    // constant across factors.
    std::array<double, Factor> upsample(double x) noexcept
    {
        // NaN-recovery: a non-finite input would permanently poison the FIR
        // delay line.  Treat it as zero and continue.
        if (! std::isfinite(x)) x = 0.0;
        std::array<double, Factor> out {};

        bufferUp_[static_cast<std::size_t>(writeIdx_up_)] = x;
        const int newest = writeIdx_up_;
        writeIdx_up_ = (writeIdx_up_ + 1) & kMask;

        const int N = static_cast<int>(coeffs_up_.size());
        for (int phase = 0; phase < Factor; ++phase)
        {
            double sum = 0.0;
            int idx = newest;
            for (int j = phase; j < N; j += Factor)
            {
                sum += coeffs_up_[static_cast<std::size_t>(j)] *
                       bufferUp_[static_cast<std::size_t>(idx)];
                idx = (idx - 1) & kMask;
            }
            out[static_cast<std::size_t>(phase)] = sum;
        }
        return out;
    }

    // Decimate `Factor` samples to one output sample.
    double downsample(const std::array<double, Factor>& in) noexcept
    {
        // Push all Factor samples through the filter; return only the last
        // (we decimate by Factor, keeping every Factor-th filtered sample).
        double out = 0.0;
        for (int phase = 0; phase < Factor; ++phase)
        {
            const double s = in[static_cast<std::size_t>(phase)];
            bufferDown_[static_cast<std::size_t>(writeIdx_down_)] =
                std::isfinite(s) ? s : 0.0;
            writeIdx_down_ = (writeIdx_down_ + 1) & kMask;

            // On the final sub-sample of each input cycle, compute the output.
            // The decimation filter is linear-phase (symmetric taps,
            // h[k] = h[N−1−k]), so fold the convolution into pairs:
            //   Σ h[k]·(x[k] + x[N−1−k])  for k < N/2  (+ centre tap).
            // kTaps = 64·Factor+1 is always odd, so there is exactly one
            // centre tap.  This halves the per-output-sample multiply count
            // of the full-length decimation convolution — the heaviest FIR
            // in the round trip — with a bit-exact result.
            if (phase == Factor - 1)
            {
                const int N = static_cast<int>(coeffs_down_.size());
                const int newest = (writeIdx_down_ - 1) & kMask;
                const int half = N / 2;   // N odd → half = (N−1)/2
                double sum = 0.0;
                for (int k = 0; k < half; ++k)
                {
                    const int i1 = (newest - k) & kMask;
                    const int i2 = (newest - (N - 1 - k)) & kMask;
                    sum += coeffs_down_[static_cast<std::size_t>(k)]
                         * (bufferDown_[static_cast<std::size_t>(i1)]
                          + bufferDown_[static_cast<std::size_t>(i2)]);
                }
                const int ic = (newest - half) & kMask;
                sum += coeffs_down_[static_cast<std::size_t>(half)]
                     * bufferDown_[static_cast<std::size_t>(ic)];
                out = sum;
            }
        }
        return out;
    }

    static constexpr int factor() noexcept { return Factor; }

    // FIR tap count: 64·Factor + 1.  Odd (linear phase, integer group
    // delay), and the constant absolute transition width makes the
    // round-trip base-rate latency IDENTICAL (63 samples) at every
    // factor — switching oversampling quality no longer moves PDC.
    static constexpr int kTaps = 64 * Factor + 1;

    // Round-trip (up + down) latency, expressed in *base-rate* samples.
    //
    // Each linear-phase FIR has group delay (kTaps−1)/2 at the upsampled
    // rate; two in series = (kTaps−1) upsampled samples.  The downsampler
    // emits its output on the final phase of each base sample and its
    // convolution reads from the just-written position, which advances
    // the alignment by one base sample — hence the −1.  Verified by
    // test_polyphase_oversampler.cpp "reported latency matches measured
    // group delay".
    static constexpr int latencyInBaseSamples() noexcept
    {
        return (kTaps - 1) / Factor - 1;  // = 63 for every factor
    }

private:
    // Circular buffer size: next power of 2 ≥ filter length.
    static constexpr std::size_t kBufferSize = []
    {
        std::size_t s = 1;
        while (s < static_cast<std::size_t>(kTaps)) s <<= 1;
        return s;
    }();
    static constexpr int kMask = static_cast<int>(kBufferSize - 1);

    std::vector<double> coeffs_up_;
    std::vector<double> coeffs_down_;

    std::array<double, kBufferSize> bufferUp_   {};
    std::array<double, kBufferSize> bufferDown_ {};
    int writeIdx_up_   { 0 };
    int writeIdx_down_ { 0 };
};

} // namespace valvra::dsp
