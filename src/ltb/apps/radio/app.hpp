// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "sub_app.hpp"
#include "ltb/exec/update_loop.hpp"
#include "ltb/gui/imgui_glfw_vulkan_setup.hpp"
#include "ltb/vlk/objs/vulkan_command_and_sync.hpp"
#include "ltb/vlk/objs/vulkan_gpu.hpp"
#include "ltb/vlk/objs/vulkan_presentation.hpp"

// standard
#include <memory>
#include <string>
#include <vector>

namespace ltb::radio
{

class RadioApp
{
public:
    explicit RadioApp( window::GlfwContext& glfw_context, window::GlfwWindow& glfw_window );

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

    vlk::objs::VulkanGpu gpu_                        = { glfw_context_, glfw_window_ };
    vk::Queue            graphics_and_compute_queue_ = nullptr;
    vk::Queue            presentation_queue_         = nullptr;

    vlk::objs::VulkanPresentation   presentation_          = { gpu_ };
    gui::ImguiGlfwVulkanSetup       imgui_                 = { glfw_window_, gpu_, presentation_ };
    vlk::objs::VulkanCommandAndSync graphics_cmd_and_sync_ = { gpu_ };

    std::vector< std::unique_ptr< SubApp > > sub_apps_ = { };

    // ImGui stores a pointer to this path, so it must outlive the ImGui context.
    std::string imgui_ini_path_ = { };

    bool initialized_      = false;
    bool exit_requested_   = false;
    bool show_main_window_ = true;
    bool show_demo_window_ = false;

    auto initialize_gpu( ) -> utils::Result< RadioApp* >;
    auto initialize_presentation( ) -> utils::Result< RadioApp* >;
    auto initialize_command_and_sync( ) -> utils::Result< RadioApp* >;

    auto gui_toolbar( ) -> void;

    auto render( ) -> utils::Result< void >;
    auto record_render_commands( vlk::objs::FrameInfo const& frame ) -> utils::Result< void >;
};

} // namespace ltb::radio
