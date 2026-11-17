// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "fft.hpp"

// standard
#include <algorithm>
#include <bit>
#include <cassert>
#include <cmath>
#include <numbers>
#include <numeric>
#include <ranges>

namespace ltb::radio::dsp
{
namespace
{

auto bit_reverse( std::size_t value, int const bit_count ) -> std::size_t
{
    auto result = 0UZ;
    for ( auto bit = 0; bit < bit_count; ++bit )
    {
        result = ( result << 1UZ ) | ( value & 1UZ );
        value >>= 1UZ;
    }
    return result;
}

auto hann( std::size_t const i, std::size_t const size ) -> float32
{
    auto const phase
        = 2.0 * std::numbers::pi * static_cast< float64 >( i ) / static_cast< float64 >( size );
    return static_cast< float32 >( 0.5 - 0.5 * std::cos( phase ) );
}

// Windows the samples and returns the sum of the window (the gain of a tone at a bin center).
template < typename Sample >
auto windowed_fft( std::span< Sample const > samples, std::vector< Complex >& spectrum ) -> float32
{
    auto const size = samples.size( );
    spectrum.resize( size );

    auto window_sum = 0.0F;
    for ( auto i = 0UZ; i < size; ++i )
    {
        auto const w  = hann( i, size );
        spectrum[ i ] = Complex( samples[ i ] ) * w;
        window_sum += w;
    }
    fft( spectrum );
    return window_sum;
}

struct ToDecibels
{
    float32 full_scale;

    auto operator( )( Complex const& bin ) const -> float32
    {
        return 20.0F * std::log10( std::max( std::abs( bin ) / full_scale, 1e-12F ) );
    }
};

} // namespace

auto fft( std::span< Complex > data ) -> void
{
    auto const size = data.size( );
    assert( std::has_single_bit( size ) );
    if ( size < 2UZ )
    {
        return;
    }

    auto const bit_count = std::countr_zero( size );
    for ( auto i = 0UZ; i < size; ++i )
    {
        if ( auto const j = bit_reverse( i, bit_count ); i < j )
        {
            std::swap( data[ i ], data[ j ] );
        }
    }

    for ( auto length = 2UZ; length <= size; length <<= 1UZ )
    {
        auto const half = length / 2UZ;
        auto const step
            = std::polar( 1.0, -2.0 * std::numbers::pi / static_cast< float64 >( length ) );

        for ( auto start = 0UZ; start < size; start += length )
        {
            auto twiddle = std::complex< float64 >( 1.0 );
            for ( auto k = 0UZ; k < half; ++k )
            {
                auto const even = data[ start + k ];
                auto const odd  = data[ start + k + half ] * Complex( twiddle );

                data[ start + k ]        = even + odd;
                data[ start + k + half ] = even - odd;
                twiddle *= step;
            }
        }
    }
}

auto power_spectrum_db( std::span< Complex const > samples, std::vector< float32 >& output ) -> void
{
    auto       spectrum   = std::vector< Complex >( );
    auto const window_sum = windowed_fft( samples, spectrum );

    // Move DC to the middle.
    std::ranges::rotate(
        spectrum,
        spectrum.begin( ) + static_cast< std::ptrdiff_t >( spectrum.size( ) / 2UZ )
    );

    output.resize( spectrum.size( ) );
    std::ranges::transform( spectrum, output.begin( ), ToDecibels{ window_sum } );
}

auto power_spectrum_db( std::span< float32 const > samples, std::vector< float32 >& output ) -> void
{
    auto       spectrum   = std::vector< Complex >( );
    auto const window_sum = windowed_fft( samples, spectrum );

    // A real sine splits its power between the positive and negative frequencies.
    auto const positive = std::span( spectrum ).first( spectrum.size( ) / 2UZ + 1UZ );
    output.resize( positive.size( ) );
    std::ranges::transform( positive, output.begin( ), ToDecibels{ 0.5F * window_sum } );
}

} // namespace ltb::radio::dsp
