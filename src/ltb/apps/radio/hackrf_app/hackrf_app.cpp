// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "hackrf_app.hpp"

// project
#include "hackrf_info_view.hpp"

namespace ltb::radio
{

HackrfApp::HackrfApp( SubAppContext const& context )
    : SubApp( context )
{
    this->add_view( std::make_unique< HackrfInfoView >( ) );
}

auto HackrfApp::name( ) const -> std::string const&
{
    static auto const app_name = std::string( "HackRF" );
    return app_name;
}

} // namespace ltb::radio
