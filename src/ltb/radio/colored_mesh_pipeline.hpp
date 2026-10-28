// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/cam/camera_render_params.hpp"
#include "ltb/utils/result.hpp"
#include "ltb/utils/types.hpp"
#include "ltb/vlk/command_pool.hpp"
#include "ltb/vlk/objs/frame_info.hpp"
#include "ltb/vlk/objs/vulkan_buffer.hpp"
#include "ltb/vlk/objs/vulkan_graphics_pipeline.hpp"

// standard
#include <list>
#include <vector>

namespace ltb::radio
{

struct ColoredMeshPipelineSettings
{
    uint32 frame_count = 0U;
};

struct ColoredTriangleMesh
{
    std::vector< glm::vec3 > positions = { };
    std::vector< glm::vec3 > colors    = { };
    std::vector< uint32 >    indices   = { };
};

// Draws indexed triangle meshes with per-vertex colors. The pipeline is compatible with
// `render_pass` and expects the camera uniforms to be updated for each frame.
class ColoredMeshPipeline
{
public:
    struct MeshPushConstants
    {
        glm::mat4 world_from_model = glm::identity< glm::mat4 >( );
    };

    ColoredMeshPipeline( vlk::objs::VulkanGpu& gpu, vlk::RenderPass& render_pass );

    auto initialize( ColoredMeshPipelineSettings settings ) -> utils::Result< void >;

    [[nodiscard( "Const getter" )]]
    auto is_initialized( ) const -> bool;

    // Returns a pointer to the mesh's push constants (valid for the life of the pipeline).
    auto initialize_mesh(
        ColoredTriangleMesh const& mesh,
        vlk::CommandPool&          command_pool,
        vk::Queue const&           queue
    ) -> utils::Result< MeshPushConstants* >;

    auto set_camera( uint32 frame_index, cam::CameraRenderParams const& camera )
        -> utils::Result< void >;

    auto draw( vlk::objs::FrameInfo const& frame ) -> utils::Result< void >;

private:
    struct Mesh
    {
        explicit Mesh( vlk::objs::VulkanGpu& gpu )
            : buffer( gpu )
        {
        }

        vlk::objs::VulkanBuffer buffer;
        uint32                  index_count    = 0U;
        MeshPushConstants       push_constants = { };
    };

    vlk::objs::VulkanGpu& gpu_;
    vlk::RenderPass&      render_pass_;

    vlk::objs::VulkanGraphicsPipeline pipeline_   = { gpu_, render_pass_ };
    vlk::objs::VulkanBuffer           camera_ubo_ = { gpu_ };

    std::list< Mesh > meshes_      = { };
    bool              initialized_ = false;

    auto draw_mesh( Mesh const& mesh, vlk::objs::FrameInfo const& frame ) -> utils::Result< void >;
};

} // namespace ltb::radio
