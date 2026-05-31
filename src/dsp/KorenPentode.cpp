#include "KorenPentode.h"

namespace valvra::dsp {

static_assert(pentode_params::k6AS6.G > 0.0, "6AS6 G must be positive");
static_assert(pentode_params::k6AS6.muG2 > 0.0, "6AS6 muG2 must be positive");
static_assert(pentode_params::k6AS6.kVb > 0.0, "6AS6 kVb must be positive");

} // namespace valvra::dsp
