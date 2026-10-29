// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/apps/radio/sub_app.hpp"

namespace ltb::radio
{

// Hosts HackRF device views.
class HackrfApp : public SubApp
{
public:
    explicit HackrfApp( SubAppContext const& context );

    [[nodiscard( "Const getter" )]]
    auto name( ) const -> std::string const& override;
};

} // namespace ltb::radio
