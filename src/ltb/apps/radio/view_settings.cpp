// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "view_settings.hpp"

// external
#include <imgui.h>
#include <imgui_internal.h>

// standard
#include <string_view>

namespace ltb::radio
{
namespace
{

constexpr auto type_name = "SubAppViews";

auto read_open( ImGuiContext*, ImGuiSettingsHandler* handler, char const* name ) -> void*
{
    auto& states = *static_cast< AppViewStates* >( handler->UserData );
    return &states[ name ];
}

auto read_line( ImGuiContext*, ImGuiSettingsHandler*, void* entry, char const* line ) -> void
{
    auto const text   = std::string_view( line );
    auto const equals = text.rfind( '=' );
    if ( equals == std::string_view::npos || equals + 1 >= text.size( ) )
    {
        return;
    }
    ( *static_cast< ViewStates* >( entry ) )[ std::string( text.substr( 0, equals ) ) ]
        = text[ equals + 1 ] == '1';
}

auto write_all( ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buffer ) -> void
{
    auto const& states = *static_cast< AppViewStates* >( handler->UserData );
    for ( auto const& [ app_name, views ] : states )
    {
        buffer->appendf( "[%s][%s]\n", handler->TypeName, app_name.c_str( ) );
        for ( auto const& [ view_name, is_open ] : views )
        {
            buffer->appendf( "%s=%d\n", view_name.c_str( ), is_open ? 1 : 0 );
        }
        buffer->append( "\n" );
    }
}

} // namespace

auto register_view_settings_handler( AppViewStates& states ) -> void
{
    auto handler       = ImGuiSettingsHandler( );
    handler.TypeName   = type_name;
    handler.TypeHash   = ImHashStr( type_name );
    handler.ReadOpenFn = &read_open;
    handler.ReadLineFn = &read_line;
    handler.WriteAllFn = &write_all;
    handler.UserData   = &states;
    ImGui::AddSettingsHandler( &handler );
}

} // namespace ltb::radio
