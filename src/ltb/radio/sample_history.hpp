// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/radio/dsp/types.hpp"

// standard
#include <mutex>
#include <span>
#include <vector>

namespace ltb::radio
{

// The most recent samples of a signal, written by the processing thread and read (without
// being consumed) by the display. Real signals are stored in the real part.
class SampleHistory
{
public:
    explicit SampleHistory( std::size_t capacity );

    auto push( std::span< dsp::Complex const > samples ) -> void;
    auto push( std::span< float32 const > samples ) -> void;

    // Overwrites `output` with up to `count` of the newest samples (oldest first). Returns the
    // total number of samples pushed so far (the index after the newest sample).
    auto copy_latest( std::size_t count, std::vector< dsp::Complex >& output ) const -> uint64;

    // Increases whenever samples are pushed, so readers can skip unchanged data.
    [[nodiscard( "Const getter" )]]
    auto version( ) const -> uint64;

    [[nodiscard( "Const getter" )]]
    auto capacity( ) const -> std::size_t;

    auto clear( ) -> void;

private:
    mutable std::mutex          mutex_;
    std::vector< dsp::Complex > data_;
    std::size_t                 write_   = 0UZ;
    std::size_t                 size_    = 0UZ;
    uint64                      version_ = 0U;
    uint64                      total_   = 0U;

    template < typename Sample >
    auto push_impl( std::span< Sample const > samples ) -> void;
};

} // namespace ltb::radio
