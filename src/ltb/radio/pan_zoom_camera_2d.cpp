// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "pan_zoom_camera_2d.hpp"

// standard
#include <algorithm>
#include <cmath>

namespace ltb::radio
{

namespace
{

constexpr auto zoom_factor         = 1.15F;
constexpr auto min_pixels_per_unit = 2.0F;
constexpr auto max_pixels_per_unit = 5000.0F;

// Screen offsets have +Y down but the world has +Y up.
auto to_world_axes( glm::vec2 const screen_offset ) -> glm::vec2
{
    return { screen_offset.x, -screen_offset.y };
}

} // namespace

auto PanZoomCamera2d::resize( glm::vec2 const viewport_size ) -> void
{
    viewport_size_ = glm::max( viewport_size, glm::vec2( 1.0F ) );
}

auto PanZoomCamera2d::pan( glm::vec2 const mouse_delta ) -> void
{
    center_ -= to_world_axes( mouse_delta ) / pixels_per_unit_;
}

auto PanZoomCamera2d::zoom( float32 const scroll_amount, glm::vec2 const cursor ) -> void
{
    auto const from_center = to_world_axes( cursor - 0.5F * viewport_size_ );
    auto const old_scale   = pixels_per_unit_;

    pixels_per_unit_ = std::clamp(
        pixels_per_unit_ * std::pow( zoom_factor, scroll_amount ),
        min_pixels_per_unit,
        max_pixels_per_unit
    );

    center_ += from_center / old_scale - from_center / pixels_per_unit_;
}

auto PanZoomCamera2d::viewport_size( ) const -> glm::vec2
{
    return viewport_size_;
}

auto PanZoomCamera2d::center( ) const -> glm::vec2
{
    return center_;
}

auto PanZoomCamera2d::pixels_per_unit( ) const -> float32
{
    return pixels_per_unit_;
}

auto PanZoomCamera2d::render_params( ) const -> cam::CameraRenderParams
{
    auto params = cam::CameraRenderParams{ };

    params.view_from_world = glm::translate( glm::mat4( 1.0F ), glm::vec3( -center_, 0.0F ) );

    auto const half_extent = 0.5F * viewport_size_ / pixels_per_unit_;
    params.clip_from_view
        = glm::ortho( -half_extent.x, half_extent.x, -half_extent.y, half_extent.y, -1.0F, 1.0F );

    // Vulkan's clip space has +Y pointing down.
    params.clip_from_view[ 1 ][ 1 ] *= -1.0F;

    params.clip_from_world = params.clip_from_view * params.view_from_world;
    params.world_from_clip = glm::inverse( params.clip_from_world );

    return params;
}

} // namespace ltb::radio
