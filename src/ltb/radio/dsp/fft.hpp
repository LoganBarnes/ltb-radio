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

// In-place forward FFT. The size must be a power of two.
auto fft( std::span< Complex > data ) -> void;

// Hann-windowed power spectrum in dB where a full-scale complex tone is 0 dB. The size of
// `samples` must be a power of two. Overwrites `output` with one value per bin, ordered from
// -fs/2 to fs/2 (DC is at index size/2).
auto power_spectrum_db( std::span< Complex const > samples, std::vector< float32 >& output )
    -> void;

// As above for a real signal where a full-scale sine is 0 dB. Only the non-negative frequencies
// (0 to fs/2, size/2 + 1 bins) are written.
auto power_spectrum_db( std::span< float32 const > samples, std::vector< float32 >& output )
    -> void;

} // namespace ltb::radio::dsp
