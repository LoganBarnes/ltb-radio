// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "app.hpp"

// project
#include "ltb/exec/app_defaults.hpp"
#include "ltb/gui/imgui_utils.hpp"
#include "ltb/radio/ltb_radio_config.hpp"
#include "ltb/utils/functors.hpp"
#include "ltb/vlk/buffer_utils.hpp"
#include "ltb/vlk/check.hpp"
#include "ltb/vlk/device_memory_utils.hpp"
#include "ltb/window/glfw_update_loop.hpp"

// external
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

// standard
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <ranges>

namespace ltb
{

namespace
{

constexpr auto gray = ImColor( 0.5F, 0.5F, 0.5F, 1.0F );

} // namespace

App::App(
    window::GlfwContext&     glfw_context,
    window::GlfwWindow&      glfw_window,
    FluidLessonApp1DSettings settings
)
    : glfw_context_( glfw_context )
    , glfw_window_( glfw_window )
    , settings_( std::move( settings ) )
{
}

auto App::initialize( ) -> utils::Result< exec::UpdateLoopStatus >
{
    if ( !this->is_initialized( ) )
    {
        LTB_CHECK( this->initialize_gpu( )
                       .and_then( &App::initialize_presentation )
                       .and_then( &App::update_points )
                       .and_then( &App::initialize_fluid )
                       .and_then( &App::initialize_camera )
                       .and_then( &App::initialize_display_pipeline )
                       .and_then( &App::initialize_meshes ) );

        initialized_ = true;
    }

    return exec::UpdateLoopStatus{
        .update_time_step = utils::duration_seconds( 0.025F ),
    };
}

auto App::is_initialized( ) const -> bool
{
    return initialized_;
}

auto App::fixed_step_update( exec::UpdateLoopStatus const& status ) -> exec::UpdateRequests
{
    if ( !cfd_.cap_iterations )
    {
        if ( auto result = this->compute( ); !result )
        {
            spdlog::error( "compute() failed: {}", result.error( ).debug_error_message( ) );
        }
    }

    return {
        .pause_updates    = cfd_.cap_iterations,
        .exit_update_loop = status.requests.exit_update_loop,
        .update_time_step = utils::duration_seconds( fluid_compute_.uniforms( ).time_step ),
    };
}

auto App::frame_update( exec::UpdateLoopStatus const& status ) -> exec::UpdateRequests
{
    utils::ignore( status );

    imgui_.new_frame( );
    this->configure_gui( );

    auto display_uniforms = fluid_display_.uniforms( );

    if ( cfd_.cap_iterations )
    {
        while ( cfd_.current_iteration < cfd_.max_iterations )
        {
            if ( auto result = this->compute( ); !result )
            {
                spdlog::error( "compute() failed: {}", result.error( ).debug_error_message( ) );
            }
            ++cfd_.current_iteration;
        }
    }
    else
    {
        display_uniforms.frame_interpolant
            = static_cast< float32 >( status.interpolant_between_updates );
    }

    display_uniforms.fluid_cell_count = cfd_.num_points;
    display_uniforms.cell_size
        = geom::dimensions( cfd_.domain ) / static_cast< float32 >( cfd_.num_points );
    fluid_display_.set_uniforms( display_uniforms );

    if ( auto result = this->render( ); !result )
    {
        spdlog::error( "render() failed: {}", result.error( ).debug_error_message( ) );
    }

    return {
        .pause_updates    = cfd_.cap_iterations,
        .exit_update_loop = status.requests.exit_update_loop,
        .update_time_step = utils::duration_seconds( fluid_compute_.uniforms( ).time_step ),
    };
}

auto App::configure_gui( ) -> void
{
    if ( camera_.handle_inputs( ) )
    {
        camera_frames_updated_.clear( );
    }

    utils::ignore(
        ImGui::DockSpaceOverViewport( 0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode )
    );

    if ( ImGui::Begin( "Simulation" ) )
    {
        this->gui_info( );
        this->gui_fixed_iterations( );
        this->gui_time_step( );
        this->gui_wave_speed( );
        this->gui_reset( );
        this->gui_reset_settings( );
    }
    ImGui::End( );
}

auto App::gui_info( ) -> void
{
    auto const display_uniforms = fluid_display_.uniforms( );

    ImGui::Text( "FPS: %.1F", ImGui::GetIO( ).Framerate );

    ImGui::Separator( );

    gui::imgui_fmt_colored( gray, "Num Points: {}", cfd_.num_points );
    gui::imgui_fmt_colored( gray, "Domain:     {} -> {}", cfd_.domain.min, cfd_.domain.max );
    gui::imgui_fmt_colored( gray, "Cell Size:  {}", display_uniforms.cell_size );

    ImGui::Separator( );
}

auto App::gui_fixed_iterations( ) -> void
{
    auto display_uniforms = fluid_display_.uniforms( );

    if ( ImGui::Checkbox( "###cap-iterations", &cfd_.cap_iterations ) && cfd_.cap_iterations )
    {
        display_uniforms.frame_interpolant = 1.0F;
        fluid_display_.set_uniforms( display_uniforms );
    }
    ImGui::SameLine( );

    auto flags = ImGuiTreeNodeFlags_None;
    if ( cfd_.cap_iterations )
    {
        flags = ImGuiTreeNodeFlags_DefaultOpen;
    }

    ImGui::BeginDisabled( !cfd_.cap_iterations );
    if ( ImGui::TreeNodeEx( "Fixed Iterations", flags ) )
    {
        if ( auto max_iterations = static_cast< int32 >( cfd_.max_iterations );
             ImGui::DragInt( "Max Iterations", &max_iterations, 0.1F ) )
        {
            cfd_.max_iterations    = static_cast< uint32 >( std::max( 1, max_iterations ) );
            cfd_.current_iteration = cfd_.max_iterations;
        }

        if ( ImGui::Button( "Run" ) )
        {
            cfd_.current_iteration = 0U;
        }

        if ( ImGui::SliderFloat( "Interpolant", &display_uniforms.frame_interpolant, 0.0F, 1.0F ) )
        {
            display_uniforms.frame_interpolant
                = std::clamp( display_uniforms.frame_interpolant, 0.0F, 1.0F );
            fluid_display_.set_uniforms( display_uniforms );
        }
        ImGui::TreePop( );
    }
    ImGui::EndDisabled( );
}

auto App::gui_time_step( ) -> void
{
    auto compute_uniforms = fluid_compute_.uniforms( );

    ImGui::Checkbox( "Dynamic Timestep", &cfd_.dynamic_time_step );

    constexpr auto time_step_range = geom::Range{ 0.001F, 1.0F };

    if ( cfd_.dynamic_time_step )
    {
        constexpr auto CFL = 0.9F;

        auto const cell_size
            = geom::dimensions( cfd_.domain ) / static_cast< float32 >( cfd_.num_points );
        compute_uniforms.time_step = CFL * cell_size / 2.0F;

        ImGui::SameLine( );
        gui::imgui_fmt_colored( gray, " ({})", compute_uniforms.time_step );
        fluid_compute_.set_uniforms( compute_uniforms );
    }
    else if ( ImGui::DragFloat(
                  "Time Step",
                  &cfd_.time_step,
                  0.001F,
                  time_step_range.min,
                  time_step_range.max
              ) )
    {
        cfd_.time_step = std::clamp( cfd_.time_step, time_step_range.min, time_step_range.max );
        compute_uniforms.time_step = cfd_.time_step;
        fluid_compute_.set_uniforms( compute_uniforms );
    }
}

auto App::gui_wave_speed( ) -> void
{
    auto compute_uniforms = fluid_compute_.uniforms( );

    auto constant_wave_speed = static_cast< bool >( compute_uniforms.constant_wave_speed );
    if ( ImGui::Checkbox( "Constant Wave Speed", &constant_wave_speed ) )
    {
        compute_uniforms.constant_wave_speed = constant_wave_speed;
        fluid_compute_.set_uniforms( compute_uniforms );
    }

    if ( compute_uniforms.constant_wave_speed
         && ImGui::DragFloat( "Wave Speed", &compute_uniforms.wave_speed, 0.1F ) )
    {
        compute_uniforms.wave_speed = std::clamp( compute_uniforms.wave_speed, -1.0F, 1.0F );
        fluid_compute_.set_uniforms( compute_uniforms );
    }
}

auto App::gui_reset( ) -> void
{
    if ( ImGui::Button( "Reset" ) )
    {
        cfd_.num_points = pending_num_points_;

        if ( auto const result = update_points( ); !result )
        {
            spdlog::error( "update_points() failed: {}", result.error( ).debug_error_message( ) );
        }
        if ( auto const result = fluid_compute_.reset_positions( cfd_.initial_positions ); !result )
        {
            spdlog::error(
                "fluid_compute_.reset_positions() failed: {}",
                result.error( ).debug_error_message( )
            );
        }
        if ( auto const result = line_display_.clear_meshes( ); !result )
        {
            spdlog::error(
                "line_display_.clear_meshes() failed: {}",
                result.error( ).debug_error_message( )
            );
        }
        else if ( auto const mesh_result = this->initialize_meshes( ); !mesh_result )
        {
            spdlog::error(
                "initialize_meshes() failed: {}",
                mesh_result.error( ).debug_error_message( )
            );
        }
    }
}

auto App::gui_reset_settings( ) -> void
{
    if ( ImGui::TreeNode( "Reset Settings" ) )
    {
        if ( constexpr auto positions_scale_range = geom::Range{ .min = -1.0F, .max = 1.0F };
             ImGui::DragFloat(
                 "Scale",
                 &cfd_.initial_positions_scale,
                 0.01F,
                 positions_scale_range.min,
                 positions_scale_range.max
             ) )
        {
            cfd_.initial_positions_scale = std::clamp(
                cfd_.initial_positions_scale,
                positions_scale_range.min,
                positions_scale_range.max
            );
        }

        constexpr auto num_points_range = geom::Range{ .min = 1, .max = 500 };
        auto           num_points       = static_cast< int32 >( pending_num_points_ );
        if ( ImGui::DragInt(
                 "Num Points",
                 &num_points,
                 1.0F,
                 num_points_range.min,
                 num_points_range.max
             ) )
        {
            pending_num_points_ = static_cast< uint32 >(
                std::clamp( num_points, num_points_range.min, num_points_range.max )
            );
        }
        ImGui::TreePop( );
    }
}

auto App::on_resize( glm::ivec2 const size ) -> utils::Result< void >
{
    camera_.resize( glm::vec2( size ) );
    camera_frames_updated_.clear( );

    return presentation_.rebuild( {
        .swapchain = presentation_.swapchain( ).settings( ),
    } );
}

auto App::clean_up( ) -> utils::Result< void >
{
    VK_CHECK( gpu_.device( ).get( ).waitIdle( ) );

    return utils::success( );
}

auto App::initialize_gpu( ) -> utils::Result< App* >
{
    auto gpu_settings = vlk::objs::VulkanGpuSettings{ };
    gpu_settings.device.device_features.push_back(
        &vk::PhysicalDeviceFeatures::tessellationShader
    );

    LTB_CHECK( gpu_.initialize( std::move( gpu_settings ) ) );

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

auto App::initialize_presentation( ) -> utils::Result< App* >
{
    LTB_CHECK( presentation_.initialize( {
        .render_pass = vlk::render_pass_settings_2d( vk::Format::eUndefined ),
    } ) );

    LTB_CHECK( imgui_.initialize( ) );

    return this;
}

auto App::update_points( ) -> utils::Result< App* >
{
    auto const num_points  = cfd_.num_points;
    auto const cfd_divisor = static_cast< float32 >( num_points );

    cfd_.initial_positions.clear( );
    cfd_.initial_positions.reserve( num_points );

    for ( auto i = 0U; i < num_points; ++i )
    {
        auto const interpolant = ( static_cast< float32 >( i ) + 0.5 ) / cfd_divisor;

        if ( auto const x_pos = std::lerp( cfd_.domain.min, cfd_.domain.max, interpolant );
             ( 0.5F < x_pos ) && ( x_pos < 1.0F ) )
        {
            cfd_.initial_positions.emplace_back( 2.0F * cfd_.initial_positions_scale );
        }
        else
        {
            cfd_.initial_positions.emplace_back( 1.0F * cfd_.initial_positions_scale );
        }
    }
    LTB_CHECK_VALID( cfd_.initial_positions.size( ) == num_points );

    return this;
}

auto App::initialize_fluid( ) -> utils::Result< App* >
{
    LTB_CHECK_VALID( cfd_.num_points > 0U );
    LTB_CHECK_VALID( cfd_.initial_positions.size( ) == cfd_.num_points );

    cfd_.dynamic_time_step
        = has_flag( settings_.features, FluidLesson1dFeature::TimeStepBasedOnCellSize );

    auto const fluid_compute_settings = radio::FluidPipeline1Settings{
        .frame_count       = exec::max_frames_in_flight,
        .initial_positions = cfd_.initial_positions,
        .compute_queue     = graphics_and_compute_queue_,
        .spirv_file        = radio::config::shader_dir_path( ) / settings_.compute_shader_filename,
    };
    LTB_CHECK_VALID( fluid_compute_.initialize( fluid_compute_settings ) );

    auto const cell_size
        = geom::dimensions( cfd_.domain ) / static_cast< float32 >( cfd_.num_points );

    auto compute_uniforms        = fluid_compute_.uniforms( );
    compute_uniforms.fluid_count = cfd_.num_points;
    compute_uniforms.cell_size   = cell_size;
    compute_uniforms.constant_wave_speed
        = has_flag( settings_.features, FluidLesson1dFeature::ConstantWaveSpeed );
    fluid_compute_.set_uniforms( compute_uniforms );

    auto display_uniforms             = fluid_display_.uniforms( );
    display_uniforms.fluid_cell_count = cfd_.num_points;
    display_uniforms.cell_size        = cell_size;
    fluid_display_.set_uniforms( display_uniforms );

    return this;
}

auto App::initialize_camera( ) -> utils::Result< App* >
{
    auto camera_buffer_layout = vlk::MemoryLayout{ };
    append_memory_size_n(
        camera_buffer_layout,
        sizeof( cam::SimpleCameraRenderParams ),
        exec::max_frames_in_flight
    );

    LTB_CHECK( camera_ubo_.initialize( {
        .layout       = std::move( camera_buffer_layout ),
        .buffer_usage = vk::BufferUsageFlagBits::eUniformBuffer,
        .memory_properties
        = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        .store_mapped_value = true,
    } ) );

    camera_.set_width( dimensions( cfd_.domain ) * 2.0F );
    camera_.set_center( { center( cfd_.domain ), 1.0F } );

    return this;
}

auto App::initialize_display_pipeline( ) -> utils::Result< App* >
{
    LTB_CHECK_VALID( camera_ubo_.is_initialized( ) );

    LTB_CHECK( fluid_display_.initialize( {
        .frame_count = exec::max_frames_in_flight,
        .camera_ubo  = camera_ubo_,
    } ) );

    LTB_CHECK( line_display_.initialize( {
        .frame_count = exec::max_frames_in_flight,
        .camera_ubo  = camera_ubo_,
    } ) );

    LTB_CHECK( graphics_cmd_and_sync_.initialize( {
        .frame_count  = exec::max_frames_in_flight,
        .image_count  = static_cast< uint32 >( presentation_.swapchain( ).images( ).size( ) ),
        .command_pool = {
            .queue_type = vlk::QueueType::Graphics,
        },
    } ) );

    return this;
}

auto App::initialize_meshes( ) -> utils::Result< App* >
{
    auto const num_points  = cfd_.num_points;
    auto const cfd_divisor = static_cast< float32 >( num_points );

    auto grid_lines = vlk::dd::SimpleMesh2{ };
    for ( auto i = 0U; i <= num_points; ++i )
    {
        auto const interpolant = static_cast< float32 >( i ) / cfd_divisor;
        auto const x_pos       = std::lerp( cfd_.domain.min, cfd_.domain.max, interpolant );

        utils::ignore(
            grid_lines.positions.emplace_back( x_pos, 0.0F ),
            grid_lines.positions.emplace_back( x_pos, 2.0F )
        );
    }
    LTB_CHECK(
        auto* const grid_uniforms,
        line_display_.initialize_mesh(
            grid_lines,
            graphics_cmd_and_sync_.command_pool( ),
            graphics_and_compute_queue_
        )
    );
    grid_uniforms->display.color = { 0.5F, 0.5F, 0.5F, 1.0F };

    return this;
}

auto App::compute( ) -> utils::Result< void >
{
    LTB_CHECK( fluid_compute_.dispatch( ) );

    return utils::success( );
}

auto App::render( ) -> utils::Result< void >
{
    LTB_CHECK(
        auto const maybe_frame,
        graphics_cmd_and_sync_.start_frame( presentation_.swapchain( ) )
    );

    if ( maybe_frame.has_value( ) )
    {
        auto const& frame = maybe_frame.value( );

        LTB_CHECK( this->update_camera_uniforms( frame ) );
        LTB_CHECK( this->record_render_commands( frame ) );

        auto wait_until_signaled = std::vector< vlk::objs::SemaphoreAndStage >{ };

        if ( auto const& compute_semaphore = fluid_compute_.get_semaphore( ) )
        {
            wait_until_signaled.push_back( {
                .semaphore = compute_semaphore.value( ),
                .stage     = vk::PipelineStageFlagBits::eVertexInput,
            } );
            fluid_compute_.clear_semaphore( );
        }
        wait_until_signaled.push_back( {
            .semaphore = frame.image_semaphore,
            .stage     = vk::PipelineStageFlagBits::eColorAttachmentOutput,
        } );

        LTB_CHECK(
            auto const& render_finished_semaphore,
            graphics_cmd_and_sync_.get_present_semaphore( frame )
        );

        LTB_CHECK( graphics_cmd_and_sync_.end_frame(
            frame,
            wait_until_signaled,
            { render_finished_semaphore },
            graphics_and_compute_queue_
        ) );

        LTB_CHECK( graphics_cmd_and_sync_
                       .present_frame( frame, presentation_.swapchain( ), presentation_queue_ ) );

        graphics_cmd_and_sync_.increment_frame( );
    }

    return utils::success( );
}

auto App::update_camera_uniforms( vlk::objs::FrameInfo const& frame ) -> utils::Result< void >
{
    if ( camera_frames_updated_.contains( frame.frame_index ) )
    {
        return utils::success( );
    }

    auto const cam_uniforms = camera_.simple_render_params( );

    LTB_CHECK_VALID( frame.frame_index < camera_ubo_.layout( ).ranges.size( ) );
    auto const& memory_range = camera_ubo_.layout( ).ranges[ frame.frame_index ];

    auto* const dst_data = camera_ubo_.mapped_data( ) + memory_range.offset;
    LTB_CHECK_VALID( memory_range.size >= sizeof( cam_uniforms ) );
    LTB_CHECK_VALID( std::memcpy( dst_data, &cam_uniforms, sizeof( cam_uniforms ) ) == dst_data );

    utils::ignore( camera_frames_updated_.emplace( frame.frame_index ) );

    return utils::success( );
}

auto App::record_render_commands( vlk::objs::FrameInfo const& frame ) -> utils::Result< void >
{
    VK_CHECK( frame.command_buffer.begin( vk::CommandBufferBeginInfo{ } ) );

    LTB_CHECK( presentation_.begin_render_pass( {
        .command_buffer    = frame.command_buffer,
        .image_index       = frame.image_index,
        .color_clear_value = { 0.35F, 0.35F, 0.35F, 1.0F },
    } ) );

    auto const [ prev_index, next_index ] = fluid_compute_.last_computed_frame_indices( );
    LTB_CHECK( fluid_display_.draw( frame, prev_index, next_index ) );
    LTB_CHECK( line_display_.draw( frame ) );

    imgui_.render( frame.command_buffer );

    frame.command_buffer.endRenderPass( );

    VK_CHECK( frame.command_buffer.end( ) );

    return utils::success( );
}

} // namespace ltb
