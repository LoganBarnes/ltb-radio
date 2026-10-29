// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/utils/result.hpp"
#include "ltb/utils/types.hpp"
#include "ltb/vlk/framebuffer.hpp"
#include "ltb/vlk/image_view.hpp"
#include "ltb/vlk/objs/frame_info.hpp"
#include "ltb/vlk/objs/vulkan_gpu.hpp"
#include "ltb/vlk/objs/vulkan_image.hpp"
#include "ltb/vlk/render_pass.hpp"

// standard
#include <memory>
#include <vector>

namespace ltb::radio
{

struct ImguiViewportSettings
{
    // Number of frames in flight. Each frame gets its own render target.
    uint32 frame_count = 0U;

    vk::Format   color_format = vk::Format::eR8G8B8A8Unorm;
    vk::Format   depth_format = vk::Format::eD32Sfloat;
    glm::vec4    clear_color  = glm::vec4( 0.1F, 0.1F, 0.1F, 1.0F );
    vk::Extent2D initial_size = { 256U, 256U };
};

// Renders Vulkan content into an offscreen target and displays it as an image in an ImGui window.
// The target is resized to fill the window's content region (in framebuffer pixels).
//
// Per frame:
//   1. While building the ImGui window: call `show( frame_index )`.
//   2. While recording commands (before the main render pass): call `begin_render_pass( frame )`,
//      record draw calls using pipelines created with `render_pass()`, then `end_render_pass( frame
//      )`.
class ImguiViewport
{
public:
    explicit ImguiViewport( vlk::objs::VulkanGpu& gpu );
    ~ImguiViewport( );

    ImguiViewport( ImguiViewport const& )                    = delete;
    auto operator=( ImguiViewport const& ) -> ImguiViewport& = delete;
    ImguiViewport( ImguiViewport&& )                         = delete;
    auto operator=( ImguiViewport&& ) -> ImguiViewport&      = delete;

    // The ImGui Vulkan backend must already be initialized.
    auto initialize( ImguiViewportSettings settings ) -> utils::Result< void >;

    [[nodiscard( "Const getter" )]]
    auto is_initialized( ) const -> bool;

    // Draws the viewport image to fill the remaining content region of the current ImGui window.
    // Resizes the render target first if the region changed. Does nothing if the region is empty.
    auto show( uint32 frame_index ) -> utils::Result< void >;

    auto begin_render_pass( vlk::objs::FrameInfo const& frame ) -> utils::Result< void >;
    auto end_render_pass( vlk::objs::FrameInfo const& frame ) -> void;

    // Size of the render target in pixels.
    [[nodiscard( "Const getter" )]]
    auto extent( ) const -> vk::Extent2D;

    // Size of the displayed image in ImGui (logical) units, and its top-left screen position.
    [[nodiscard( "Const getter" )]]
    auto logical_size( ) const -> glm::vec2;
    [[nodiscard( "Const getter" )]]
    auto logical_origin( ) const -> glm::vec2;

    // The mouse is over the viewport image (and not blocked by another window).
    [[nodiscard( "Const getter" )]]
    auto is_hovered( ) const -> bool;

    // A mouse button was pressed on the viewport image and is still held (even if the mouse left
    // it).
    [[nodiscard( "Const getter" )]]
    auto is_active( ) const -> bool;

    // For creating pipelines that render into this viewport.
    [[nodiscard( "Const getter" )]]
    auto render_pass( ) -> vlk::RenderPass&;

private:
    struct Target
    {
        Target( vlk::objs::VulkanGpu& gpu, vlk::RenderPass& render_pass );

        vlk::objs::VulkanImage color_image;
        vlk::ImageView         color_view;
        vlk::objs::VulkanImage depth_image;
        vlk::ImageView         depth_view;
        vlk::Framebuffer       framebuffer;
        vk::DescriptorSet      texture = nullptr;
    };

    vlk::objs::VulkanGpu& gpu_;

    ImguiViewportSettings settings_    = { };
    vk::Extent2D          extent_      = { };
    vlk::RenderPass       render_pass_ = { gpu_.device( ) };
    vk::UniqueSampler     sampler_     = { };

    std::vector< std::unique_ptr< Target > > targets_ = { };

    glm::vec2 logical_size_   = { };
    glm::vec2 logical_origin_ = { };

    bool hovered_ = false;
    bool active_  = false;

    bool initialized_ = false;

    auto initialize_render_pass( ) -> utils::Result< void >;
    auto create_targets( vk::Extent2D extent ) -> utils::Result< void >;
    auto destroy_targets( ) -> void;
    auto resize( vk::Extent2D extent ) -> utils::Result< void >;
};

} // namespace ltb::radio
