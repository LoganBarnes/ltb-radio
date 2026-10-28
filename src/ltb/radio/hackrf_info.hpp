// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/utils/result.hpp"
#include "ltb/utils/types.hpp"

// standard
#include <array>
#include <string>
#include <vector>

namespace ltb::radio
{

// Keeps libhackrf initialized for the lifetime of the object.
class HackrfLibrary
{
public:
    [[nodiscard( "Creating the library can fail" )]]
    static auto create( ) -> utils::Result< HackrfLibrary >;

    HackrfLibrary( HackrfLibrary const& )                    = delete;
    auto operator=( HackrfLibrary const& ) -> HackrfLibrary& = delete;

    HackrfLibrary( HackrfLibrary&& other ) noexcept;
    auto operator=( HackrfLibrary&& other ) noexcept -> HackrfLibrary&;

    ~HackrfLibrary( );

private:
    HackrfLibrary( ) = default;

    bool owns_library_ = false;
};

// Information available without opening the device.
struct HackrfListing
{
    std::string serial;
    std::string usb_board_name;
};

// Information that requires opening the device.
struct HackrfDetails
{
    std::string             board_name;
    std::string             firmware_version;
    std::array< uint32, 2 > mcu_part_id;
    std::array< uint32, 4 > mcu_serial;
};

[[nodiscard( "Const getter" )]]
auto hackrf_library_version( ) -> std::string;

[[nodiscard( "Listing can fail" )]]
auto list_hackrf_devices( HackrfLibrary const& library )
    -> utils::Result< std::vector< HackrfListing > >;

[[nodiscard( "Reading can fail" )]]
auto read_hackrf_details( HackrfLibrary const& library, std::string const& serial )
    -> utils::Result< HackrfDetails >;

} // namespace ltb::radio
