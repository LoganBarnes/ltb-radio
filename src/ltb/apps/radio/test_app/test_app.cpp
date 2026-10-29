// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "test_app.hpp"

// project
#include "imgui_demo_view.hpp"
#include "viewport_2d_view.hpp"
#include "viewport_3d_view.hpp"

namespace ltb::radio
{

TestApp::TestApp( SubAppContext const& context )
    : SubApp( context )
{
    this->add_view( std::make_unique< Viewport3dView >( ) );
    this->add_view( std::make_unique< Viewport2dView >( ) );
    this->add_view( std::make_unique< ImguiDemoView >( ) );
}

auto TestApp::name( ) const -> std::string const&
{
    static auto const app_name = std::string( "Test App" );
    return app_name;
}

} // namespace ltb::radio
