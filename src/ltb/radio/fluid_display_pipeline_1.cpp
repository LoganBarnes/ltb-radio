// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "ltb/radio/fluid_display_pipeline_1.hpp"

// project
#include "ltb/radio/ltb_radio_config.hpp"
#include "ltb/vlk/buffer_utils.hpp"
#include "ltb/vlk/check.hpp"
#include "ltb/vlk/device_memory_utils.hpp"
#include "ltb/vlk/objs/frame_info.hpp"

// external
#include "spdlog/fmt/bundled/os.h"
#include <spdlog/spdlog.h>

namespace ltb::radio
{

FluidDisplayPipeline1::FluidDisplayPipeline1(
    vlk::objs::VulkanGpu&          gpu,
    vlk::objs::VulkanPresentation& presentation,
    vlk::objs::VulkanBuffer const& gpu_fluid
)
    : gpu_( gpu )
    , presentation_( presentation )
    , gpu_fluid_( gpu_fluid )
{
}

auto FluidDisplayPipeline1::initialize( FluidDisplayPipeline1Settings const& settings )
    -> utils::Result< void >
{
    if ( this->is_initialized( ) )
    {
        return utils::success( );
    }
    LTB_CHECK_VALID( gpu_.is_initialized( ) );
    LTB_CHECK_VALID( presentation_.is_initialized( ) );
    LTB_CHECK_VALID( gpu_fluid_.is_initialized( ) );
    LTB_CHECK_VALID( settings.frame_count > 0U );
    LTB_CHECK_VALID( settings.camera_ubo.is_initialized( ) );

    auto shader_modules = std::vector< vlk::ShaderModuleSettings >{
        {
            .spirv_file = config::shader_dir_path( ) / "display1d.vert.spv",
            .stage      = vk::ShaderStageFlagBits::eVertex,
        },
        {
            .spirv_file = config::shader_dir_path( ) / "display1d.tesc.spv",
            .stage      = vk::ShaderStageFlagBits::eTessellationControl,
        },
        {
            .spirv_file = config::shader_dir_path( ) / "display1d.tese.spv",
            .stage      = vk::ShaderStageFlagBits::eTessellationEvaluation,
        },
        {
            .spirv_file = config::shader_dir_path( ) / "white.frag.spv",
            .stage      = vk::ShaderStageFlagBits::eFragment,
        },
    };

    auto uniform_bindings = std::vector{
        // camera UBO
        vk::DescriptorSetLayoutBinding{ }
            .setBinding( 0U )
            .setDescriptorType( vk::DescriptorType::eUniformBuffer )
            .setDescriptorCount( 1U )
            .setStageFlags( vk::ShaderStageFlagBits::eTessellationEvaluation ),
    };

    auto uniform_push_constants = std::vector{
        // simulation uniforms
        vk::PushConstantRange{ }
            .setStageFlags( vk::ShaderStageFlagBits::eTessellationEvaluation )
            .setOffset( 0U )
            .setSize( sizeof( SimulationUniforms ) ),
    };

    auto vertex_bindings = std::vector{
        vk::VertexInputBindingDescription{ }
            .setBinding( 0U )
            .setStride( sizeof( float32 ) )
            .setInputRate( vk::VertexInputRate::eVertex ),
        vk::VertexInputBindingDescription{ }
            .setBinding( 1U )
            .setStride( sizeof( float32 ) )
            .setInputRate( vk::VertexInputRate::eVertex ),
    };

    auto vertex_attributes = std::vector{
        vk::VertexInputAttributeDescription{ }
            .setBinding( 0U )
            .setLocation( 0U )
            .setFormat( vk::Format::eR32Sfloat )
            .setOffset( 0U ),
        vk::VertexInputAttributeDescription{ }
            .setBinding( 1U )
            .setLocation( 1U )
            .setFormat( vk::Format::eR32Sfloat )
            .setOffset( 0U ),
    };

    constexpr auto tessellation_state
        = vk::PipelineTessellationStateCreateInfo{ }.setPatchControlPoints( 1U );

    LTB_CHECK( pipeline_.initialize( {
        .shader_modules         = std::move( shader_modules ),
        .descriptor_set_count   = settings.frame_count,
        .uniform_binding_sets   = { std::move( uniform_bindings ) },
        .uniform_push_constants = std::move( uniform_push_constants ),

        .pipeline = {
            .vertex_bindings    = std::move( vertex_bindings ),
            .vertex_attributes  = std::move( vertex_attributes ),
            .primitive_topology = vk::PrimitiveTopology::ePatchList,
            .depth_stencil      = std::nullopt,
            .tessellation_state = tessellation_state,
        },
    } ) );

    LTB_CHECK_VALID( 1UZ == pipeline_.descriptor_sets( ).size( ) );

    auto const& uniform_descriptor_sets = pipeline_.descriptor_sets( ).front( ).get( );
    for ( auto frame_index = 0U; frame_index < settings.frame_count; ++frame_index )
    {
        LTB_CHECK_VALID( frame_index < settings.camera_ubo.layout( ).ranges.size( ) );
        LTB_CHECK_VALID( frame_index < uniform_descriptor_sets.size( ) );

        auto const& ubo_memory_range = settings.camera_ubo.layout( ).ranges[ frame_index ];
        auto const& descriptor_set   = uniform_descriptor_sets[ frame_index ];

        auto const ubo_info = vk::DescriptorBufferInfo{ }
                                  .setBuffer( settings.camera_ubo.buffer( ).get( ) )
                                  .setOffset( ubo_memory_range.offset )
                                  .setRange( ubo_memory_range.size );

        auto const descriptor_writes = std::vector{
            vk::WriteDescriptorSet{ }
                .setDstSet( descriptor_set )
                .setDstBinding( 0U )
                .setDstArrayElement( 0U )
                .setDescriptorType( vk::DescriptorType::eUniformBuffer )
                .setBufferInfo( ubo_info ),
        };

        gpu_.device( ).get( ).updateDescriptorSets( descriptor_writes, { } );
    }

    initialized_ = true;

    return utils::success( );
}

auto FluidDisplayPipeline1::is_initialized( ) const -> bool
{
    return initialized_;
}

auto FluidDisplayPipeline1::set_uniforms( SimulationUniforms const uniforms ) -> void
{
    simulation_uniforms_ = uniforms;
}

auto FluidDisplayPipeline1::draw(
    vlk::objs::FrameInfo const& frame,
    uint32 const                prev_fluid_index,
    uint32 const                next_fluid_index
) -> utils::Result< void >
{
    if ( simulation_uniforms_.fluid_cell_count <= 0U )
    {
        return utils::success( );
    }

    pipeline_.bind( frame.command_buffer );

    LTB_CHECK( pipeline_.bind_descriptor_sets( frame ) );

    constexpr auto model_offset = 0U;
    frame.command_buffer.pushConstants(
        pipeline_.pipeline_layout( ).get( ),
        vk::ShaderStageFlagBits::eTessellationEvaluation,
        model_offset,
        sizeof( simulation_uniforms_ ),
        &simulation_uniforms_
    );

    LTB_CHECK_VALID( prev_fluid_index < gpu_fluid_.layout( ).ranges.size( ) );
    LTB_CHECK_VALID( next_fluid_index < gpu_fluid_.layout( ).ranges.size( ) );
    auto const& prev_offset = gpu_fluid_.layout( ).ranges[ prev_fluid_index ].offset;
    auto const& next_offset = gpu_fluid_.layout( ).ranges[ next_fluid_index ].offset;

    constexpr auto first_binding = 0U;

    auto const vertex_buffers = std::array{
        gpu_fluid_.buffer( ).get( ),
        gpu_fluid_.buffer( ).get( ),
    };
    auto const vertex_offsets = std::array{
        prev_offset,
        next_offset,
    };
    frame.command_buffer.bindVertexBuffers( first_binding, vertex_buffers, vertex_offsets );

    auto const     vertex_count   = simulation_uniforms_.fluid_cell_count;
    constexpr auto instance_count = 1U;
    constexpr auto first_vertex   = 0U;
    constexpr auto first_instance = 0U;
    frame.command_buffer.draw( vertex_count, instance_count, first_vertex, first_instance );

    return utils::success( );
}

auto FluidDisplayPipeline1::uniforms( ) const -> SimulationUniforms const&
{
    return simulation_uniforms_;
}

} // namespace ltb::radio
