// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/apps/radio/receiver/receiver_app.hpp"

namespace ltb::radio
{

// VHF omnidirectional range (108-117.95 MHz) navigation receiver. Shows the radial on a course
// deviation indicator and how it is measured from the two 30 Hz tones.
class VorRadioApp : public ReceiverApp
{
public:
    explicit VorRadioApp( SubAppContext const& context );
};

} // namespace ltb::radio
