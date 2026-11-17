// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// standard
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <span>
#include <vector>

namespace ltb::radio
{

// A thread-safe FIFO of samples with a fixed capacity. When full, the oldest samples are
// overwritten so a slow consumer never blocks the producer (e.g. a libhackrf or audio thread).
template < typename Sample >
class SampleRingBuffer
{
public:
    explicit SampleRingBuffer( std::size_t capacity )
        : data_( std::max( capacity, std::size_t{ 1 } ) )
    {
    }

    // Returns the number of old samples that were overwritten.
    auto push( std::span< Sample const > samples ) -> std::size_t
    {
        auto overwritten = std::size_t{ 0 };
        {
            auto const lock = std::scoped_lock( mutex_ );

            // Only the newest samples can fit.
            if ( samples.size( ) > data_.size( ) )
            {
                overwritten += samples.size( ) - data_.size( );
                samples = samples.last( data_.size( ) );
            }
            if ( auto const free = data_.size( ) - size_; samples.size( ) > free )
            {
                auto const dropped = samples.size( ) - free;
                read_              = ( read_ + dropped ) % data_.size( );
                size_ -= dropped;
                overwritten += dropped;
            }

            auto write = ( read_ + size_ ) % data_.size( );
            for ( auto const sample : samples )
            {
                data_[ write ] = sample;
                write          = ( write + 1 ) % data_.size( );
            }
            size_ += samples.size( );
        }
        available_.notify_one( );
        return overwritten;
    }

    // Copies up to `output.size()` samples into `output` and returns the count. Waits up to
    // `timeout` for samples if the buffer is empty.
    template < typename Rep, typename Period >
    auto pop( std::span< Sample > output, std::chrono::duration< Rep, Period > timeout )
        -> std::size_t
    {
        auto lock = std::unique_lock( mutex_ );
        available_.wait_for( lock, timeout, HasSamples{ *this } );
        return this->pop_locked( output );
    }

    // Copies up to `output.size()` samples into `output` without waiting and returns the count.
    auto pop( std::span< Sample > output ) -> std::size_t
    {
        auto const lock = std::scoped_lock( mutex_ );
        return this->pop_locked( output );
    }

    [[nodiscard( "Const getter" )]]
    auto size( ) const -> std::size_t
    {
        auto const lock = std::scoped_lock( mutex_ );
        return size_;
    }

    [[nodiscard( "Const getter" )]]
    auto capacity( ) const -> std::size_t
    {
        return data_.size( );
    }

    auto clear( ) -> void
    {
        auto const lock = std::scoped_lock( mutex_ );
        read_           = 0;
        size_           = 0;
    }

private:
    struct HasSamples
    {
        SampleRingBuffer const& buffer;

        auto operator( )( ) const -> bool
        {
            return buffer.size_ > 0;
        }
    };

    mutable std::mutex      mutex_;
    std::condition_variable available_;

    std::vector< Sample > data_;
    std::size_t           read_ = 0;
    std::size_t           size_ = 0;

    auto pop_locked( std::span< Sample > output ) -> std::size_t
    {
        auto const count = std::min( output.size( ), size_ );
        for ( auto& sample : output.first( count ) )
        {
            sample = data_[ read_ ];
            read_  = ( read_ + 1 ) % data_.size( );
        }
        size_ -= count;
        return count;
    }
};

} // namespace ltb::radio
