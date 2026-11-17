// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/radio/sample_ring_buffer.hpp"
#include "ltb/utils/result.hpp"
#include "ltb/utils/types.hpp"

// standard
#include <memory>

namespace ltb::radio
{

// Plays mono float samples from a ring buffer on the default output device. Silence is played
// when the buffer runs dry. Playback stops when destroyed.
class AudioOutput
{
public:
    [[nodiscard( "Starting can fail" )]]
    static auto start( SampleRingBuffer< float32 >& source, uint32 sample_rate_hz )
        -> utils::Result< std::unique_ptr< AudioOutput > >;

    ~AudioOutput( );

    AudioOutput( AudioOutput const& )                    = delete;
    auto operator=( AudioOutput const& ) -> AudioOutput& = delete;
    AudioOutput( AudioOutput&& )                         = delete;
    auto operator=( AudioOutput&& ) -> AudioOutput&      = delete;

private:
    AudioOutput( );

    // Keeps miniaudio out of the header.
    struct Device;
    std::unique_ptr< Device > device_;

    friend struct AudioCallback;
};

} // namespace ltb::radio
