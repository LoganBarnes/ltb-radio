// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/apps/radio/app_view.hpp"
#include "ltb/radio/hackrf_info.hpp"

// standard
#include <optional>
#include <string>
#include <vector>

namespace ltb::radio
{

// Lists connected HackRF devices and shows details of the selected one.
class HackrfInfoView : public AppView
{
public:
    [[nodiscard( "Const getter" )]]
    auto name( ) const -> std::string const& override;

    auto configure_gui( uint32 frame_index ) -> void override;

private:
    std::optional< HackrfLibrary > library_ = std::nullopt;

    std::vector< HackrfListing >   devices_        = { };
    std::optional< std::size_t >   selected_index_ = std::nullopt;
    std::optional< HackrfDetails > details_        = std::nullopt;
    std::string                    error_message_  = { };
    bool                           needs_refresh_  = true;

    auto refresh( ) -> void;
    auto select( std::size_t index ) -> void;
};

} // namespace ltb::radio
