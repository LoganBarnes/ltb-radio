// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "vor_demodulator.hpp"

// standard
#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>
#include <ranges>

namespace ltb::radio::dsp
{
namespace
{

constexpr auto two_pi                = 2.0 * std::numbers::pi;
constexpr auto tone_hz               = 30.0;
constexpr auto subcarrier_hz         = 9960.0;
constexpr auto subcarrier_dev_hz     = 480.0;
constexpr auto subcarrier_passband   = 600.0;
constexpr auto tone_passband_hz      = 50.0;
constexpr auto audio_cutoff_hz       = 2500.0;
constexpr auto audio_transition_hz   = 1500.0;
constexpr auto audio_dc_cutoff_hz    = 300.0;
constexpr auto carrier_time_constant = 1.0;
constexpr auto meter_time_constant   = 0.5;
constexpr auto min_variable_level    = 0.1F;
constexpr auto min_reference_level   = 0.3F;

constexpr auto subcarrier_decimation = 12UZ;
constexpr auto reference_decimation  = 4UZ;
constexpr auto variable_decimation   = subcarrier_decimation * reference_decimation;

// Seconds a linear-phase FIR delays its input.
auto group_delay( std::size_t const tap_count, float64 const rate_hz ) -> float64
{
    return 0.5 * static_cast< float64 >( tap_count - 1UZ ) / rate_hz;
}

struct ToComplex
{
    auto operator( )( float32 const value ) const -> Complex
    {
        return { value, 0.0F };
    }
};

} // namespace

auto VorDemodulator::ToneMeter::process( std::span< float32 const > const samples ) -> void
{
    static auto const step      = std::polar( 1.0, -two_pi * tone_hz / tone_rate_hz );
    static auto const smoothing = 1.0 / ( meter_time_constant * tone_rate_hz );

    for ( auto const sample : samples )
    {
        correlation += smoothing * ( static_cast< float64 >( sample ) * oscillator - correlation );
        oscillator *= step;
    }
    oscillator /= std::abs( oscillator );
}

VorDemodulator::VorDemodulator( )
    : variable_filter_(
          design_decimation_filter( tone_passband_hz, input_rate_hz, variable_decimation ),
          variable_decimation
      )
    , subcarrier_filter_(
          design_decimation_filter( subcarrier_passband, input_rate_hz, subcarrier_decimation ),
          subcarrier_decimation
      )
    , reference_filter_(
          design_decimation_filter( tone_passband_hz, subcarrier_rate_hz, reference_decimation ),
          reference_decimation
      )
    , audio_filter_( design_low_pass( audio_cutoff_hz, audio_transition_hz, input_rate_hz ), 1UZ )
{
    subcarrier_mixer_.set_shift( subcarrier_hz, input_rate_hz );
    discriminator_.configure( subcarrier_dev_hz, subcarrier_rate_hz );
    audio_dc_blocker_.configure( audio_dc_cutoff_hz, input_rate_hz );

    // The discriminator measures the frequency halfway between two samples.
    reference_delay_s_ = group_delay( subcarrier_filter_.tap_count( ), input_rate_hz )
                       + 0.5 / subcarrier_rate_hz
                       + group_delay( reference_filter_.tap_count( ), subcarrier_rate_hz )
                       - group_delay( variable_filter_.tap_count( ), input_rate_hz );
}

auto VorDemodulator::process( std::span< Complex const > const channel ) -> void
{
    envelope( channel, envelope_ );

    // Normalize by the carrier so levels don't depend on signal strength. Start from the first
    // block's average; starting from zero would make the first samples huge.
    if ( carrier_level_ <= 0.0F && !envelope_.empty( ) )
    {
        carrier_level_ = std::reduce( envelope_.begin( ), envelope_.end( ), 0.0F )
                       / static_cast< float32 >( envelope_.size( ) );
    }
    auto const smoothing
        = static_cast< float32 >( 1.0 / ( carrier_time_constant * input_rate_hz ) );
    modulation_.resize( envelope_.size( ) );
    for ( auto&& [ amplitude, modulation ] : std::views::zip( envelope_, modulation_ ) )
    {
        carrier_level_ += smoothing * ( amplitude - carrier_level_ );
        modulation = amplitude / std::max( carrier_level_, 1e-9F ) - 1.0F;
    }

    variable_.clear( );
    variable_filter_.process( modulation_, variable_ );
    variable_meter_.process( variable_ );

    modulation_complex_.resize( modulation_.size( ) );
    std::ranges::transform( modulation_, modulation_complex_.begin( ), ToComplex{ } );
    subcarrier_mixer_.process( modulation_complex_, mixed_ );
    subcarrier_.clear( );
    subcarrier_filter_.process( mixed_, subcarrier_ );

    discriminator_.process( subcarrier_, frequency_ );
    reference_.clear( );
    reference_filter_.process( frequency_, reference_ );
    reference_meter_.process( reference_ );

    audio_.clear( );
    audio_filter_.process( modulation_, audio_ );
    audio_dc_blocker_.process( audio_ );
}

auto VorDemodulator::modulation( ) const -> std::vector< float32 > const&
{
    return modulation_;
}

auto VorDemodulator::variable( ) const -> std::vector< float32 > const&
{
    return variable_;
}

auto VorDemodulator::subcarrier( ) const -> std::vector< Complex > const&
{
    return subcarrier_;
}

auto VorDemodulator::reference( ) const -> std::vector< float32 > const&
{
    return reference_;
}

auto VorDemodulator::audio( ) const -> std::vector< float32 > const&
{
    return audio_;
}

auto VorDemodulator::reference_delay_s( ) const -> float64
{
    return reference_delay_s_;
}

auto VorDemodulator::reading( ) const -> VorReading
{
    // The variable tone lags the reference by the radial. Undo the extra phase the reference
    // picked up from its longer filter chain.
    auto const lag
        = std::arg( reference_meter_.correlation ) - std::arg( variable_meter_.correlation );
    auto const radians = lag + two_pi * tone_hz * reference_delay_s_;
    auto       degrees = std::fmod( radians * 180.0 / std::numbers::pi, 360.0 );
    if ( degrees < 0.0 )
    {
        degrees += 360.0;
    }

    // A tone of amplitude A correlates to A / 2.
    auto const variable_level
        = static_cast< float32 >( 2.0 * std::abs( variable_meter_.correlation ) );
    auto const reference_level
        = static_cast< float32 >( 2.0 * std::abs( reference_meter_.correlation ) );

    return {
        .radial_deg      = static_cast< float32 >( degrees ),
        .variable_level  = variable_level,
        .reference_level = reference_level,
        .valid = variable_level > min_variable_level && reference_level > min_reference_level,
    };
}

} // namespace ltb::radio::dsp
