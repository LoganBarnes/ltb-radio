// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/cam/camera_render_params.hpp"
#include "ltb/utils/types.hpp"

namespace ltb::radio
{

// An orthographic camera looking at the XY plane (+Y is up) that can pan and zoom.
// Screen-space inputs are in the same units as the viewport size (typically ImGui pixels).
class PanZoomCamera2d
{
public:
    auto resize( glm::vec2 viewport_size ) -> void;

    // Moves the view so the world follows the mouse. The delta is in screen pixels (+Y down).
    auto pan( glm::vec2 mouse_delta ) -> void;

    // Zooms in (positive) or out (negative) while keeping the world point under the cursor fixed.
    // `cursor` is relative to the top-left of the viewport.
    auto zoom( float32 scroll_amount, glm::vec2 cursor ) -> void;

    [[nodiscard( "Const getter" )]]
    auto viewport_size( ) const -> glm::vec2;

    [[nodiscard( "Const getter" )]]
    auto center( ) const -> glm::vec2;

    [[nodiscard( "Const getter" )]]
    auto pixels_per_unit( ) const -> float32;

    [[nodiscard( "Const computation" )]]
    auto render_params( ) const -> cam::CameraRenderParams;

private:
    glm::vec2 viewport_size_   = glm::vec2( 1.0F );
    glm::vec2 center_          = glm::vec2( 0.0F );
    float32   pixels_per_unit_ = 50.0F;
};

} // namespace ltb::radio
