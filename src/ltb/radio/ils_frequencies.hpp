// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/utils/types.hpp"

// standard
#include <optional>

namespace ltb::radio
{

// Charts list only an ILS's localizer frequency; receivers tune the glideslope it is paired with
// (FAA AIM table 1-1-4, ICAO Annex 10). These return std::nullopt for non-ILS frequencies.
[[nodiscard( "Pure computation" )]]
auto paired_glideslope_hz( float64 localizer_hz ) -> std::optional< float64 >;

[[nodiscard( "Pure computation" )]]
auto paired_localizer_hz( float64 glideslope_hz ) -> std::optional< float64 >;

} // namespace ltb::radio
