// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/utils/result.hpp"

// standard
#include <filesystem>

namespace ltb::radio
{

// The per-user directory for persistent app settings (e.g. the ImGui window layout).
// The directory is created if it doesn't exist.
[[nodiscard( "Finding the directory can fail" )]]
auto user_config_dir( ) -> utils::Result< std::filesystem::path >;

} // namespace ltb::radio
