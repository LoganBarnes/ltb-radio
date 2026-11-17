// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////

// project
#include "ltb/radio/radio_engine.hpp"

// external
#include <gtest/gtest.h>

// standard
#include <thread>

namespace ltb::radio
{
namespace
{

TEST( RadioEngine, TestSignalReachesEveryStageAndStopsCleanly )
{
    for ( auto const modulation : { dsp::Modulation::Am, dsp::Modulation::Fm } )
    {
        auto engine = RadioEngine(
            modulation,
            {
                .center_frequency_hz  = modulation == dsp::Modulation::Am ? 750.0e3 : 99.8e6,
                .station_frequency_hz = modulation == dsp::Modulation::Am ? 1'000.0e3 : 100.1e6,
                .channel_bandwidth_hz = modulation == dsp::Modulation::Am ? 10.0e3 : 180.0e3,
                .muted                = true,
            }
        );

        ASSERT_TRUE( engine.start( ) );
        EXPECT_TRUE( engine.is_running( ) );
        std::this_thread::sleep_for( std::chrono::milliseconds( 300 ) );

        auto samples = std::vector< dsp::Complex >( );
        for ( auto stage = 0UZ; stage < engine.stages( ).size( ); ++stage )
        {
            engine.history( stage ).copy_latest( 256UZ, samples );
            EXPECT_EQ( samples.size( ), 256UZ ) << engine.stages( )[ stage ].name;
        }
        EXPECT_LT( engine.status( ).processing_load, 1.0F );

        // Retuning while running must not stop the radio.
        auto settings = engine.settings( );
        settings.station_frequency_hz += 10.0e3;
        EXPECT_TRUE( engine.apply( settings ) );
        EXPECT_TRUE( engine.is_running( ) );

        engine.stop( );
        EXPECT_FALSE( engine.is_running( ) );
    }
}

} // namespace
} // namespace ltb::radio

namespace ltb::radio
{
namespace
{

TEST( RadioEngine, VorTestSignalReportsTheSimulatedRadial )
{
    auto engine = RadioEngine(
        dsp::Modulation::Vor,
        {
            .center_frequency_hz  = 117.05e6,
            .station_frequency_hz = 117.3e6,
            .channel_bandwidth_hz = 25.0e3,
            .muted                = true,
        }
    );

    ASSERT_TRUE( engine.start( ) );
    std::this_thread::sleep_for( std::chrono::milliseconds( 2000 ) );

    auto const reading = engine.status( ).vor;
    EXPECT_TRUE( reading.valid );
    EXPECT_NEAR( reading.radial_deg, 196.0F, 2.0F ); // The simulated Cedar City VOR.

    engine.stop( );
    EXPECT_FALSE( engine.status( ).vor.valid );
}

} // namespace
} // namespace ltb::radio
