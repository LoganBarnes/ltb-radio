// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/radio/dsp/demodulators.hpp"
#include "ltb/radio/dsp/fir_filter.hpp"
#include "ltb/radio/dsp/mixer.hpp"
#include "ltb/radio/dsp/types.hpp"

// standard
#include <span>
#include <vector>

namespace ltb::radio::dsp
{

struct VorReading
{
    // The radial the receiver is on (bearing from the station, degrees magnetic).
    float32 radial_deg = 0.0F;

    // Depth of the variable 30 Hz AM (nominally 0.3) and the reference subcarrier's FM
    // deviation relative to 480 Hz (nominally 1).
    float32 variable_level  = 0.0F;
    float32 reference_level = 0.0F;

    // Both tones are strong enough to trust the radial.
    bool valid = false;
};

// Recovers the radial from a VOR's AM signal. The variable 30 Hz tone's phase depends on the
// direction from the station; the reference 30 Hz tone (FM on a 9960 Hz subcarrier) has the same
// phase in every direction. The radial is the phase by which the variable tone lags the
// reference.
class VorDemodulator
{
public:
    static constexpr auto input_rate_hz      = 48.0e3;
    static constexpr auto subcarrier_rate_hz = 4.0e3;
    static constexpr auto tone_rate_hz       = 1.0e3;

    VorDemodulator( );

    // `channel` is complex baseband at 48 kS/s with the VOR's carrier at 0 Hz. Outputs are
    // overwritten each call.
    auto process( std::span< Complex const > channel ) -> void;

    // The AM relative to the carrier (envelope / carrier - 1), at 48 kS/s.
    [[nodiscard( "Const getter" )]]
    auto modulation( ) const -> std::vector< float32 > const&;

    // The variable 30 Hz tone, at 1 kS/s.
    [[nodiscard( "Const getter" )]]
    auto variable( ) const -> std::vector< float32 > const&;

    // The 9960 Hz subcarrier shifted to 0 Hz, at 4 kS/s.
    [[nodiscard( "Const getter" )]]
    auto subcarrier( ) const -> std::vector< Complex > const&;

    // The reference 30 Hz tone (the subcarrier's FM), at 1 kS/s.
    [[nodiscard( "Const getter" )]]
    auto reference( ) const -> std::vector< float32 > const&;

    // The ident (Morse) and voice, without the 30 Hz tones or subcarrier, at 48 kS/s.
    [[nodiscard( "Const getter" )]]
    auto audio( ) const -> std::vector< float32 > const&;

    [[nodiscard( "Const getter" )]]
    auto reading( ) const -> VorReading;

    // How much later the reference tone comes out of its filters than the variable tone.
    [[nodiscard( "Const getter" )]]
    auto reference_delay_s( ) const -> float64;

private:
    // Measures a 30 Hz tone's amplitude and phase against a fixed-frequency oscillator.
    struct ToneMeter
    {
        std::complex< float64 > oscillator  = 1.0;
        std::complex< float64 > correlation = 0.0;

        auto process( std::span< float32 const > samples ) -> void;
    };

    float32 carrier_level_ = 0.0F;

    DecimatingFir< float32 > variable_filter_;
    Mixer                    subcarrier_mixer_ = { };
    DecimatingFir< Complex > subcarrier_filter_;
    FmDiscriminator          discriminator_ = { };
    DecimatingFir< float32 > reference_filter_;
    DecimatingFir< float32 > audio_filter_;
    DcBlocker                audio_dc_blocker_ = { };

    float64 reference_delay_s_ = 0.0;

    ToneMeter variable_meter_  = { };
    ToneMeter reference_meter_ = { };

    std::vector< float32 > envelope_           = { };
    std::vector< float32 > modulation_         = { };
    std::vector< Complex > modulation_complex_ = { };
    std::vector< Complex > mixed_              = { };
    std::vector< float32 > variable_           = { };
    std::vector< Complex > subcarrier_         = { };
    std::vector< float32 > frequency_          = { };
    std::vector< float32 > reference_          = { };
    std::vector< float32 > audio_              = { };
};

} // namespace ltb::radio::dsp
