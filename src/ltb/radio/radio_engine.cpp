// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "radio_engine.hpp"

// project
#include "ltb/radio/dsp/test_signal.hpp"

// external
#include <spdlog/spdlog.h>

// standard
#include <chrono>
#include <functional>
#include <ranges>

namespace ltb::radio
{
namespace
{

constexpr auto sample_rate_hz       = dsp::RadioPipeline::input_rate_hz;
constexpr auto audio_rate_hz        = dsp::RadioPipeline::audio_rate_hz;
constexpr auto iq_buffer_seconds    = 0.5;
constexpr auto audio_buffer_seconds = 0.25;
constexpr auto history_capacity     = 16384UZ;
constexpr auto worker_block_size    = 32768UZ;
constexpr auto test_block_duration  = std::chrono::milliseconds( 10 );
constexpr auto test_noise_level     = 0.01F;
constexpr auto load_smoothing       = 0.1F;

auto samples_for( float64 const seconds, float64 const rate_hz ) -> std::size_t
{
    return static_cast< std::size_t >( seconds * rate_hz );
}

// Sends a changed value to the hardware. If the device rejects it, the old value is restored so
// the settings keep describing the hardware, and the first error is kept in `result`.
template < typename Value, typename Setter >
auto update_hardware(
    Value&                 value,
    Value const&           old,
    Setter const&          set,
    utils::Result< void >& result
) -> void
{
    if ( value == old )
    {
        return;
    }
    if ( auto set_result = set( value ); !set_result )
    {
        value = old;
        if ( result )
        {
            result = std::move( set_result );
        }
    }
}

struct MakeHistory
{
    auto operator( )( dsp::StageInfo const& /*stage*/ ) const -> std::unique_ptr< SampleHistory >
    {
        return std::make_unique< SampleHistory >( history_capacity );
    }
};

} // namespace

struct RadioEngine::HistoryObserver : dsp::StageObserver
{
    explicit HistoryObserver( std::vector< std::unique_ptr< SampleHistory > >& histories )
        : histories_( histories )
    {
    }

    auto on_stage_output( std::size_t const stage, std::span< dsp::Complex const > const samples )
        -> void override
    {
        histories_.at( stage )->push( samples );
    }

    auto on_stage_output( std::size_t const stage, std::span< float32 const > const samples )
        -> void override
    {
        histories_.at( stage )->push( samples );
    }

private:
    std::vector< std::unique_ptr< SampleHistory > >& histories_;
};

struct RadioEngine::Worker
{
    RadioEngine& engine;

    auto operator( )( std::stop_token const& stop ) const -> void
    {
        engine.run_worker( stop );
    }
};

struct RadioEngine::TestSource
{
    RadioEngine& engine;

