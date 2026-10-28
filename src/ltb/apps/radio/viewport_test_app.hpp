// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "sub_app.hpp"
#include "ltb/radio/colored_mesh_pipeline.hpp"
#include "ltb/radio/imgui_viewport.hpp"
#include "ltb/radio/orbit_camera.hpp"

// standard
#include <memory>
#include <string>

namespace ltb::radio
{

// Renders a cube into a docked ImGui window to test viewport resizing and camera updates.
// Left-drag orbits the camera and the mouse wheel zooms.
class ViewportTestApp : public SubApp
{
public:
    [[nodiscard( "Const getter" )]]
    auto name( ) const -> std::string const& override;

    auto initialize( SubAppContext const& context ) -> utils::Result< void > override;
    auto configure_gui( uint32 frame_index ) -> void override;
    auto record_commands( vlk::objs::FrameInfo const& frame ) -> utils::Result< void > override;

private:
    std::unique_ptr< ImguiViewport >       viewport_ = nullptr;
    std::unique_ptr< ColoredMeshPipeline > cube_     = nullptr;

    OrbitCamera camera_ = { };

    auto handle_input( ) -> void;
};

} // namespace ltb::radio
