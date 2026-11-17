// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "am_radio_app.hpp"

namespace ltb::radio
{
namespace
{

// Cedar City Regional (KCDC). The ASOS weather broadcast is continuous; the others are only
// active while someone transmits, so they're best heard with the squelch on.
constexpr auto asos_hz   = 119.025e6;
constexpr auto ctaf_hz   = 123.0e6;
constexpr auto center_hz = 125.575e6; // Salt Lake Center (approach/departure)

constexpr auto lo_offset_hz = 250.0e3;

} // namespace

AmRadioApp::AmRadioApp( SubAppContext const& context )
    : ReceiverApp(
          context,
          {
              .name        = "AM Radio",
              .view_prefix = "AM",
              .modulation  = dsp::Modulation::Am,
              .radio = {
                  .center_frequency_hz  = asos_hz - lo_offset_hz,
                  .station_frequency_hz = asos_hz,
                  .channel_bandwidth_hz = 8.0e3,
                  .agc_enabled          = true,
              },
              .tuning = {
                  .display_scale_hz = 1.0e6,
                  .display_format   = "%.3f MHz",
                  .step_hz          = 25.0e3,
                  .lo_offset_hz     = lo_offset_hz,
                  .presets          = {
                      { .label = "ASOS", .frequency_hz = asos_hz },
                      { .label = "CTAF", .frequency_hz = ctaf_hz },
                      { .label = "ZLC", .frequency_hz = center_hz },
                  },
              },
          }
      )
{
}

} // namespace ltb::radio
