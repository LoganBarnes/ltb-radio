// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////

// project
#include "ltb/radio/dsp/radio_pipeline.hpp"
#include "ltb/radio/dsp/test_signal.hpp"
#include "ltb/radio/ils_frequencies.hpp"

// external
#include <gtest/gtest.h>

// standard
#include <set>

namespace ltb::radio
{
namespace
{

TEST( IlsFrequencies, MatchesTheFaaTable )
{
    // Pairs quoted in the FAA AIM, table 1-1-4.
    EXPECT_EQ( paired_glideslope_hz( 108.10e6 ), 334.70e6 );
    EXPECT_EQ( paired_glideslope_hz( 108.50e6 ), 329.90e6 );

    // Cedar City ILS RWY 20 (I-ECC).
    EXPECT_EQ( paired_glideslope_hz( 110.10e6 ), 334.40e6 );
    EXPECT_EQ( paired_localizer_hz( 334.40e6 ), 110.10e6 );

    // VOR and out-of-band frequencies have no pair.
    EXPECT_EQ( paired_glideslope_hz( 108.20e6 ), std::nullopt );
    EXPECT_EQ( paired_glideslope_hz( 117.30e6 ), std::nullopt );
}

TEST( IlsFrequencies, EveryChannelIsPairedExactlyOnce )
{
    // Localizers use odd tenths (plus 50 kHz) from 108.10 to 111.95 MHz. Glideslopes use every
    // 150 kHz step from 329.15 to 335.00 MHz.
    auto glideslopes = std::set< int64 >( );
    for ( auto tenth = 1; tenth <= 39; tenth += 2 )
    {
        for ( auto const offset_khz : { 0, 50 } )
        {
            auto const localizer = 108.0e6 + tenth * 100.0e3 + offset_khz * 1.0e3;
            auto const paired    = paired_glideslope_hz( localizer );
            ASSERT_TRUE( paired.has_value( ) ) << localizer;
            EXPECT_EQ( paired_localizer_hz( *paired ), localizer );
            glideslopes.insert( std::llround( *paired / 1.0e3 ) );
        }
    }

    ASSERT_EQ( glideslopes.size( ), 40UZ );
    auto expected = 329'150LL;
    for ( auto const khz : glideslopes )
    {
        EXPECT_EQ( khz, expected );
        expected += 150;
    }
}

auto run_ils( dsp::TestStation const& station ) -> dsp::IlsReading
{
    auto generator = dsp::TestSignalGenerator(
        dsp::Modulation::Ils,
        dsp::RadioPipeline::input_rate_hz,
        { station },
        0.001F
    );
    auto pipeline = dsp::RadioPipeline(
        dsp::Modulation::Ils,
        { .shift_hz = station.frequency_hz, .channel_bandwidth_hz = 15.0e3 }
    );

    auto block = std::vector< dsp::Complex >( 24'000UZ );
    auto audio = std::vector< float32 >( );
    for ( auto i = 0; i < 200; ++i ) // 2 seconds
    {
        generator.generate( block );
        pipeline.process( block, audio, nullptr );
    }
    return pipeline.ils_reading( );
}

TEST( IlsDemodulator, MeasuresLocalizerAndGlideslopeDdm )
{
    auto const localizer = run_ils(
        { .frequency_hz = 250.0e3,
          .level        = 0.1F,
          .tone_hz      = 1'020.0,
          .ils_depth    = 0.2,
          .ils_ddm      = 0.05 }
    );
    EXPECT_TRUE( localizer.valid );
    EXPECT_NEAR( localizer.depth_90, 0.225F, 0.005F );
    EXPECT_NEAR( localizer.depth_150, 0.175F, 0.005F );
    EXPECT_NEAR( localizer.ddm( ), 0.05F, 0.003F );

    auto const glideslope = run_ils(
        { .frequency_hz = -300.0e3,
          .level        = 0.1F,
          .tone_hz      = 0.0,
          .ils_depth    = 0.4,
          .ils_ddm      = -0.06 }
    );
    EXPECT_TRUE( glideslope.valid );
    EXPECT_NEAR( glideslope.sdm( ), 0.8F, 0.01F );
    EXPECT_NEAR( glideslope.ddm( ), -0.06F, 0.003F );
}

TEST( IlsDemodulator, InvalidWithoutAStation )
{
    EXPECT_FALSE( run_ils( { .frequency_hz = 250.0e3, .level = 0.0F } ).valid );
}

} // namespace
} // namespace ltb::radio
