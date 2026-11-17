// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////

// project
#include "ltb/radio/dsp/demodulators.hpp"
#include "ltb/radio/dsp/fft.hpp"
#include "ltb/radio/dsp/fir_filter.hpp"
#include "ltb/radio/dsp/mixer.hpp"
#include "ltb/radio/dsp/radio_pipeline.hpp"
#include "ltb/radio/dsp/test_signal.hpp"

// external
#include <gtest/gtest.h>

// standard
#include <algorithm>
#include <cmath>
#include <functional>
#include <numbers>
#include <numeric>
#include <ranges>

namespace ltb::radio::dsp
{
namespace
{

constexpr auto pi = std::numbers::pi;

auto tone( float64 const frequency_hz, float64 const sample_rate_hz, std::size_t const count )
    -> std::vector< Complex >
{
    auto samples = std::vector< Complex >( count );
    for ( auto i = 0UZ; i < count; ++i )
    {
        samples[ i ] = Complex(
            std::
                polar( 1.0, 2.0 * pi * frequency_hz * static_cast< float64 >( i ) / sample_rate_hz )
        );
    }
    return samples;
}

// Magnitude of the filter's frequency response in dB.
auto response_db(
    std::vector< float32 > const& taps,
    float64 const                 frequency_hz,
    float64 const                 sample_rate_hz
) -> float64
{
    auto sum = std::complex< float64 >( );
    for ( auto i = 0UZ; i < taps.size( ); ++i )
    {
        sum += static_cast< float64 >( taps[ i ] )
             * std::polar(
                   1.0,
                   -2.0 * pi * frequency_hz * static_cast< float64 >( i ) / sample_rate_hz
             );
    }
    return 20.0 * std::log10( std::abs( sum ) );
}

// The frequency of the strongest bin (excluding DC) in the last 4096 samples of a real signal.
auto dominant_frequency( std::vector< float32 > const& audio, float64 const sample_rate_hz )
    -> float64
{
    constexpr auto size = 4096UZ;
    EXPECT_GE( audio.size( ), size );

    auto spectrum = std::vector< float32 >( );
    power_spectrum_db( std::span( audio ).last( size ), spectrum );

    auto const peak = std::ranges::max_element( spectrum.begin( ) + 2, spectrum.end( ) );
    return static_cast< float64 >( std::distance( spectrum.begin( ), peak ) ) * sample_rate_hz
         / static_cast< float64 >( size );
}

auto run_pipeline(
    Modulation const           modulation,
    std::vector< TestStation > stations,
    PipelineSettings const&    settings,
    float64 const              seconds
) -> std::vector< float32 >
{
    constexpr auto fs = RadioPipeline::input_rate_hz;

    auto generator = TestSignalGenerator( modulation, fs, std::move( stations ), 0.001F );
    auto pipeline  = RadioPipeline( modulation, settings );

    auto block = std::vector< Complex >( 24'000UZ );
    auto audio = std::vector< float32 >( );
    for ( auto i = 0; i < static_cast< int >( seconds * fs / 24'000.0 ); ++i )
    {
        generator.generate( block );
        pipeline.process( block, audio, nullptr );
    }
    return audio;
}

TEST( Fft, ComplexToneLandsInItsBinAtZeroDb )
{
    constexpr auto size = 1024UZ;
    constexpr auto fs   = 1024.0;

    auto spectrum = std::vector< float32 >( );
    power_spectrum_db( tone( 100.0, fs, size ), spectrum );

    ASSERT_EQ( spectrum.size( ), size );
    auto const peak = std::ranges::max_element( spectrum );
    EXPECT_EQ(
        std::distance( spectrum.begin( ), peak ),
        static_cast< std::ptrdiff_t >( size / 2UZ + 100UZ )
    );
    EXPECT_NEAR( *peak, 0.0F, 0.01F );
}

TEST( Fft, NegativeFrequencyIsBelowCenter )
{
    auto spectrum = std::vector< float32 >( );
    power_spectrum_db( tone( -64.0, 512.0, 512UZ ), spectrum );

    auto const peak = std::ranges::max_element( spectrum );
    EXPECT_EQ( std::distance( spectrum.begin( ), peak ), 256 - 64 );
}

TEST( Fft, RealSineIsZeroDb )
{
    auto sine = std::vector< float32 >( 1024UZ );
    for ( auto i = 0UZ; i < sine.size( ); ++i )
    {
        sine[ i ] = static_cast< float32 >(
            std::sin( 2.0 * pi * 50.0 * static_cast< float64 >( i ) / 1024.0 )
        );
    }

    auto spectrum = std::vector< float32 >( );
    power_spectrum_db( sine, spectrum );

    ASSERT_EQ( spectrum.size( ), 513UZ );
    EXPECT_NEAR( spectrum[ 50 ], 0.0F, 0.01F );
}

TEST( Fir, LowPassHasUnityDcGainAndAttenuatesTheStopBand )
{
    constexpr auto fs = 48'000.0;

    auto const taps = design_low_pass( 5'000.0, 2'000.0, fs );

    EXPECT_EQ( taps.size( ) % 2UZ, 1UZ );
    EXPECT_NEAR( response_db( taps, 0.0, fs ), 0.0, 1e-3 );
    EXPECT_GT( response_db( taps, 3'000.0, fs ), -0.1 );
    EXPECT_LT( response_db( taps, 7'000.0, fs ), -60.0 );
    EXPECT_LT( response_db( taps, 15'000.0, fs ), -60.0 );
}

TEST( Fir, BlockSizeDoesNotChangeTheOutput )
{
    auto const input = tone( 1'000.0, 48'000.0, 1000UZ );
    auto const taps  = design_low_pass( 4'000.0, 2'000.0, 48'000.0 );

    auto whole = std::vector< Complex >( );
    DecimatingFir< Complex >( taps, 3UZ ).process( input, whole );

    auto pieces = std::vector< Complex >( );
    auto filter = DecimatingFir< Complex >( taps, 3UZ );
    for ( auto start = 0UZ; start < input.size( ); start += 7UZ )
    {
        filter.process(
            std::span( input ).subspan( start, std::min( 7UZ, input.size( ) - start ) ),
            pieces
        );
    }

    ASSERT_EQ( whole.size( ), pieces.size( ) );
    EXPECT_EQ( whole.size( ), 334UZ ); // ceil(1000 / 3)
    for ( auto i = 0UZ; i < whole.size( ); ++i )
    {
        EXPECT_NEAR( std::abs( whole[ i ] - pieces[ i ] ), 0.0F, 1e-5F ) << i;
    }
}

TEST( Mixer, ShiftsTheToneToDc )
{
    constexpr auto fs = 1.0e6;

    auto mixer = Mixer( );
    mixer.set_shift( 125.0e3, fs );

    auto output = std::vector< Complex >( );
    mixer.process( tone( 125.0e3, fs, 4096UZ ), output );

    for ( auto const& sample : output )
    {
        EXPECT_NEAR( std::abs( sample - Complex( 1.0F ) ), 0.0F, 1e-4F );
    }
}

TEST( FmDiscriminator, OutputIsFrequencyOverDeviation )
{
    constexpr auto fs = 240.0e3;

    auto discriminator = FmDiscriminator( );
    discriminator.configure( 75.0e3, fs );

    auto output = std::vector< float32 >( );
    discriminator.process( tone( 37.5e3, fs, 100UZ ), output );

    for ( auto const value : output | std::views::drop( 1 ) )
    {
        EXPECT_NEAR( value, 0.5F, 1e-4F );
    }
}

TEST( RadioPipeline, AmRecoversTheTunedStationsTone )
{
    // The receiver is centered at 0 Hz. The tuned station is 20 kHz from a stronger neighbor.
    auto const audio = run_pipeline(
        Modulation::Am,
        {
            { .frequency_hz = 250.0e3, .level = 0.1F, .tone_hz = 1'000.0 },
            { .frequency_hz = 270.0e3, .level = 0.3F, .tone_hz = 3'000.0 },
        },
        { .shift_hz = 250.0e3, .channel_bandwidth_hz = 10.0e3 },
        0.5
    );

    EXPECT_NEAR( static_cast< float64 >( audio.size( ) ), 24'000.0, 100.0 );
    EXPECT_NEAR( dominant_frequency( audio, RadioPipeline::audio_rate_hz ), 1'000.0, 12.0 );
}

TEST( RadioPipeline, FmRecoversTheTunedStationsTone )
{
    auto const audio = run_pipeline(
        Modulation::Fm,
        {
            { .frequency_hz = -300.0e3, .level = 0.1F, .tone_hz = 1'000.0 },
            { .frequency_hz = 100.0e3, .level = 0.3F, .tone_hz = 3'000.0 },
        },
        { .shift_hz = -300.0e3, .channel_bandwidth_hz = 200.0e3 },
        0.5
    );

    EXPECT_NEAR( static_cast< float64 >( audio.size( ) ), 24'000.0, 100.0 );
    EXPECT_NEAR( dominant_frequency( audio, RadioPipeline::audio_rate_hz ), 1'000.0, 12.0 );
}

TEST( RadioPipeline, SquelchMutesAnEmptyChannel )
{
    auto const settings = PipelineSettings{
        .shift_hz             = 250.0e3,
        .channel_bandwidth_hz = 8.0e3,
        .squelch_enabled      = true,
        .squelch_db           = -40.0F,
    };

    auto const quiet = run_pipeline( Modulation::Am, { }, settings, 0.2 );
    EXPECT_TRUE( std::ranges::all_of( quiet, std::bind_front( std::equal_to<>{ }, 0.0F ) ) );

    auto const busy = run_pipeline(
        Modulation::Am,
        { { .frequency_hz = 250.0e3, .level = 0.1F, .tone_hz = 1'000.0 } },
        settings,
        0.2
    );
    EXPECT_NEAR( dominant_frequency( busy, RadioPipeline::audio_rate_hz ), 1'000.0, 12.0 );
}

// Smallest angle between two bearings.
auto angle_between( float64 const a_deg, float64 const b_deg ) -> float64
{
    auto const difference = std::fmod( std::abs( a_deg - b_deg ), 360.0 );
    return std::min( difference, 360.0 - difference );
}

TEST( RadioPipeline, VorRecoversTheRadial )
{
    constexpr auto fs = RadioPipeline::input_rate_hz;

    for ( auto const radial : { 0.0, 45.0, 90.0, 196.0, 271.5, 359.0 } )
    {
        auto generator = TestSignalGenerator(
            Modulation::Vor,
            fs,
            { { .frequency_hz = 250.0e3,
                .level        = 0.1F,
                .tone_hz      = 1'020.0,
                .radial_deg   = radial } },
            0.001F
        );
        auto pipeline = RadioPipeline(
            Modulation::Vor,
            { .shift_hz = 250.0e3, .channel_bandwidth_hz = 25.0e3 }
        );

        auto block = std::vector< Complex >( 24'000UZ );
        auto audio = std::vector< float32 >( );
        for ( auto i = 0; i < 300; ++i ) // 3 seconds
        {
            generator.generate( block );
            pipeline.process( block, audio, nullptr );
        }

        auto const reading = pipeline.vor_reading( );
        EXPECT_TRUE( reading.valid ) << radial;
        EXPECT_LT( angle_between( reading.radial_deg, radial ), 1.0 )
            << radial << " vs " << reading.radial_deg;
        EXPECT_NEAR( reading.variable_level, 0.3F, 0.03F );
        EXPECT_NEAR( reading.reference_level, 1.0F, 0.1F );
    }
}

TEST( RadioPipeline, VorIsInvalidWithoutAStation )
{
    auto generator
        = TestSignalGenerator( Modulation::Vor, RadioPipeline::input_rate_hz, { }, 0.01F );
    auto pipeline
        = RadioPipeline( Modulation::Vor, { .shift_hz = 250.0e3, .channel_bandwidth_hz = 25.0e3 } );

    auto block = std::vector< Complex >( 24'000UZ );
    auto audio = std::vector< float32 >( );
    for ( auto i = 0; i < 100; ++i )
    {
        generator.generate( block );
        pipeline.process( block, audio, nullptr );
    }
    EXPECT_FALSE( pipeline.vor_reading( ).valid );
}

TEST( RadioPipeline, ReportsEveryStage )
{
    struct CountingObserver : StageObserver
    {
        std::vector< std::size_t > counts = std::vector< std::size_t >( 10UZ );

        auto on_stage_output( std::size_t const stage, std::span< Complex const > const samples )
            -> void override
        {
            counts.at( stage ) += samples.size( );
        }

        auto on_stage_output( std::size_t const stage, std::span< float32 const > const samples )
            -> void override
        {
            counts.at( stage ) += samples.size( );
        }
    };

    for ( auto const modulation : { Modulation::Am, Modulation::Fm, Modulation::Vor } )
    {
        auto const stages   = RadioPipeline::stage_infos( modulation );
        auto       pipeline = RadioPipeline( modulation, { } );
        auto       observer = CountingObserver( );
        auto       audio    = std::vector< float32 >( );

        pipeline.process( std::vector< Complex >( 240'000UZ ), audio, &observer );

        ASSERT_LE( stages.size( ), observer.counts.size( ) );
        for ( auto i = 0UZ; i < stages.size( ); ++i )
        {
            // Each stage produces samples at its advertised rate (0.1 s of input).
            EXPECT_NEAR(
                static_cast< float64 >( observer.counts[ i ] ),
                stages[ i ].sample_rate_hz * 0.1,
                stages[ i ].sample_rate_hz * 0.002
            ) << stages[ i ].name;
        }
    }
}

} // namespace
} // namespace ltb::radio::dsp
