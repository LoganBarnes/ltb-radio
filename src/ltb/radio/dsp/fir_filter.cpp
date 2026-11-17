// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "fir_filter.hpp"

// standard
#include <algorithm>
#include <cmath>
#include <functional>
#include <numbers>
#include <numeric>
#include <ranges>

namespace ltb::radio::dsp
{
namespace
{

constexpr auto blackman_transition_factor = 5.5;
constexpr auto max_tap_count              = 2047UZ;

struct BlackmanSincTap
{
    float64 normalized_cutoff;
    float64 last_index;

    auto operator( )( std::size_t const i ) const -> float32
    {
        constexpr auto pi = std::numbers::pi;

        auto const n      = static_cast< float64 >( i );
        auto const t      = n - 0.5 * last_index;
        auto const sinc   = t == 0.0 ? 2.0 * normalized_cutoff
                                     : std::sin( 2.0 * pi * normalized_cutoff * t ) / ( pi * t );
        auto const window = 0.42 - 0.5 * std::cos( 2.0 * pi * n / last_index )
                          + 0.08 * std::cos( 4.0 * pi * n / last_index );
        return static_cast< float32 >( sinc * window );
    }
};

template < typename Sample >
struct WeightSample
{
    auto operator( )( float32 const tap, Sample const& sample ) const -> Sample
    {
        return sample * tap;
    }
};

auto to_offset( std::size_t const index ) -> std::ptrdiff_t
{
    return static_cast< std::ptrdiff_t >( index );
}

} // namespace

auto design_low_pass(
    float64 const cutoff_hz,
    float64 const transition_hz,
    float64 const sample_rate_hz
) -> std::vector< float32 >
{
    auto const wanted_taps = static_cast< std::size_t >(
        std::ceil( blackman_transition_factor * sample_rate_hz / std::max( transition_hz, 1.0 ) )
    );
    auto const tap_count = std::clamp( wanted_taps | 1UZ, 3UZ, max_tap_count );

    auto taps = std::views::iota( 0UZ, tap_count )
              | std::views::transform(
                    BlackmanSincTap{
                        .normalized_cutoff = cutoff_hz / sample_rate_hz,
                        .last_index        = static_cast< float64 >( tap_count - 1UZ ),
                    }
              )
              | std::ranges::to< std::vector >( );

    auto const sum = std::reduce( taps.begin( ), taps.end( ), 0.0F );
    std::ranges::transform( taps, taps.begin( ), std::bind_back( std::divides<>{ }, sum ) );
    return taps;
}

auto design_decimation_filter(
    float64 const     passband_hz,
    float64 const     input_rate_hz,
    std::size_t const decimation
) -> std::vector< float32 >
{
    // Anything above (output_rate - passband) folds back into the passband after decimation.
    auto const output_rate = input_rate_hz / static_cast< float64 >( decimation );
    auto const stop_hz     = output_rate - passband_hz;
    return design_low_pass( 0.5 * ( passband_hz + stop_hz ), stop_hz - passband_hz, input_rate_hz );
}

template < typename Sample >
DecimatingFir< Sample >::DecimatingFir( )
    : DecimatingFir( { 1.0F }, 1UZ )
{
}

template < typename Sample >
DecimatingFir< Sample >::DecimatingFir( std::vector< float32 > taps, std::size_t const decimation )
    : reversed_taps_( std::move( taps ) )
    , decimation_( std::max( decimation, 1UZ ) )
{
    if ( reversed_taps_.empty( ) )
    {
        reversed_taps_ = { 1.0F };
    }
    std::ranges::reverse( reversed_taps_ );
    this->reset( );
}

template < typename Sample >
auto DecimatingFir< Sample >::process(
    std::span< Sample const > input,
    std::vector< Sample >&    output
) -> void
{
    history_.insert( history_.end( ), input.begin( ), input.end( ) );

    auto const tap_count = reversed_taps_.size( );
    for ( ; next_ + tap_count <= history_.size( ); next_ += decimation_ )
    {
        output.push_back(
            std::transform_reduce(
                reversed_taps_.begin( ),
                reversed_taps_.end( ),
                history_.begin( ) + to_offset( next_ ),
                Sample{ },
                std::plus<>{ },
                WeightSample< Sample >{ }
            )
        );
    }

    auto const consumed = std::min( next_, history_.size( ) );
    history_.erase( history_.begin( ), history_.begin( ) + to_offset( consumed ) );
    next_ -= consumed;
}

template < typename Sample >
auto DecimatingFir< Sample >::reset( ) -> void
{
    // Start with a full window of zeros so the first output lines up with the first input.
    history_.assign( reversed_taps_.size( ) - 1UZ, Sample{ } );
    next_ = 0UZ;
}

template < typename Sample >
auto DecimatingFir< Sample >::decimation( ) const -> std::size_t
{
    return decimation_;
}

template < typename Sample >
auto DecimatingFir< Sample >::tap_count( ) const -> std::size_t
{
    return reversed_taps_.size( );
}

template class DecimatingFir< float32 >;
template class DecimatingFir< Complex >;

} // namespace ltb::radio::dsp
