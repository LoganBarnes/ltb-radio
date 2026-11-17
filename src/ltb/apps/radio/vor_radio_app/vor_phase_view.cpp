// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "vor_phase_view.hpp"

// project
#include "ltb/radio/dsp/vor_demodulator.hpp"

// external
#include <imgui.h>

// standard
#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>
#include <optional>

namespace ltb::radio
{
namespace
{

constexpr auto period_ms    = 1000.0F / 30.0F;
constexpr auto ms_per_tone  = static_cast< float32 >( 1000.0 / dsp::VorDemodulator::tone_rate_hz );
constexpr auto extra_ms     = 40.0F;
constexpr auto label_margin = 4.0F;

constexpr auto background_color = IM_COL32( 15, 15, 20, 255 );
constexpr auto grid_color       = IM_COL32( 255, 255, 255, 28 );
constexpr auto axis_color       = IM_COL32( 255, 255, 255, 70 );
constexpr auto label_color      = IM_COL32( 210, 210, 210, 200 );
constexpr auto reference_color  = IM_COL32( 255, 150, 70, 255 );
constexpr auto variable_color   = IM_COL32( 90, 190, 255, 255 );
constexpr auto lag_color        = IM_COL32( 255, 220, 80, 255 );

// Where samples land on the plot. Times are in ms relative to the newest shared sample.
struct PlotMapping
{
    ImVec2  origin;
    ImVec2  size;
    float32 window_ms;
    float32 pre_trigger_ms;
    float32 trigger_ms;

    [[nodiscard( "Const computation" )]]
    auto x( float32 const time_ms ) const -> float32
    {
        return origin.x + size.x * ( time_ms - trigger_ms + pre_trigger_ms ) / window_ms;
    }

    [[nodiscard( "Const computation" )]]
    auto is_visible( float32 const time_ms ) const -> bool
    {
        auto const relative = time_ms - trigger_ms;
        return relative >= -pre_trigger_ms && relative <= window_ms - pre_trigger_ms;
    }
};

// The time of sample `i`. `end_total` is the sample index at time 0.
struct SampleTimes
{
    uint64      total;
    uint64      end_total;
    std::size_t count;
    float32     shift_ms;

