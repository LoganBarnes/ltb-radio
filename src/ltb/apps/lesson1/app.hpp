// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/cam/camera_2d.hpp"
#include "ltb/radio/fluid_display_pipeline_1.hpp"
#include "ltb/radio/fluid_pipeline_1.hpp"
#include "ltb/exec/update_loop.hpp"
#include "ltb/gui/imgui_glfw_vulkan_setup.hpp"
#include "ltb/utils/enum_flags.hpp"
#include "ltb/vlk/dd/lines_pipeline_2.hpp"
#include "ltb/vlk/objs/vulkan_buffer.hpp"
#include "ltb/vlk/objs/vulkan_command_and_sync.hpp"
#include "ltb/vlk/objs/vulkan_gpu.hpp"

// standard
#include <unordered_set>

namespace ltb
{

// https://drzgan.github.io/Python_CFD/2.1D%20linear%20convection.html

struct CfdParams
{
    geom::Range< float32 > domain                  = { .min = 0.0F, .max = 2.0F };
    uint32                 num_points              = 60U;
    uint32                 max_iterations          = 1U;
    uint32                 current_iteration       = 1U;
    bool                   cap_iterations          = true;
    float32                time_scale              = 1.0F;
    std::vector< float32 > initial_positions       = { };
    float32                initial_positions_scale = 1.0F;
    bool                   dynamic_time_step       = true;
    float32                time_step               = 0.025F;
};

enum class FluidLesson1dFeature
{
    ConstantWaveSpeed,
    TimeStepBasedOnCellSize,
};

struct FluidLessonApp1DSettings
{
    // Relative to `radio::config::shader_dir_path()`, e.g. "lesson1.comp.spv".
    std::filesystem::path compute_shader_filename = "";

    // True if the wave speed should be constant regardless of wave value.
    utils::Flags< FluidLesson1dFeature > features = { };
};

// Shared 1D fluid-lesson application. Every `lessonN-app` executable instantiates this class
// directly (via `fluid_lesson_app_1d_main()` below), configured with its own settings, rather
// than subclassing it.
class App
{
public:
    explicit App(
        window::GlfwContext&     glfw_context,
        window::GlfwWindow&      glfw_window,
        FluidLessonApp1DSettings settings
    );

    auto initialize( ) -> utils::Result< exec::UpdateLoopStatus >;

    [[nodiscard( "Const getter" )]]
    auto is_initialized( ) const -> bool;

    auto fixed_step_update( exec::UpdateLoopStatus const& status ) -> exec::UpdateRequests;
    auto frame_update( exec::UpdateLoopStatus const& status ) -> exec::UpdateRequests;

    auto configure_gui( ) -> void;

    auto on_resize( glm::ivec2 size ) -> utils::Result< void >;
    auto clean_up( ) -> utils::Result< void >;

private:
    window::GlfwContext& glfw_context_;
    window::GlfwWindow&  glfw_window_;

    FluidLessonApp1DSettings settings_;

    vlk::objs::VulkanGpu gpu_                        = { glfw_context_, glfw_window_ };
    vk::Queue            graphics_and_compute_queue_ = nullptr;
    vk::Queue            presentation_queue_         = nullptr;

    vlk::objs::VulkanPresentation presentation_ = { gpu_ };
    gui::ImguiGlfwVulkanSetup     imgui_        = { glfw_window_, gpu_, presentation_ };

    CfdParams           cfd_                = { };
    uint32              pending_num_points_ = cfd_.num_points;
    radio::FluidPipeline1 fluid_compute_      = { gpu_ };

    vlk::objs::VulkanBuffer      camera_ubo_            = { gpu_ };
    cam::Camera2d                camera_                = { };
    std::unordered_set< uint32 > camera_frames_updated_ = { };

    radio::FluidDisplayPipeline1 fluid_display_ = {
        gpu_,
        presentation_,
        fluid_compute_.fluid_buffer( ),
    };
    vlk::dd::LinesPipeline2         line_display_          = { gpu_, presentation_ };
    vlk::objs::VulkanCommandAndSync graphics_cmd_and_sync_ = { gpu_ };

    bool initialized_ = false;

    auto initialize_gpu( ) -> utils::Result< App* >;
    auto initialize_presentation( ) -> utils::Result< App* >;
    auto update_points( ) -> utils::Result< App* >;
    auto initialize_fluid( ) -> utils::Result< App* >;
    auto initialize_camera( ) -> utils::Result< App* >;
    auto initialize_display_pipeline( ) -> utils::Result< App* >;
    auto initialize_meshes( ) -> utils::Result< App* >;

    auto gui_info( ) -> void;
    auto gui_fixed_iterations( ) -> void;
    auto gui_time_step( ) -> void;
    auto gui_wave_speed( ) -> void;
    auto gui_reset( ) -> void;
    auto gui_reset_settings( ) -> void;

    auto compute( ) -> utils::Result< void >;

    auto render( ) -> utils::Result< void >;
    auto update_camera_uniforms( vlk::objs::FrameInfo const& frame ) -> utils::Result< void >;
    auto record_render_commands( vlk::objs::FrameInfo const& frame ) -> utils::Result< void >;
};

} // namespace ltb
