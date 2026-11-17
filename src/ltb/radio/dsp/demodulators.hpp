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

// AM: the magnitude of each sample. Overwrites `output`.
auto envelope( std::span< Complex const > input, std::vector< float32 >& output ) -> void;

// FM: the phase change between consecutive samples, scaled so `max_deviation_hz` maps to ±1.
class FmDiscriminator
{
public:
    auto configure( float64 max_deviation_hz, float64 sample_rate_hz ) -> void;

    // Overwrites `output`.
    auto process( std::span< Complex const > input, std::vector< float32 >& output ) -> void;

private:
    Complex previous_ = { };
    float32 scale_    = 1.0F;
};

// Removes the DC component with a single-pole high-pass filter.
class DcBlocker
{
public:
    auto configure( float64 cutoff_hz, float64 sample_rate_hz ) -> void;
    auto process( std::span< float32 > samples ) -> void;

private:
    float32 pole_            = 0.999F;
    float32 previous_input_  = 0.0F;
    float32 previous_output_ = 0.0F;
};

// Undoes broadcast FM pre-emphasis with a single-pole low-pass (time constant `tau_seconds`).
class DeEmphasis
{
public:
    auto configure( float64 tau_seconds, float64 sample_rate_hz ) -> void;
    auto process( std::span< float32 > samples ) -> void;

private:
    float32 alpha_    = 1.0F;
    float32 previous_ = 0.0F;
};

// Automatic gain control: scales the signal so its peak level approaches `target`.
class Agc
{
public:
    auto configure( float64 sample_rate_hz ) -> void;
    auto process( std::span< float32 > samples ) -> void;

    [[nodiscard( "Const getter" )]]
    auto gain( ) const -> float32;

private:
    float32 target_   = 0.5F;
    float32 max_gain_ = 1000.0F;
    float32 attack_   = 0.01F;
    float32 release_  = 0.0001F;
    float32 level_    = 0.0F;
};

// Turns an AM envelope into the modulation relative to the carrier (envelope / carrier - 1), so
// tone depths don't depend on signal strength. The carrier level is tracked slowly.
class CarrierNormalizer
{
public:
    auto configure( float64 time_constant_s, float64 sample_rate_hz ) -> void;

    // Overwrites `output`.
    auto process( std::span< float32 const > envelope, std::vector< float32 >& output ) -> void;

private:
    float32 smoothing_    = 1e-4F;
    float32 carrier_      = 0.0F;
    uint64  sample_count_ = 0U;
};

// Measures the amplitude and phase of a tone at a fixed frequency: a single DFT bin, averaged
// over time by two exponential stages. The second stage suppresses the ripple that other tones
// (and the tone's own negative frequency) leave after the first.
class ToneMeter
{
public:
    auto configure( float64 tone_hz, float64 sample_rate_hz, float64 time_constant_s ) -> void;
    auto process( std::span< float32 const > samples ) -> void;

    // Peak amplitude of the tone.
    [[nodiscard( "Const getter" )]]
    auto amplitude( ) const -> float32;

    // Phase (radians) relative to a cosine that started at the first sample.
    [[nodiscard( "Const getter" )]]
    auto phase( ) const -> float64;

private:
    std::complex< float64 > oscillator_  = 1.0;
    std::complex< float64 > step_        = 1.0;
    std::complex< float64 > correlation_ = 0.0;
    std::complex< float64 > smoothed_    = 0.0;
    float64                 smoothing_   = 0.0;
};

} // namespace ltb::radio::dsp
