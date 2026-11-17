// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "fm_radio_app.hpp"

namespace ltb::radio
{
namespace
{

// Stations received in Michael Ossmann's "Software Defined Radio with HackRF" lesson 1.
constexpr auto default_station_hz = 96.5e6;
constexpr auto other_station_hz   = 97.9e6;
constexpr auto lo_offset_hz       = 300.0e3;

} // namespace

FmRadioApp::FmRadioApp( SubAppContext const& context )
    : ReceiverApp(
          context,
          {
              .name        = "FM Radio",
              .view_prefix = "FM",
              .modulation  = dsp::Modulation::Fm,
              .radio = {
                  .center_frequency_hz  = default_station_hz - lo_offset_hz,
                  .station_frequency_hz = default_station_hz,
                  .channel_bandwidth_hz = 180.0e3,
                  .deemphasis_tau_s     = 75.0e-6,
              },
              .tuning = {
                  .display_scale_hz = 1.0e6,
                  .display_format   = "%.2f MHz",
                  .step_hz          = 100.0e3,
                  .lo_offset_hz     = lo_offset_hz,
                  .presets          = {
                      { .label = "96.5", .frequency_hz = default_station_hz },
                      { .label = "97.9", .frequency_hz = other_station_hz },
                  },
              },
          }
      )
{
}

} // namespace ltb::radio
