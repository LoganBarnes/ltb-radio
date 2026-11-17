// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/apps/radio/receiver/receiver_app.hpp"

namespace ltb::radio
{

// Wideband (broadcast) FM, 88-108 MHz. Mono.
class FmRadioApp : public ReceiverApp
{
public:
    explicit FmRadioApp( SubAppContext const& context );
};

} // namespace ltb::radio
