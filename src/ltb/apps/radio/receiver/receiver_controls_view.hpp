// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/apps/radio/app_view.hpp"
#include "ltb/radio/hackrf_info.hpp"
#include "ltb/radio/radio_engine.hpp"

// standard
#include <string>
#include <vector>

namespace ltb::radio
{

// A station that can be tuned with one click.
struct StationPreset
{
    std::string label;
    float64     frequency_hz = 0.0;
};

struct TuningOptions
{
    // Shown in the frequency inputs (e.g. 1e3 for kHz).
    float64     display_scale_hz = 1.0e3;
    char const* display_format   = "%.1f kHz";
    float64     step_hz          = 10.0e3;

    // The receiver is tuned this far below the station to keep the HackRF's DC spike (at the
    // center frequency) away from the signal.
    float64 lo_offset_hz = 250.0e3;

    std::vector< StationPreset > presets = { };
};

// Starts/stops a radio and edits its settings. Also toggles the pipeline stage views.
class ReceiverControlsView : public AppView
{
public:
    ReceiverControlsView(
        RadioEngine&            engine,
        std::string             name,
        TuningOptions           tuning,
        std::vector< AppView* > stage_views
    );

    [[nodiscard( "Const getter" )]]
    auto name( ) const -> std::string const& override;

    auto configure_gui( uint32 frame_index ) -> void override;

private:
    RadioEngine&            engine_;
    std::string             name_;
    TuningOptions           tuning_;
    std::vector< AppView* > stage_views_;

    std::vector< HackrfListing > devices_       = { };
    bool                         needs_refresh_ = true;
    std::string                  error_message_ = { };

    auto gui_run_controls( ) -> void;
    auto gui_source( RadioSettings& settings ) -> void;
    auto gui_tuning( RadioSettings& settings ) -> void;
    auto gui_gain( RadioSettings& settings ) -> void;
    auto gui_demodulation( RadioSettings& settings ) -> void;
    auto gui_audio( RadioSettings& settings ) -> void;
    auto gui_status( ) -> void;
    auto gui_stage_views( ) -> void;

    // Tunes to the station, moving the receiver if the station is outside the received band.
    auto tune( RadioSettings& settings, float64 station_hz ) const -> void;

    auto refresh_devices( ) -> void;
    auto report( utils::Result< void > const& result ) -> void;
};

} // namespace ltb::radio
