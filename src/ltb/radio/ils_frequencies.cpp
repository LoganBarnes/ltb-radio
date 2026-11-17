// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "ils_frequencies.hpp"

// standard
#include <algorithm>
#include <array>
#include <cmath>

namespace ltb::radio
{
namespace
{

struct IlsPair
{
    int64 localizer_khz;
    int64 glideslope_khz;
};

// Checked entry by entry against FAA AIM table 1-1-4.
constexpr auto ils_pairs = std::array{
    IlsPair{ 108'100, 334'700 }, IlsPair{ 108'150, 334'550 }, IlsPair{ 108'300, 334'100 },
    IlsPair{ 108'350, 333'950 }, IlsPair{ 108'500, 329'900 }, IlsPair{ 108'550, 329'750 },
    IlsPair{ 108'700, 330'500 }, IlsPair{ 108'750, 330'350 }, IlsPair{ 108'900, 329'300 },
    IlsPair{ 108'950, 329'150 }, IlsPair{ 109'100, 331'400 }, IlsPair{ 109'150, 331'250 },
    IlsPair{ 109'300, 332'000 }, IlsPair{ 109'350, 331'850 }, IlsPair{ 109'500, 332'600 },
    IlsPair{ 109'550, 332'450 }, IlsPair{ 109'700, 333'200 }, IlsPair{ 109'750, 333'050 },
    IlsPair{ 109'900, 333'800 }, IlsPair{ 109'950, 333'650 }, IlsPair{ 110'100, 334'400 },
    IlsPair{ 110'150, 334'250 }, IlsPair{ 110'300, 335'000 }, IlsPair{ 110'350, 334'850 },
    IlsPair{ 110'500, 329'600 }, IlsPair{ 110'550, 329'450 }, IlsPair{ 110'700, 330'200 },
    IlsPair{ 110'750, 330'050 }, IlsPair{ 110'900, 330'800 }, IlsPair{ 110'950, 330'650 },
    IlsPair{ 111'100, 331'700 }, IlsPair{ 111'150, 331'550 }, IlsPair{ 111'300, 332'300 },
    IlsPair{ 111'350, 332'150 }, IlsPair{ 111'500, 332'900 }, IlsPair{ 111'550, 332'750 },
    IlsPair{ 111'700, 333'500 }, IlsPair{ 111'750, 333'350 }, IlsPair{ 111'900, 331'100 },
    IlsPair{ 111'950, 330'950 },
};

auto to_khz( float64 const hz ) -> int64
{
    return static_cast< int64 >( std::llround( hz / 1.0e3 ) );
}

auto to_hz( int64 const khz ) -> float64
{
    return static_cast< float64 >( khz ) * 1.0e3;
}

} // namespace

auto paired_glideslope_hz( float64 const localizer_hz ) -> std::optional< float64 >
{
    auto const pair
        = std::ranges::find( ils_pairs, to_khz( localizer_hz ), &IlsPair::localizer_khz );
    return pair == ils_pairs.end( ) ? std::nullopt : std::optional( to_hz( pair->glideslope_khz ) );
}

auto paired_localizer_hz( float64 const glideslope_hz ) -> std::optional< float64 >
{
    auto const pair
        = std::ranges::find( ils_pairs, to_khz( glideslope_hz ), &IlsPair::glideslope_khz );
    return pair == ils_pairs.end( ) ? std::nullopt : std::optional( to_hz( pair->localizer_khz ) );
}

} // namespace ltb::radio
