// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/apps/radio/app_view.hpp"
#include "ltb/radio/radio_engine.hpp"

// standard
#include <string>

namespace ltb::radio
{

// A cockpit-style VOR head: compass card turned to the selected course (OBS), course deviation
// needle (full scale is 10 degrees), TO/FROM flag and a pointer to the station.
//
// Drag the card or scroll over it to change the course.
class VorIndicatorView : public AppView
{
public:
    VorIndicatorView( RadioEngine& engine, std::string name );

    [[nodiscard( "Const getter" )]]
    auto name( ) const -> std::string const& override;

    auto configure_gui( uint32 frame_index ) -> void override;

private:
    RadioEngine& engine_;
    std::string  name_;

    float32 course_deg_ = 0.0F;

    auto gui_readout( dsp::VorReading const& reading, bool valid ) -> void;
    auto gui_instrument( dsp::VorReading const& reading, bool valid ) -> void;
};

} // namespace ltb::radio
