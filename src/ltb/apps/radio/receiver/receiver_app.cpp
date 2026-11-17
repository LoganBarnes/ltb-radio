// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "receiver_app.hpp"

// project
#include "stage_plot_view.hpp"

// standard
#include <format>

namespace ltb::radio
{

ReceiverApp::ReceiverApp( SubAppContext const& context, ReceiverAppSettings settings )
    : SubApp( context )
    , name_( std::move( settings.name ) )
    , engine_( settings.modulation, std::move( settings.radio ) )
{
    auto const& stages = engine_.stages( );

    auto stage_views = std::vector< AppView* >( );
    auto plot_views  = std::vector< std::unique_ptr< AppView > >( );
    for ( auto i = 0UZ; i < stages.size( ); ++i )
    {
        auto view = std::make_unique< StagePlotView >(
            engine_,
            i,
            std::format( "{} {}. {}", settings.view_prefix, i + 1UZ, stages[ i ].name )
        );
        stage_views.push_back( view.get( ) );
        plot_views.push_back( std::move( view ) );
    }

    // Open by default (until a saved layout says otherwise): the input and the output.
    stage_views.front( )->is_open = true;
    stage_views.back( )->is_open  = true;

    auto controls = std::make_unique< ReceiverControlsView >(
        engine_,
        settings.view_prefix + " Controls",
        settings.tuning,
        stage_views
    );
    controls->is_open = true;

    this->add_view( std::move( controls ) );
    for ( auto& view : plot_views )
    {
        this->add_view( std::move( view ) );
    }
}

auto ReceiverApp::name( ) const -> std::string const&
{
    return name_;
}

} // namespace ltb::radio
