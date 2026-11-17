// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "vor_radio_app.hpp"

// project
#include "vor_indicator_view.hpp"
#include "vor_phase_view.hpp"

namespace ltb::radio
{
namespace
{

// Southern Utah VORs.
constexpr auto cedar_city_hz = 117.30e6;
constexpr auto milford_hz    = 112.10e6;
constexpr auto st_george_hz  = 108.60e6;

constexpr auto lo_offset_hz = 250.0e3;

} // namespace

VorRadioApp::VorRadioApp( SubAppContext const& context )
    : ReceiverApp(
          context,
          {
              .name        = "VOR",
              .view_prefix = "VOR",
              .modulation  = dsp::Modulation::Vor,
              .radio = {
                  .center_frequency_hz  = cedar_city_hz - lo_offset_hz,
                  .station_frequency_hz = cedar_city_hz,
                  .channel_bandwidth_hz = 25.0e3,
              },
              .tuning = {
                  .display_scale_hz = 1.0e6,
                  .display_format   = "%.2f MHz",
                  .step_hz          = 50.0e3,
                  .lo_offset_hz     = lo_offset_hz,
                  .presets          = {
                      { .label = "CDC 117.30", .frequency_hz = cedar_city_hz },
                      { .label = "MLF 112.10", .frequency_hz = milford_hz },
                      { .label = "UTI 108.60", .frequency_hz = st_george_hz },
                  },
              },
          }
      )
{
    auto indicator     = std::make_unique< VorIndicatorView >( this->engine( ), "VOR Indicator" );
    indicator->is_open = true;
    this->add_view( std::move( indicator ) );

    auto phase     = std::make_unique< VorPhaseView >( this->engine( ), "VOR 30 Hz Comparison" );
    phase->is_open = true;
    this->add_view( std::move( phase ) );
}

} // namespace ltb::radio
