// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "demodulators.hpp"

// standard
#include <algorithm>
#include <cmath>
#include <numbers>
#include <ranges>

namespace ltb::radio::dsp
{
namespace
{

// The per-sample coefficient of a single-pole filter with the given time constant.
auto smoothing_factor( float64 const tau_seconds, float64 const sample_rate_hz ) -> float32
{
    return static_cast< float32 >( 1.0 - std::exp( -1.0 / ( tau_seconds * sample_rate_hz ) ) );
}

struct Magnitude
{
    auto operator( )( Complex const& sample ) const -> float32
    {
        return std::abs( sample );
    }
};

} // namespace

auto envelope( std::span< Complex const > input, std::vector< float32 >& output ) -> void
{
    output.resize( input.size( ) );
    std::ranges::transform( input, output.begin( ), Magnitude{ } );
}

auto FmDiscriminator::configure( float64 const max_deviation_hz, float64 const sample_rate_hz )
    -> void
{
    scale_
        = static_cast< float32 >( sample_rate_hz / ( 2.0 * std::numbers::pi * max_deviation_hz ) );
}

auto FmDiscriminator::process( std::span< Complex const > input, std::vector< float32 >& output )
    -> void
{
    output.resize( input.size( ) );
    for ( auto&& [ in, out ] : std::views::zip( input, output ) )
    {
        out       = std::arg( in * std::conj( previous_ ) ) * scale_;
        previous_ = in;
    }
}

auto DcBlocker::configure( float64 const cutoff_hz, float64 const sample_rate_hz ) -> void
{
    pole_ = static_cast< float32 >(
        std::exp( -2.0 * std::numbers::pi * cutoff_hz / sample_rate_hz )
    );
}

auto DcBlocker::process( std::span< float32 > samples ) -> void
{
    for ( auto& sample : samples )
    {
        auto const input = sample;
        sample           = input - previous_input_ + pole_ * previous_output_;
        previous_input_  = input;
        previous_output_ = sample;
    }
}

auto DeEmphasis::configure( float64 const tau_seconds, float64 const sample_rate_hz ) -> void
{
    alpha_ = tau_seconds > 0.0 ? smoothing_factor( tau_seconds, sample_rate_hz ) : 1.0F;
}

auto DeEmphasis::process( std::span< float32 > samples ) -> void
{
    for ( auto& sample : samples )
    {
        previous_ += alpha_ * ( sample - previous_ );
        sample = previous_;
    }
}

auto Agc::configure( float64 const sample_rate_hz ) -> void
{
    attack_  = smoothing_factor( 0.005, sample_rate_hz );
    release_ = smoothing_factor( 0.5, sample_rate_hz );
}

auto Agc::process( std::span< float32 > samples ) -> void
{
    for ( auto& sample : samples )
    {
        auto const magnitude = std::abs( sample );
        level_ += ( magnitude > level_ ? attack_ : release_ ) * ( magnitude - level_ );
        sample *= this->gain( );
    }
}

auto Agc::gain( ) const -> float32
{
    return std::min( target_ / std::max( level_, 1e-9F ), max_gain_ );
}

auto CarrierNormalizer::configure( float64 const time_constant_s, float64 const sample_rate_hz )
    -> void
{
    smoothing_ = static_cast< float32 >( 1.0 / ( time_constant_s * sample_rate_hz ) );
}

auto CarrierNormalizer::process(
    std::span< float32 const > const envelope,
    std::vector< float32 >&          output
) -> void
{
    output.resize( envelope.size( ) );
    for ( auto&& [ amplitude, modulation ] : std::views::zip( envelope, output ) )
    {
        // A running average until a full time constant has been seen, so the estimate isn't
        // dominated by the first samples (or by a filter's start-up ramp).
        ++sample_count_;
        auto const weight = std::max( smoothing_, 1.0F / static_cast< float32 >( sample_count_ ) );
        carrier_ += weight * ( amplitude - carrier_ );
        modulation = amplitude / std::max( carrier_, 1e-9F ) - 1.0F;
    }
}

auto ToneMeter::configure(
    float64 const tone_hz,
    float64 const sample_rate_hz,
    float64 const time_constant_s
) -> void
{
    step_      = std::polar( 1.0, -2.0 * std::numbers::pi * tone_hz / sample_rate_hz );
    smoothing_ = 1.0 / ( time_constant_s * sample_rate_hz );
}

auto ToneMeter::process( std::span< float32 const > const samples ) -> void
{
    for ( auto const sample : samples )
    {
        correlation_
            += smoothing_ * ( static_cast< float64 >( sample ) * oscillator_ - correlation_ );
        smoothed_ += smoothing_ * ( correlation_ - smoothed_ );
        oscillator_ *= step_;
    }
    oscillator_ /= std::abs( oscillator_ );
}

auto ToneMeter::amplitude( ) const -> float32
{
    // A tone of amplitude A correlates to A / 2.
    return static_cast< float32 >( 2.0 * std::abs( smoothed_ ) );
}

auto ToneMeter::phase( ) const -> float64
{
    return std::arg( smoothed_ );
}

} // namespace ltb::radio::dsp
