// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "hackrf_info_app.hpp"

// project
#include "ltb/gui/imgui_utils.hpp"

// external
#include <imgui.h>

// standard
#include <algorithm>
#include <ranges>

namespace ltb::radio
{

auto HackrfInfoApp::name( ) const -> std::string const&
{
    static auto const app_name = std::string{ "HackRF Info" };
    return app_name;
}

auto HackrfInfoApp::configure_gui( ) -> void
{
    if ( needs_refresh_ )
    {
        this->refresh( );
    }

    if ( ImGui::Begin( "HackRF Info", &is_open ) )
    {
        gui::imgui_fmt< ImGui::Text >( "libhackrf {}", hackrf_library_version( ) );

        if ( ImGui::Button( "Refresh" ) )
        {
            needs_refresh_ = true;
        }

        if ( !error_message_.empty( ) )
        {
            gui::imgui_fmt_colored( ImVec4( 1.0F, 0.4F, 0.4F, 1.0F ), "{}", error_message_ );
        }

        ImGui::SeparatorText( "Devices" );
        if ( devices_.empty( ) )
        {
            ImGui::TextUnformatted( "No HackRF devices found." );
        }
        for ( auto const i : std::views::iota( std::size_t{ 0 }, devices_.size( ) ) )
        {
            auto const& device = devices_[ i ];
            auto const  label  = device.usb_board_name + " " + device.serial;
            if ( ImGui::Selectable( label.c_str( ), selected_index_ == i ) )
            {
                this->select( i );
            }
        }

        if ( details_.has_value( ) )
        {
            ImGui::SeparatorText( "Details" );
            gui::imgui_fmt< ImGui::Text >( "Board:    {}", details_->board_name );
            gui::imgui_fmt< ImGui::Text >( "Firmware: {}", details_->firmware_version );
            gui::imgui_fmt<
                ImGui::Text >( "Serial:   {}", devices_[ selected_index_.value( ) ].serial );
            gui::imgui_fmt< ImGui::Text >(
                "MCU part ID: {:08x} {:08x}",
                details_->mcu_part_id[ 0 ],
                details_->mcu_part_id[ 1 ]
            );
        }
    }
    ImGui::End( );
}

auto HackrfInfoApp::refresh( ) -> void
{
    needs_refresh_ = false;
    error_message_.clear( );

    auto const previous_serial
        = selected_index_.transform( [ this ]( auto const i ) { return devices_[ i ].serial; } );

    devices_.clear( );
    selected_index_.reset( );
    details_.reset( );

    if ( !library_.has_value( ) )
    {
        auto library = HackrfLibrary::create( );
        if ( !library )
        {
            error_message_ = library.error( ).debug_error_message( );
            return;
        }
        library_ = std::move( library.value( ) );
    }

    auto devices = list_hackrf_devices( library_.value( ) );
    if ( !devices )
    {
        error_message_ = devices.error( ).debug_error_message( );
        return;
    }
    devices_ = std::move( devices.value( ) );

    if ( devices_.empty( ) )
    {
        return;
    }

    // Keep the previous selection if it's still connected, otherwise pick the first device.
    auto const previous = std::ranges::find( devices_, previous_serial, &HackrfListing::serial );
    this->select(
        static_cast< std::size_t >( std::distance( devices_.begin( ), previous ) )
        % devices_.size( )
    );
}

auto HackrfInfoApp::select( std::size_t const index ) -> void
{
    selected_index_ = index;
    details_.reset( );
    error_message_.clear( );

    auto details = read_hackrf_details( library_.value( ), devices_[ index ].serial );
    if ( !details )
    {
        error_message_ = details.error( ).debug_error_message( );
        return;
    }
    details_ = std::move( details.value( ) );
}

} // namespace ltb::radio
