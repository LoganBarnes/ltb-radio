// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "radio_pipeline.hpp"

// standard
#include <algorithm>
#include <cmath>
#include <functional>
#include <numeric>

namespace ltb::radio::dsp
{
namespace
{

constexpr auto am_intermediate_passband_hz = 20.0e3;
constexpr auto am_channel_transition_hz    = 3.0e3;
constexpr auto am_dc_cutoff_hz             = 30.0;
constexpr auto fm_channel_transition_hz    = 40.0e3;
constexpr auto fm_max_deviation_hz         = 75.0e3;
constexpr auto fm_audio_passband_hz        = 15.0e3;
constexpr auto fm_audio_stopband_hz        = 19.0e3;
constexpr auto power_smoothing             = 0.5F;
constexpr auto squelch_hysteresis_db       = 3.0F;

struct Power
{
    auto operator( )( Complex const& sample ) const -> float32
    {
        return std::norm( sample );
    }
};

template < typename Sample >
auto observe(
    StageObserver* const         observer,
    std::size_t const            stage,
    std::vector< Sample > const& samples
) -> void
{
    if ( observer != nullptr )
    {
        observer->on_stage_output( stage, std::span< Sample const >( samples ) );
    }
}

} // namespace

RadioPipeline::RadioPipeline( Modulation const modulation, PipelineSettings settings )
    : modulation_( modulation )
    // An invalid bandwidth makes `configure` design the filters.
    , settings_{ .channel_bandwidth_hz = 0.0 }
{
    discriminator_.configure( fm_max_deviation_hz, input_rate_hz / 10.0 );
    dc_blocker_.configure( am_dc_cutoff_hz, audio_rate_hz );
    agc_.configure( audio_rate_hz );
    this->configure( settings );
}

auto RadioPipeline::stage_infos( Modulation const modulation ) -> std::vector< StageInfo >
{
    auto stages = std::vector< StageInfo >{
        {
            .name        = "RF Input",
            .description = "Raw I/Q samples from the source, centered on the receiver's frequency.",
            .sample_rate_hz = input_rate_hz,
            .kind           = StageKind::RfInput,
        },
        {
            .name        = "Frequency Shift",
            .description = "Multiplied by a complex oscillator so the tuned station moves to 0 Hz.",
            .sample_rate_hz = input_rate_hz,
            .kind           = StageKind::Baseband,
        },
        {
            .name           = "Decimate by 5",
            .description    = "Low-pass filtered, then every 5th sample is kept.",
            .sample_rate_hz = input_rate_hz / 5.0,
            .kind           = StageKind::Baseband,
        },
    };

    if ( modulation == Modulation::Am )
    {
        stages.insert(
            stages.end( ),
            {
                {
                    .name           = "Decimate by 5 (again)",
                    .description    = "Low-pass filtered, then every 5th sample is kept.",
                    .sample_rate_hz = input_rate_hz / 25.0,
                    .kind           = StageKind::Baseband,
                },
                {
                    .name           = "Channel Filter",
                    .description    = "Keeps only the station's bandwidth, then decimates by 2.",
                    .sample_rate_hz = audio_rate_hz,
                    .kind           = StageKind::Baseband,
                },
                {
                    .name           = "Envelope",
                    .description    = "|I + jQ|: the carrier's amplitude, which follows the audio.",
                    .sample_rate_hz = audio_rate_hz,
                    .kind           = StageKind::Real,
                },
                {
                    .name        = "Audio",
                    .description = "The carrier (DC) is removed and the AGC normalizes the level.",
                    .sample_rate_hz = audio_rate_hz,
                    .kind           = StageKind::Real,
                },
            }
        );
    }
    else
    {
        stages.insert(
            stages.end( ),
            {
                {
                    .name           = "Channel Filter",
                    .description    = "Keeps only the station's bandwidth, then decimates by 2.",
                    .sample_rate_hz = input_rate_hz / 10.0,
                    .kind           = StageKind::Baseband,
                },
                {
                    .name           = "FM Discriminator",
                    .description    = "The phase change between samples (instantaneous frequency). "
                                      "Broadcasts carry mono audio, a 19 kHz stereo pilot, the "
                                      "38 kHz stereo difference and 57 kHz RDS data.",
                    .sample_rate_hz = input_rate_hz / 10.0,
                    .kind           = StageKind::Real,
                },
                {
                    .name           = "De-emphasis",
                    .description    = "Undoes the transmitter's treble boost (pre-emphasis).",
                    .sample_rate_hz = input_rate_hz / 10.0,
                    .kind           = StageKind::Real,
                },
                {
                    .name           = "Audio",
                    .description    = "Low-pass filtered to 15 kHz (removing the pilot and "
                                      "subcarriers), then decimated by 5.",
                    .sample_rate_hz = audio_rate_hz,
                    .kind           = StageKind::Real,
                },
            }
        );
    }
    return stages;
}

auto RadioPipeline::bandwidth_limits( Modulation const modulation ) -> std::pair< float64, float64 >
{
    // The upper limits keep the channel filters' stop bands clear of aliases.
    return modulation == Modulation::Am ? std::pair( 2.0e3, 30.0e3 ) : std::pair( 50.0e3, 200.0e3 );
}

auto RadioPipeline::configure( PipelineSettings settings ) -> void
{
    auto const [ min_bandwidth, max_bandwidth ] = bandwidth_limits( modulation_ );
    settings.channel_bandwidth_hz
        = std::clamp( settings.channel_bandwidth_hz, min_bandwidth, max_bandwidth );

    auto const bandwidth_changed = settings.channel_bandwidth_hz != settings_.channel_bandwidth_hz;
    settings_                    = settings;

    mixer_.set_shift( settings_.shift_hz, input_rate_hz );
    de_emphasis_.configure( settings_.deemphasis_tau_s, input_rate_hz / 10.0 );

    if ( bandwidth_changed )
    {
        this->design_filters( );
    }
}

auto RadioPipeline::settings( ) const -> PipelineSettings const&
{
    return settings_;
}

auto RadioPipeline::process(
    std::span< Complex const > iq,
    std::vector< float32 >&    audio,
    StageObserver* const       observer
) -> void
{
    if ( observer != nullptr )
    {
        observer->on_stage_output( 0UZ, iq );
    }

    mixer_.process( iq, shifted_ );
    observe( observer, 1UZ, shifted_ );

    if ( modulation_ == Modulation::Am )
    {
        this->process_am( observer );
    }
    else
    {
        this->process_fm( observer );
    }

    this->update_squelch( );
    if ( !squelch_open_ )
    {
        std::ranges::fill( audio_block_, 0.0F );
    }

    std::ranges::transform(
        audio_block_,
        std::back_inserter( audio ),
        std::bind_front( std::multiplies<>{ }, settings_.volume )
    );
}

auto RadioPipeline::channel_power_db( ) const -> float32
{
    return channel_power_db_;
}

auto RadioPipeline::is_squelch_open( ) const -> bool
{
    return squelch_open_;
}

auto RadioPipeline::update_squelch( ) -> void
{
    if ( !channel_.empty( ) )
    {
        auto const power    = std::transform_reduce(
                                  channel_.begin( ),
                                  channel_.end( ),
                                  0.0F,
                                  std::plus<>{ },
                                  Power{ }
                              )
                            / static_cast< float32 >( channel_.size( ) );
        auto const power_db = 10.0F * std::log10( std::max( power, 1e-20F ) );
        channel_power_db_ += power_smoothing * ( power_db - channel_power_db_ );
    }

    // Hysteresis keeps a signal near the threshold from chattering.
    auto const threshold
        = squelch_open_ ? settings_.squelch_db - squelch_hysteresis_db : settings_.squelch_db;
    squelch_open_ = !settings_.squelch_enabled || channel_power_db_ > threshold;
}

auto RadioPipeline::design_filters( ) -> void
{
    auto const half_bandwidth = 0.5 * settings_.channel_bandwidth_hz;

    if ( modulation_ == Modulation::Am )
    {
        decimate_1_
            = { design_decimation_filter( am_intermediate_passband_hz, input_rate_hz, 5UZ ), 5UZ };
        decimate_2_
            = { design_decimation_filter( am_intermediate_passband_hz, input_rate_hz / 5.0, 5UZ ),
                5UZ };
        channel_filter_ = {
            design_low_pass(
                half_bandwidth + 0.5 * am_channel_transition_hz,
                am_channel_transition_hz,
                input_rate_hz / 25.0
            ),
            2UZ,
        };
    }
    else
    {
        decimate_1_     = { design_decimation_filter( half_bandwidth, input_rate_hz, 5UZ ), 5UZ };
        channel_filter_ = {
            design_low_pass(
                half_bandwidth + 0.5 * fm_channel_transition_hz,
                fm_channel_transition_hz,
                input_rate_hz / 5.0
            ),
            2UZ,
        };
        audio_filter_ = {
            design_low_pass(
                0.5 * ( fm_audio_passband_hz + fm_audio_stopband_hz ),
                fm_audio_stopband_hz - fm_audio_passband_hz,
                input_rate_hz / 10.0
            ),
            5UZ,
        };
    }
}

auto RadioPipeline::process_am( StageObserver* const observer ) -> void
{
    decimated_1_.clear( );
    decimate_1_.process( shifted_, decimated_1_ );
    observe( observer, 2UZ, decimated_1_ );

    decimated_2_.clear( );
    decimate_2_.process( decimated_1_, decimated_2_ );
    observe( observer, 3UZ, decimated_2_ );

    channel_.clear( );
    channel_filter_.process( decimated_2_, channel_ );
    observe( observer, 4UZ, channel_ );

    envelope( channel_, demodulated_ );
    observe( observer, 5UZ, demodulated_ );

    audio_block_ = demodulated_;
    dc_blocker_.process( audio_block_ );
    if ( settings_.agc_enabled )
    {
        agc_.process( audio_block_ );
    }
    observe( observer, 6UZ, audio_block_ );
}

auto RadioPipeline::process_fm( StageObserver* const observer ) -> void
{
    decimated_1_.clear( );
    decimate_1_.process( shifted_, decimated_1_ );
    observe( observer, 2UZ, decimated_1_ );

    channel_.clear( );
    channel_filter_.process( decimated_1_, channel_ );
    observe( observer, 3UZ, channel_ );

    discriminator_.process( channel_, demodulated_ );
    observe( observer, 4UZ, demodulated_ );

    filtered_ = demodulated_;
    de_emphasis_.process( filtered_ );
    observe( observer, 5UZ, filtered_ );

    audio_block_.clear( );
    audio_filter_.process( filtered_, audio_block_ );
    observe( observer, 6UZ, audio_block_ );
}

} // namespace ltb::radio::dsp
