// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/radio/dsp/types.hpp"

// standard
#include <span>
#include <vector>

namespace ltb::radio::dsp
{

// Shifts a complex signal down by `shift_hz`: a tone at +shift_hz moves to 0 Hz.
class Mixer
{
public:
    auto set_shift( float64 shift_hz, float64 sample_rate_hz ) -> void;

    // Overwrites `output` with the shifted input.
    auto process( std::span< Complex const > input, std::vector< Complex >& output ) -> void;

private:
    std::complex< float64 > phasor_ = 1.0;
    std::complex< float64 > step_   = 1.0;
};

} // namespace ltb::radio::dsp
