// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "plot_camera.hpp"

// external
#include <glm/gtc/matrix_transform.hpp>

// standard
#include <algorithm>
#include <cmath>

namespace ltb::radio
{
namespace
{

constexpr auto zoom_factor = 1.15F;

// Smaller spans lose too much float precision to be useful.
constexpr auto min_relative_span = 1e-5F;

} // namespace

auto PlotCamera::set_bounds( glm::vec2 const min, glm::vec2 const max ) -> void
{
    if ( glm::all( glm::lessThan( min, max ) ) )
    {
        min_ = min;
        max_ = max;
    }
}

auto PlotCamera::min( ) const -> glm::vec2
{
    return min_;
}

auto PlotCamera::max( ) const -> glm::vec2
{
    return max_;
}

auto PlotCamera::pan( glm::vec2 const pixel_delta, glm::vec2 const viewport_size ) -> void
{
    auto const span  = max_ - min_;
    auto const delta = glm::vec2( -pixel_delta.x, pixel_delta.y ) * span
                     / glm::max( viewport_size, glm::vec2( 1.0F ) );
    min_ += delta;
    max_ += delta;
}

auto PlotCamera::zoom(
    float32 const    scroll_amount,
    glm::vec2 const  cursor,
    glm::vec2 const  viewport_size,
    glm::bvec2 const axes
) -> void
{
    auto const anchor = this->plot_from_screen( cursor, viewport_size );
    auto const scale  = std::pow( zoom_factor, -scroll_amount );

    for ( auto axis = 0; axis < 2; ++axis )
    {
        auto const new_min = anchor[ axis ] + ( min_[ axis ] - anchor[ axis ] ) * scale;
        auto const new_max = anchor[ axis ] + ( max_[ axis ] - anchor[ axis ] ) * scale;
        auto const limit = min_relative_span * std::max( std::abs( new_min ), std::abs( new_max ) );

        if ( axes[ axis ] && new_max - new_min > std::max( limit, 1e-12F ) )
        {
            min_[ axis ] = new_min;
            max_[ axis ] = new_max;
        }
    }
}

auto PlotCamera::plot_from_screen( glm::vec2 const screen, glm::vec2 const viewport_size ) const
    -> glm::vec2
{
    auto const t = screen / glm::max( viewport_size, glm::vec2( 1.0F ) );
    return { min_.x + t.x * ( max_.x - min_.x ), max_.y - t.y * ( max_.y - min_.y ) };
}

auto PlotCamera::screen_from_plot( glm::vec2 const plot, glm::vec2 const viewport_size ) const
    -> glm::vec2
{
    auto const t = ( plot - min_ ) / ( max_ - min_ );
    return glm::vec2( t.x, 1.0F - t.y ) * viewport_size;
}

auto PlotCamera::clip_from_plot( ) const -> glm::mat4
{
    auto clip_from_plot = glm::ortho( min_.x, max_.x, min_.y, max_.y, -1.0F, 1.0F );
    clip_from_plot[ 1 ][ 1 ] *= -1.0F;
    clip_from_plot[ 3 ][ 1 ] *= -1.0F;
    return clip_from_plot;
}

} // namespace ltb::radio
