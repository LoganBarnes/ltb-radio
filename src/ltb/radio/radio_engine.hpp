// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/radio/audio_output.hpp"
#include "ltb/radio/dsp/radio_pipeline.hpp"
#include "ltb/radio/hackrf_info.hpp"
#include "ltb/radio/hackrf_receiver.hpp"
#include "ltb/radio/sample_history.hpp"
#include "ltb/radio/sample_ring_buffer.hpp"

// standard
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace ltb::radio
{

enum class SourceType
{
    TestSignal,
    Hackrf,
};

struct RadioSettings
{
    SourceType  source        = SourceType::TestSignal;
    std::string hackrf_serial = { }; // Empty selects the first device.

    // The receiver (hardware) frequency. The station must be within the received bandwidth.
    float64 center_frequency_hz  = 0.0;
    float64 station_frequency_hz = 0.0;

    uint32 lna_gain_db = 16U;
    uint32 vga_gain_db = 20U;
    bool   amp_enabled = false;

    float64 channel_bandwidth_hz = 10.0e3;
    float32 volume               = 0.5F;
    bool    muted                = false;
    bool    agc_enabled          = true;
    float64 deemphasis_tau_s     = 75.0e-6;
    bool    squelch_enabled      = false;
    float32 squelch_db           = -50.0F;

    auto operator==( RadioSettings const& ) const -> bool = default;
};

struct RadioStatus
{
    bool    running               = false;
    float32 processing_load       = 0.0F; // Processing time / signal time.
    float32 audio_buffer_seconds  = 0.0F;
    uint64  dropped_input_samples = 0U;
    float32 channel_power_db      = -200.0F;
    bool    squelch_open          = true;
    // Only valid for VOR radios.
    dsp::VorReading vor = { };
    // Only valid for ILS radios.
    dsp::IlsReading ils = { };
    // Set when audio could not start; the radio still runs without sound.
    std::string audio_error = { };
};

// Runs a radio: a sample source (test signal or HackRF) feeds a processing thread running a
// `dsp::RadioPipeline`, whose audio is played and whose stages are kept for display.
//
// All public functions must be called from the same (GUI) thread.
class RadioEngine
{
public:
    RadioEngine( dsp::Modulation modulation, RadioSettings settings );
    ~RadioEngine( );

    RadioEngine( RadioEngine const& )                    = delete;
    auto operator=( RadioEngine const& ) -> RadioEngine& = delete;
    RadioEngine( RadioEngine&& )                         = delete;
    auto operator=( RadioEngine&& ) -> RadioEngine&      = delete;

    auto start( ) -> utils::Result< void >;
    auto stop( ) -> void;

    [[nodiscard( "Const getter" )]]
    auto is_running( ) const -> bool;

    [[nodiscard( "Const getter" )]]
    auto settings( ) const -> RadioSettings const&;

    // Takes effect immediately while running. Changing the source (or the HackRF device)
    // restarts the radio. Hardware settings the device rejects keep their old values, and the
    // first rejection is returned.
    auto apply( RadioSettings settings ) -> utils::Result< void >;

    [[nodiscard( "Const getter" )]]
    auto modulation( ) const -> dsp::Modulation;

    [[nodiscard( "Const getter" )]]
    auto stages( ) const -> std::vector< dsp::StageInfo > const&;

    // Recent output of each stage (indexed like `stages()`).
    [[nodiscard( "Const getter" )]]
    auto history( std::size_t stage ) const -> SampleHistory const&;

    [[nodiscard( "Const getter" )]]
    auto status( ) const -> RadioStatus;

    // Created on first use. Only one may exist per process, so device listing should use this.
    auto hackrf_library( ) -> utils::Result< HackrfLibrary const* >;

private:
    struct HistoryObserver;
    struct Worker;
    struct TestSource;

    dsp::Modulation                                 modulation_;
    RadioSettings                                   settings_;
    std::vector< dsp::StageInfo >                   stages_;
    std::vector< std::unique_ptr< SampleHistory > > histories_;

    SampleRingBuffer< dsp::Complex > iq_buffer_;
    SampleRingBuffer< float32 >      audio_buffer_;

    std::optional< HackrfLibrary >    library_     = std::nullopt;
    std::unique_ptr< HackrfReceiver > hackrf_      = nullptr;
    std::unique_ptr< AudioOutput >    audio_       = nullptr;
    std::string                       audio_error_ = { };

    // Shared with the worker and test source threads.
    std::mutex                             pending_mutex_            = { };
    std::optional< dsp::PipelineSettings > pending_pipeline_         = std::nullopt;
    std::atomic< float64 >                 test_center_frequency_hz_ = 0.0;
    std::atomic< float32 >                 processing_load_          = 0.0F;
    std::atomic< float32 >                 channel_power_db_         = -200.0F;
    std::atomic< bool >                    squelch_open_             = true;
    mutable std::mutex                     navigation_mutex_         = { };
    dsp::VorReading                        vor_reading_              = { };
    dsp::IlsReading                        ils_reading_              = { };
    std::atomic< uint64 >                  dropped_test_samples_     = 0U;

    std::jthread worker_thread_ = { };
    std::jthread source_thread_ = { };

    [[nodiscard( "Pure computation" )]]
    auto pipeline_settings( ) const -> dsp::PipelineSettings;

    auto apply_hardware( RadioSettings const& old ) -> utils::Result< void >;

    [[nodiscard( "Const getter" )]]
    auto vor_reading( ) const -> dsp::VorReading;

    [[nodiscard( "Const getter" )]]
    auto ils_reading( ) const -> dsp::IlsReading;

    auto run_worker( std::stop_token const& stop ) -> void;
    auto run_test_source( std::stop_token const& stop ) -> void;
};

} // namespace ltb::radio
