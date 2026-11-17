// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/apps/radio/app_view.hpp"
#include "ltb/radio/radio_engine.hpp"

// standard
#include <chrono>
#include <optional>
#include <string>

namespace ltb::radio
{

// An ILS course deviation indicator: the localizer needle (vertical) and glideslope needle
// (horizontal), plus bars comparing the 90 Hz and 150 Hz tone depths.
//
// The localizer and glideslope are too far apart to receive at once, so the view switches
// between them and keeps each needle's last reading for a few seconds.
class IlsIndicatorView : public AppView
{
public:
    IlsIndicatorView( RadioEngine& engine, std::string name, float64 lo_offset_hz );

    [[nodiscard( "Const getter" )]]
    auto name( ) const -> std::string const& override;

    auto configure_gui( uint32 frame_index ) -> void override;

private:
    using Clock = std::chrono::steady_clock;

    struct Needle
    {
        dsp::IlsReading                    reading = { };
        std::optional< Clock::time_point > updated = std::nullopt;

        [[nodiscard( "Const computation" )]]
        auto is_fresh( Clock::time_point now ) const -> bool;
    };

    RadioEngine& engine_;
    std::string  name_;
    float64      lo_offset_hz_;

    Needle localizer_  = { };
    Needle glideslope_ = { };

    auto update_needles( ) -> void;
    auto gui_tuning( ) -> void;
    auto gui_readouts( ) -> void;
    auto gui_instrument( ) const -> void;
    auto tune( float64 frequency_hz ) -> void;
};

} // namespace ltb::radio
