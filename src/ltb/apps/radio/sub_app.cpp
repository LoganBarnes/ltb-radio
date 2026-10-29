// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "sub_app.hpp"

// project
#include "ltb/vlk/check.hpp"

// external
#include <imgui.h>
#include <spdlog/spdlog.h>

// standard
#include <ranges>

namespace ltb::radio
{

SubApp::SubApp( SubAppContext const& context )
    : context_( context )
{
}

auto SubApp::gui_view_menu_items( ) -> void
{
    for ( auto const& view : views_ )
    {
        ImGui::MenuItem( view->name( ).c_str( ), nullptr, &view->is_open );
    }
}

auto SubApp::configure_gui( uint32 const frame_index ) -> void
{
    for ( auto const& view : views_ | std::views::filter( &AppView::is_open ) )
    {
        if ( !view->is_initialized )
        {
            if ( auto result = view->initialize( context_ ); !result )
            {
                spdlog::error(
                    "Failed to initialize '{}': {}",
                    view->name( ),
                    result.error( ).debug_error_message( )
                );
                view->is_open = false;
                continue;
            }
            view->is_initialized = true;
        }
        view->configure_gui( frame_index );
    }
}

auto SubApp::record_commands( vlk::objs::FrameInfo const& frame ) -> utils::Result< void >
{
    for ( auto const& view : views_ | std::views::filter( &AppView::is_open ) )
    {
        LTB_CHECK( view->record_commands( frame ) );
    }
    return utils::success( );
}

auto SubApp::add_view( std::unique_ptr< AppView > view ) -> void
{
    views_.push_back( std::move( view ) );
}

} // namespace ltb::radio
