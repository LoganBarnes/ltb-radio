// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "ils_demodulator.hpp"

namespace ltb::radio::dsp
{
namespace
{

constexpr auto tone_passband_hz      = 200.0;
constexpr auto tone_decimation       = 48UZ;
constexpr auto carrier_time_constant = 1.0;
constexpr auto meter_time_constant   = 0.25;
constexpr auto audio_cutoff_hz       = 2500.0;
constexpr auto audio_transition_hz   = 1500.0;
constexpr auto audio_dc_cutoff_hz    = 500.0;
constexpr auto min_sdm               = 0.15F;

} // namespace

IlsDemodulator::IlsDemodulator( )
    : tone_filter_(
          design_decimation_filter( tone_passband_hz, input_rate_hz, tone_decimation ),
          tone_decimation
      )
    , audio_filter_( design_low_pass( audio_cutoff_hz, audio_transition_hz, input_rate_hz ), 1UZ )
{
    carrier_.configure( carrier_time_constant, input_rate_hz );
    meter_90_.configure( 90.0, tone_rate_hz, meter_time_constant );
    meter_150_.configure( 150.0, tone_rate_hz, meter_time_constant );

    // Two high-pass stages keep the 90 and 150 Hz tones from humming in the audio.
    audio_dc_blocker_1_.configure( audio_dc_cutoff_hz, input_rate_hz );
    audio_dc_blocker_2_.configure( audio_dc_cutoff_hz, input_rate_hz );
}

auto IlsDemodulator::process( std::span< Complex const > const channel ) -> void
{
    envelope( channel, envelope_ );
    carrier_.process( envelope_, modulation_ );

    tones_.clear( );
    tone_filter_.process( modulation_, tones_ );
    meter_90_.process( tones_ );
    meter_150_.process( tones_ );

    audio_.clear( );
    audio_filter_.process( modulation_, audio_ );
    audio_dc_blocker_1_.process( audio_ );
    audio_dc_blocker_2_.process( audio_ );
}

auto IlsDemodulator::modulation( ) const -> std::vector< float32 > const&
{
    return modulation_;
}

auto IlsDemodulator::tones( ) const -> std::vector< float32 > const&
{
    return tones_;
}

auto IlsDemodulator::audio( ) const -> std::vector< float32 > const&
{
    return audio_;
}

auto IlsDemodulator::reading( ) const -> IlsReading
{
    auto reading
        = IlsReading{ .depth_90 = meter_90_.amplitude( ), .depth_150 = meter_150_.amplitude( ) };
    reading.valid = reading.sdm( ) > min_sdm;
    return reading;
}

} // namespace ltb::radio::dsp
