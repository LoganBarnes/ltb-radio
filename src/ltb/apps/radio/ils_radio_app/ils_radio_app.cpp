// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "ils_radio_app.hpp"

// project
#include "ils_indicator_view.hpp"

namespace ltb::radio
{
namespace
{

// Cedar City (KCDC) ILS RWY 20, I-ECC. Charts list only the localizer; the glideslope is paired.
constexpr auto cedar_city_localizer_hz = 110.10e6;

constexpr auto lo_offset_hz = 250.0e3;

} // namespace

IlsRadioApp::IlsRadioApp( SubAppContext const& context )
    : ReceiverApp(
          context,
          {
              .name        = "ILS",
              .view_prefix = "ILS",
              .modulation  = dsp::Modulation::Ils,
              .radio = {
                  .center_frequency_hz  = cedar_city_localizer_hz - lo_offset_hz,
                  .station_frequency_hz = cedar_city_localizer_hz,
                  .channel_bandwidth_hz = 15.0e3,
              },
              .tuning = {
                  .display_scale_hz = 1.0e6,
                  .display_format   = "%.2f MHz",
                  .step_hz          = 50.0e3,
                  .lo_offset_hz     = lo_offset_hz,
                  .presets          = {
                      { .label = "KCDC 20 (I-ECC) 110.10", .frequency_hz = cedar_city_localizer_hz },
                  },
              },
          }
      )
{
    auto indicator
        = std::make_unique< IlsIndicatorView >( this->engine( ), "ILS Indicator", lo_offset_hz );
    indicator->is_open = true;
    this->add_view( std::move( indicator ) );
}

} // namespace ltb::radio
