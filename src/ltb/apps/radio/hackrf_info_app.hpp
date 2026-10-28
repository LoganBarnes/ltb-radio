// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "sub_app.hpp"
#include "ltb/radio/hackrf_info.hpp"

// standard
#include <optional>
#include <string>
#include <vector>

namespace ltb::radio
{

// Lists connected HackRF devices and shows details of the selected one.
class HackrfInfoApp : public SubApp
{
public:
    [[nodiscard( "Const getter" )]]
    auto name( ) const -> std::string const& override;

    auto configure_gui( ) -> void override;

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
