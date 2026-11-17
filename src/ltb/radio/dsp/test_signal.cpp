// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "test_signal.hpp"

// standard
#include <cmath>
#include <numbers>

namespace ltb::radio::dsp
{
namespace
{

constexpr auto two_pi            = 2.0 * std::numbers::pi;
constexpr auto am_depth          = 0.8;
constexpr auto fm_deviation_hz   = 75.0e3;
constexpr auto keyed_fraction    = 0.4;
constexpr auto vor_tone_hz       = 30.0;
constexpr auto vor_subcarrier_hz = 9960.0;
constexpr auto vor_deviation_hz  = 480.0;
constexpr auto vor_depth         = 0.3;
constexpr auto vor_ident_depth   = 0.1;
constexpr auto tremolo_hz        = 0.5;
constexpr auto edge_of_band      = 0.45;

auto rotation( float64 const frequency_hz, float64 const sample_rate_hz ) -> std::complex< float64 >
{
    return std::polar( 1.0, two_pi * frequency_hz / sample_rate_hz );
}

} // namespace

TestSignalGenerator::TestSignalGenerator(
    Modulation const           modulation,
    float64 const              sample_rate_hz,
    std::vector< TestStation > stations,
    float32 const              noise_level
)
    : modulation_( modulation )
    , sample_rate_hz_( sample_rate_hz )
    , noise_level_( noise_level )
{
    for ( auto const& settings : stations )
    {
        stations_.push_back( {
            .settings        = settings,
            .tone_step       = rotation( settings.tone_hz, sample_rate_hz ),
            .tremolo_step    = rotation( tremolo_hz, sample_rate_hz ),
            .nav_tone_step   = rotation( vor_tone_hz, sample_rate_hz ),
            .radial_rotation = std::polar( 1.0, -settings.radial_deg * std::numbers::pi / 180.0 ),
        } );
    }
}

auto TestSignalGenerator::default_stations( Modulation const modulation )
    -> std::vector< TestStation >
{
    if ( modulation == Modulation::Ils )
    {
        // Cedar City ILS RWY 20: an aircraft a little left of course and below the glide path.
        // The glideslope has no ident (tone 0 Hz).
        return {
            {
                .frequency_hz = 110.10e6,
                .level        = 0.10F,
                .tone_hz      = 1'020.0,
                .ils_depth    = 0.2,
                .ils_ddm      = 0.05,
            },
            {
                .frequency_hz = 334.40e6,
                .level        = 0.08F,
                .tone_hz      = 0.0,
                .ils_depth    = 0.4,
                .ils_ddm      = -0.06,
            },
        };
    }
    if ( modulation == Modulation::Vor )
    {
        // Southern Utah VORs, with the radials of a receiver near Cedar City (approximate).
        return {
            { .frequency_hz = 117.3e6, .level = 0.10F, .tone_hz = 1'020.0, .radial_deg = 196.0 },
            { .frequency_hz = 112.1e6, .level = 0.03F, .tone_hz = 1'020.0, .radial_deg = 160.0 },
            { .frequency_hz = 108.6e6, .level = 0.03F, .tone_hz = 1'020.0, .radial_deg = 30.0 },
        };
    }
    if ( modulation == Modulation::Am )
    {
        return {
            // Aviation: continuous weather, plus intermittent traffic.
            { .frequency_hz = 119.025e6, .level = 0.10F, .tone_hz = 440.0 },
            { .frequency_hz = 118.9e6, .level = 0.03F, .tone_hz = 660.0 },
            { .frequency_hz = 123.0e6, .level = 0.08F, .tone_hz = 880.0, .keyed_period_s = 5.0 },
            { .frequency_hz = 125.575e6, .level = 0.05F, .tone_hz = 550.0, .keyed_period_s = 7.0 },
        };
    }
    return {
        { .frequency_hz = 96.5e6, .level = 0.10F, .tone_hz = 440.0 },
        { .frequency_hz = 96.9e6, .level = 0.05F, .tone_hz = 660.0 },
        { .frequency_hz = 97.9e6, .level = 0.03F, .tone_hz = 880.0 },
    };
}

auto TestSignalGenerator::set_center_frequency( float64 const center_frequency_hz ) -> void
{
    center_frequency_hz_ = center_frequency_hz;
}

auto TestSignalGenerator::generate( std::span< Complex > output ) -> void
{
    for ( auto& sample : output )
    {
        sample = Complex( this->next_noise( ), this->next_noise( ) ) * noise_level_;
    }
    for ( auto& station : stations_ )
    {
        this->add_station( station, output );
    }
}

auto TestSignalGenerator::add_station( Station& station, std::span< Complex > output ) const -> void
{
    auto const offset_hz = station.settings.frequency_hz - center_frequency_hz_;
    auto const period    = station.settings.keyed_period_s;
    auto const seconds   = static_cast< float64 >( station.sample_count ) / sample_rate_hz_;
    auto const keyed     = period <= 0.0 || std::fmod( seconds, period ) < keyed_fraction * period;
    auto const visible   = keyed && std::abs( offset_hz ) < edge_of_band * sample_rate_hz_;
    station.sample_count += output.size( );

    for ( auto& sample : output )
    {
        // The audio keeps playing while out of view so retuning doesn't restart it.
        auto const audio = 0.8 * station.tone.imag( ) * ( 0.6 + 0.4 * station.tremolo.imag( ) );
        station.tone *= station.tone_step;
        station.tremolo *= station.tremolo_step;

        auto amplitude = static_cast< float64 >( station.settings.level );
        auto frequency = offset_hz;
        if ( modulation_ == Modulation::Am )
        {
            amplitude *= 1.0 + am_depth * audio;
        }
        else if ( modulation_ == Modulation::Fm )
        {
            frequency += fm_deviation_hz * audio;
        }
        else if ( modulation_ == Modulation::Ils )
        {
            // 90 and 150 Hz are the 3rd and 5th harmonics of 30 Hz, phase locked like a real ILS.
            auto const cubed  = station.nav_tone * station.nav_tone * station.nav_tone;
            auto const fifth  = cubed * station.nav_tone * station.nav_tone;
            auto const ident  = station.tremolo.imag( ) > 0.5 ? station.tone.imag( ) : 0.0;
            auto const depth  = station.settings.ils_depth;
            auto const offset = 0.5 * station.settings.ils_ddm;
            amplitude *= 1.0 + ( depth + offset ) * cubed.real( )
                       + ( depth - offset ) * fifth.real( ) + vor_ident_depth * ident;
            station.nav_tone *= station.nav_tone_step;
        }
        else
        {
            // Reference 30 Hz as FM on the subcarrier; the variable 30 Hz lags it by the radial.
            auto const reference = station.nav_tone.real( );
            auto const variable  = ( station.nav_tone * station.radial_rotation ).real( );
            auto const ident     = station.tremolo.imag( ) > 0.5 ? station.tone.imag( ) : 0.0;
            amplitude *= 1.0 + vor_depth * variable
                       + vor_depth * std::cos( station.subcarrier_phase ) + vor_ident_depth * ident;

            station.subcarrier_phase
                += two_pi * ( vor_subcarrier_hz + vor_deviation_hz * reference ) / sample_rate_hz_;
            station.nav_tone *= station.nav_tone_step;
        }

        if ( visible )
        {
            sample += Complex( std::polar( amplitude, station.phase ) );
        }
        station.phase += two_pi * frequency / sample_rate_hz_;
    }

    station.phase            = std::remainder( station.phase, two_pi );
    station.subcarrier_phase = std::remainder( station.subcarrier_phase, two_pi );
    station.nav_tone /= std::abs( station.nav_tone );
    station.tone /= std::abs( station.tone );
    station.tremolo /= std::abs( station.tremolo );
}

// xorshift32: cheap uniform noise in [-1, 1].
auto TestSignalGenerator::next_noise( ) -> float32
{
    noise_state_ ^= noise_state_ << 13U;
    noise_state_ ^= noise_state_ >> 17U;
    noise_state_ ^= noise_state_ << 5U;
    return static_cast< float32 >( noise_state_ ) / 2147483648.0F - 1.0F;
}

} // namespace ltb::radio::dsp
