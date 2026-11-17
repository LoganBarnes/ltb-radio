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

// Windowed-sinc (Blackman) low-pass taps with unity DC gain. The tap count is derived from the
// transition width (the distance between the pass and stop band edges) and is always odd.
[[nodiscard( "Pure computation" )]]
auto design_low_pass( float64 cutoff_hz, float64 transition_hz, float64 sample_rate_hz )
    -> std::vector< float32 >;

// Taps for decimating by `decimation` while keeping [-passband_hz, passband_hz] free of aliases.
[[nodiscard( "Pure computation" )]]
auto design_decimation_filter( float64 passband_hz, float64 input_rate_hz, std::size_t decimation )
    -> std::vector< float32 >;

// A streaming FIR filter that keeps every `decimation`-th output. State is kept between calls,
// so a signal can be processed in blocks of any size.
template < typename Sample >
class DecimatingFir
{
public:
    DecimatingFir( );
    DecimatingFir( std::vector< float32 > taps, std::size_t decimation );

    // Appends the filtered and decimated samples to `output`.
    auto process( std::span< Sample const > input, std::vector< Sample >& output ) -> void;

    auto reset( ) -> void;

    [[nodiscard( "Const getter" )]]
    auto decimation( ) const -> std::size_t;

    [[nodiscard( "Const getter" )]]
    auto tap_count( ) const -> std::size_t;

private:
    std::vector< float32 > reversed_taps_;
    std::size_t            decimation_;

    // Unconsumed input. `next_` is the start of the next output's window.
    std::vector< Sample > history_ = { };
    std::size_t           next_    = 0UZ;
};

extern template class DecimatingFir< float32 >;
extern template class DecimatingFir< Complex >;

} // namespace ltb::radio::dsp
