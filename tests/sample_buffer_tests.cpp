// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////

// project
#include "ltb/radio/sample_history.hpp"
#include "ltb/radio/sample_ring_buffer.hpp"

// external
#include <gtest/gtest.h>

// standard
#include <array>

namespace ltb::radio
{
namespace
{

TEST( SampleRingBuffer, OverwritesTheOldestSamplesWhenFull )
{
    auto buffer = SampleRingBuffer< int >( 4UZ );

    EXPECT_EQ( buffer.push( std::array{ 1, 2, 3 } ), 0UZ );
    EXPECT_EQ( buffer.push( std::array{ 4, 5, 6 } ), 2UZ );

    auto output = std::array< int, 8 >{ };
    ASSERT_EQ( buffer.pop( std::span( output ) ), 4UZ );
    EXPECT_EQ(
        ( std::array{ output[ 0 ], output[ 1 ], output[ 2 ], output[ 3 ] } ),
        ( std::array{ 3, 4, 5, 6 } )
    );
    EXPECT_EQ( buffer.size( ), 0UZ );
}

TEST( SampleRingBuffer, KeepsTheNewestOfAnOversizedPush )
{
    auto buffer = SampleRingBuffer< int >( 2UZ );

    EXPECT_EQ( buffer.push( std::array{ 1, 2, 3, 4, 5 } ), 3UZ );

    auto output = std::array< int, 2 >{ };
    ASSERT_EQ( buffer.pop( std::span( output ) ), 2UZ );
    EXPECT_EQ( output, ( std::array{ 4, 5 } ) );
}

TEST( SampleRingBuffer, PopTimesOutWhenEmpty )
{
    auto buffer = SampleRingBuffer< int >( 2UZ );
    auto output = std::array< int, 2 >{ };
    EXPECT_EQ( buffer.pop( std::span( output ), std::chrono::milliseconds( 1 ) ), 0UZ );
}

TEST( SampleRingBuffer, PopIfAvailableWaitsForTheMinimum )
{
    auto buffer = SampleRingBuffer< int >( 8UZ );
    auto output = std::array< int, 2 >{ };

    buffer.push( std::array{ 1, 2 } );
    EXPECT_EQ( buffer.pop_if_available( std::span( output ), 3UZ ), 0UZ );
    EXPECT_EQ( buffer.size( ), 2UZ );

    // Once the minimum is reached, pops as many as fit (like `pop`).
    buffer.push( std::array{ 3 } );
    ASSERT_EQ( buffer.pop_if_available( std::span( output ), 3UZ ), 2UZ );
    EXPECT_EQ( output, ( std::array{ 1, 2 } ) );
    EXPECT_EQ( buffer.size( ), 1UZ );
}

TEST( SampleHistory, CopiesTheNewestSamplesOldestFirst )
{
    auto history = SampleHistory( 4UZ );
    history.push( std::array{ 1.0F, 2.0F, 3.0F } );
    history.push( std::array{ 4.0F, 5.0F } );

    auto output = std::vector< dsp::Complex >( );
    history.copy_latest( 3UZ, output );
    EXPECT_EQ( output, ( std::vector< dsp::Complex >{ 3.0F, 4.0F, 5.0F } ) );

    history.copy_latest( 10UZ, output );
    EXPECT_EQ( output.size( ), 4UZ );
}

} // namespace
} // namespace ltb::radio
