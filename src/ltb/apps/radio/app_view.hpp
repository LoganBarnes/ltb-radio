// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "sub_app_context.hpp"
#include "ltb/utils/result.hpp"
#include "ltb/vlk/objs/frame_info.hpp"

// standard
#include <string>

namespace ltb::radio
{

// An optional (dockable) view of a sub application. Views are toggled from the View menu
// while their sub application is running.
class AppView
{
public:
    virtual ~AppView( ) = default;

    // Also used as the menu item label, so it should match the ImGui window title.
    [[nodiscard( "Const getter" )]]
    virtual auto name( ) const -> std::string const& = 0;

    // Called once, the first time the view is opened. A failure closes the view.
    virtual auto initialize( SubAppContext const& context ) -> utils::Result< void >
    {
        utils::ignore( context );
        return utils::success( );
    }

    // `frame_index` is the frame in flight that will be recorded after this GUI is built.
    virtual auto configure_gui( uint32 frame_index ) -> void = 0;

    // Called while the frame's command buffer is recording, before the main render pass begins.
    virtual auto record_commands( vlk::objs::FrameInfo const& frame ) -> utils::Result< void >
    {
        utils::ignore( frame );
        return utils::success( );
    }

    bool is_open        = false;
    bool is_initialized = false;
};

} // namespace ltb::radio
