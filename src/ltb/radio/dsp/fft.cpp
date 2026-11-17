// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "fft.hpp"

// standard
#include <algorithm>
#include <bit>
#include <cassert>
#include <cmath>
#include <functional>
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

template < typename Sample >
struct ApplyWindow
{
    auto operator( )( Sample const& sample, float32 const weight ) const -> Complex
    {
        return Complex( sample ) * weight;
    }
};

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
    SpectrumAnalyzer( ).power_spectrum_db( samples, output );
}

auto power_spectrum_db( std::span< float32 const > samples, std::vector< float32 >& output ) -> void
{
    SpectrumAnalyzer( ).power_spectrum_db( samples, output );
}

auto SpectrumAnalyzer::power_spectrum_db(
    std::span< Complex const > samples,
    std::vector< float32 >&    output
) -> void
{
    this->windowed_fft( samples );

    // Move DC to the middle.
    std::ranges::rotate(
        spectrum_,
        spectrum_.begin( ) + static_cast< std::ptrdiff_t >( spectrum_.size( ) / 2UZ )
    );

    output.resize( spectrum_.size( ) );
    std::ranges::transform( spectrum_, output.begin( ), ToDecibels{ window_sum_ } );
}

auto SpectrumAnalyzer::power_spectrum_db(
    std::span< float32 const > samples,
    std::vector< float32 >&    output
) -> void
{
    this->windowed_fft( samples );

    // A real sine splits its power between the positive and negative frequencies.
    auto const positive = std::span( spectrum_ ).first( spectrum_.size( ) / 2UZ + 1UZ );
    output.resize( positive.size( ) );
    std::ranges::transform( positive, output.begin( ), ToDecibels{ 0.5F * window_sum_ } );
}

template < typename Sample >
auto SpectrumAnalyzer::windowed_fft( std::span< Sample const > samples ) -> void
{
    auto const size = samples.size( );
    if ( window_.size( ) != size )
    {
        window_     = std::views::iota( 0UZ, size )
                    | std::views::transform( std::bind_back( hann, size ) )
                    | std::ranges::to< std::vector >( );
        window_sum_ = std::reduce( window_.begin( ), window_.end( ), 0.0F );
    }

    spectrum_.resize( size );
    std::ranges::transform( samples, window_, spectrum_.begin( ), ApplyWindow< Sample >{ } );
    fft( spectrum_ );
}

} // namespace ltb::radio::dsp
