// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "viewport_3d_view.hpp"

// project
#include "ltb/gui/imgui_utils.hpp"
#include "ltb/vlk/check.hpp"

// external
#include <imgui.h>
#include <spdlog/spdlog.h>

namespace ltb::radio
{
namespace
{

auto make_cube( ) -> ColoredMesh
{
    // Each face has its own vertices (so it can have its own color) and is wound counter-clockwise
    // when viewed from outside, matching the pipeline's front face.
    struct Face
    {
        glm::vec3 normal;
        glm::vec3 right;
        glm::vec3 up;
        glm::vec3 color;
    };

    constexpr auto faces = std::array{
        Face{ { 0, 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 }, { 0.9F, 0.2F, 0.2F } },
        Face{ { 0, 0, -1 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0.2F, 0.9F, 0.9F } },
        Face{ { 1, 0, 0 }, { 0, 0, -1 }, { 0, 1, 0 }, { 0.2F, 0.9F, 0.2F } },
        Face{ { -1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 }, { 0.9F, 0.2F, 0.9F } },
        Face{ { 0, 1, 0 }, { 1, 0, 0 }, { 0, 0, -1 }, { 0.2F, 0.2F, 0.9F } },
        Face{ { 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, 1 }, { 0.9F, 0.9F, 0.2F } },
    };

    auto mesh = ColoredMesh{ };
    for ( auto const& face : faces )
    {
        auto const first = static_cast< uint32 >( mesh.positions.size( ) );
        for ( auto const& [ x, y ] : std::array{
                  std::pair{ -1, -1 },
                  std::pair{ -1, 1 },
                  std::pair{ 1, 1 },
                  std::pair{ 1, -1 }
              } )
        {
            mesh.positions.push_back(
                0.5F
                * ( face.normal + static_cast< float32 >( x ) * face.right
                    + static_cast< float32 >( y ) * face.up )
            );
            mesh.colors.push_back( face.color );
        }
        mesh.indices.insert(
            mesh.indices.end( ),
            { first, first + 2U, first + 1U, first, first + 3U, first + 2U }
        );
    }
    return mesh;
}

} // namespace

auto Viewport3dView::name( ) const -> std::string const&
{
    static auto const view_name = std::string( "3D Viewport" );
    return view_name;
}

auto Viewport3dView::initialize( SubAppContext const& context ) -> utils::Result< void >
{
    viewport_ = std::make_unique< ImguiViewport >( context.gpu );
    LTB_CHECK( viewport_->initialize( { .frame_count = context.frame_count } ) );

    cube_ = std::make_unique< ColoredMeshPipeline >( context.gpu, viewport_->render_pass( ) );
    LTB_CHECK( cube_->initialize( { .frame_count = context.frame_count } ) );
    LTB_CHECK(
        cube_->initialize_mesh( make_cube( ), context.command_pool, context.graphics_queue )
    );

    return utils::success( );
}

auto Viewport3dView::configure_gui( uint32 const frame_index ) -> void
{
    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0.0F, 0.0F ) );
    auto const window_open = ImGui::Begin( "3D Viewport", &is_open );
    ImGui::PopStyleVar( );

    if ( window_open )
    {
        if ( auto result = viewport_->show( frame_index ); !result )
        {
            spdlog::error( "Viewport failed: {}", result.error( ).debug_error_message( ) );
        }

        this->handle_input( );

        auto const extent = viewport_->extent( );
        camera_.resize(
            { static_cast< float32 >( extent.width ), static_cast< float32 >( extent.height ) }
        );

        auto const eye = camera_.eye( );
        ImGui::SetCursorPos( ImVec2( 8.0F, 24.0F ) );
        gui::imgui_fmt_colored(
            ImVec4( 1.0F, 1.0F, 1.0F, 1.0F ),
            "{}x{} px\neye: {:.2f} {:.2f} {:.2f}",
            extent.width,
            extent.height,
            eye.x,
            eye.y,
            eye.z
        );
    }
    ImGui::End( );

    if ( auto result = cube_->set_camera( frame_index, camera_.render_params( ) ); !result )
    {
        spdlog::error( "Camera update failed: {}", result.error( ).debug_error_message( ) );
    }
}

auto Viewport3dView::record_commands( vlk::objs::FrameInfo const& frame ) -> utils::Result< void >
{
    LTB_CHECK( viewport_->begin_render_pass( frame ) );
    LTB_CHECK( cube_->draw( frame ) );
    viewport_->end_render_pass( frame );

    return utils::success( );
}

auto Viewport3dView::handle_input( ) -> void
{
    auto const& io = ImGui::GetIO( );

    if ( viewport_->is_active( ) && ImGui::IsMouseDragging( ImGuiMouseButton_Left, 0.0F ) )
    {
        camera_.orbit( { io.MouseDelta.x, io.MouseDelta.y } );
    }
    if ( viewport_->is_hovered( ) )
    {
        camera_.zoom( io.MouseWheel );
    }
}

} // namespace ltb::radio
