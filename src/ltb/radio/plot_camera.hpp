// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/utils/types.hpp"

// external
#include <glm/glm.hpp>

namespace ltb::radio
{

// Shows a rectangle of plot space in a viewport. Unlike `PanZoomCamera2d`, each axis has its
// own scale so signals with very different X and Y ranges fill the view.
// Screen positions are relative to the viewport's top-left corner (+Y down).
class PlotCamera
{
public:
    // Ignored if either axis is empty or inverted.
    auto set_bounds( glm::vec2 min, glm::vec2 max ) -> void;

    [[nodiscard( "Const getter" )]]
    auto min( ) const -> glm::vec2;

    [[nodiscard( "Const getter" )]]
    auto max( ) const -> glm::vec2;

    // Moves the view so the plot follows the mouse.
    auto pan( glm::vec2 pixel_delta, glm::vec2 viewport_size ) -> void;

    // Zooms in (positive) or out (negative) on the selected axes while keeping the plot point
    // under the cursor fixed.
    auto zoom( float32 scroll_amount, glm::vec2 cursor, glm::vec2 viewport_size, glm::bvec2 axes )
        -> void;

    [[nodiscard( "Const computation" )]]
    auto plot_from_screen( glm::vec2 screen, glm::vec2 viewport_size ) const -> glm::vec2;

    [[nodiscard( "Const computation" )]]
    auto screen_from_plot( glm::vec2 plot, glm::vec2 viewport_size ) const -> glm::vec2;

    // Vulkan clip space (+Y down) from plot space (+Y up).
    [[nodiscard( "Const computation" )]]
    auto clip_from_plot( ) const -> glm::mat4;

private:
    glm::vec2 min_ = glm::vec2( 0.0F, -1.0F );
    glm::vec2 max_ = glm::vec2( 1.0F, 1.0F );
};

} // namespace ltb::radio
