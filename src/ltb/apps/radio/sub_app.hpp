// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "app_view.hpp"
#include "sub_app_context.hpp"

// standard
#include <memory>
#include <string>
#include <vector>

namespace ltb::radio
{

// A full-featured application (e.g. a tuner) selected from the toolbar's "Apps" menu.
// Only one runs at a time. Its views are listed in the "View" menu while it runs.
class SubApp
{
public:
    explicit SubApp( SubAppContext const& context );
    virtual ~SubApp( ) = default;

    SubApp( SubApp const& )                    = delete;
    auto operator=( SubApp const& ) -> SubApp& = delete;
    SubApp( SubApp&& )                         = delete;
    auto operator=( SubApp&& ) -> SubApp&      = delete;

    [[nodiscard( "Const getter" )]]
    virtual auto name( ) const -> std::string const& = 0;

    // Draws a menu item for each view. Call between BeginMenu and EndMenu.
    auto gui_view_menu_items( ) -> void;

    // Initializes newly opened views and draws all open views.
    auto configure_gui( uint32 frame_index ) -> void;

    auto record_commands( vlk::objs::FrameInfo const& frame ) -> utils::Result< void >;

protected:
    auto add_view( std::unique_ptr< AppView > view ) -> void;

private:
    SubAppContext context_;

    std::vector< std::unique_ptr< AppView > > views_ = { };
};

} // namespace ltb::radio