    auto operator( )( std::stop_token const& stop ) const -> void
    {
        engine.run_test_source( stop );
    }
};

RadioEngine::RadioEngine( dsp::Modulation const modulation, RadioSettings settings )
    : modulation_( modulation )
    , settings_( std::move( settings ) )
    , stages_( dsp::RadioPipeline::stage_infos( modulation ) )
    , histories_(
          stages_ | std::views::transform( MakeHistory{ } ) | std::ranges::to< std::vector >( )
      )
    , iq_buffer_( samples_for( iq_buffer_seconds, sample_rate_hz ) )
    , audio_buffer_( samples_for( audio_buffer_seconds, audio_rate_hz ) )
{
}

RadioEngine::~RadioEngine( )
{
    this->stop( );
}

auto RadioEngine::start( ) -> utils::Result< void >
{
    if ( this->is_running( ) )
    {
        return utils::success( );
    }

    iq_buffer_.clear( );
    audio_buffer_.clear( );
    for ( auto const& history : histories_ )
    {
        history->clear( );
    }
    processing_load_          = 0.0F;
    dropped_test_samples_     = 0U;
    test_center_frequency_hz_ = settings_.center_frequency_hz;
    {
        auto const lock   = std::scoped_lock( pending_mutex_ );
        pending_pipeline_ = this->pipeline_settings( );
    }

    if ( settings_.source == SourceType::Hackrf )
    {
        LTB_CHECK( auto const* const library, this->hackrf_library( ) );
        LTB_CHECK(
            auto receiver,
            HackrfReceiver::start(
                *library,
                settings_.hackrf_serial,
                {
                    .center_frequency_hz = settings_.center_frequency_hz,
                    .sample_rate_hz      = sample_rate_hz,
                    .lna_gain_db         = settings_.lna_gain_db,
                    .vga_gain_db         = settings_.vga_gain_db,
                    .amp_enabled         = settings_.amp_enabled,
                },
                iq_buffer_
            )
        );
        hackrf_ = std::move( receiver );
    }
    else
    {
        source_thread_ = std::jthread( TestSource{ *this } );
    }

    worker_thread_ = std::jthread( Worker{ *this } );

    if ( auto audio = AudioOutput::start( audio_buffer_, static_cast< uint32 >( audio_rate_hz ) ) )
    {
        audio_ = std::move( audio.value( ) );
        audio_error_.clear( );
    }
    else
    {
        audio_error_ = audio.error( ).debug_error_message( );
        spdlog::warn( "Running without audio: {}", audio_error_ );
    }

    return utils::success( );
}

auto RadioEngine::stop( ) -> void
{
    // Stop the producers first, then the consumers.
    hackrf_.reset( );
    source_thread_ = { };
    worker_thread_ = { };
    audio_.reset( );

    auto const lock = std::scoped_lock( navigation_mutex_ );
    vor_reading_    = { };
    ils_reading_    = { };
}

auto RadioEngine::vor_reading( ) const -> dsp::VorReading
{
    auto const lock = std::scoped_lock( navigation_mutex_ );
    return vor_reading_;
}

auto RadioEngine::ils_reading( ) const -> dsp::IlsReading
{
    auto const lock = std::scoped_lock( navigation_mutex_ );
    return ils_reading_;
}

auto RadioEngine::is_running( ) const -> bool
{
    return worker_thread_.joinable( );
}

auto RadioEngine::settings( ) const -> RadioSettings const&
{
    return settings_;
}

auto RadioEngine::apply( RadioSettings settings ) -> utils::Result< void >
{
    auto const old = std::exchange( settings_, std::move( settings ) );
    if ( !this->is_running( ) )
    {
        return utils::success( );
    }

    auto const device_changed
        = settings_.source == SourceType::Hackrf && old.hackrf_serial != settings_.hackrf_serial;
    if ( old.source != settings_.source || device_changed )
    {
        this->stop( );
        return this->start( );
    }

    // First, so the pipeline's shift uses the frequency the hardware is actually tuned to.
    auto result = this->apply_hardware( old );

    test_center_frequency_hz_ = settings_.center_frequency_hz;
    {
        auto const lock   = std::scoped_lock( pending_mutex_ );
        pending_pipeline_ = this->pipeline_settings( );
    }
    return result;
}

auto RadioEngine::modulation( ) const -> dsp::Modulation
{
    return modulation_;
}

auto RadioEngine::stages( ) const -> std::vector< dsp::StageInfo > const&
{
    return stages_;
}

auto RadioEngine::history( std::size_t const stage ) const -> SampleHistory const&
{
    return *histories_.at( stage );
}

auto RadioEngine::status( ) const -> RadioStatus
{
    return {
        .running         = this->is_running( ),
        .processing_load = processing_load_.load( ),
        .audio_buffer_seconds
        = static_cast< float32 >( static_cast< float64 >( audio_buffer_.size( ) ) / audio_rate_hz ),
        .dropped_input_samples
        = dropped_test_samples_.load( ) + ( hackrf_ ? hackrf_->dropped_samples( ) : 0U ),
        .channel_power_db = channel_power_db_.load( ),
        .squelch_open     = squelch_open_.load( ),
        .vor              = this->vor_reading( ),
        .ils              = this->ils_reading( ),
        .audio_error      = audio_error_,
    };
}

auto RadioEngine::hackrf_library( ) -> utils::Result< HackrfLibrary const* >
{
    if ( !library_.has_value( ) )
    {
        LTB_CHECK( auto library, HackrfLibrary::create( ) );
        library_ = std::move( library );
    }
    return &library_.value( );
}

auto RadioEngine::pipeline_settings( ) const -> dsp::PipelineSettings
{
    return {
        .shift_hz             = settings_.station_frequency_hz - settings_.center_frequency_hz,
        .channel_bandwidth_hz = settings_.channel_bandwidth_hz,
        .volume               = settings_.muted ? 0.0F : settings_.volume,
        .agc_enabled          = settings_.agc_enabled,
        .deemphasis_tau_s     = settings_.deemphasis_tau_s,
        .squelch_enabled      = settings_.squelch_enabled,
        .squelch_db           = settings_.squelch_db,
    };
}

auto RadioEngine::apply_hardware( RadioSettings const& old ) -> utils::Result< void >
{
    auto result = utils::success( );
    if ( !hackrf_ )
    {
        return result;
    }

    auto* const hackrf = hackrf_.get( );
    update_hardware(
        settings_.center_frequency_hz,
        old.center_frequency_hz,
        std::bind_front( &HackrfReceiver::set_center_frequency, hackrf ),
        result
    );
    update_hardware(
        settings_.lna_gain_db,
        old.lna_gain_db,
        std::bind_front( &HackrfReceiver::set_lna_gain, hackrf ),
        result
    );
    update_hardware(
        settings_.vga_gain_db,
        old.vga_gain_db,
        std::bind_front( &HackrfReceiver::set_vga_gain, hackrf ),
        result
    );
    update_hardware(
        settings_.amp_enabled,
        old.amp_enabled,
        std::bind_front( &HackrfReceiver::set_amp_enabled, hackrf ),
        result
    );
    return result;
}

auto RadioEngine::run_worker( std::stop_token const& stop ) -> void
{
    auto pipeline = std::optional< dsp::RadioPipeline >( );
    auto observer = HistoryObserver( histories_ );
    auto block    = std::vector< dsp::Complex >( worker_block_size );
    auto audio    = std::vector< float32 >( );

    while ( !stop.stop_requested( ) )
    {
        auto const count = iq_buffer_.pop( std::span( block ), std::chrono::milliseconds( 50 ) );

        if ( auto const lock = std::scoped_lock( pending_mutex_ ); pending_pipeline_.has_value( ) )
        {
            if ( pipeline.has_value( ) )
            {
                pipeline->configure( pending_pipeline_.value( ) );
            }
            else
            {
                pipeline.emplace( modulation_, pending_pipeline_.value( ) );
            }
            pending_pipeline_.reset( );
        }

        if ( count == 0UZ || !pipeline.has_value( ) )
        {
            continue;
        }

        auto const begin = std::chrono::steady_clock::now( );
        audio.clear( );
        pipeline->process( std::span( block ).first( count ), audio, &observer );
        auto const elapsed
            = std::chrono::duration< float64 >( std::chrono::steady_clock::now( ) - begin );

        auto const load = static_cast< float32 >(
            elapsed.count( ) * sample_rate_hz / static_cast< float64 >( count )
        );
        processing_load_
            = processing_load_.load( ) + load_smoothing * ( load - processing_load_.load( ) );

        channel_power_db_ = pipeline->channel_power_db( );
        squelch_open_     = pipeline->is_squelch_open( );
        {
            auto const lock = std::scoped_lock( navigation_mutex_ );
            vor_reading_    = pipeline->vor_reading( );
            ils_reading_    = pipeline->ils_reading( );
        }
        audio_buffer_.push( audio );
    }
}

auto RadioEngine::run_test_source( std::stop_token const& stop ) -> void
{
    auto generator = dsp::TestSignalGenerator(
        modulation_,
        sample_rate_hz,
        dsp::TestSignalGenerator::default_stations( modulation_ ),
        test_noise_level
    );

    auto const block_seconds = std::chrono::duration< float64 >( test_block_duration ).count( );
    auto       block = std::vector< dsp::Complex >( samples_for( block_seconds, sample_rate_hz ) );
    auto       next  = std::chrono::steady_clock::now( );

    while ( !stop.stop_requested( ) )
    {
        generator.set_center_frequency( test_center_frequency_hz_.load( ) );
        generator.generate( block );
        dropped_test_samples_ += iq_buffer_.push( block );

        // Real-time pacing. Don't try to catch up after a long stall (e.g. a debugger pause).
        next += test_block_duration;
        if ( auto const now = std::chrono::steady_clock::now( );
             now - next > 10 * test_block_duration )
        {
            next = now;
        }
        std::this_thread::sleep_until( next );
    }
}

} // namespace ltb::radio
