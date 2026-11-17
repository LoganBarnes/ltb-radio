// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "plot_line_pipeline.hpp"

// project
#include "ltb/radio/ltb_radio_config.hpp"
#include "ltb/vlk/check.hpp"
#include "ltb/vlk/device_memory_utils.hpp"

// standard
#include <algorithm>
#include <cstring>

namespace ltb::radio
{

PlotLinePipeline::PlotLinePipeline( vlk::objs::VulkanGpu& gpu, vlk::RenderPass& render_pass )
    : gpu_( gpu )
    , render_pass_( render_pass )
{
}

auto PlotLinePipeline::initialize( PlotLinePipelineSettings const settings )
    -> utils::Result< void >
{
    if ( this->is_initialized( ) )
    {
        return utils::success( );
    }
    LTB_CHECK_VALID( settings.frame_count > 0U );
    LTB_CHECK_VALID( settings.max_points > 0U );
    settings_ = settings;

    auto layout = vlk::MemoryLayout{ };
    vlk::append_memory_size_n(
        layout,
        settings.max_points * sizeof( glm::vec2 ),
        settings.frame_count
    );

    LTB_CHECK( vertex_buffer_.initialize( {
        .layout       = std::move( layout ),
        .buffer_usage = vk::BufferUsageFlagBits::eVertexBuffer,
        .memory_properties
        = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        .store_mapped_value = true,
    } ) );

    auto shader_modules = std::vector< vlk::ShaderModuleSettings >{
        {
            .spirv_file = config::shader_dir_path( ) / "plot_line.vert.spv",
            .stage      = vk::ShaderStageFlagBits::eVertex,
        },
        {
            .spirv_file = config::shader_dir_path( ) / "plot_line.frag.spv",
            .stage      = vk::ShaderStageFlagBits::eFragment,
        },
    };

    auto push_constants = std::vector{
        vk::PushConstantRange{ }
            .setStageFlags( vk::ShaderStageFlagBits::eVertex )
            .setOffset( 0U )
            .setSize( sizeof( PushConstants ) ),
    };

    auto vertex_bindings = std::vector{
        vk::VertexInputBindingDescription{ }
            .setBinding( 0U )
            .setStride( sizeof( glm::vec2 ) )
            .setInputRate( vk::VertexInputRate::eVertex ),
    };

    auto vertex_attributes = std::vector{
        vk::VertexInputAttributeDescription{ }
            .setBinding( 0U )
            .setLocation( 0U )
            .setFormat( vk::Format::eR32G32Sfloat )
            .setOffset( 0U ),
    };

    LTB_CHECK( pipeline_.initialize( {
        .shader_modules         = std::move( shader_modules ),
        .descriptor_set_count   = settings.frame_count,
        .uniform_binding_sets   = { },
        .uniform_push_constants = std::move( push_constants ),

        .pipeline = {
            .vertex_bindings    = std::move( vertex_bindings ),
            .vertex_attributes  = std::move( vertex_attributes ),
            .primitive_topology = vk::PrimitiveTopology::eLineStrip,
            .rasterizer         = vk::PipelineRasterizationStateCreateInfo{ }
                                      .setPolygonMode( vk::PolygonMode::eFill )
                                      .setLineWidth( 1.0F )
                                      .setCullMode( vk::CullModeFlagBits::eNone ),
            // Lines are drawn in order; later lines are on top.
            .depth_stencil = vk::PipelineDepthStencilStateCreateInfo{ }
                                 .setDepthTestEnable( false )
                                 .setDepthWriteEnable( false ),
        },
    } ) );

    initialized_ = true;

    return utils::success( );
}

auto PlotLinePipeline::is_initialized( ) const -> bool
{
    return initialized_;
}

auto PlotLinePipeline::draw(
    vlk::objs::FrameInfo const&       frame,
    glm::mat4 const&                  clip_from_plot,
    std::span< PlotLine const > const lines
) -> utils::Result< void >
{
    LTB_CHECK_VALID( this->is_initialized( ) );
    LTB_CHECK_VALID( frame.frame_index < vertex_buffer_.layout( ).ranges.size( ) );

    auto const& range  = vertex_buffer_.layout( ).ranges[ frame.frame_index ];
    auto* const points = vertex_buffer_.mapped_data( ) + range.offset;

    pipeline_.bind( frame.command_buffer );
    frame.command_buffer.bindVertexBuffers( 0U, vertex_buffer_.buffer( ).get( ), range.offset );

    auto first = 0U;
    for ( auto const& line : lines )
    {
        auto const count = std::
            min( static_cast< uint32 >( line.points.size( ) ), settings_.max_points - first );
        if ( count < 2U )
        {
            continue;
        }
        std::memcpy(
            points + first * sizeof( glm::vec2 ),
            line.points.data( ),
            count * sizeof( glm::vec2 )
        );

        auto const push_constants
            = PushConstants{ .clip_from_plot = clip_from_plot, .color = line.color };
        frame.command_buffer.pushConstants(
            pipeline_.pipeline_layout( ).get( ),
            vk::ShaderStageFlagBits::eVertex,
            0U,
            sizeof( push_constants ),
            &push_constants
        );
        frame.command_buffer.draw( count, 1U, first, 0U );
        first += count;
    }

    return utils::success( );
}

} // namespace ltb::radio
