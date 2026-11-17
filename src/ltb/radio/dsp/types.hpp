// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/utils/types.hpp"

// standard
#include <complex>

namespace ltb::radio::dsp
{

using Complex = std::complex< float32 >;

enum class Modulation
{
    Am,
    Fm,
    // VHF omnidirectional range: AM with navigation tones.
    Vor,
    // Instrument landing system localizer or glideslope: AM with 90 and 150 Hz tones.
    Ils,
};

} // namespace ltb::radio::dsp
