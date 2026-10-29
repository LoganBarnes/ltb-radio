// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// standard
#include <map>
#include <string>

namespace ltb::radio
{

// View name -> whether the view is open.
using ViewStates = std::map< std::string, bool >;

// Sub app name -> its view states.
using AppViewStates = std::map< std::string, ViewStates >;

// Saves and loads `states` in the ImGui ini file, next to the window layout.
// Call after the ImGui context is created and before the first frame (when the ini is loaded).
// `states` must outlive the ImGui context.
auto register_view_settings_handler( AppViewStates& states ) -> void;

} // namespace ltb::radio
