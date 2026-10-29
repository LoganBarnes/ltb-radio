// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "app.hpp"

// project
#include "ltb/exec/app_defaults.hpp"
#include "ltb/radio/user_config.hpp"
#include "ltb/vlk/check.hpp"
#include "ltb/vlk/render_pass.hpp"
#include "hackrf_app/hackrf_app.hpp"
#include "test_app/test_app.hpp"

// external
#include <imgui.h>
#include <spdlog/spdlog.h>

namespace ltb::radio
{
namespace
{

constexpr auto default_time_step = utils::duration_seconds( 1.0 / 60.0 );

template < typename App >
auto make_sub_app( SubAppContext const& context ) -> std::unique_ptr< SubApp >
{
    return std::make_unique< App >( context );
}

} // namespace

RadioApp::RadioApp( window::GlfwContext& glfw_context, window::GlfwWindow& glfw_window )
    : glfw_context_( glfw_context )
    , glfw_window_( glfw_window )
{
    sub_app_entries_.push_back( { .name = "HackRF", .create = &make_sub_app< HackrfApp > } );
    sub_app_entries_.push_back( { .name = "Test App", .create = &make_sub_app< TestApp > } );

    // The first entry is the default app.
    requested_index_  = 0UZ;
    change_requested_ = true;
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
    if ( auto result = this->apply_app_change( ); !result )
    {
        spdlog::error( "Switching apps failed: {}", result.error( ).debug_error_message( ) );
    }

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

    if ( active_app_ )
    {
        active_app_->configure_gui( graphics_cmd_and_sync_.frame_index( ) );

        auto  states = active_app_->view_states( );
        auto& saved  = saved_view_states_[ active_app_->name( ) ];
        if ( states != saved )
        {
            saved = std::move( states );
            ImGui::MarkIniSettingsDirty( );
        }
    }
}

auto RadioApp::apply_app_change( ) -> utils::Result< void >
{
    if ( !change_requested_ )
    {
        return utils::success( );
    }
    change_requested_ = false;

    // The previous app's resources may still be in use by frames in flight.
    VK_CHECK( gpu_.device( ).get( ).waitIdle( ) );
    active_app_.reset( );
    active_index_ = std::nullopt;

    if ( requested_index_.has_value( ) )
    {
        LTB_CHECK_VALID( *requested_index_ < sub_app_entries_.size( ) );

        active_app_ = sub_app_entries_[ *requested_index_ ].create(
            SubAppContext{
                .gpu            = gpu_,
                .command_pool   = graphics_cmd_and_sync_.command_pool( ),
                .graphics_queue = graphics_and_compute_queue_,
                .frame_count    = exec::max_frames_in_flight,
            }
        );
        active_index_ = requested_index_;

        if ( auto const saved = saved_view_states_.find( active_app_->name( ) );
             saved != saved_view_states_.end( ) )
        {
            active_app_->restore_view_states( saved->second );
        }
    }

    return utils::success( );
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
        if ( sub_app_entries_.empty( ) )
        {
            ImGui::MenuItem( "(none)", nullptr, false, false );
        }
        for ( auto i = 0UZ; i < sub_app_entries_.size( ); ++i )
        {
            auto const is_active = active_index_ == i;
            if ( ImGui::MenuItem( sub_app_entries_[ i ].name.c_str( ), nullptr, is_active ) )
            {
                // Selecting the running app closes it.
                requested_index_  = is_active ? std::nullopt : std::optional( i );
                change_requested_ = true;
            }
        }
        ImGui::EndMenu( );
    }

    if ( ImGui::BeginMenu( "View" ) )
    {
        if ( active_app_ )
        {
            active_app_->gui_view_menu_items( );
        }
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
    active_app_.reset( );

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
    register_view_settings_handler( saved_view_states_ );

    // Keep the window layout between runs regardless of the working directory.
    if ( auto const config_dir = user_config_dir( ); config_dir )
    {
        imgui_ini_path_             = ( config_dir.value( ) / "imgui.ini" ).string( );
        ImGui::GetIO( ).IniFilename = imgui_ini_path_.c_str( );
    }
    else
    {
        spdlog::warn(
            "Window layout will use the default location: {}",
            config_dir.error( ).debug_error_message( )
        );
    }

    // The default app is created before the first frame, so its saved views are needed now.
    // ImGui won't reload the ini on the first frame once it has been loaded.
    if ( auto const* const ini_path = ImGui::GetIO( ).IniFilename; ini_path != nullptr )
    {
        ImGui::LoadIniSettingsFromDisk( ini_path );
    }

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

    if ( active_app_ )
    {
        LTB_CHECK( active_app_->record_commands( frame ) );
    }

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
