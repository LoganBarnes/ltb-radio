// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "receiver_controls_view.hpp"
#include "ltb/apps/radio/sub_app.hpp"
#include "ltb/radio/radio_engine.hpp"

// standard
#include <string>

namespace ltb::radio
{

struct ReceiverAppSettings
{
    std::string     name;
    std::string     view_prefix; // Prepended to view names so each app has its own layout.
    dsp::Modulation modulation;
    RadioSettings   radio;
    TuningOptions   tuning;
};

// A radio with a controls view and a plot view for each stage of its pipeline.
class ReceiverApp : public SubApp
{
public:
    ReceiverApp( SubAppContext const& context, ReceiverAppSettings settings );

    [[nodiscard( "Const getter" )]]
    auto name( ) const -> std::string const& override;

protected:
    [[nodiscard( "Getter" )]]
    auto engine( ) -> RadioEngine&;

private:
    std::string name_;

    // Views reference the engine, which stops its threads before the views are destroyed.
    RadioEngine engine_;
};

} // namespace ltb::radio
