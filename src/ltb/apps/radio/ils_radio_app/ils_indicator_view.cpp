// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "ils_indicator_view.hpp"

// project
#include "ltb/radio/ils_frequencies.hpp"

// external
#include <imgui.h>
#include <spdlog/spdlog.h>

// standard
#include <algorithm>
#include <format>

namespace ltb::radio
{
namespace
{

// DDM for a full-scale needle deflection.
constexpr auto localizer_full_scale_ddm  = 0.155F;
constexpr auto glideslope_full_scale_ddm = 0.175F;
constexpr auto dot_count                 = 5;
constexpr auto stale_after               = std::chrono::seconds( 3 );

constexpr auto face_color      = IM_COL32( 25, 25, 30, 255 );
constexpr auto bezel_color     = IM_COL32( 90, 90, 100, 255 );
constexpr auto mark_color      = IM_COL32( 230, 230, 230, 255 );
constexpr auto needle_color    = IM_COL32( 255, 255, 255, 255 );
constexpr auto stale_color     = IM_COL32( 140, 140, 140, 160 );
constexpr auto flag_color      = IM_COL32( 220, 40, 40, 255 );
constexpr auto tone_90_color   = ImVec4( 0.35F, 0.75F, 1.0F, 1.0F );
constexpr auto tone_150_color  = ImVec4( 1.0F, 0.6F, 0.3F, 1.0F );
constexpr auto highlight_color = ImVec4( 0.4F, 1.0F, 0.4F, 1.0F );

enum class IlsComponent
{
    Localizer,
    Glideslope,
    None,
};

auto component_at( float64 const frequency_hz ) -> IlsComponent
{
    if ( paired_glideslope_hz( frequency_hz ).has_value( ) )
    {
        return IlsComponent::Localizer;
    }
    if ( paired_localizer_hz( frequency_hz ).has_value( ) )
    {
        return IlsComponent::Glideslope;
    }
    return IlsComponent::None;
}

// Two bars comparing the tone depths. The longer one is the side the receiver is on.
auto tone_bars( dsp::IlsReading const& reading, float32 const nominal_depth ) -> void
{
    auto const scale = 2.0F * nominal_depth;
    ImGui::PushStyleColor( ImGuiCol_PlotHistogram, tone_90_color );
    ImGui::ProgressBar(
        std::clamp( reading.depth_90 / scale, 0.0F, 1.0F ),
        ImVec2( -1.0F, 0.0F ),
        std::format( "90 Hz  {:.1f}%", 100.0F * reading.depth_90 ).c_str( )
    );
    ImGui::PopStyleColor( );
    ImGui::PushStyleColor( ImGuiCol_PlotHistogram, tone_150_color );
    ImGui::ProgressBar(
        std::clamp( reading.depth_150 / scale, 0.0F, 1.0F ),
        ImVec2( -1.0F, 0.0F ),
        std::format( "150 Hz {:.1f}%", 100.0F * reading.depth_150 ).c_str( )
    );
    ImGui::PopStyleColor( );
}

} // namespace

auto IlsIndicatorView::Needle::is_fresh( Clock::time_point const now ) const -> bool
{
    return updated.has_value( ) && now - *updated < stale_after;
}

IlsIndicatorView::IlsIndicatorView(
    RadioEngine&  engine,
    std::string   name,
    float64 const lo_offset_hz
)
    : engine_( engine )
    , name_( std::move( name ) )
    , lo_offset_hz_( lo_offset_hz )
{
}

auto IlsIndicatorView::name( ) const -> std::string const&
{
    return name_;
}

auto IlsIndicatorView::configure_gui( uint32 const /*frame_index*/ ) -> void
{
    ImGui::SetNextWindowSize( ImVec2( 380.0F, 560.0F ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowPos( ImVec2( 380.0F, 60.0F ), ImGuiCond_FirstUseEver );

    this->update_needles( );

    if ( ImGui::Begin( name_.c_str( ), &is_open ) )
    {
        this->gui_tuning( );
        this->gui_readouts( );
        this->gui_instrument( );
    }
    ImGui::End( );
}

auto IlsIndicatorView::update_needles( ) -> void
{
    auto const status = engine_.status( );
    if ( !status.running || !status.ils.valid )
    {
        return;
    }

    auto const component = component_at( engine_.settings( ).station_frequency_hz );
    auto&      needle    = component == IlsComponent::Glideslope ? glideslope_ : localizer_;
    if ( component != IlsComponent::None )
    {
        needle.reading = status.ils;
        needle.updated = Clock::now( );
    }
}

auto IlsIndicatorView::gui_tuning( ) -> void
{
    auto const station   = engine_.settings( ).station_frequency_hz;
    auto const component = component_at( station );

    auto const localizer  = component == IlsComponent::Glideslope ? paired_localizer_hz( station )
                                                                  : std::optional( station );
    auto const glideslope = localizer ? paired_glideslope_hz( *localizer ) : std::nullopt;

    if ( !glideslope )
    {
        ImGui::TextColored(
            ImVec4( 1.0F, 0.8F, 0.3F, 1.0F ),
            "%.2f MHz is not an ILS localizer.",
            station / 1.0e6
        );
        ImGui::TextDisabled( "Localizers are 108.10-111.95 MHz (odd tenths)." );
        return;
    }

    ImGui::TextUnformatted( "Receive:" );
    ImGui::SameLine( );
    if ( ImGui::RadioButton(
             std::format( "Localizer {:.2f}", *localizer / 1.0e6 ).c_str( ),
             component == IlsComponent::Localizer
         ) )
    {
        this->tune( *localizer );
    }
    ImGui::SameLine( );
    if ( ImGui::RadioButton(
             std::format( "Glideslope {:.2f}", *glideslope / 1.0e6 ).c_str( ),
             component == IlsComponent::Glideslope
         ) )
    {
        this->tune( *glideslope );
    }
    ImGui::TextDisabled( "The glideslope is paired with the localizer automatically." );
}

auto IlsIndicatorView::gui_readouts( ) -> void
{
    auto const now       = Clock::now( );
    auto const component = component_at( engine_.settings( ).station_frequency_hz );

    auto const readout = [ & ](
                             char const* const label,
                             Needle const&     needle,
                             char const* const positive,
                             char const* const negative,
                             bool const        active
                         ) {
        auto const color = active ? highlight_color : ImGui::GetStyleColorVec4( ImGuiCol_Text );
        if ( !needle.is_fresh( now ) )
        {
            ImGui::TextColored( color, "%-11s OFF", label );
            return;
        }
        auto const ddm = needle.reading.ddm( );
        ImGui::TextColored(
            color,
            "%-11s DDM %+.3f (fly %s)",
            label,
            ddm,
            ddm > 0.0F ? positive : negative
        );
    };

    ImGui::SeparatorText( "Deviation" );
    readout( "Localizer:", localizer_, "right", "left", component == IlsComponent::Localizer );
    readout( "Glideslope:", glideslope_, "down", "up", component == IlsComponent::Glideslope );

    ImGui::SeparatorText( "Tone depths" );
    auto const status = engine_.status( );
    if ( status.running && status.ils.valid )
    {
        tone_bars( status.ils, component == IlsComponent::Glideslope ? 0.4F : 0.2F );
    }
    else
    {
        ImGui::TextDisabled(
            status.running ? "No ILS signal. Raise the gain or check the frequency."
                           : "Start the radio."
        );
    }
}

auto IlsIndicatorView::gui_instrument( ) const -> void
{
    auto const available = ImGui::GetContentRegionAvail( );
    auto const size      = std::max( std::min( available.x, available.y ), 50.0F );
    auto const top_left  = ImGui::GetCursorScreenPos( );
    auto const center    = ImVec2( top_left.x + 0.5F * available.x, top_left.y + 0.5F * size );
    auto const radius    = 0.5F * size - 4.0F;
    auto const half      = radius * 0.7F;
    ImGui::Dummy( ImVec2( available.x, size ) );

    auto* const draw_list = ImGui::GetWindowDrawList( );
    draw_list->AddCircleFilled( center, radius, face_color, 96 );
    draw_list->AddCircle( center, radius, bezel_color, 96, 3.0F );

    // Localizer dots across and glideslope dots down, crossing at the center.
    for ( auto dot = -dot_count; dot <= dot_count; ++dot )
    {
        auto const offset
            = half * static_cast< float32 >( dot ) / static_cast< float32 >( dot_count );
        auto const size_px = dot == 0 ? 5.0F : 3.0F;
        draw_list
            ->AddCircle( ImVec2( center.x + offset, center.y ), size_px, mark_color, 12, 1.5F );
        draw_list
            ->AddCircle( ImVec2( center.x, center.y + offset ), size_px, mark_color, 12, 1.5F );
    }

    auto const now = Clock::now( );

    // Localizer: 90 Hz dominates left of course, so a positive DDM means fly right (needle right).
    if ( localizer_.is_fresh( now ) )
    {
        auto const x
            = center.x
            + half
                  * std::clamp( localizer_.reading.ddm( ) / localizer_full_scale_ddm, -1.0F, 1.0F );
        draw_list->AddLine(
            ImVec2( x, center.y - half ),
            ImVec2( x, center.y + half ),
            needle_color,
            3.0F
        );
    }
    else
    {
        draw_list->AddLine(
            ImVec2( center.x, center.y - half ),
            ImVec2( center.x, center.y + half ),
            stale_color,
            2.0F
        );
        draw_list->AddRectFilled(
            ImVec2( center.x - radius * 0.6F, center.y - radius * 0.55F ),
            ImVec2( center.x - radius * 0.25F, center.y - radius * 0.4F ),
            flag_color,
            2.0F
        );
        draw_list->AddText(
            ImVec2( center.x - radius * 0.57F, center.y - radius * 0.55F ),
            IM_COL32_WHITE,
            "LOC"
        );
    }

    // Glideslope: 90 Hz dominates above the path, so a positive DDM means fly down (needle down).
    if ( glideslope_.is_fresh( now ) )
    {
        auto const y
            = center.y
            + half
                  * std::
                      clamp( glideslope_.reading.ddm( ) / glideslope_full_scale_ddm, -1.0F, 1.0F );
        draw_list->AddLine(
            ImVec2( center.x - half, y ),
            ImVec2( center.x + half, y ),
            needle_color,
            3.0F
        );
    }
    else
    {
        draw_list->AddLine(
            ImVec2( center.x - half, center.y ),
            ImVec2( center.x + half, center.y ),
            stale_color,
            2.0F
        );
        draw_list->AddRectFilled(
            ImVec2( center.x + radius * 0.25F, center.y + radius * 0.4F ),
            ImVec2( center.x + radius * 0.6F, center.y + radius * 0.55F ),
            flag_color,
            2.0F
        );
        draw_list->AddText(
            ImVec2( center.x + radius * 0.3F, center.y + radius * 0.4F ),
            IM_COL32_WHITE,
            "GS"
        );
    }
}

auto IlsIndicatorView::tune( float64 const frequency_hz ) -> void
{
    auto settings                 = engine_.settings( );
    settings.station_frequency_hz = frequency_hz;
    settings.center_frequency_hz  = frequency_hz - lo_offset_hz_;
    if ( auto result = engine_.apply( settings ); !result )
    {
        spdlog::error( "Tuning failed: {}", result.error( ).debug_error_message( ) );
    }
}

} // namespace ltb::radio
