// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "orbit_camera.hpp"

// standard
#include <algorithm>
#include <cmath>

namespace ltb::radio
{

namespace
{

constexpr auto radians_per_pixel = 0.01F;
constexpr auto zoom_factor       = 0.9F;
constexpr auto max_pitch         = glm::radians( 89.0F );
constexpr auto min_distance      = 0.5F;
constexpr auto max_distance      = 50.0F;

} // namespace

auto OrbitCamera::resize( glm::vec2 const viewport_size ) -> void
{
    viewport_size_ = glm::max( viewport_size, glm::vec2( 1.0F ) );
}

auto OrbitCamera::orbit( glm::vec2 const mouse_delta ) -> void
{
    yaw_ -= mouse_delta.x * radians_per_pixel;
    pitch_ = std::clamp( pitch_ + mouse_delta.y * radians_per_pixel, -max_pitch, max_pitch );
}

auto OrbitCamera::zoom( float32 const scroll_amount ) -> void
{
    distance_ = std::
        clamp( distance_ * std::pow( zoom_factor, scroll_amount ), min_distance, max_distance );
}

auto OrbitCamera::viewport_size( ) const -> glm::vec2
{
    return viewport_size_;
}

auto OrbitCamera::eye( ) const -> glm::vec3
{
    auto const direction = glm::vec3(
        std::cos( pitch_ ) * std::sin( yaw_ ),
        std::sin( pitch_ ),
        std::cos( pitch_ ) * std::cos( yaw_ )
    );
    return target_ + distance_ * direction;
}

auto OrbitCamera::render_params( ) const -> cam::CameraRenderParams
{
    auto params = cam::CameraRenderParams{ };

    params.view_from_world = glm::lookAt( this->eye( ), target_, glm::vec3( 0.0F, 1.0F, 0.0F ) );

    auto const aspect_ratio = viewport_size_.x / viewport_size_.y;
    params.clip_from_view
        = glm::perspective( fov_y_radians_, aspect_ratio, near_plane_, far_plane_ );

    // Vulkan's clip space has +Y pointing down.
    params.clip_from_view[ 1 ][ 1 ] *= -1.0F;

    params.clip_from_world = params.clip_from_view * params.view_from_world;
    params.world_from_clip = glm::inverse( params.clip_from_world );

    return params;
}

} // namespace ltb::radio
