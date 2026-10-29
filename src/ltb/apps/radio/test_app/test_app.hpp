// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/apps/radio/sub_app.hpp"

namespace ltb::radio
{

// Collects development views: viewport tests, the ImGui demo and device info.
class TestApp : public SubApp
{
public:
    explicit TestApp( SubAppContext const& context );

    [[nodiscard( "Const getter" )]]
    auto name( ) const -> std::string const& override;
};

} // namespace ltb::radio
