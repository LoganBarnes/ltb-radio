// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/vlk/objs/vulkan_buffer.hpp"
#include "ltb/vlk/objs/vulkan_command_and_sync.hpp"
#include "ltb/vlk/objs/vulkan_compute_pipeline.hpp"

// standard
#include <unordered_set>

namespace ltb::radio
{

struct FluidPipeline1Settings
{
    uint32                 frame_count       = 0U;
    std::vector< float32 > initial_positions = { };
    vk::Queue              compute_queue     = nullptr;
    std::filesystem::path  spirv_file        = "";
};

struct FluidComputeUniforms
{
    uint32     fluid_count         = 0U;
    float32    time_step           = 0.025F;
    float32    cell_size           = 0.1F;
    float32    wave_speed          = 1.0F;
    vk::Bool32 constant_wave_speed = true;
};

struct FrameIndices
{
    uint32 prev_frame_index = 0U;
    uint32 next_frame_index = 0U;
};

class FluidPipeline1
{
public:
    explicit( false ) FluidPipeline1( vlk::objs::VulkanGpu& gpu );

    auto initialize( FluidPipeline1Settings settings ) -> utils::Result< void >;
    auto reset_positions( std::vector< float32 > const& positions ) -> utils::Result< void >;

    [[nodiscard( "Const getter" )]]
    auto is_initialized( ) const -> bool;

    auto set_uniforms( FluidComputeUniforms uniforms ) -> void;
    auto dispatch( ) -> utils::Result< void >;

    [[nodiscard( "Const getter" )]]
    auto last_computed_frame_indices( ) const -> FrameIndices;
    [[nodiscard( "Const getter" )]]
    auto get_semaphore( ) const -> std::optional< vk::Semaphore > const&;
    auto clear_semaphore( ) -> void;

    [[nodiscard( "Const getter" )]]
    auto fluid_buffer( ) const -> vlk::objs::VulkanBuffer const&;

    [[nodiscard( "Const getter" )]]
    auto uniforms( ) const -> FluidComputeUniforms const&;

private:
    vlk::objs::VulkanGpu& gpu_;

    vlk::objs::VulkanComputePipeline compute_      = { gpu_ };
    vlk::objs::VulkanCommandAndSync  cmd_and_sync_ = { gpu_ };

    FluidPipeline1Settings settings_ = { };

    FluidComputeUniforms         cpu_uniforms_           = { };
    vlk::objs::VulkanBuffer      gpu_uniforms_           = { gpu_ };
    std::unordered_set< uint32 > uniform_frames_updated_ = { };

    vlk::objs::VulkanBuffer fluid_buffer_         = { gpu_ };
    glm::uvec3              dispatch_group_count_ = { 1U, 1U, 1U };

    std::optional< vk::Semaphore > compute_finished_semaphore_ = std::nullopt;

    bool initialized_ = false;

    auto update_uniform_buffer( vlk::objs::FrameInfo const& frame ) -> utils::Result< void >;
    auto record_compute_commands( vlk::objs::FrameInfo const& frame ) -> utils::Result< void >;
};

} // namespace ltb::radio
