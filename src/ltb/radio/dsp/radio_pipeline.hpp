// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/radio/dsp/demodulators.hpp"
#include "ltb/radio/dsp/fir_filter.hpp"
#include "ltb/radio/dsp/mixer.hpp"
#include "ltb/radio/dsp/types.hpp"
#include "ltb/radio/dsp/vor_demodulator.hpp"

// standard
#include <span>
#include <string>
#include <vector>

namespace ltb::radio::dsp
{

enum class StageKind
{
    // Complex samples centered on the receiver's (hardware) center frequency.
    RfInput,
    // Complex samples centered on the tuned station.
    Baseband,
    // Complex samples unrelated to the RF spectrum (e.g. a subcarrier).
    Complex,
    // Real samples (demodulated or audio).
    Real,
};

struct StageInfo
{
    std::string name;
    std::string description;
    float64     sample_rate_hz = 0.0;
    StageKind   kind           = StageKind::Real;
};

struct PipelineSettings
{
    // Tuned station relative to the input's center frequency.
    float64 shift_hz             = 0.0;
    float64 channel_bandwidth_hz = 10.0e3;
    float32 volume               = 0.5F;
    bool    agc_enabled          = true;
    float64 deemphasis_tau_s     = 75.0e-6;

    // Mutes the audio while the channel's power is below the threshold (dB relative to a
    // full-scale signal).
    bool    squelch_enabled = false;
    float32 squelch_db      = -50.0F;

    auto operator==( PipelineSettings const& ) const -> bool = default;
};

// Receives the output of every stage as a block is processed. Called on the processing thread.
class StageObserver
{
public:
    virtual ~StageObserver( ) = default;

    virtual auto on_stage_output( std::size_t stage, std::span< Complex const > samples ) -> void
        = 0;
    virtual auto on_stage_output( std::size_t stage, std::span< float32 const > samples ) -> void
        = 0;
};

// Demodulates 2.4 MS/s I/Q samples into 48 kS/s mono audio.
//
// AM: shift -> /5 -> /5 -> channel filter /2 -> envelope -> DC block + AGC
// FM: shift -> /5 -> channel filter /2 -> discriminator -> de-emphasis -> audio filter /5
// VOR: as AM to the channel filter, then `VorDemodulator` (navigation tones and ident audio)
class RadioPipeline
{
public:
    static constexpr auto input_rate_hz = 2.4e6;
    static constexpr auto audio_rate_hz = 48.0e3;

    // VOR stages with the two 30 Hz tones.
    static constexpr auto vor_variable_stage  = 6UZ;
    static constexpr auto vor_reference_stage = 8UZ;

    RadioPipeline( Modulation modulation, PipelineSettings settings );

    // The stages reported to a `StageObserver`, in order.
    [[nodiscard( "Pure computation" )]]
    static auto stage_infos( Modulation modulation ) -> std::vector< StageInfo >;

    // The supported channel bandwidth range for the modulation.
    [[nodiscard( "Pure computation" )]]
    static auto bandwidth_limits( Modulation modulation ) -> std::pair< float64, float64 >;

    // Filters are redesigned (and their state reset) only when the bandwidth changes.
    auto configure( PipelineSettings settings ) -> void;

    [[nodiscard( "Const getter" )]]
    auto settings( ) const -> PipelineSettings const&;

    // Smoothed power of the channel filter's output in dB (0 dB is a full-scale signal).
    [[nodiscard( "Const getter" )]]
    auto channel_power_db( ) const -> float32;

    [[nodiscard( "Const getter" )]]
    auto is_squelch_open( ) const -> bool;

    // Only valid for `Modulation::Vor`.
    [[nodiscard( "Const getter" )]]
    auto vor_reading( ) const -> VorReading;

    // Appends the audio for `iq` to `audio`.
    auto
    process( std::span< Complex const > iq, std::vector< float32 >& audio, StageObserver* observer )
        -> void;

private:
    Modulation       modulation_;
    PipelineSettings settings_;

    Mixer                    mixer_          = { };
    DecimatingFir< Complex > decimate_1_     = { };
    DecimatingFir< Complex > decimate_2_     = { };
    DecimatingFir< Complex > channel_filter_ = { };
    DecimatingFir< float32 > audio_filter_   = { };
    FmDiscriminator          discriminator_  = { };
    DeEmphasis               de_emphasis_    = { };
    DcBlocker                dc_blocker_     = { };
    Agc                      agc_            = { };
    VorDemodulator           vor_            = { };

    std::vector< Complex > shifted_     = { };
    std::vector< Complex > decimated_1_ = { };
    std::vector< Complex > decimated_2_ = { };
    std::vector< Complex > channel_     = { };
    std::vector< float32 > demodulated_ = { };
    std::vector< float32 > filtered_    = { };
    std::vector< float32 > audio_block_ = { };

    float32 channel_power_db_ = -200.0F;
    bool    squelch_open_     = true;

    auto design_filters( ) -> void;
    auto process_am( StageObserver* observer ) -> void;
    auto process_fm( StageObserver* observer ) -> void;
    auto process_vor( StageObserver* observer ) -> void;
    auto process_to_channel( StageObserver* observer ) -> void;
    auto update_squelch( ) -> void;
};

} // namespace ltb::radio::dsp
