// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "sample_history.hpp"

// standard
#include <algorithm>

namespace ltb::radio
{

SampleHistory::SampleHistory( std::size_t const capacity )
    : data_( std::max( capacity, 1UZ ) )
{
}

auto SampleHistory::push( std::span< dsp::Complex const > const samples ) -> void
{
    this->push_impl( samples );
}

auto SampleHistory::push( std::span< float32 const > const samples ) -> void
{
    this->push_impl( samples );
}

template < typename Sample >
auto SampleHistory::push_impl( std::span< Sample const > samples ) -> void
{
    // Older samples would be overwritten anyway.
    samples = samples.last( std::min( samples.size( ), data_.size( ) ) );

    auto const lock = std::scoped_lock( mutex_ );
    for ( auto const& sample : samples )
    {
        data_[ write_ ] = dsp::Complex( sample );
        write_          = ( write_ + 1UZ ) % data_.size( );
    }
    size_ = std::min( size_ + samples.size( ), data_.size( ) );
    ++version_;
}

auto SampleHistory::copy_latest( std::size_t count, std::vector< dsp::Complex >& output ) const
    -> void
{
    auto const lock = std::scoped_lock( mutex_ );

    count = std::min( count, size_ );
    output.resize( count );

    auto read = ( write_ + data_.size( ) - count ) % data_.size( );
    for ( auto& sample : output )
    {
        sample = data_[ read ];
        read   = ( read + 1UZ ) % data_.size( );
    }
}

auto SampleHistory::version( ) const -> uint64
{
    auto const lock = std::scoped_lock( mutex_ );
    return version_;
}

auto SampleHistory::capacity( ) const -> std::size_t
{
    return data_.size( );
}

auto SampleHistory::clear( ) -> void
{
    auto const lock = std::scoped_lock( mutex_ );
    write_          = 0UZ;
    size_           = 0UZ;
    ++version_;
}

} // namespace ltb::radio
