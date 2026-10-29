// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/utils/types.hpp"
#include "ltb/vlk/command_pool.hpp"
#include "ltb/vlk/objs/vulkan_gpu.hpp"

// external
#include <vulkan/vulkan.hpp>

namespace ltb::radio
{

// Vulkan objects owned by the main app that sub applications and their views can use.
struct SubAppContext
{
    vlk::objs::VulkanGpu& gpu;
    vlk::CommandPool&     command_pool;
    vk::Queue             graphics_queue = nullptr;
    uint32                frame_count    = 0U;
};

} // namespace ltb::radio
