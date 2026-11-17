// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/apps/radio/receiver/receiver_app.hpp"

namespace ltb::radio
{

// Amplitude modulation, defaulting to the aviation band (118-137 MHz). Also used by CB radio and
// broadcast AM (530-1700 kHz, which is below the HackRF's rated range).
class AmRadioApp : public ReceiverApp
{
public:
    explicit AmRadioApp( SubAppContext const& context );
};

} // namespace ltb::radio
