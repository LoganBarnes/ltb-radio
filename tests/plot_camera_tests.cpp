// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////

// project
#include "ltb/radio/plot_camera.hpp"

// external
#include <gtest/gtest.h>

namespace ltb::radio
{
namespace
{

constexpr auto viewport = glm::vec2( 200.0F, 100.0F );

auto expect_near( glm::vec2 const a, glm::vec2 const b ) -> void
{
    EXPECT_NEAR( a.x, b.x, 1e-4F );
    EXPECT_NEAR( a.y, b.y, 1e-4F );
}

TEST( PlotCamera, MapsCornersToBounds )
{
    auto camera = PlotCamera( );
    camera.set_bounds( { -10.0F, 0.0F }, { 10.0F, 5.0F } );

    expect_near( camera.plot_from_screen( { 0.0F, 0.0F }, viewport ), { -10.0F, 5.0F } );
    expect_near( camera.plot_from_screen( viewport, viewport ), { 10.0F, 0.0F } );
    expect_near( camera.screen_from_plot( { 0.0F, 2.5F }, viewport ), { 100.0F, 50.0F } );

    // Plot +Y is up, Vulkan clip +Y is down.
    auto const clip = camera.clip_from_plot( ) * glm::vec4( -10.0F, 5.0F, 0.0F, 1.0F );
    expect_near( { clip.x, clip.y }, { -1.0F, -1.0F } );
}

TEST( PlotCamera, ZoomKeepsTheCursorFixed )
{
    auto camera = PlotCamera( );
    camera.set_bounds( { 0.0F, -1.0F }, { 100.0F, 1.0F } );

    auto const cursor = glm::vec2( 50.0F, 20.0F );
    auto const before = camera.plot_from_screen( cursor, viewport );
    camera.zoom( 3.0F, cursor, viewport, { true, false } );

    expect_near( camera.plot_from_screen( cursor, viewport ), before );
    EXPECT_LT( camera.max( ).x - camera.min( ).x, 100.0F );
    EXPECT_FLOAT_EQ( camera.max( ).y - camera.min( ).y, 2.0F );
}

TEST( PlotCamera, PanFollowsTheMouse )
{
    auto camera = PlotCamera( );
    camera.set_bounds( { 0.0F, 0.0F }, { 200.0F, 100.0F } );

    auto const grabbed = camera.plot_from_screen( { 20.0F, 20.0F }, viewport );
    camera.pan( { 30.0F, -10.0F }, viewport );
    expect_near( camera.plot_from_screen( { 50.0F, 10.0F }, viewport ), grabbed );
}

} // namespace
} // namespace ltb::radio
