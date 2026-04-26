// KorenTriode.cpp
// All logic is header-only for inlining in the hot audio path.
// This file exists only to satisfy the CMake target list and to hold
// out-of-line static assertions / param validation.

#include "KorenTriode.h"

namespace valvra::dsp {

static_assert(params::kRSD_1.mu  > 0.0,  "12AX7 RSD_1 μ must be positive");
static_assert(params::kRSD_1.G   > 0.0,  "12AX7 RSD_1 G must be positive");
static_assert(params::kRSD_1.gamma > 1.0,"Langmuir exponent must be > 1.0");
static_assert(params::kEHX_1.mu  < params::kRSD_1.mu,
              "EHX_1 should have lower μ than RSD_1 (measured spread)");

} // namespace valvra::dsp
