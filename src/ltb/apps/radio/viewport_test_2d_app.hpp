// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "sub_app.hpp"
#include "ltb/radio/colored_mesh_pipeline.hpp"
#include "ltb/radio/imgui_viewport.hpp"
#include "ltb/radio/pan_zoom_camera_2d.hpp"

// standard
#include <memory>
#include <string>

namespace ltb::radio
{

// Renders a grid, a waveform and some shapes in a docked ImGui window to test 2D viewport
// resizing, panning (left/middle drag) and zooming (mouse wheel).
class ViewportTest2dApp : public SubApp
{
public:
    [[nodiscard( "Const getter" )]]
    auto name( ) const -> std::string const& override;

    auto initialize( SubAppContext const& context ) -> utils::Result< void > override;
    auto configure_gui( uint32 frame_index ) -> void override;
    auto record_commands( vlk::objs::FrameInfo const& frame ) -> utils::Result< void > override;

private:
    std::unique_ptr< ImguiViewport >       viewport_  = nullptr;
    std::unique_ptr< ColoredMeshPipeline > triangles_ = nullptr;
    std::unique_ptr< ColoredMeshPipeline > lines_     = nullptr;

    PanZoomCamera2d camera_ = { };

    auto handle_input( ) -> void;
};

} // namespace ltb::radio
