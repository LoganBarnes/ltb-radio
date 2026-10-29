// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/apps/radio/app_view.hpp"

namespace ltb::radio
{

// Shows Dear ImGui's built-in demo window.
class ImguiDemoView : public AppView
{
public:
    [[nodiscard( "Const getter" )]]
    auto name( ) const -> std::string const& override;

    auto configure_gui( uint32 frame_index ) -> void override;
};

} // namespace ltb::radio
