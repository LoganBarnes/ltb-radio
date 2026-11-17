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
    carrier_.configure( carrier_time_constant, input_rate_hz );
    variable_meter_.configure( tone_hz, tone_rate_hz, meter_time_constant );
    reference_meter_.configure( tone_hz, tone_rate_hz, meter_time_constant );

    // The discriminator measures the frequency halfway between two samples.
    reference_delay_s_ = group_delay( subcarrier_filter_.tap_count( ), input_rate_hz )
                       + 0.5 / subcarrier_rate_hz
                       + group_delay( reference_filter_.tap_count( ), subcarrier_rate_hz )
                       - group_delay( variable_filter_.tap_count( ), input_rate_hz );
}

auto VorDemodulator::process( std::span< Complex const > const channel ) -> void
{
    envelope( channel, envelope_ );

    // Normalize by the carrier so levels don't depend on signal strength.
    carrier_.process( envelope_, modulation_ );

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
    auto const lag     = reference_meter_.phase( ) - variable_meter_.phase( );
    auto const radians = lag + two_pi * tone_hz * reference_delay_s_;
    auto       degrees = std::fmod( radians * 180.0 / std::numbers::pi, 360.0 );
    if ( degrees < 0.0 )
    {
        degrees += 360.0;
    }

    auto const variable_level  = variable_meter_.amplitude( );
    auto const reference_level = reference_meter_.amplitude( );

    return {
        .radial_deg      = static_cast< float32 >( degrees ),
        .variable_level  = variable_level,
        .reference_level = reference_level,
        .valid = variable_level > min_variable_level && reference_level > min_reference_level,
    };
}

} // namespace ltb::radio::dsp
