// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////

// project
#include "app.hpp"
#include "ltb/exec/app_main.hpp"

auto main( ltb::int32 const argc, char const* argv[] ) -> int
{
    using namespace ltb;
    assert( argc > 0 );

#if !defined( NDEBUG )
    spdlog::set_level( spdlog::level::debug );
#endif

    auto const executable_path = std::filesystem::path{ argv[ 0 ] };
    auto const title           = executable_path.filename( ).string( );

    auto glfw_context = window::GlfwContext{ };
    auto glfw_window  = window::GlfwWindow{
        glfw_context,
        window::WindowSettings{
             .title        = title,
             .initial_size = std::nullopt,
        },
    };

    auto app = App{
        glfw_context,
        glfw_window,
        {
            .compute_shader_filename = "lesson1.comp.spv",
            .features                = FluidLesson1dFeature::ConstantWaveSpeed,
        }
    };

    if ( auto result = window::run_update_loop( glfw_context, glfw_window, app ); !result )
    {
        spdlog::error( result.error( ).debug_error_message( ) );
        return EXIT_FAILURE;
    }

    spdlog::info( "Exiting without errors" );
    return EXIT_SUCCESS;
}
