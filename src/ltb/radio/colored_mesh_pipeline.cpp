// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "colored_mesh_pipeline.hpp"

// project
#include "ltb/radio/ltb_radio_config.hpp"
#include "ltb/vlk/check.hpp"
#include "ltb/vlk/command_buffer.hpp"
#include "ltb/vlk/device_memory_utils.hpp"

// standard
#include <array>
#include <cstring>

namespace ltb::radio
{

namespace
{

constexpr auto position_size_bytes = sizeof( glm::vec3 );
constexpr auto color_size_bytes    = sizeof( glm::vec3 );
constexpr auto index_size_bytes    = sizeof( uint32 );
constexpr auto vertex_format       = vk::Format::eR32G32B32Sfloat;

} // namespace

ColoredMeshPipeline::ColoredMeshPipeline( vlk::objs::VulkanGpu& gpu, vlk::RenderPass& render_pass )
    : gpu_( gpu )
    , render_pass_( render_pass )
{
}

auto ColoredMeshPipeline::initialize( ColoredMeshPipelineSettings const settings )
    -> utils::Result< void >
{
    if ( this->is_initialized( ) )
    {
        return utils::success( );
    }
    LTB_CHECK_VALID( settings.frame_count > 0U );

    auto camera_layout = vlk::MemoryLayout{ };
    vlk::append_memory_size_n(
        camera_layout,
        sizeof( cam::CameraRenderParams ),
        settings.frame_count
    );

    LTB_CHECK( camera_ubo_.initialize( {
        .layout       = std::move( camera_layout ),
        .buffer_usage = vk::BufferUsageFlagBits::eUniformBuffer,
        .memory_properties
        = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        .store_mapped_value = true,
    } ) );

    auto shader_modules = std::vector< vlk::ShaderModuleSettings >{
        {
            .spirv_file = config::shader_dir_path( ) / "colored_mesh.vert.spv",
            .stage      = vk::ShaderStageFlagBits::eVertex,
        },
        {
            .spirv_file = config::shader_dir_path( ) / "colored_mesh.frag.spv",
            .stage      = vk::ShaderStageFlagBits::eFragment,
        },
    };

    auto uniform_bindings = std::vector{
        vk::DescriptorSetLayoutBinding{ }
            .setBinding( 0U )
            .setDescriptorType( vk::DescriptorType::eUniformBuffer )
            .setDescriptorCount( 1U )
            .setStageFlags( vk::ShaderStageFlagBits::eVertex ),
    };

    auto uniform_push_constants = std::vector{
        vk::PushConstantRange{ }
            .setStageFlags( vk::ShaderStageFlagBits::eVertex )
            .setOffset( 0U )
            .setSize( sizeof( MeshPushConstants ) ),
    };

    auto vertex_bindings = std::vector{
        vk::VertexInputBindingDescription{ }
            .setBinding( 0U )
            .setStride( position_size_bytes )
            .setInputRate( vk::VertexInputRate::eVertex ),
        vk::VertexInputBindingDescription{ }
            .setBinding( 1U )
            .setStride( color_size_bytes )
            .setInputRate( vk::VertexInputRate::eVertex ),
    };

    auto vertex_attributes = std::vector{
        vk::VertexInputAttributeDescription{ }
            .setBinding( 0U )
            .setLocation( 0U )
            .setFormat( vertex_format )
            .setOffset( 0U ),
        vk::VertexInputAttributeDescription{ }
            .setBinding( 1U )
            .setLocation( 1U )
            .setFormat( vertex_format )
            .setOffset( 0U ),
    };

    LTB_CHECK( pipeline_.initialize( {
        .shader_modules         = std::move( shader_modules ),
        .descriptor_set_count   = settings.frame_count,
        .uniform_binding_sets   = { std::move( uniform_bindings ) },
        .uniform_push_constants = std::move( uniform_push_constants ),

        .pipeline = {
            .vertex_bindings    = std::move( vertex_bindings ),
            .vertex_attributes  = std::move( vertex_attributes ),
            .primitive_topology = settings.topology,
            .rasterizer         = vk::PipelineRasterizationStateCreateInfo{ }
                              .setPolygonMode( vk::PolygonMode::eFill )
                              .setLineWidth( 1.0F )
                              .setCullMode( vk::CullModeFlagBits::eBack )
                              .setFrontFace( vk::FrontFace::eCounterClockwise ),
        },
    } ) );
    LTB_CHECK_VALID( 1UZ == pipeline_.descriptor_sets( ).size( ) );

    auto const& descriptor_sets = pipeline_.descriptor_sets( ).front( ).get( );
    LTB_CHECK_VALID( settings.frame_count <= descriptor_sets.size( ) );
    LTB_CHECK_VALID( settings.frame_count <= camera_ubo_.layout( ).ranges.size( ) );

    for ( auto frame_index = 0U; frame_index < settings.frame_count; ++frame_index )
    {
        auto const& memory_range = camera_ubo_.layout( ).ranges[ frame_index ];

        auto const buffer_info = vk::DescriptorBufferInfo{ }
                                     .setBuffer( camera_ubo_.buffer( ).get( ) )
                                     .setOffset( memory_range.offset )
                                     .setRange( memory_range.size );

        auto const writes = std::vector{
            vk::WriteDescriptorSet{ }
                .setDstSet( descriptor_sets[ frame_index ] )
                .setDstBinding( 0U )
                .setDstArrayElement( 0U )
                .setDescriptorType( vk::DescriptorType::eUniformBuffer )
                .setBufferInfo( buffer_info ),
        };
        gpu_.device( ).get( ).updateDescriptorSets( writes, { } );
    }

    initialized_ = true;

    return utils::success( );
}

auto ColoredMeshPipeline::is_initialized( ) const -> bool
{
    return initialized_;
}

auto ColoredMeshPipeline::initialize_mesh(
    ColoredMesh const& mesh,
    vlk::CommandPool&  command_pool,
    vk::Queue const&   queue
) -> utils::Result< MeshPushConstants* >
{
    LTB_CHECK_VALID( command_pool.is_initialized( ) );
    LTB_CHECK_VALID( !mesh.positions.empty( ) );
    LTB_CHECK_VALID( mesh.positions.size( ) == mesh.colors.size( ) );
    LTB_CHECK_VALID( !mesh.indices.empty( ) );

    auto vbo_layout = vlk::MemoryLayout{ };
    vlk::append_memory_size( vbo_layout, mesh.positions.size( ) * position_size_bytes );
    vlk::append_memory_size( vbo_layout, mesh.colors.size( ) * color_size_bytes );
    vlk::append_memory_size( vbo_layout, mesh.indices.size( ) * index_size_bytes );

    auto& new_mesh = meshes_.emplace_back( gpu_ );

    LTB_CHECK( new_mesh.buffer.initialize( {
        .layout       = std::move( vbo_layout ),
        .buffer_usage = vk::BufferUsageFlagBits::eVertexBuffer
                      | vk::BufferUsageFlagBits::eIndexBuffer
                      | vk::BufferUsageFlagBits::eTransferDst,
        .memory_properties  = vk::MemoryPropertyFlagBits::eDeviceLocal,
        .store_mapped_value = false,
    } ) );

    auto staging = vlk::objs::VulkanBuffer{ gpu_ };
    LTB_CHECK( staging.initialize( {
        .layout       = new_mesh.buffer.layout( ),
        .buffer_usage = vk::BufferUsageFlagBits::eTransferSrc,
        .memory_properties
        = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        .store_mapped_value = false,
    } ) );

    auto const host_data = std::array< void const*, 3U >{
        mesh.positions.data( ),
        mesh.colors.data( ),
        mesh.indices.data( ),
    };
    LTB_CHECK_VALID( host_data.size( ) == staging.layout( ).ranges.size( ) );

    LTB_CHECK(
        auto* const mapped_data,
        staging.memory( ).map( { .size = staging.layout( ).total_size } )
    );
    for ( auto i = 0UZ; i < host_data.size( ); ++i )
    {
        auto const& range = staging.layout( ).ranges[ i ];
        std::memcpy( mapped_data + range.offset, host_data[ i ], range.size );
    }
    staging.memory( ).unmap( );

    auto tmp_command_buffer = vlk::CommandBuffer{ gpu_.device( ), command_pool };
    LTB_CHECK( tmp_command_buffer.initialize( ) );
    auto const& command_buffer = tmp_command_buffer.get( );

    VK_CHECK( command_buffer.begin(
        vk::CommandBufferBeginInfo{ }.setFlags( vk::CommandBufferUsageFlagBits::eOneTimeSubmit )
    ) );
    command_buffer.copyBuffer(
        staging.buffer( ).get( ),
        new_mesh.buffer.buffer( ).get( ),
        { vk::BufferCopy{ }.setSize( staging.layout( ).total_size ) }
    );
    VK_CHECK( command_buffer.end( ) );

    VK_CHECK( queue.submit( vk::SubmitInfo{ }.setCommandBuffers( command_buffer ), nullptr ) );
    VK_CHECK( queue.waitIdle( ) );

    new_mesh.index_count = static_cast< uint32 >( mesh.indices.size( ) );

    return &new_mesh.push_constants;
}

auto ColoredMeshPipeline::set_camera(
    uint32 const                   frame_index,
    cam::CameraRenderParams const& camera
) -> utils::Result< void >
{
    LTB_CHECK_VALID( this->is_initialized( ) );
    LTB_CHECK_VALID( frame_index < camera_ubo_.layout( ).ranges.size( ) );

    auto const& range = camera_ubo_.layout( ).ranges[ frame_index ];
    LTB_CHECK_VALID( range.size >= sizeof( camera ) );

    std::memcpy( camera_ubo_.mapped_data( ) + range.offset, &camera, sizeof( camera ) );

    return utils::success( );
}

auto ColoredMeshPipeline::draw( vlk::objs::FrameInfo const& frame ) -> utils::Result< void >
{
    LTB_CHECK_VALID( this->is_initialized( ) );

    pipeline_.bind( frame.command_buffer );
    LTB_CHECK( pipeline_.bind_descriptor_sets( frame ) );

    for ( auto const& mesh : meshes_ )
    {
        LTB_CHECK( this->draw_mesh( mesh, frame ) );
    }
    return utils::success( );
}

auto ColoredMeshPipeline::draw_mesh( Mesh const& mesh, vlk::objs::FrameInfo const& frame )
    -> utils::Result< void >
{
    if ( mesh.index_count == 0U )
    {
        return utils::success( );
    }

    frame.command_buffer.pushConstants(
        pipeline_.pipeline_layout( ).get( ),
        vk::ShaderStageFlagBits::eVertex,
        0U,
        sizeof( mesh.push_constants ),
        &mesh.push_constants
    );

    auto const& ranges = mesh.buffer.layout( ).ranges;
    LTB_CHECK_VALID( ranges.size( ) == 3U );

    auto const buffer         = mesh.buffer.buffer( ).get( );
    auto const vertex_buffers = std::array{ buffer, buffer };
    auto const vertex_offsets = std::array{ ranges[ 0 ].offset, ranges[ 1 ].offset };
    frame.command_buffer.bindVertexBuffers( 0U, vertex_buffers, vertex_offsets );
    frame.command_buffer.bindIndexBuffer( buffer, ranges[ 2 ].offset, vk::IndexType::eUint32 );
    frame.command_buffer.drawIndexed( mesh.index_count, 1U, 0U, 0, 0U );

    return utils::success( );
}

} // namespace ltb::radio
