// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/utils/result.hpp"
#include "ltb/utils/types.hpp"
#include "ltb/vlk/command_pool.hpp"
#include "ltb/vlk/objs/frame_info.hpp"
#include "ltb/vlk/objs/vulkan_gpu.hpp"

// standard
#include <string>

namespace ltb::radio
{

// Vulkan objects owned by the main app that sub applications can use.
struct SubAppContext
{
    vlk::objs::VulkanGpu& gpu;
    vlk::CommandPool&     command_pool;
    vk::Queue             graphics_queue = nullptr;
    uint32                frame_count    = 0U;
};

// A sub application that is launched from the toolbar's "Apps" menu and draws its own
// (dockable) ImGui windows while open.
class SubApp
{
public:
    virtual ~SubApp( ) = default;

    [[nodiscard( "Const getter" )]]
    virtual auto name( ) const -> std::string const& = 0;

    // Called once, the first time the app is opened. A failure closes the app.
    virtual auto initialize( SubAppContext const& context ) -> utils::Result< void >
    {
        utils::ignore( context );
        return utils::success( );
    }

    // `frame_index` is the frame in flight that will be recorded after this GUI is built.
    virtual auto configure_gui( uint32 frame_index ) -> void = 0;

    // Called while the frame's command buffer is recording, before the main render pass begins.
    virtual auto record_commands( vlk::objs::FrameInfo const& frame ) -> utils::Result< void >
    {
        utils::ignore( frame );
        return utils::success( );
    }

    bool is_open        = false;
    bool is_initialized = false;
};

} // namespace ltb::radio
