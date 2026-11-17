// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "audio_output.hpp"

// external
#include <miniaudio.h>

// standard
#include <algorithm>
#include <span>

namespace ltb::radio
{

namespace
{

// After running dry, wait for this much audio before playing again so bursty input doesn't
// cause a click every block.
constexpr auto prime_seconds = 0.06;

} // namespace

struct AudioOutput::Device
{
    ma_device                    device        = { };
    SampleRingBuffer< float32 >* source        = nullptr;
    std::size_t                  prime_samples = 0UZ;
    bool                         priming       = true;
    bool                         initialized   = false;
};

struct AudioCallback
{
    static auto call(
        ma_device* const device,
        void* const      output,
        void const* /*input*/,
        ma_uint32 const frame_count
    ) -> void
    {
        auto const samples = std::span( static_cast< float32* >( output ), frame_count );
        auto&      self    = *static_cast< AudioOutput::Device* >( device->pUserData );

        if ( self.priming && self.source->size( ) < self.prime_samples )
        {
            std::ranges::fill( samples, 0.0F );
            return;
        }

        auto const count = self.source->pop( samples );
        std::ranges::fill( samples.subspan( count ), 0.0F );
        self.priming = count < samples.size( );
    }
};

AudioOutput::AudioOutput( )
    : device_( std::make_unique< Device >( ) )
{
}

auto AudioOutput::start( SampleRingBuffer< float32 >& source, uint32 const sample_rate_hz )
    -> utils::Result< std::unique_ptr< AudioOutput > >
{
    // The constructor is private.
    auto output = std::unique_ptr< AudioOutput >( new AudioOutput( ) );

    auto config              = ma_device_config_init( ma_device_type_playback );
    config.playback.format   = ma_format_f32;
    config.playback.channels = 1U;
    config.sampleRate        = sample_rate_hz;
    config.dataCallback      = &AudioCallback::call;
    config.pUserData         = output->device_.get( );

    output->device_->source = &source;
    output->device_->prime_samples
        = static_cast< std::size_t >( prime_seconds * static_cast< float64 >( sample_rate_hz ) );

    if ( auto const code = ma_device_init( nullptr, &config, &output->device_->device );
         code != MA_SUCCESS )
    {
        return LTB_MAKE_UNEXPECTED_ERROR(
            "ma_device_init failed: {}",
            ma_result_description( code )
        );
    }
    output->device_->initialized = true;

    if ( auto const code = ma_device_start( &output->device_->device ); code != MA_SUCCESS )
    {
        return LTB_MAKE_UNEXPECTED_ERROR(
            "ma_device_start failed: {}",
            ma_result_description( code )
        );
    }

    return output;
}

AudioOutput::~AudioOutput( )
{
    if ( device_->initialized )
    {
        // Stops the audio thread before the device is released.
        ma_device_uninit( &device_->device );
    }
}

} // namespace ltb::radio
