// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/radio/dsp/types.hpp"
#include "ltb/radio/hackrf_info.hpp"
#include "ltb/radio/sample_ring_buffer.hpp"
#include "ltb/utils/result.hpp"

// standard
#include <atomic>
#include <memory>
#include <string>
#include <vector>

struct hackrf_device;

namespace ltb::radio
{

struct HackrfReceiverSettings
{
    float64 center_frequency_hz = 100.0e6;
    float64 sample_rate_hz      = 2.4e6;
    uint32  lna_gain_db         = 16U;
    uint32  vga_gain_db         = 20U;
    bool    amp_enabled         = false;
};

// Streams I/Q samples from a HackRF into a ring buffer. Streaming stops when destroyed.
class HackrfReceiver
{
public:
    // Opens the device with `serial` (or the first device when empty) and starts receiving.
    [[nodiscard( "Starting can fail" )]]
    static auto start(
        HackrfLibrary const&              library,
        std::string const&                serial,
        HackrfReceiverSettings const&     settings,
        SampleRingBuffer< dsp::Complex >& output
    ) -> utils::Result< std::unique_ptr< HackrfReceiver > >;

    ~HackrfReceiver( );

    HackrfReceiver( HackrfReceiver const& )                    = delete;
    auto operator=( HackrfReceiver const& ) -> HackrfReceiver& = delete;
    HackrfReceiver( HackrfReceiver&& )                         = delete;
    auto operator=( HackrfReceiver&& ) -> HackrfReceiver&      = delete;

    // LNA gain is rounded down to a multiple of 8 dB (0-40) and VGA gain to a multiple of
    // 2 dB (0-62) by the hardware.
    auto set_center_frequency( float64 frequency_hz ) -> utils::Result< void >;
    auto set_lna_gain( uint32 gain_db ) -> utils::Result< void >;
    auto set_vga_gain( uint32 gain_db ) -> utils::Result< void >;
    auto set_amp_enabled( bool enabled ) -> utils::Result< void >;

    // Samples overwritten because the processing thread fell behind.
    [[nodiscard( "Const getter" )]]
    auto dropped_samples( ) const -> uint64;

private:
    explicit HackrfReceiver( SampleRingBuffer< dsp::Complex >& output );

    // Converts and forwards a transfer. Called on libhackrf's transfer thread.
    auto receive( std::span< uint8 const > bytes ) -> void;

    friend struct HackrfReceiveCallback;

    hackrf_device*                    device_ = nullptr;
    SampleRingBuffer< dsp::Complex >& output_;

    // Only used on the libhackrf transfer thread.
    std::vector< dsp::Complex > converted_ = { };

    std::atomic< uint64 > dropped_samples_ = 0U;
};

} // namespace ltb::radio
