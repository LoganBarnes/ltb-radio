// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "receiver_controls_view.hpp"

// external
#include <imgui.h>

// standard
#include <algorithm>
#include <cmath>
#include <format>

namespace ltb::radio
{
namespace
{

constexpr auto error_color   = ImVec4( 1.0F, 0.4F, 0.4F, 1.0F );
constexpr auto warning_color = ImVec4( 1.0F, 0.8F, 0.3F, 1.0F );
constexpr auto running_color = ImVec4( 0.4F, 1.0F, 0.4F, 1.0F );
constexpr auto min_level_db  = -100.0F;

struct DeEmphasisOption
{
    char const* label;
    float64     tau_s;
};

constexpr auto deemphasis_options = std::array{
    DeEmphasisOption{ "75 us (Americas, Korea)", 75.0e-6 },
    DeEmphasisOption{ "50 us (Europe, Asia)", 50.0e-6 },
    DeEmphasisOption{ "Off", 0.0 },
};

auto help_marker( char const* const text ) -> void
{
    ImGui::SameLine( );
    ImGui::TextDisabled( "(?)" );
    if ( ImGui::BeginItemTooltip( ) )
    {
        ImGui::PushTextWrapPos( ImGui::GetFontSize( ) * 30.0F );
        ImGui::TextUnformatted( text );
        ImGui::PopTextWrapPos( );
        ImGui::EndTooltip( );
    }
}

// A slider that snaps to multiples of `step` (HackRF gains have fixed steps).
auto stepped_slider( char const* const label, uint32& value, int const max, int const step ) -> void
{
    auto as_int = static_cast< int >( value );
    if ( ImGui::SliderInt( label, &as_int, 0, max, "%d dB" ) )
    {
        value = static_cast< uint32 >( ( as_int / step ) * step );
    }
}

} // namespace

ReceiverControlsView::ReceiverControlsView(
    RadioEngine&            engine,
    std::string             name,
    TuningOptions           tuning,
    std::vector< AppView* > stage_views
)
    : engine_( engine )
    , name_( std::move( name ) )
    , tuning_( std::move( tuning ) )
    , stage_views_( std::move( stage_views ) )
{
}

auto ReceiverControlsView::name( ) const -> std::string const&
{
    return name_;
}

auto ReceiverControlsView::configure_gui( uint32 const /*frame_index*/ ) -> void
{
    ImGui::SetNextWindowSize( ImVec2( 340.0F, 640.0F ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowPos( ImVec2( 20.0F, 40.0F ), ImGuiCond_FirstUseEver );

    if ( ImGui::Begin( name_.c_str( ), &is_open ) )
    {
        ImGui::PushItemWidth( -ImGui::GetFontSize( ) * 7.0F );

        this->gui_run_controls( );

        auto settings = engine_.settings( );
        this->gui_source( settings );
        this->gui_tuning( settings );
        if ( settings.source == SourceType::Hackrf )
        {
            this->gui_gain( settings );
        }
        this->gui_demodulation( settings );
        this->gui_audio( settings );

        if ( settings != engine_.settings( ) )
        {
            this->report( engine_.apply( std::move( settings ) ) );
        }

        this->gui_status( );
        this->gui_stage_views( );

        ImGui::PopItemWidth( );
    }
    ImGui::End( );
}

auto ReceiverControlsView::gui_run_controls( ) -> void
{
    auto const running = engine_.is_running( );
    if ( ImGui::Button( running ? "Stop" : "Start", ImVec2( 80.0F, 0.0F ) ) )
    {
        if ( running )
        {
            engine_.stop( );
        }
        else
        {
            this->report( engine_.start( ) );
        }
    }
    ImGui::SameLine( );
    if ( running )
    {
        ImGui::TextColored( running_color, "Running" );
    }
    else
    {
        ImGui::TextDisabled( "Stopped" );
    }

    if ( !error_message_.empty( ) )
    {
        ImGui::PushTextWrapPos( 0.0F );
        ImGui::TextColored( error_color, "%s", error_message_.c_str( ) );
        ImGui::PopTextWrapPos( );
        if ( ImGui::SmallButton( "Dismiss" ) )
        {
            error_message_.clear( );
        }
    }
}

auto ReceiverControlsView::gui_source( RadioSettings& settings ) -> void
{
    ImGui::SeparatorText( "Source" );

    if ( ImGui::RadioButton( "Test signal", settings.source == SourceType::TestSignal ) )
    {
        settings.source = SourceType::TestSignal;
    }
    help_marker( "Simulated stations near the default frequency. No hardware needed." );
    ImGui::SameLine( );
    if ( ImGui::RadioButton( "HackRF", settings.source == SourceType::Hackrf ) )
    {
        settings.source = SourceType::Hackrf;
    }

    if ( settings.source != SourceType::Hackrf )
    {
        return;
    }

    if ( needs_refresh_ )
    {
        this->refresh_devices( );
    }

    auto const preview = settings.hackrf_serial.empty( ) ? std::string( "First available" )
                                                         : settings.hackrf_serial;
    if ( ImGui::BeginCombo( "Device", preview.c_str( ) ) )
    {
        if ( ImGui::Selectable( "First available", settings.hackrf_serial.empty( ) ) )
        {
            settings.hackrf_serial.clear( );
        }
        for ( auto const& device : devices_ )
        {
            auto const label = std::format( "{} {}", device.usb_board_name, device.serial );
            if ( ImGui::Selectable( label.c_str( ), settings.hackrf_serial == device.serial ) )
            {
                settings.hackrf_serial = device.serial;
            }
        }
        ImGui::EndCombo( );
    }
    ImGui::SameLine( );
    if ( ImGui::SmallButton( "Refresh" ) )
    {
        needs_refresh_ = true;
    }
    if ( devices_.empty( ) )
    {
        ImGui::TextColored( warning_color, "No HackRF devices found." );
    }
}

auto ReceiverControlsView::gui_tuning( RadioSettings& settings ) -> void
{
    ImGui::SeparatorText( "Tuning" );

    auto const scale   = tuning_.display_scale_hz;
    auto       station = settings.station_frequency_hz / scale;
    if ( ImGui::InputDouble(
             "Station",
             &station,
             tuning_.step_hz / scale,
             10.0 * tuning_.step_hz / scale,
             tuning_.display_format
         ) )
    {
        this->tune( settings, station * scale );
    }
    help_marker( "Or right-click a spectrum plot to tune there." );

    if ( !tuning_.presets.empty( ) )
    {
        ImGui::TextUnformatted( "Presets:" );
        for ( auto const& preset : tuning_.presets )
        {
            ImGui::SameLine( );
            if ( ImGui::SmallButton( preset.label.c_str( ) ) )
            {
                this->tune( settings, preset.frequency_hz );
            }
        }
    }

    ImGui::Text( "Receiver: %.4f MHz", settings.center_frequency_hz / 1.0e6 );
    ImGui::SameLine( );
    if ( ImGui::SmallButton( "Recenter" ) )
    {
        settings.center_frequency_hz = settings.station_frequency_hz - tuning_.lo_offset_hz;
    }
    help_marker(
        "The receiver (hardware) is tuned below the station so its DC spike at the center "
        "frequency stays out of the channel. The Frequency Shift stage moves the station to 0 Hz."
    );
    ImGui::Text(
        "Offset: %+.1f kHz",
        ( settings.station_frequency_hz - settings.center_frequency_hz ) / 1.0e3
    );
}

auto ReceiverControlsView::gui_gain( RadioSettings& settings ) -> void
{
    ImGui::SeparatorText( "RF Gain" );

    stepped_slider( "LNA (IF)", settings.lna_gain_db, 40, 8 );
    help_marker( "Low-noise amplifier gain, in 8 dB steps." );
    stepped_slider( "VGA (baseband)", settings.vga_gain_db, 62, 2 );
    help_marker( "Variable gain amplifier, in 2 dB steps." );
    ImGui::Checkbox( "RF amp (+11 dB)", &settings.amp_enabled );
    help_marker( "Front-end amplifier. Strong nearby signals can overload it." );
}

auto ReceiverControlsView::gui_demodulation( RadioSettings& settings ) -> void
{
    ImGui::SeparatorText( "Demodulation" );

    auto const [ min_hz, max_hz ] = dsp::RadioPipeline::bandwidth_limits( engine_.modulation( ) );
    auto bandwidth_khz            = static_cast< float32 >( settings.channel_bandwidth_hz / 1.0e3 );
    if ( ImGui::SliderFloat(
             "Bandwidth",
             &bandwidth_khz,
             static_cast< float32 >( min_hz / 1.0e3 ),
             static_cast< float32 >( max_hz / 1.0e3 ),
             "%.1f kHz"
         ) )
    {
        settings.channel_bandwidth_hz = static_cast< float64 >( bandwidth_khz ) * 1.0e3;
    }
    help_marker( "Width of the channel filter around the station (the highlighted band)." );

    if ( engine_.modulation( ) == dsp::Modulation::Am )
    {
        ImGui::Checkbox( "AGC", &settings.agc_enabled );
        help_marker(
            "Automatic gain control keeps the volume steady for weak and strong stations."
        );
        return;
    }

    auto const current = std::ranges::
        find( deemphasis_options, settings.deemphasis_tau_s, &DeEmphasisOption::tau_s );
    auto const preview = current == deemphasis_options.end( ) ? "Custom" : current->label;
    if ( ImGui::BeginCombo( "De-emphasis", preview ) )
    {
        for ( auto const& option : deemphasis_options )
        {
            if ( ImGui::Selectable( option.label, settings.deemphasis_tau_s == option.tau_s ) )
            {
                settings.deemphasis_tau_s = option.tau_s;
            }
        }
        ImGui::EndCombo( );
    }
}

auto ReceiverControlsView::gui_audio( RadioSettings& settings ) -> void
{
    ImGui::SeparatorText( "Audio" );
    ImGui::SliderFloat( "Volume", &settings.volume, 0.0F, 1.0F, "%.2f" );
    ImGui::Checkbox( "Mute", &settings.muted );

    ImGui::Checkbox( "Squelch", &settings.squelch_enabled );
    help_marker(
        "Mutes the audio while the channel is quieter than the threshold, so you only hear "
        "transmissions. Set the threshold just above the level shown when nobody is talking."
    );
    ImGui::BeginDisabled( !settings.squelch_enabled );
    ImGui::SliderFloat( "Threshold", &settings.squelch_db, min_level_db, 0.0F, "%.0f dB" );
    ImGui::EndDisabled( );

    // Channel level meter, green while the squelch is open.
    auto const status = engine_.status( );
    auto const level  = status.running ? status.channel_power_db : min_level_db;
    auto const label
        = status.running ? std::format( "Channel {:.0f} dB", level ) : std::string( "Channel -" );
    auto const color
        = status.running && status.squelch_open ? running_color : ImVec4( 0.5F, 0.5F, 0.5F, 1.0F );
    ImGui::PushStyleColor( ImGuiCol_PlotHistogram, color );
    ImGui::ProgressBar(
        std::clamp( 1.0F - level / min_level_db, 0.0F, 1.0F ),
        ImVec2( -1.0F, 0.0F ),
        label.c_str( )
    );
    ImGui::PopStyleColor( );
}

auto ReceiverControlsView::gui_status( ) -> void
{
    ImGui::SeparatorText( "Status" );

    auto const status = engine_.status( );
    auto const load   = std::format( "DSP load {:.0f}%", 100.0F * status.processing_load );
    ImGui::ProgressBar(
        std::min( status.processing_load, 1.0F ),
        ImVec2( -1.0F, 0.0F ),
        load.c_str( )
    );
    ImGui::Text( "Audio buffered: %.0f ms", 1000.0F * status.audio_buffer_seconds );
    ImGui::Text(
        "Dropped input samples: %llu",
        static_cast< unsigned long long >( status.dropped_input_samples )
    );
    if ( !status.audio_error.empty( ) )
    {
        ImGui::PushTextWrapPos( 0.0F );
        ImGui::TextColored( warning_color, "No audio: %s", status.audio_error.c_str( ) );
        ImGui::PopTextWrapPos( );
    }
}

auto ReceiverControlsView::gui_stage_views( ) -> void
{
    ImGui::SeparatorText( "Pipeline" );

    auto const& stages = engine_.stages( );
    for ( auto i = 0UZ; i < std::min( stages.size( ), stage_views_.size( ) ); ++i )
    {
        ImGui::Checkbox( stage_views_[ i ]->name( ).c_str( ), &stage_views_[ i ]->is_open );
        if ( ImGui::BeginItemTooltip( ) )
        {
            ImGui::PushTextWrapPos( ImGui::GetFontSize( ) * 30.0F );
            ImGui::TextUnformatted( stages[ i ].description.c_str( ) );
            ImGui::PopTextWrapPos( );
            ImGui::EndTooltip( );
        }
        if ( i + 1UZ < stages.size( ) )
        {
            ImGui::TextDisabled( "   |" );
        }
    }
}

auto ReceiverControlsView::tune( RadioSettings& settings, float64 const station_hz ) const -> void
{
    settings.station_frequency_hz = std::max( station_hz, 0.0 );

    auto const limit
        = 0.45 * dsp::RadioPipeline::input_rate_hz - 0.5 * settings.channel_bandwidth_hz;
    if ( std::abs( settings.station_frequency_hz - settings.center_frequency_hz ) > limit )
    {
        settings.center_frequency_hz = settings.station_frequency_hz - tuning_.lo_offset_hz;
    }
}

auto ReceiverControlsView::refresh_devices( ) -> void
{
    needs_refresh_ = false;
    devices_.clear( );

    auto library = engine_.hackrf_library( );
    if ( !library )
    {
        error_message_ = library.error( ).debug_error_message( );
        return;
    }
    if ( auto devices = list_hackrf_devices( *library.value( ) ) )
    {
        devices_ = std::move( devices.value( ) );
    }
    else
    {
        error_message_ = devices.error( ).debug_error_message( );
    }
}

auto ReceiverControlsView::report( utils::Result< void > const& result ) -> void
{
    if ( !result )
    {
        error_message_ = result.error( ).debug_error_message( );
    }
}

} // namespace ltb::radio
