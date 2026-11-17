// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/apps/radio/receiver/receiver_app.hpp"

namespace ltb::radio
{

// Instrument landing system receiver: the localizer (108.10-111.95 MHz) and its paired
// glideslope (329.15-335.00 MHz), shown on a course deviation indicator.
class IlsRadioApp : public ReceiverApp
{
public:
    explicit IlsRadioApp( SubAppContext const& context );
};

} // namespace ltb::radio
