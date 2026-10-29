// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "imgui_demo_view.hpp"

// external
#include <imgui.h>

namespace ltb::radio
{

auto ImguiDemoView::name( ) const -> std::string const&
{
    static auto const view_name = std::string( "ImGui Demo" );
    return view_name;
}

auto ImguiDemoView::configure_gui( uint32 const /*frame_index*/ ) -> void
{
    ImGui::ShowDemoWindow( &is_open );
}

} // namespace ltb::radio
