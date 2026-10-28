// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "ltb/radio/fluid_pipeline_1.hpp"

// project
#include "ltb/vlk/buffer_utils.hpp"
#include "ltb/vlk/check.hpp"
#include "ltb/vlk/command_pool.hpp"
#include "ltb/vlk/device_memory_utils.hpp"
#include "ltb/vlk/objs/frame_info.hpp"

// external
#include <spdlog/fmt/bundled/os.h>
#include <spdlog/spdlog.h>

// standard
#include <ranges>

namespace ltb::radio
{
namespace
{

constexpr auto workgroup_size_x = 64U;

template < typename DataType >
auto copy_with_staging_buffer(
    vlk::objs::VulkanGpu&            gpu,
    vlk::objs::VulkanCommandAndSync& cmd_and_sync,
    vlk::objs::VulkanBuffer&         gpu_buffer,
    std::vector< DataType > const&   cpu_data,
    vk::Queue const&                 queue
) -> utils::Result< void >
{
    LTB_CHECK_VALID( !cpu_data.empty( ) );

    auto staging = vlk::objs::VulkanBuffer{ gpu };

    auto const buffer_size
        = static_cast< vk::DeviceSize >( cpu_data.size( ) * sizeof( cpu_data.front( ) ) );

    LTB_CHECK( staging.initialize( {
        .layout       = { .total_size = buffer_size },
        .buffer_usage = vk::BufferUsageFlagBits::eTransferSrc,
        .memory_properties
        = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        .store_mapped_value = false,
    } ) );
    LTB_CHECK_VALID( buffer_size <= staging.memory( ).size( ) );

    LTB_CHECK( auto* const staging_data, staging.memory( ).map( ) );
    LTB_CHECK_VALID( std::memcpy( staging_data, cpu_data.data( ), buffer_size ) == staging_data );
    staging.memory( ).unmap( );

    auto tmp_copy_command_buffer = vlk::CommandBuffer{
        gpu.device( ),
        cmd_and_sync.command_pool( ),
    };
    LTB_CHECK( tmp_copy_command_buffer.initialize( ) );

    auto const copy_ranges = gpu_buffer.layout( ).ranges
                           | std::views::transform( vlk::MakeCopyRegion{ } )
                           | std::ranges::to< std::vector >( );

    LTB_CHECK(
        vlk::copy_buffer(
            tmp_copy_command_buffer.get( ),
            queue,
            staging.buffer( ),
            gpu_buffer.buffer( ),
            copy_ranges
        )
    );

    return utils::success( );
}

} // namespace

FluidPipeline1::FluidPipeline1( vlk::objs::VulkanGpu& gpu )
    : gpu_( gpu )
{
}

auto FluidPipeline1::initialize( FluidPipeline1Settings settings ) -> utils::Result< void >
{
    if ( this->is_initialized( ) )
    {
        return utils::success( );
    }
    LTB_CHECK_VALID( gpu_.is_initialized( ) );
    settings_ = std::move( settings );
    LTB_CHECK_VALID( settings_.frame_count > 0U );
    LTB_CHECK_VALID( !settings_.initial_positions.empty( ) );
    LTB_CHECK_VALID( settings_.compute_queue != nullptr );

    auto shader_module = vlk::ShaderModuleSettings{
        .spirv_file = settings_.spirv_file,
        .stage      = vk::ShaderStageFlagBits::eCompute,
    };

    auto uniform_bindings = std::vector{
        vk::DescriptorSetLayoutBinding{ }
            .setBinding( 0U )
            .setDescriptorType( vk::DescriptorType::eStorageBuffer )
            .setDescriptorCount( 1U )
            .setStageFlags( vk::ShaderStageFlagBits::eCompute ),
        vk::DescriptorSetLayoutBinding{ }
            .setBinding( 1U )
            .setDescriptorType( vk::DescriptorType::eStorageBuffer )
            .setDescriptorCount( 1U )
            .setStageFlags( vk::ShaderStageFlagBits::eCompute ),
        vk::DescriptorSetLayoutBinding{ }
            .setBinding( 2U )
            .setDescriptorType( vk::DescriptorType::eUniformBuffer )
            .setDescriptorCount( 1U )
            .setStageFlags( vk::ShaderStageFlagBits::eCompute ),
    };

    LTB_CHECK( compute_.initialize( {
        .shader_module        = std::move( shader_module ),
        .descriptor_set_count = settings_.frame_count,
        .uniform_bindings     = std::move( uniform_bindings ),
    } ) );

    LTB_CHECK( cmd_and_sync_.initialize( {
        .frame_count  = settings_.frame_count,
        .image_count  = 0UZ,
        .command_pool = {
            .queue_type = vlk::QueueType::Compute,
        },
    } ) );

    auto const uniform_alignment
        = gpu_.physical_device( ).properties( ).limits.minUniformBufferOffsetAlignment;

    auto uniform_layout = vlk::MemoryLayout{ };
    vlk::append_memory_size_n(
        uniform_layout,
        { sizeof( FluidComputeUniforms ), uniform_alignment },
        settings_.frame_count
    );

    LTB_CHECK( gpu_uniforms_.initialize( {
        .layout       = std::move( uniform_layout ),
        .buffer_usage = vk::BufferUsageFlagBits::eUniformBuffer,
        .memory_properties
        = vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible,
        .store_mapped_value = true,
    } ) );

    initialized_ = true;

    LTB_CHECK( reset_positions( settings_.initial_positions ) );

    return utils::success( );
}

auto FluidPipeline1::is_initialized( ) const -> bool
{
    return initialized_;
}

auto FluidPipeline1::reset_positions( std::vector< float32 > const& positions )
    -> utils::Result< void >
{
    LTB_CHECK_VALID( this->is_initialized( ) );

    // A lazy way of synchronization. This function is not expected to be
    // called often so this slower form of synchronization is acceptable.
    VK_CHECK( settings_.compute_queue.waitIdle( ) );

    fluid_buffer_.reset( );

    auto const buffer_size = positions.size( ) * sizeof( positions.front( ) );

    auto const ssbo_alignment
        = gpu_.physical_device( ).properties( ).limits.minStorageBufferOffsetAlignment;

    auto fluid_layout = vlk::MemoryLayout{ };
    vlk::append_memory_size_n(
        fluid_layout,
        vk::MemoryRequirements{ buffer_size, ssbo_alignment },
        settings_.frame_count
    );

    LTB_CHECK( fluid_buffer_.initialize( {
        .layout       = std::move( fluid_layout ),
        .buffer_usage = vk::BufferUsageFlagBits::eStorageBuffer
                      | vk::BufferUsageFlagBits::eVertexBuffer
                      | vk::BufferUsageFlagBits::eTransferDst,
        .memory_properties  = vk::MemoryPropertyFlagBits::eDeviceLocal,
        .store_mapped_value = false,
    } ) );
    LTB_CHECK_VALID( buffer_size * settings_.frame_count <= fluid_buffer_.memory( ).size( ) );

    LTB_CHECK( copy_with_staging_buffer(
        gpu_,
        cmd_and_sync_,
        fluid_buffer_,
        positions,
        settings_.compute_queue
    ) );

    auto const& descriptor_sets = compute_.descriptor_sets( ).get( );
    for ( auto frame_index = 0U; frame_index < settings_.frame_count; ++frame_index )
    {
        auto const prev_frame_index
            = ( ( frame_index + settings_.frame_count ) - 1U ) % settings_.frame_count;

        LTB_CHECK_VALID( prev_frame_index < fluid_buffer_.layout( ).ranges.size( ) );
        LTB_CHECK_VALID( frame_index < fluid_buffer_.layout( ).ranges.size( ) );
        LTB_CHECK_VALID( frame_index < gpu_uniforms_.layout( ).ranges.size( ) );
        LTB_CHECK_VALID( frame_index < descriptor_sets.size( ) );

        auto const& prev_memory_range = fluid_buffer_.layout( ).ranges[ prev_frame_index ];
        auto const& curr_memory_range = fluid_buffer_.layout( ).ranges[ frame_index ];
        auto const& ubo_memory_range  = gpu_uniforms_.layout( ).ranges[ frame_index ];
        auto const& descriptor_set    = descriptor_sets[ frame_index ];

        auto const ssbo_prev_frame_info = vk::DescriptorBufferInfo{ }
                                              .setBuffer( fluid_buffer_.buffer( ).get( ) )
                                              .setOffset( prev_memory_range.offset )
                                              .setRange( prev_memory_range.size );

        auto const ssbo_curr_frame_info = vk::DescriptorBufferInfo{ }
                                              .setBuffer( fluid_buffer_.buffer( ).get( ) )
                                              .setOffset( curr_memory_range.offset )
                                              .setRange( curr_memory_range.size );

        auto const ubo_info = vk::DescriptorBufferInfo{ }
                                  .setBuffer( gpu_uniforms_.buffer( ).get( ) )
                                  .setOffset( ubo_memory_range.offset )
                                  .setRange( ubo_memory_range.size );

        auto const descriptor_writes = std::vector{
            vk::WriteDescriptorSet{ }
                .setDstSet( descriptor_set )
                .setDstBinding( 0U )
                .setDstArrayElement( 0U )
                .setDescriptorType( vk::DescriptorType::eStorageBuffer )
                .setBufferInfo( ssbo_prev_frame_info ),
            vk::WriteDescriptorSet{ }
                .setDstSet( descriptor_set )
                .setDstBinding( 1U )
                .setDstArrayElement( 0U )
                .setDescriptorType( vk::DescriptorType::eStorageBuffer )
                .setBufferInfo( ssbo_curr_frame_info ),
            vk::WriteDescriptorSet{ }
                .setDstSet( descriptor_set )
                .setDstBinding( 2U )
                .setDstArrayElement( 0U )
                .setDescriptorType( vk::DescriptorType::eUniformBuffer )
                .setBufferInfo( ubo_info ),
        };

        gpu_.device( ).get( ).updateDescriptorSets( descriptor_writes, { } );
    }

    cpu_uniforms_.fluid_count = static_cast< uint32 >( positions.size( ) );

    auto const groups       = cpu_uniforms_.fluid_count / workgroup_size_x;
    auto const remainder    = cpu_uniforms_.fluid_count % workgroup_size_x;
    dispatch_group_count_.x = groups + std::min( remainder, 1U );

    return utils::success( );
}

auto FluidPipeline1::set_uniforms( FluidComputeUniforms const uniforms ) -> void
{
    cpu_uniforms_ = uniforms;
    uniform_frames_updated_.clear( );
}

auto FluidPipeline1::dispatch( ) -> utils::Result< void >
{
    LTB_CHECK( auto const maybe_frame, cmd_and_sync_.start_frame( ) );

    if ( maybe_frame.has_value( ) )
    {
        auto const& frame = maybe_frame.value( );

        LTB_CHECK( this->update_uniform_buffer( frame ) );
        /// \todo: consider re-using command buffers if possible
        LTB_CHECK( this->record_compute_commands( frame ) );

        auto wait_until_signaled = std::vector< vlk::objs::SemaphoreAndStage >{ };

        if ( compute_finished_semaphore_.has_value( ) )
        {
            wait_until_signaled.push_back( {
                .semaphore = compute_finished_semaphore_.value( ),
                .stage     = vk::PipelineStageFlagBits::eComputeShader,
            } );
            compute_finished_semaphore_ = std::nullopt;
        }

        auto const& compute_finished_semaphore = cmd_and_sync_.get_frame_objects( ).frame_semaphore;

        LTB_CHECK( cmd_and_sync_.end_frame(
            frame,
            wait_until_signaled,
            { compute_finished_semaphore },
            settings_.compute_queue
        ) );

        compute_finished_semaphore_ = compute_finished_semaphore;
        cmd_and_sync_.increment_frame( );
    }

    return utils::success( );
}

auto FluidPipeline1::last_computed_frame_indices( ) const -> FrameIndices
{
    auto const last_frame = cmd_and_sync_.previous_frame( );

    return {
        .prev_frame_index = cmd_and_sync_.compute_previous_frame( last_frame ),
        .next_frame_index = last_frame,
    };
}

auto FluidPipeline1::get_semaphore( ) const -> std::optional< vk::Semaphore > const&
{
    return compute_finished_semaphore_;
}

auto FluidPipeline1::clear_semaphore( ) -> void
{
    compute_finished_semaphore_ = std::nullopt;
}

auto FluidPipeline1::fluid_buffer( ) const -> vlk::objs::VulkanBuffer const&
{
    return fluid_buffer_;
}

auto FluidPipeline1::uniforms( ) const -> FluidComputeUniforms const&
{
    return cpu_uniforms_;
}

auto FluidPipeline1::update_uniform_buffer( vlk::objs::FrameInfo const& frame )
    -> utils::Result< void >
{
    if ( uniform_frames_updated_.contains( frame.frame_index ) )
    {
        return utils::success( );
    }

    LTB_CHECK_VALID( frame.frame_index < gpu_uniforms_.layout( ).ranges.size( ) );
    auto const& memory_range = gpu_uniforms_.layout( ).ranges[ frame.frame_index ];

    auto* const dst_data = gpu_uniforms_.mapped_data( ) + memory_range.offset;
    LTB_CHECK_VALID( memory_range.size >= sizeof( cpu_uniforms_ ) );
    LTB_CHECK_VALID( std::memcpy( dst_data, &cpu_uniforms_, sizeof( cpu_uniforms_ ) ) == dst_data );

    uniform_frames_updated_.emplace( frame.frame_index );

    return utils::success( );
}

auto FluidPipeline1::record_compute_commands( vlk::objs::FrameInfo const& frame )
    -> utils::Result< void >
{
    VK_CHECK( frame.command_buffer.begin( vk::CommandBufferBeginInfo{ } ) );

    compute_.bind( frame.command_buffer );

    LTB_CHECK( compute_.bind_descriptor_sets( frame ) );

    frame.command_buffer.dispatch(
        // determined by size of fluid buffer and
        // workgroup size defined in compute shader
        dispatch_group_count_.x,
        dispatch_group_count_.y,
        dispatch_group_count_.z
    );

    VK_CHECK( frame.command_buffer.end( ) );

    return utils::success( );
}

} // namespace ltb::radio
