// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/vlk/dd/mesh_data.hpp"
#include "ltb/vlk/dd/simple_mesh_2.hpp"
#include "ltb/vlk/objs/vulkan_graphics_pipeline.hpp"

namespace ltb::radio
{

struct FluidDisplayPipeline1Settings
{
    uint32                         frame_count = 0U;
    vlk::objs::VulkanBuffer const& camera_ubo;
};

struct SimulationUniforms
{
    float32 frame_interpolant = 1.0F;
    float32 cell_size         = 1.0F;
    uint32  fluid_cell_count  = 0U;
    float32 line_thickness    = 0.01F;
};

class FluidDisplayPipeline1
{
public:
    FluidDisplayPipeline1(
        vlk::objs::VulkanGpu&          gpu,
        vlk::objs::VulkanPresentation& presentation,
        vlk::objs::VulkanBuffer const& gpu_fluid
    );

    auto initialize( FluidDisplayPipeline1Settings const& settings ) -> utils::Result< void >;

    [[nodiscard( "Const getter" )]]
    auto is_initialized( ) const -> bool;

    auto set_uniforms( SimulationUniforms uniforms ) -> void;
    auto draw( vlk::objs::FrameInfo const& frame, uint32 prev_fluid_index, uint32 next_fluid_index )
        -> utils::Result< void >;

    [[nodiscard( "Const getter" )]]
    auto uniforms( ) const -> SimulationUniforms const&;

private:
    vlk::objs::VulkanGpu&          gpu_;
    vlk::objs::VulkanPresentation& presentation_;
    vlk::objs::VulkanBuffer const& gpu_fluid_;

    vlk::objs::VulkanGraphicsPipeline pipeline_            = { gpu_, presentation_ };
    SimulationUniforms                simulation_uniforms_ = { };

    bool initialized_ = false;
};

} // namespace ltb::radio
