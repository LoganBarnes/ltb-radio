// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "user_config.hpp"

// standard
#include <cstdlib>

namespace ltb::radio
{

namespace
{

constexpr auto app_dir_name = "ltb-radio";

auto env_path( char const* name ) -> std::filesystem::path
{
    auto const* const value = std::getenv( name );
    return ( value != nullptr ) ? std::filesystem::path{ value } : std::filesystem::path{ };
}

auto base_config_dir( ) -> std::filesystem::path
{
#if defined( _WIN32 )
    return env_path( "APPDATA" );
#elif defined( __APPLE__ )
    if ( auto const home = env_path( "HOME" ); !home.empty( ) )
    {
        return home / "Library" / "Application Support";
    }
    return { };
#else
    if ( auto const xdg = env_path( "XDG_CONFIG_HOME" ); !xdg.empty( ) )
    {
        return xdg;
    }
    if ( auto const home = env_path( "HOME" ); !home.empty( ) )
    {
        return home / ".config";
    }
    return { };
#endif
}

} // namespace

auto user_config_dir( ) -> utils::Result< std::filesystem::path >
{
    auto const base = base_config_dir( );
    if ( base.empty( ) )
    {
        return LTB_MAKE_UNEXPECTED_ERROR( "Could not determine the user config directory" );
    }

    auto const dir = base / app_dir_name;

    auto error_code = std::error_code{ };
    std::filesystem::create_directories( dir, error_code );
    if ( error_code )
    {
        return LTB_MAKE_UNEXPECTED_ERROR(
            "Could not create '{}': {}",
            dir.string( ),
            error_code.message( )
        );
    }

    return dir;
}

} // namespace ltb::radio