    [[nodiscard( "Const computation" )]]
    auto operator( )( std::size_t const i ) const -> float32
    {
        auto const first
            = static_cast< float32 >( total - count ) - static_cast< float32 >( end_total );
        return ( first + static_cast< float32 >( i ) ) * ms_per_tone - shift_ms;
    }
};

// The latest time (no later than `latest_ms`) at which the tone peaks. The tone's phase is fit
// over all the samples, which is far steadier than picking the largest sample.
auto find_peak_ms(
    std::vector< dsp::Complex > const& samples,
    SampleTimes const&                 times,
    float32 const                      latest_ms
) -> float32
{
    auto correlation = std::complex< float64 >( );
    for ( auto i = 0UZ; i < samples.size( ); ++i )
    {
        auto const phase
            = 2.0 * std::numbers::pi * static_cast< float64 >( times( i ) / period_ms );
        correlation += static_cast< float64 >( samples[ i ].real( ) ) * std::polar( 1.0, -phase );
    }

    // cos(2 pi t / T + phase) peaks where 2 pi t / T + phase = 2 pi k.
    auto const first_peak
        = static_cast< float32 >( -std::arg( correlation ) / ( 2.0 * std::numbers::pi ) )
        * period_ms;
    return first_peak + std::floor( ( latest_ms - first_peak ) / period_ms ) * period_ms;
}

// Screen points for the visible samples, scaled to unit height.
auto make_trace(
    std::vector< dsp::Complex > const& samples,
    SampleTimes const&                 times,
    PlotMapping const&                 mapping
) -> std::vector< ImVec2 >
{
    auto peak = 1e-9F;
    for ( auto const& sample : samples )
    {
        peak = std::max( peak, std::abs( sample.real( ) ) );
    }

    auto points = std::vector< ImVec2 >( );
    for ( auto i = 0UZ; i < samples.size( ); ++i )
    {
        if ( auto const time = times( i ); mapping.is_visible( time ) )
        {
            auto const y = samples[ i ].real( ) / peak;
            points.emplace_back(
                mapping.x( time ),
                mapping.origin.y + mapping.size.y * ( 0.5F - 0.42F * y )
            );
        }
    }
    return points;
}

} // namespace

VorPhaseView::VorPhaseView( RadioEngine& engine, std::string name )
    : engine_( engine )
    , name_( std::move( name ) )
    , reference_delay_ms_( 1000.0 * dsp::VorDemodulator( ).reference_delay_s( ) )
{
}

auto VorPhaseView::name( ) const -> std::string const&
{
    return name_;
}

auto VorPhaseView::configure_gui( uint32 const /*frame_index*/ ) -> void
{
    ImGui::SetNextWindowSize( ImVec2( 560.0F, 300.0F ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowPos( ImVec2( 760.0F, 60.0F ), ImGuiCond_FirstUseEver );

    if ( ImGui::Begin( name_.c_str( ), &is_open ) )
    {
        ImGui::SetNextItemWidth( ImGui::GetFontSize( ) * 10.0F );
        ImGui::SliderFloat( "Window", &window_ms_, period_ms, 500.0F, "%.0f ms" );
        ImGui::SameLine( );
        ImGui::Checkbox( "Freeze", &frozen_ );
        ImGui::SameLine( );
        ImGui::TextDisabled( "(?)" );
        if ( ImGui::BeginItemTooltip( ) )
        {
            ImGui::PushTextWrapPos( ImGui::GetFontSize( ) * 30.0F );
            ImGui::TextUnformatted(
                "Both 30 Hz tones scaled to the same height. The reference has the same phase in "
                "every direction; the variable tone lags it by the radial (one cycle = 360 degrees "
                "= 33.3 ms). The reference is shifted to cancel its longer filter delay."
            );
            ImGui::PopTextWrapPos( );
            ImGui::EndTooltip( );
        }

        if ( !frozen_ )
        {
            this->update_data( );
        }
        this->draw_plot( );
    }
    ImGui::End( );
}

auto VorPhaseView::update_data( ) -> void
{
    // Extra samples so there is always a reference peak with a full window after it.
    auto const count
        = static_cast< std::size_t >( ( window_ms_ + period_ms + extra_ms ) / ms_per_tone );
    variable_total_
        = engine_.history( dsp::RadioPipeline::vor_variable_stage ).copy_latest( count, variable_ );
    reference_total_ = engine_.history( dsp::RadioPipeline::vor_reference_stage )
                           .copy_latest( count, reference_ );

    auto const status = engine_.status( );
    reading_          = status.vor;
    reading_valid_    = status.running && status.vor.valid;
}

auto VorPhaseView::draw_plot( ) const -> void
{
    auto const origin = ImGui::GetCursorScreenPos( );
    auto const size   = ImGui::GetContentRegionAvail( );
    if ( size.x < 10.0F || size.y < 10.0F )
    {
        return;
    }
    ImGui::Dummy( size );

    auto* const draw_list = ImGui::GetWindowDrawList( );
    draw_list->PushClipRect( origin, ImVec2( origin.x + size.x, origin.y + size.y ), true );
    draw_list
        ->AddRectFilled( origin, ImVec2( origin.x + size.x, origin.y + size.y ), background_color );

    if ( variable_.empty( ) || reference_.empty( ) )
    {
        draw_list->AddText(
            ImVec2( origin.x + 8.0F, origin.y + 8.0F ),
            label_color,
            "No data. Start the radio."
        );
        draw_list->PopClipRect( );
        return;
    }

    // Line both tones up on the newest sample they share, then lock the view to a reference peak
    // that has a full window of data after it.
    auto const end_total       = std::min( variable_total_, reference_total_ );
    auto const pre_trigger_ms  = 0.1F * window_ms_;
    auto const variable_times  = SampleTimes{ variable_total_, end_total, variable_.size( ), 0.0F };
    auto const reference_times = SampleTimes{
        reference_total_,
        end_total,
        reference_.size( ),
        static_cast< float32 >( reference_delay_ms_ ),
    };
    auto const trigger_ms
        = find_peak_ms( reference_, reference_times, pre_trigger_ms - window_ms_ );
    auto const mapping = PlotMapping{ origin, size, window_ms_, pre_trigger_ms, trigger_ms };

    // Grid every 10 ms from the reference peak, labeled.
    for ( auto relative_ms = -10.0F * std::floor( pre_trigger_ms / 10.0F );
          relative_ms <= window_ms_ - pre_trigger_ms;
          relative_ms += 10.0F )
    {
        auto const x = mapping.x( trigger_ms + relative_ms );
        draw_list->AddLine(
            ImVec2( x, origin.y ),
            ImVec2( x, origin.y + size.y ),
            relative_ms == 0.0F ? axis_color : grid_color
        );
        draw_list->AddText(
            ImVec2( x + 2.0F, origin.y + size.y - ImGui::GetFontSize( ) - 2.0F ),
            label_color,
            std::format( "{:+.0f} ms", relative_ms ).c_str( )
        );
    }
    draw_list->AddLine(
        ImVec2( origin.x, origin.y + 0.5F * size.y ),
        ImVec2( origin.x + size.x, origin.y + 0.5F * size.y ),
        axis_color
    );

    auto const variable  = make_trace( variable_, variable_times, mapping );
    auto const reference = make_trace( reference_, reference_times, mapping );

    draw_list->AddPolyline(
        reference.data( ),
        static_cast< int >( reference.size( ) ),
        reference_color,
        ImDrawFlags_None,
        2.0F
    );
    draw_list->AddPolyline(
        variable.data( ),
        static_cast< int >( variable.size( ) ),
        variable_color,
        ImDrawFlags_None,
        2.0F
    );

    auto legend_y = origin.y + label_margin;
    draw_list->AddText(
        ImVec2( origin.x + size.x * 0.45F, legend_y ),
        reference_color,
        "Reference 30 Hz (FM subcarrier)"
    );
    legend_y += ImGui::GetFontSize( );
    draw_list->AddText(
        ImVec2( origin.x + size.x * 0.45F, legend_y ),
        variable_color,
        "Variable 30 Hz (AM)"
    );

    if ( reading_valid_ )
    {
        // The reference peak and, one radial later, the matching variable peak.
        auto const lag_ms     = reading_.radial_deg / 360.0F * period_ms;
        auto const peak_x     = mapping.x( trigger_ms );
        auto const variable_x = mapping.x( trigger_ms + lag_ms );
        auto const arrow_y    = origin.y + 0.04F * size.y;

        draw_list->AddLine(
            ImVec2( peak_x, origin.y ),
            ImVec2( peak_x, origin.y + size.y ),
            reference_color,
            1.0F
        );
        draw_list->AddLine(
            ImVec2( variable_x, origin.y ),
            ImVec2( variable_x, origin.y + size.y ),
            variable_color,
            1.0F
        );
        draw_list
            ->AddLine( ImVec2( peak_x, arrow_y ), ImVec2( variable_x, arrow_y ), lag_color, 2.0F );
        draw_list->AddTriangleFilled(
            ImVec2( variable_x, arrow_y ),
            ImVec2( variable_x - 8.0F, arrow_y - 5.0F ),
            ImVec2( variable_x - 8.0F, arrow_y + 5.0F ),
            lag_color
        );

        auto const label
            = std::format( "lag {:.1f}\u00B0 = {:.1f} ms", reading_.radial_deg, lag_ms );
        draw_list->AddText( ImVec2( peak_x + 6.0F, arrow_y + 4.0F ), lag_color, label.c_str( ) );
    }

    draw_list->PopClipRect( );
}

} // namespace ltb::radio
