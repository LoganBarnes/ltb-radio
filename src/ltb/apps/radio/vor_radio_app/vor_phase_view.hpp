// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/apps/radio/app_view.hpp"
#include "ltb/radio/radio_engine.hpp"

// standard
#include <string>
#include <vector>

namespace ltb::radio
{

// Overlays the variable and reference 30 Hz tones (each scaled to the same height) to show how
// the radial is measured: the variable tone lags the reference by the radial. Like an
// oscilloscope trigger, the view stays locked to a peak of the reference tone.
class VorPhaseView : public AppView
{
public:
    VorPhaseView( RadioEngine& engine, std::string name );

    [[nodiscard( "Const getter" )]]
    auto name( ) const -> std::string const& override;

    auto configure_gui( uint32 frame_index ) -> void override;

private:
    RadioEngine& engine_;
    std::string  name_;

    // The reference tone is drawn this much earlier to undo its longer filter delay.
    float64 reference_delay_ms_;

    float32 window_ms_ = 100.0F;
    bool    frozen_    = false;

    std::vector< dsp::Complex > variable_        = { };
    std::vector< dsp::Complex > reference_       = { };
    uint64                      variable_total_  = 0U;
    uint64                      reference_total_ = 0U;
    dsp::VorReading             reading_         = { };
    bool                        reading_valid_   = false;

    auto update_data( ) -> void;
    auto draw_plot( ) const -> void;
};

} // namespace ltb::radio
