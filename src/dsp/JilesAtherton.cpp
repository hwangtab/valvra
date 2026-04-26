// JilesAtherton.cpp — header-only logic; this file holds static checks.

#include "JilesAtherton.h"

namespace valvra::dsp {

static_assert(ja_params::kNiPermalloy_Peerless.Ms > 0.0,
              "Peerless Ms must be positive");
static_assert(ja_params::kNiPermalloy_Peerless.c >= 0.0 &&
              ja_params::kNiPermalloy_Peerless.c <= 1.0,
              "Reversibility c must be in [0,1]");
static_assert(ja_params::kSiSteel_M6.Ms > ja_params::kNiPermalloy_Peerless.Ms,
              "Si-steel should saturate higher than permalloy");

} // namespace valvra::dsp
