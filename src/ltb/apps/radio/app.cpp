// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "app.hpp"

// project
#include "hackrf_info_app.hpp"
#include "ltb/exec/app_defaults.hpp"
#include "ltb/vlk/check.hpp"
#include "ltb/vlk/render_pass.hpp"

// external
#include <imgui.h>
#include <spdlog/spdlog.h>

// standard
#include <ranges>

namespace ltb::radio
{
namespace
{

constexpr auto default_time_step = utils::duration_seconds( 1.0 / 60.0 );

}

RadioApp::RadioApp( window::GlfwContext& glfw_context, window::GlfwWindow& glfw_window )
    : glfw_context_( glfw_context )
    , glfw_window_( glfw_window )
{
    sub_apps_.push_back( std::make_unique< HackrfInfoApp >( ) );
}

auto RadioApp::initialize( ) -> utils::Result< exec::UpdateLoopStatus >
{
    if ( !this->is_initialized( ) )
    {
        LTB_CHECK( this->initialize_gpu( )
                       .and_then( &RadioApp::initialize_presentation )
                       .and_then( &RadioApp::initialize_command_and_sync ) );

        initialized_ = true;
    }

    return exec::UpdateLoopStatus{
        .update_time_step = default_time_step,
    };
}

auto RadioApp::is_initialized( ) const -> bool
{
    return initialized_;
}

auto RadioApp::fixed_step_update( exec::UpdateLoopStatus const& status ) -> exec::UpdateRequests
{
    return {
        .pause_updates    = false,
        .exit_update_loop = status.requests.exit_update_loop || exit_requested_,
        .update_time_step = status.update_time_step,
    };
}

auto RadioApp::frame_update( exec::UpdateLoopStatus const& status ) -> exec::UpdateRequests
{
    imgui_.new_frame( );
    this->configure_gui( );

    if ( auto result = this->render( ); !result )
    {
        spdlog::error( "render() failed: {}", result.error( ).debug_error_message( ) );
    }

    return {
        .pause_updates    = false,
        .exit_update_loop = status.requests.exit_update_loop || exit_requested_,
        .update_time_step = status.update_time_step,
    };
}

auto RadioApp::configure_gui( ) -> void
{
    this->gui_toolbar( );

    utils::ignore(
        ImGui::DockSpaceOverViewport( 0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode )
    );

    if ( show_main_window_ && ImGui::Begin( "Radio", &show_main_window_ ) )
    {
        ImGui::TextUnformatted( "Select a sub application from the Apps menu." );
    }
    if ( show_main_window_ )
    {
        ImGui::End( );
    }

    if ( show_demo_window_ )
    {
        ImGui::ShowDemoWindow( &show_demo_window_ );
    }

    for ( auto const& sub_app : sub_apps_ | std::views::filter( &SubApp::is_open ) )
    {
        sub_app->configure_gui( );
    }
}

auto RadioApp::gui_toolbar( ) -> void
{
    if ( !ImGui::BeginMainMenuBar( ) )
    {
        return;
    }

    if ( ImGui::BeginMenu( "File" ) )
    {
        if ( ImGui::MenuItem( "Exit" ) )
        {
            exit_requested_ = true;
        }
        ImGui::EndMenu( );
    }

    if ( ImGui::BeginMenu( "Apps" ) )
    {
        if ( sub_apps_.empty( ) )
        {
            ImGui::MenuItem( "(none)", nullptr, false, false );
        }
        for ( auto const& sub_app : sub_apps_ )
        {
            ImGui::MenuItem( sub_app->name( ).c_str( ), nullptr, &sub_app->is_open );
        }
        ImGui::EndMenu( );
    }

    if ( ImGui::BeginMenu( "View" ) )
    {
        ImGui::MenuItem( "Radio", nullptr, &show_main_window_ );
        ImGui::MenuItem( "ImGui Demo", nullptr, &show_demo_window_ );
        ImGui::EndMenu( );
    }

    ImGui::EndMainMenuBar( );
}

auto RadioApp::on_resize( glm::ivec2 const /*size*/ ) -> utils::Result< void >
{
    return presentation_.rebuild( {
        .swapchain = presentation_.swapchain( ).settings( ),
    } );
}

auto RadioApp::clean_up( ) -> utils::Result< void >
{
    VK_CHECK( gpu_.device( ).get( ).waitIdle( ) );

    return utils::success( );
}

auto RadioApp::initialize_gpu( ) -> utils::Result< RadioApp* >
{
    LTB_CHECK( gpu_.initialize( vlk::objs::VulkanGpuSettings{ } ) );

    LTB_CHECK_VALID( gpu_.device( ).queues( ).contains( vlk::QueueType::Graphics ) );
    LTB_CHECK_VALID( gpu_.device( ).queues( ).contains( vlk::QueueType::Compute ) );
    LTB_CHECK_VALID( gpu_.device( ).queues( ).contains( vlk::QueueType::Surface ) );

    auto const& graphics_queue = gpu_.device( ).queues( ).at( vlk::QueueType::Graphics );
    auto const& compute_queue  = gpu_.device( ).queues( ).at( vlk::QueueType::Compute );
    auto const& surface_queue  = gpu_.device( ).queues( ).at( vlk::QueueType::Surface );

    LTB_CHECK_VALID( graphics_queue == compute_queue );

    graphics_and_compute_queue_ = graphics_queue;
    presentation_queue_         = surface_queue;

    return this;
}

auto RadioApp::initialize_presentation( ) -> utils::Result< RadioApp* >
{
    LTB_CHECK( presentation_.initialize( {
        .render_pass = vlk::render_pass_settings_2d( vk::Format::eUndefined ),
    } ) );

    LTB_CHECK( imgui_.initialize( ) );

    return this;
}

auto RadioApp::initialize_command_and_sync( ) -> utils::Result< RadioApp* >
{
    LTB_CHECK( graphics_cmd_and_sync_.initialize( {
        .frame_count  = exec::max_frames_in_flight,
        .image_count  = static_cast< uint32 >( presentation_.swapchain( ).images( ).size( ) ),
        .command_pool = {
            .queue_type = vlk::QueueType::Graphics,
        },
    } ) );

    return this;
}

auto RadioApp::render( ) -> utils::Result< void >
{
    LTB_CHECK(
        auto const maybe_frame,
        graphics_cmd_and_sync_.start_frame( presentation_.swapchain( ) )
    );

    if ( maybe_frame.has_value( ) )
    {
        auto const& frame = maybe_frame.value( );

        LTB_CHECK( this->record_render_commands( frame ) );

        LTB_CHECK(
            auto const& render_finished_semaphore,
            graphics_cmd_and_sync_.get_present_semaphore( frame )
        );

        LTB_CHECK( graphics_cmd_and_sync_.end_frame(
            frame,
            {
                vlk::objs::SemaphoreAndStage{
                    .semaphore = frame.image_semaphore,
                    .stage     = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                },
            },
            { render_finished_semaphore },
            graphics_and_compute_queue_
        ) );

        LTB_CHECK( graphics_cmd_and_sync_
                       .present_frame( frame, presentation_.swapchain( ), presentation_queue_ ) );

        graphics_cmd_and_sync_.increment_frame( );
    }

    return utils::success( );
}

auto RadioApp::record_render_commands( vlk::objs::FrameInfo const& frame ) -> utils::Result< void >
{
    VK_CHECK( frame.command_buffer.begin( vk::CommandBufferBeginInfo{ } ) );

    LTB_CHECK( presentation_.begin_render_pass( {
        .command_buffer    = frame.command_buffer,
        .image_index       = frame.image_index,
        .color_clear_value = { 0.15F, 0.15F, 0.15F, 1.0F },
    } ) );

    imgui_.render( frame.command_buffer );

    frame.command_buffer.endRenderPass( );

    VK_CHECK( frame.command_buffer.end( ) );

    return utils::success( );
}

} // namespace ltb::radio
