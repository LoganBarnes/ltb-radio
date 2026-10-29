// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "viewport_2d_view.hpp"

// project
#include "ltb/gui/imgui_utils.hpp"
#include "ltb/vlk/check.hpp"

// external
#include <imgui.h>
#include <spdlog/spdlog.h>

// standard
#include <cmath>
#include <numbers>

namespace ltb::radio
{
namespace
{

constexpr auto grid_half_size   = 20;
constexpr auto waveform_samples = 400;
constexpr auto grid_color       = glm::vec3( 0.25F );
constexpr auto axis_color       = glm::vec3( 0.6F );
constexpr auto waveform_color   = glm::vec3( 0.2F, 1.0F, 0.4F );

auto add_line( ColoredMesh& mesh, glm::vec3 const a, glm::vec3 const b, glm::vec3 const color )
    -> void
{
    auto const first = static_cast< uint32 >( mesh.positions.size( ) );
    mesh.positions.insert( mesh.positions.end( ), { a, b } );
    mesh.colors.insert( mesh.colors.end( ), { color, color } );
    mesh.indices.insert( mesh.indices.end( ), { first, first + 1U } );
}

auto make_grid( ) -> ColoredMesh
{
    auto const extent = static_cast< float32 >( grid_half_size );

    auto mesh = ColoredMesh{ };
    for ( auto i = -grid_half_size; i <= grid_half_size; ++i )
    {
        auto const v     = static_cast< float32 >( i );
        auto const color = i == 0 ? axis_color : grid_color;
        auto const z     = i == 0 ? 0.1F : 0.0F;

        add_line( mesh, { v, -extent, z }, { v, extent, z }, color );
        add_line( mesh, { -extent, v, z }, { extent, v, z }, color );
    }
    return mesh;
}

auto make_waveform( ) -> ColoredMesh
{
    auto mesh = ColoredMesh{ };

    auto const point = []( int const i ) {
        auto const t     = static_cast< float32 >( i ) / static_cast< float32 >( waveform_samples );
        auto const x     = ( t - 0.5F ) * 2.0F * static_cast< float32 >( grid_half_size );
        auto const phase = 8.0F * std::numbers::pi_v< float32 > * t;
        return glm::vec3( x, 3.0F * std::sin( phase ) * std::cos( 0.5F * phase ), 0.2F );
    };

    for ( auto i = 0; i < waveform_samples; ++i )
    {
        add_line( mesh, point( i ), point( i + 1 ), waveform_color );
    }
    return mesh;
}

// Counter-clockwise quads (as seen with +Y up) matching the pipeline's front face.
auto make_quads( ) -> ColoredMesh
{
    struct Quad
    {
        glm::vec2 min;
        glm::vec2 max;
        glm::vec3 color;
    };

    constexpr auto quads = std::array{
        Quad{ { -6.0F, -6.0F }, { -3.0F, -3.0F }, { 0.9F, 0.2F, 0.2F } },
        Quad{ { 2.0F, -8.0F }, { 8.0F, -4.0F }, { 0.2F, 0.4F, 0.9F } },
        Quad{ { 4.0F, 4.0F }, { 5.0F, 9.0F }, { 0.9F, 0.8F, 0.2F } },
    };

    auto mesh = ColoredMesh{ };
    for ( auto const& quad : quads )
    {
        auto const first = static_cast< uint32 >( mesh.positions.size( ) );
        mesh.positions.insert(
            mesh.positions.end( ),
            {
                { quad.min.x, quad.min.y, 0.05F },
                { quad.max.x, quad.min.y, 0.05F },
                { quad.max.x, quad.max.y, 0.05F },
                { quad.min.x, quad.max.y, 0.05F },
            }
        );
        mesh.colors.insert( mesh.colors.end( ), 4U, quad.color );
        mesh.indices.insert(
            mesh.indices.end( ),
            { first, first + 1U, first + 2U, first, first + 2U, first + 3U }
        );
    }
    return mesh;
}

} // namespace

auto Viewport2dView::name( ) const -> std::string const&
{
    static auto const view_name = std::string( "2D Viewport" );
    return view_name;
}

auto Viewport2dView::initialize( SubAppContext const& context ) -> utils::Result< void >
{
    viewport_ = std::make_unique< ImguiViewport >( context.gpu );
    LTB_CHECK( viewport_->initialize( { .frame_count = context.frame_count } ) );

    triangles_ = std::make_unique< ColoredMeshPipeline >( context.gpu, viewport_->render_pass( ) );
    LTB_CHECK( triangles_->initialize( { .frame_count = context.frame_count } ) );
    LTB_CHECK(
        triangles_->initialize_mesh( make_quads( ), context.command_pool, context.graphics_queue )
    );

    lines_ = std::make_unique< ColoredMeshPipeline >( context.gpu, viewport_->render_pass( ) );
    LTB_CHECK( lines_->initialize( {
        .frame_count = context.frame_count,
        .topology    = vk::PrimitiveTopology::eLineList,
    } ) );
    LTB_CHECK(
        lines_->initialize_mesh( make_grid( ), context.command_pool, context.graphics_queue )
    );
    LTB_CHECK(
        lines_->initialize_mesh( make_waveform( ), context.command_pool, context.graphics_queue )
    );

    return utils::success( );
}

auto Viewport2dView::configure_gui( uint32 const frame_index ) -> void
{
    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0.0F, 0.0F ) );
    auto const window_open = ImGui::Begin( "2D Viewport", &is_open );
    ImGui::PopStyleVar( );

    if ( window_open )
    {
        if ( auto result = viewport_->show( frame_index ); !result )
        {
            spdlog::error( "Viewport failed: {}", result.error( ).debug_error_message( ) );
        }

        this->handle_input( );
        camera_.resize( viewport_->logical_size( ) );

        auto const extent = viewport_->extent( );
        auto const center = camera_.center( );
        ImGui::SetCursorPos( ImVec2( 8.0F, 24.0F ) );
        gui::imgui_fmt_colored(
            ImVec4( 1.0F, 1.0F, 1.0F, 1.0F ),
            "{}x{} px\ncenter: {:.2f} {:.2f}\nzoom: {:.1f} px/unit",
            extent.width,
            extent.height,
            center.x,
            center.y,
            camera_.pixels_per_unit( )
        );
    }
    ImGui::End( );

    auto const params = camera_.render_params( );
    for ( auto* const pipeline : { triangles_.get( ), lines_.get( ) } )
    {
        if ( auto result = pipeline->set_camera( frame_index, params ); !result )
        {
            spdlog::error( "Camera update failed: {}", result.error( ).debug_error_message( ) );
        }
    }
}

auto Viewport2dView::record_commands( vlk::objs::FrameInfo const& frame ) -> utils::Result< void >
{
    LTB_CHECK( viewport_->begin_render_pass( frame ) );
    LTB_CHECK( triangles_->draw( frame ) );
    LTB_CHECK( lines_->draw( frame ) );
    viewport_->end_render_pass( frame );

    return utils::success( );
}

auto Viewport2dView::handle_input( ) -> void
{
    auto const& io = ImGui::GetIO( );

    if ( viewport_->is_active( )
         && ( ImGui::IsMouseDragging( ImGuiMouseButton_Left, 0.0F )
              || ImGui::IsMouseDragging( ImGuiMouseButton_Middle, 0.0F ) ) )
    {
        camera_.pan( { io.MouseDelta.x, io.MouseDelta.y } );
    }
    if ( viewport_->is_hovered( ) && io.MouseWheel != 0.0F )
    {
        auto const cursor
            = glm::vec2( io.MousePos.x, io.MousePos.y ) - viewport_->logical_origin( );
        camera_.zoom( io.MouseWheel, cursor );
    }
}

} // namespace ltb::radio
