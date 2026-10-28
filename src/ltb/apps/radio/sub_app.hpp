// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// standard
#include <string>

namespace ltb::radio
{

// A sub application that is launched from the toolbar's "Apps" menu and draws its own
// (dockable) ImGui windows while open.
class SubApp
{
public:
    virtual ~SubApp( ) = default;

    [[nodiscard( "Const getter" )]]
    virtual auto name( ) const -> std::string const& = 0;

    virtual auto configure_gui( ) -> void = 0;

    bool is_open = false;
};

} // namespace ltb::radio
