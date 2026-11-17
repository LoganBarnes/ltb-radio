// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "mixer.hpp"

// standard
#include <numbers>
#include <ranges>

namespace ltb::radio::dsp
{

auto Mixer::set_shift( float64 const shift_hz, float64 const sample_rate_hz ) -> void
{
    step_ = std::polar( 1.0, -2.0 * std::numbers::pi * shift_hz / sample_rate_hz );
}

auto Mixer::process( std::span< Complex const > input, std::vector< Complex >& output ) -> void
{
    output.resize( input.size( ) );
    for ( auto&& [ in, out ] : std::views::zip( input, output ) )
    {
        out = in * Complex( phasor_ );
        phasor_ *= step_;
    }

    // Keep rounding errors from growing or shrinking the oscillator.
    phasor_ /= std::abs( phasor_ );
}

} // namespace ltb::radio::dsp
