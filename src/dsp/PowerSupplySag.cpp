#include "PowerSupplySag.h"

namespace valvra::dsp {
static_assert(psu_presets::k5U4GB.Z_internal > psu_presets::kGZ34.Z_internal,
              "5U4GB has higher impedance than GZ34");
static_assert(psu_presets::kSolidState.Z_internal < psu_presets::kGZ34.Z_internal,
              "Solid-state rectifier should have lowest impedance");
} // namespace valvra::dsp
