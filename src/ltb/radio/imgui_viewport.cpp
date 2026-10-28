// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "imgui_viewport.hpp"

// project
#include "ltb/vlk/check.hpp"

// external
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <spdlog/spdlog.h>

// standard
#include <algorithm>
#include <cmath>

namespace ltb::radio
{

namespace
{

auto to_extent( ImVec2 const size, ImVec2 const scale ) -> vk::Extent2D
{
    auto const to_pixels = []( float32 const logical_size, float32 const scale_factor ) {
        return static_cast< uint32 >( std::max( 1.0F, std::floor( logical_size * scale_factor ) ) );
    };

    return { to_pixels( size.x, scale.x ), to_pixels( size.y, scale.y ) };
}

} // namespace

ImguiViewport::Target::Target( vlk::objs::VulkanGpu& gpu, vlk::RenderPass& render_pass )
    : color_image( gpu )
    , color_view( gpu.device( ) )
    , depth_image( gpu )
    , depth_view( gpu.device( ) )
    , framebuffer( gpu.device( ), render_pass )
{
}

ImguiViewport::ImguiViewport( vlk::objs::VulkanGpu& gpu )
    : gpu_( gpu )
{
}

ImguiViewport::~ImguiViewport( )
{
    this->destroy_targets( );
}

auto ImguiViewport::initialize( ImguiViewportSettings settings ) -> utils::Result< void >
{
    if ( this->is_initialized( ) )
    {
        return utils::success( );
    }
    LTB_CHECK_VALID( gpu_.is_initialized( ) );
    LTB_CHECK_VALID( settings.frame_count > 0U );
    LTB_CHECK_VALID( settings.initial_size.width > 0U && settings.initial_size.height > 0U );

    settings_ = std::move( settings );

    LTB_CHECK( this->initialize_render_pass( ) );

    auto const sampler_info = vk::SamplerCreateInfo{ }
                                  .setMagFilter( vk::Filter::eLinear )
                                  .setMinFilter( vk::Filter::eLinear )
                                  .setAddressModeU( vk::SamplerAddressMode::eClampToEdge )
                                  .setAddressModeV( vk::SamplerAddressMode::eClampToEdge )
                                  .setAddressModeW( vk::SamplerAddressMode::eClampToEdge );
    VK_CHECK( sampler_, gpu_.device( ).get( ).createSamplerUnique( sampler_info ) );

    LTB_CHECK( this->create_targets( settings_.initial_size ) );

    initialized_ = true;

    return utils::success( );
}

auto ImguiViewport::is_initialized( ) const -> bool
{
    return initialized_;
}

auto ImguiViewport::show( uint32 const frame_index ) -> utils::Result< void >
{
    LTB_CHECK_VALID( this->is_initialized( ) );
    LTB_CHECK_VALID( frame_index < targets_.size( ) );

    hovered_ = false;
    active_  = false;

    auto const available = ImGui::GetContentRegionAvail( );
    if ( available.x < 1.0F || available.y < 1.0F )
    {
        return utils::success( );
    }

    if ( auto const wanted = to_extent( available, ImGui::GetIO( ).DisplayFramebufferScale );
         wanted != extent_ )
    {
        LTB_CHECK( this->resize( wanted ) );
    }

    auto const cursor_pos = ImGui::GetCursorScreenPos( );

    ImGui::Image(
        ImTextureRef(
            reinterpret_cast< ImTextureID >(
                static_cast< VkDescriptorSet >( targets_[ frame_index ]->texture )
            )
        ),
        available
    );

    // The invisible button captures the mouse so dragging in the viewport doesn't move the window.
    ImGui::SetCursorScreenPos( cursor_pos );
    ImGui::InvisibleButton(
        "##viewport",
        available,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight
            | ImGuiButtonFlags_MouseButtonMiddle
    );
    hovered_ = ImGui::IsItemHovered( );
    active_  = ImGui::IsItemActive( );

    return utils::success( );
}

auto ImguiViewport::begin_render_pass( vlk::objs::FrameInfo const& frame ) -> utils::Result< void >
{
    LTB_CHECK_VALID( this->is_initialized( ) );
    LTB_CHECK_VALID( frame.frame_index < targets_.size( ) );

    auto const& target = *targets_[ frame.frame_index ];

    auto const clear_values = std::array{
        vk::ClearValue{ }.setColor( {
            settings_.clear_color.r,
            settings_.clear_color.g,
            settings_.clear_color.b,
            settings_.clear_color.a,
        } ),
        vk::ClearValue{ }.setDepthStencil( { 1.0F, 0U } ),
    };

    frame.command_buffer.beginRenderPass(
        vk::RenderPassBeginInfo{ }
            .setRenderPass( render_pass_.get( ) )
            .setFramebuffer( target.framebuffer.get( ) )
            .setRenderArea( vk::Rect2D{ }.setExtent( extent_ ) )
            .setClearValues( clear_values ),
        vk::SubpassContents::eInline
    );

    frame.command_buffer.setViewport(
        0U,
        vk::Viewport{ }
            .setWidth( static_cast< float32 >( extent_.width ) )
            .setHeight( static_cast< float32 >( extent_.height ) )
            .setMinDepth( 0.0F )
            .setMaxDepth( 1.0F )
    );
    frame.command_buffer.setScissor( 0U, vk::Rect2D{ }.setExtent( extent_ ) );

    return utils::success( );
}

auto ImguiViewport::end_render_pass( vlk::objs::FrameInfo const& frame ) -> void
{
    frame.command_buffer.endRenderPass( );
}

auto ImguiViewport::extent( ) const -> vk::Extent2D
{
    return extent_;
}

auto ImguiViewport::is_hovered( ) const -> bool
{
    return hovered_;
}

auto ImguiViewport::is_active( ) const -> bool
{
    return active_;
}

auto ImguiViewport::render_pass( ) -> vlk::RenderPass&
{
    return render_pass_;
}

auto ImguiViewport::initialize_render_pass( ) -> utils::Result< void >
{
    auto render_pass_settings
        = vlk::render_pass_settings_3d( settings_.color_format, settings_.depth_format );

    // The color target is sampled by ImGui instead of being presented.
    render_pass_settings.attachments.front( ).setFinalLayout(
        vk::ImageLayout::eShaderReadOnlyOptimal
    );

    // Make the color writes visible to ImGui's fragment shader.
    render_pass_settings.dependencies.push_back(
        vk::SubpassDependency{ }
            .setSrcSubpass( 0U )
            .setDstSubpass( VK_SUBPASS_EXTERNAL )
            .setSrcStageMask( vk::PipelineStageFlagBits::eColorAttachmentOutput )
            .setSrcAccessMask( vk::AccessFlagBits::eColorAttachmentWrite )
            .setDstStageMask( vk::PipelineStageFlagBits::eFragmentShader )
            .setDstAccessMask( vk::AccessFlagBits::eShaderRead )
    );

    return render_pass_.initialize( std::move( render_pass_settings ) );
}

auto ImguiViewport::create_targets( vk::Extent2D const extent ) -> utils::Result< void >
{
    targets_.reserve( settings_.frame_count );

    for ( auto i = 0U; i < settings_.frame_count; ++i )
    {
        auto& target = *targets_.emplace_back( std::make_unique< Target >( gpu_, render_pass_ ) );

        LTB_CHECK( target.color_image.initialize( {
            .image = {
                .extent = { extent.width, extent.height, 1U },
                .format = settings_.color_format,
                .usage  = vk::ImageUsageFlagBits::eColorAttachment
                        | vk::ImageUsageFlagBits::eSampled,
            },
            .memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
        } ) );
        LTB_CHECK( target.color_view.initialize( {
            .image  = target.color_image.image( ).get( ),
            .format = settings_.color_format,
        } ) );

        LTB_CHECK( target.depth_image.initialize( {
            .image = {
                .extent = { extent.width, extent.height, 1U },
                .format = settings_.depth_format,
                .usage  = vk::ImageUsageFlagBits::eDepthStencilAttachment,
            },
            .memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
        } ) );
        LTB_CHECK( target.depth_view.initialize( {
            .image       = target.depth_image.image( ).get( ),
            .format      = settings_.depth_format,
            .aspect_mask = vk::ImageAspectFlagBits::eDepth,
        } ) );

        LTB_CHECK( target.framebuffer.initialize( {
            .attachments = { target.color_view.get( ), target.depth_view.get( ) },
            .extent      = extent,
            .layers      = 1U,
        } ) );

        target.texture = ImGui_ImplVulkan_AddTexture(
            sampler_.get( ),
            target.color_view.get( ),
            static_cast< VkImageLayout >( vk::ImageLayout::eShaderReadOnlyOptimal )
        );
        LTB_CHECK_VALID( target.texture );
    }

    extent_ = extent;

    return utils::success( );
}

auto ImguiViewport::destroy_targets( ) -> void
{
    for ( auto const& target : targets_ )
    {
        if ( target->texture )
        {
            ImGui_ImplVulkan_RemoveTexture( target->texture );
        }
    }
    targets_.clear( );
}

auto ImguiViewport::resize( vk::Extent2D const extent ) -> utils::Result< void >
{
    spdlog::debug( "Resizing viewport to {}x{}", extent.width, extent.height );

    // Previous frames may still be using the old targets.
    VK_CHECK( gpu_.device( ).get( ).waitIdle( ) );

    this->destroy_targets( );
    return this->create_targets( extent );
}

} // namespace ltb::radio
