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

// A simulated transmitter playing a tone with a slow tremolo.
struct TestStation
{
    float64 frequency_hz = 0.0;
    float32 level        = 0.1F;
    float64 tone_hz      = 440.0;

    // When positive, the station transmits for the first 40% of every period (like a pilot
    // keying a microphone). Otherwise it transmits continuously.
    float64 keyed_period_s = 0.0;

    // VOR only: the radial the simulated receiver is on. `tone_hz` is the ident tone.
    float64 radial_deg = 0.0;
};

// Synthesizes I/Q samples as a receiver tuned to `center_frequency` would see them, so the
// radio pipeline can be exercised without hardware.
class TestSignalGenerator
{
public:
    TestSignalGenerator(
        Modulation                 modulation,
        float64                    sample_rate_hz,
        std::vector< TestStation > stations,
        float32                    noise_level
    );

    // A few stations near the default AM or FM frequencies.
    [[nodiscard( "Pure computation" )]]
    static auto default_stations( Modulation modulation ) -> std::vector< TestStation >;

    auto set_center_frequency( float64 center_frequency_hz ) -> void;

    // Overwrites `output` with the next samples.
    auto generate( std::span< Complex > output ) -> void;

private:
    struct Station
    {
        TestStation             settings;
        std::complex< float64 > tone         = 1.0;
        std::complex< float64 > tone_step    = 1.0;
        std::complex< float64 > tremolo      = 1.0;
        std::complex< float64 > tremolo_step = 1.0;
        float64                 phase        = 0.0;
        uint64                  sample_count = 0U;

        // VOR navigation tones.
        std::complex< float64 > nav_tone         = 1.0;
        std::complex< float64 > nav_tone_step    = 1.0;
        std::complex< float64 > radial_rotation  = 1.0;
        float64                 subcarrier_phase = 0.0;
    };

    Modulation             modulation_;
    float64                sample_rate_hz_;
    float64                center_frequency_hz_ = 0.0;
    std::vector< Station > stations_;
    float32                noise_level_;
    uint32                 noise_state_ = 0x2545F491U;

    auto add_station( Station& station, std::span< Complex > output ) const -> void;
    auto next_noise( ) -> float32;
};

} // namespace ltb::radio::dsp
