// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/radio/dsp/demodulators.hpp"
#include "ltb/radio/dsp/fir_filter.hpp"
#include "ltb/radio/dsp/types.hpp"

// standard
#include <span>
#include <vector>

namespace ltb::radio::dsp
{

struct IlsReading
{
    // Depth of modulation of each tone (nominally 0.2 each on a localizer, 0.4 on a glideslope).
    float32 depth_90  = 0.0F;
    float32 depth_150 = 0.0F;

    // The tones are strong enough to trust.
    bool valid = false;

    // Difference in depth of modulation. Positive when 90 Hz dominates: left of the localizer
    // course or above the glideslope (as seen from an approaching aircraft).
    [[nodiscard( "Const computation" )]]
    auto ddm( ) const -> float32
    {
        return depth_90 - depth_150;
    }

    // Sum of the depths of modulation.
    [[nodiscard( "Const computation" )]]
    auto sdm( ) const -> float32
    {
        return depth_90 + depth_150;
    }
};

// Measures an ILS localizer's or glideslope's 90 Hz and 150 Hz AM tones. Their difference in
// depth (DDM) is how far off course or off the glide path the receiver is.
class IlsDemodulator
{
public:
    static constexpr auto input_rate_hz = 48.0e3;
    static constexpr auto tone_rate_hz  = 1.0e3;

    IlsDemodulator( );

    // `channel` is complex baseband at 48 kS/s with the carrier at 0 Hz. Outputs are overwritten
    // each call.
    auto process( std::span< Complex const > channel ) -> void;

    // The AM relative to the carrier (envelope / carrier - 1), at 48 kS/s.
    [[nodiscard( "Const getter" )]]
    auto modulation( ) const -> std::vector< float32 > const&;

    // The 90 Hz and 150 Hz tones (low-pass filtered modulation), at 1 kS/s.
    [[nodiscard( "Const getter" )]]
    auto tones( ) const -> std::vector< float32 > const&;

    // The Morse ident (1020 Hz) and any voice, without the tones, at 48 kS/s.
    [[nodiscard( "Const getter" )]]
    auto audio( ) const -> std::vector< float32 > const&;

    [[nodiscard( "Const getter" )]]
    auto reading( ) const -> IlsReading;

private:
    CarrierNormalizer        carrier_ = { };
    DecimatingFir< float32 > tone_filter_;
    ToneMeter                meter_90_  = { };
    ToneMeter                meter_150_ = { };
    DecimatingFir< float32 > audio_filter_;
    DcBlocker                audio_dc_blocker_1_ = { };
    DcBlocker                audio_dc_blocker_2_ = { };

    std::vector< float32 > envelope_   = { };
    std::vector< float32 > modulation_ = { };
    std::vector< float32 > tones_      = { };
    std::vector< float32 > audio_      = { };
};

} // namespace ltb::radio::dsp
