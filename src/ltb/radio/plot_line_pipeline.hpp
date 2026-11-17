// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/utils/result.hpp"
#include "ltb/utils/types.hpp"
#include "ltb/vlk/objs/frame_info.hpp"
#include "ltb/vlk/objs/vulkan_buffer.hpp"
#include "ltb/vlk/objs/vulkan_graphics_pipeline.hpp"

// external
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// standard
#include <span>

namespace ltb::radio
{

struct PlotLinePipelineSettings
{
    uint32 frame_count = 0U;

    // Total points that can be drawn per frame (across all lines).
    uint32 max_points = 65536U;
};

// A connected line through `points` in plot space.
struct PlotLine
{
    std::span< glm::vec2 const > points;
    glm::vec4                    color = glm::vec4( 1.0F );
};

// Draws line strips whose points change every frame (e.g. live waveforms). Each frame in
// flight has its own region of a host-visible vertex buffer.
class PlotLinePipeline
{
public:
    PlotLinePipeline( vlk::objs::VulkanGpu& gpu, vlk::RenderPass& render_pass );

    auto initialize( PlotLinePipelineSettings settings ) -> utils::Result< void >;

    [[nodiscard( "Const getter" )]]
    auto is_initialized( ) const -> bool;

    // Uploads the points and records the draw calls. Call while recording `frame` inside a
    // render pass compatible with `render_pass`. Points beyond `max_points` are not drawn.
    auto draw(
        vlk::objs::FrameInfo const& frame,
        glm::mat4 const&            clip_from_plot,
        std::span< PlotLine const > lines
    ) -> utils::Result< void >;

private:
    struct PushConstants
    {
        glm::mat4 clip_from_plot = glm::identity< glm::mat4 >( );
        glm::vec4 color          = glm::vec4( 1.0F );
    };

    vlk::objs::VulkanGpu& gpu_;
    vlk::RenderPass&      render_pass_;

    PlotLinePipelineSettings          settings_      = { };
    vlk::objs::VulkanGraphicsPipeline pipeline_      = { gpu_, render_pass_ };
    vlk::objs::VulkanBuffer           vertex_buffer_ = { gpu_ };

    bool initialized_ = false;
};

} // namespace ltb::radio
