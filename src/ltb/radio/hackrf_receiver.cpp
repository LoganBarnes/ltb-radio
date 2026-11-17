// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "hackrf_receiver.hpp"

// external
#include <hackrf.h>

// standard
#include <bit>
#include <cmath>

namespace ltb::radio
{
namespace
{

auto check( int const code, char const* const operation ) -> utils::Result< void >
{
    if ( code != HACKRF_SUCCESS )
    {
        return LTB_MAKE_UNEXPECTED_ERROR(
            "{} failed: {}",
            operation,
            hackrf_error_name( static_cast< hackrf_error >( code ) )
        );
    }
    return utils::success( );
}

auto to_sample( uint8 const byte ) -> float32
{
    return static_cast< float32 >( std::bit_cast< int8 >( byte ) ) / 128.0F;
}

} // namespace

struct HackrfReceiveCallback
{
    static auto call( hackrf_transfer* const transfer ) -> int
    {
        static_cast< HackrfReceiver* >( transfer->rx_ctx )
            ->receive(
                std::span( transfer->buffer, static_cast< std::size_t >( transfer->valid_length ) )
            );
        return 0;
    }
};

HackrfReceiver::HackrfReceiver( SampleRingBuffer< dsp::Complex >& output )
    : output_( output )
{
}

auto HackrfReceiver::start(
    HackrfLibrary const& /*library*/,
    std::string const&                settings_serial,
    HackrfReceiverSettings const&     settings,
    SampleRingBuffer< dsp::Complex >& output
) -> utils::Result< std::unique_ptr< HackrfReceiver > >
{
    // The constructor is private.
    auto receiver = std::unique_ptr< HackrfReceiver >( new HackrfReceiver( output ) );

    auto const* const serial = settings_serial.empty( ) ? nullptr : settings_serial.c_str( );
    LTB_CHECK(
        check( hackrf_open_by_serial( serial, &receiver->device_ ), "hackrf_open_by_serial" )
    );

    auto* const device = receiver->device_;
    LTB_CHECK(
        check( hackrf_set_sample_rate( device, settings.sample_rate_hz ), "hackrf_set_sample_rate" )
    );

    // The analog filter just narrower than the sample rate keeps aliases out.
    auto const filter_hz = hackrf_compute_baseband_filter_bw_round_down_lt(
        static_cast< uint32 >( settings.sample_rate_hz )
    );
    LTB_CHECK( check(
        hackrf_set_baseband_filter_bandwidth( device, filter_hz ),
        "hackrf_set_baseband_filter_bandwidth"
    ) );

    LTB_CHECK( receiver->set_center_frequency( settings.center_frequency_hz ) );
    LTB_CHECK( receiver->set_lna_gain( settings.lna_gain_db ) );
    LTB_CHECK( receiver->set_vga_gain( settings.vga_gain_db ) );
    LTB_CHECK( receiver->set_amp_enabled( settings.amp_enabled ) );

    LTB_CHECK( check(
        hackrf_start_rx( device, &HackrfReceiveCallback::call, receiver.get( ) ),
        "hackrf_start_rx"
    ) );

    return receiver;
}

HackrfReceiver::~HackrfReceiver( )
{
    if ( device_ != nullptr )
    {
        // Stops the transfers (joining libhackrf's thread) before the device is released.
        utils::ignore( hackrf_stop_rx( device_ ) );
        utils::ignore( hackrf_close( device_ ) );
    }
}

auto HackrfReceiver::set_center_frequency( float64 const frequency_hz ) -> utils::Result< void >
{
    return check(
        hackrf_set_freq( device_, static_cast< uint64 >( std::llround( frequency_hz ) ) ),
        "hackrf_set_freq"
    );
}

auto HackrfReceiver::set_lna_gain( uint32 const gain_db ) -> utils::Result< void >
{
    return check( hackrf_set_lna_gain( device_, gain_db ), "hackrf_set_lna_gain" );
}

auto HackrfReceiver::set_vga_gain( uint32 const gain_db ) -> utils::Result< void >
{
    return check( hackrf_set_vga_gain( device_, gain_db ), "hackrf_set_vga_gain" );
}

auto HackrfReceiver::set_amp_enabled( bool const enabled ) -> utils::Result< void >
{
    return check(
        hackrf_set_amp_enable( device_, static_cast< uint8 >( enabled ? 1 : 0 ) ),
        "hackrf_set_amp_enable"
    );
}

auto HackrfReceiver::dropped_samples( ) const -> uint64
{
    return dropped_samples_.load( );
}

auto HackrfReceiver::receive( std::span< uint8 const > const bytes ) -> void
{
    // Interleaved signed 8-bit I and Q.
    converted_.resize( bytes.size( ) / 2UZ );
    for ( auto i = 0UZ; i < converted_.size( ); ++i )
    {
        converted_[ i ] = { to_sample( bytes[ 2UZ * i ] ), to_sample( bytes[ 2UZ * i + 1UZ ] ) };
    }
    dropped_samples_ += output_.push( converted_ );
}

} // namespace ltb::radio
