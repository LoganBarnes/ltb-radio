// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "hackrf_info.hpp"

// external
#include <hackrf.h>

// standard
#include <algorithm>
#include <memory>
#include <ranges>
#include <utility>

namespace ltb::radio
{

namespace
{

struct DeviceListDeleter
{
    auto operator( )( hackrf_device_list_t* list ) const -> void
    {
        hackrf_device_list_free( list );
    }
};

struct DeviceDeleter
{
    auto operator( )( hackrf_device* device ) const -> void
    {
        utils::ignore( hackrf_close( device ) );
    }
};

using DeviceListPtr = std::unique_ptr< hackrf_device_list_t, DeviceListDeleter >;
using DevicePtr     = std::unique_ptr< hackrf_device, DeviceDeleter >;

auto is_success( int const code ) -> bool
{
    return code == HACKRF_SUCCESS;
}

auto error_name( int const code ) -> char const*
{
    return hackrf_error_name( static_cast< hackrf_error >( code ) );
}

} // namespace

auto HackrfLibrary::create( ) -> utils::Result< HackrfLibrary >
{
    if ( auto const code = hackrf_init( ); !is_success( code ) )
    {
        return LTB_MAKE_UNEXPECTED_ERROR( "hackrf_init() failed: {}", error_name( code ) );
    }

    auto library          = HackrfLibrary{ };
    library.owns_library_ = true;
    return library;
}

HackrfLibrary::HackrfLibrary( HackrfLibrary&& other ) noexcept
    : owns_library_( std::exchange( other.owns_library_, false ) )
{
}

auto HackrfLibrary::operator=( HackrfLibrary&& other ) noexcept -> HackrfLibrary&
{
    std::swap( owns_library_, other.owns_library_ );
    return *this;
}

HackrfLibrary::~HackrfLibrary( )
{
    if ( owns_library_ )
    {
        utils::ignore( hackrf_exit( ) );
    }
}

auto hackrf_library_version( ) -> std::string
{
    return ::hackrf_library_version( );
}

auto list_hackrf_devices( HackrfLibrary const& /*library*/ )
    -> utils::Result< std::vector< HackrfListing > >
{
    auto const list = DeviceListPtr{ hackrf_device_list( ) };
    if ( !list )
    {
        return LTB_MAKE_UNEXPECTED_ERROR( "hackrf_device_list() failed" );
    }

    auto listings = std::vector< HackrfListing >{ };
    listings.reserve( static_cast< std::size_t >( list->devicecount ) );

    for ( auto const i : std::views::iota( 0, list->devicecount ) )
    {
        // Entries can be null (e.g. when the serial could not be read).
        if ( char const* const serial = list->serial_numbers[ i ] )
        {
            listings.push_back( {
                .serial         = serial,
                .usb_board_name = hackrf_usb_board_id_name( list->usb_board_ids[ i ] ),
            } );
        }
    }

    return listings;
}

auto read_hackrf_details( HackrfLibrary const& /*library*/, std::string const& serial )
    -> utils::Result< HackrfDetails >
{
    hackrf_device* raw_device = nullptr;
    if ( auto const code = hackrf_open_by_serial( serial.c_str( ), &raw_device );
         !is_success( code ) )
    {
        return LTB_MAKE_UNEXPECTED_ERROR(
            "hackrf_open_by_serial({}) failed: {}",
            serial,
            error_name( code )
        );
    }
    auto const device = DevicePtr{ raw_device };

    auto details = HackrfDetails{ };

    auto board_id = static_cast< uint8_t >( BOARD_ID_UNDETECTED );
    if ( auto const code = hackrf_board_id_read( device.get( ), &board_id ); !is_success( code ) )
    {
        return LTB_MAKE_UNEXPECTED_ERROR( "hackrf_board_id_read() failed: {}", error_name( code ) );
    }
    details.board_name = hackrf_board_id_name( static_cast< hackrf_board_id >( board_id ) );

    // The buffer is one byte larger than the length passed to leave room for the null byte.
    auto version = std::array< char, 256 >{ };
    if ( auto const code = hackrf_version_string_read( device.get( ), version.data( ), 255 );
         !is_success( code ) )
    {
        return LTB_MAKE_UNEXPECTED_ERROR(
            "hackrf_version_string_read() failed: {}",
            error_name( code )
        );
    }
    details.firmware_version = version.data( );

    auto partid_serialno = read_partid_serialno_t{ };
    if ( auto const code = hackrf_board_partid_serialno_read( device.get( ), &partid_serialno );
         !is_success( code ) )
    {
        return LTB_MAKE_UNEXPECTED_ERROR(
            "hackrf_board_partid_serialno_read() failed: {}",
            error_name( code )
        );
    }
    std::ranges::copy( partid_serialno.part_id, details.mcu_part_id.begin( ) );
    std::ranges::copy( partid_serialno.serial_no, details.mcu_serial.begin( ) );

    return details;
}

} // namespace ltb::radio
