// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/cam/camera_render_params.hpp"
#include "ltb/utils/types.hpp"

namespace ltb::radio
{

// A perspective camera that orbits a target point. +Y is up.
class OrbitCamera
{
public:
    // Sets the viewport size (in pixels), which determines the aspect ratio.
    auto resize( glm::vec2 viewport_size ) -> void;

    // Rotates around the target. The delta is in pixels.
    auto orbit( glm::vec2 mouse_delta ) -> void;

    // Moves toward (positive) or away from (negative) the target.
    auto zoom( float32 scroll_amount ) -> void;

    [[nodiscard( "Const getter" )]]
    auto viewport_size( ) const -> glm::vec2;

    [[nodiscard( "Const getter" )]]
    auto eye( ) const -> glm::vec3;

    [[nodiscard( "Const computation" )]]
    auto render_params( ) const -> cam::CameraRenderParams;

private:
    glm::vec3 target_        = glm::vec3( 0.0F );
    float32   distance_      = 4.0F;
    float32   yaw_           = glm::radians( 35.0F );
    float32   pitch_         = glm::radians( 25.0F );
    float32   fov_y_radians_ = glm::radians( 45.0F );
    float32   near_plane_    = 0.1F;
    float32   far_plane_     = 100.0F;

    glm::vec2 viewport_size_ = glm::vec2( 1.0F );
};

} // namespace ltb::radio
