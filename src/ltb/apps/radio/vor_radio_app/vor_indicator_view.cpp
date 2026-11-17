// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "vor_indicator_view.hpp"

// external
#include <imgui.h>

// standard
#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <numbers>

namespace ltb::radio
{
namespace
{

constexpr auto full_scale_deg = 10.0F;
constexpr auto dot_count      = 5;

constexpr auto card_color    = IM_COL32( 25, 25, 30, 255 );
constexpr auto bezel_color   = IM_COL32( 90, 90, 100, 255 );
constexpr auto tick_color    = IM_COL32( 230, 230, 230, 255 );
constexpr auto index_color   = IM_COL32( 255, 200, 0, 255 );
constexpr auto needle_color  = IM_COL32( 255, 255, 255, 255 );
constexpr auto station_color = IM_COL32( 90, 220, 120, 200 );
constexpr auto flag_color    = IM_COL32( 220, 40, 40, 255 );
constexpr auto error_color   = ImVec4( 1.0F, 0.4F, 0.4F, 1.0F );

constexpr auto card_labels
    = std::array{ "N", "3", "6", "E", "12", "15", "S", "21", "24", "W", "30", "33" };

// Wraps to [-180, 180).
auto wrap_180( float32 const degrees ) -> float32
{
    return std::remainder( degrees, 360.0F );
}

// Wraps to [0, 360).
auto wrap_360( float32 const degrees ) -> float32
{
    auto const wrapped = std::fmod( degrees, 360.0F );
    return wrapped < 0.0F ? wrapped + 360.0F : wrapped;
}

// A point `radius` from `center` in the direction `degrees` clockwise from up.
auto polar( ImVec2 const center, float32 const radius, float32 const degrees ) -> ImVec2
{
    auto const radians = degrees * std::numbers::pi_v< float32 > / 180.0F;
    return { center.x + radius * std::sin( radians ), center.y - radius * std::cos( radians ) };
}

struct Deviation
{
    // Negative means the course is to the left.
    float32 degrees = 0.0F;
    bool    to      = false;
};

// Course deviation from the selected course as a CDI shows it.
auto course_deviation( float32 const radial_deg, float32 const course_deg ) -> Deviation
{
    // Within 90 degrees of the selected radial: FROM the station.
    if ( std::abs( wrap_180( radial_deg - course_deg ) ) < 90.0F )
    {
        return { .degrees = wrap_180( course_deg - radial_deg ), .to = false };
    }
    return { .degrees = wrap_180( radial_deg - ( course_deg + 180.0F ) ), .to = true };
}

} // namespace

VorIndicatorView::VorIndicatorView( RadioEngine& engine, std::string name )
    : engine_( engine )
    , name_( std::move( name ) )
{
}

auto VorIndicatorView::name( ) const -> std::string const&
{
    return name_;
}

auto VorIndicatorView::configure_gui( uint32 const /*frame_index*/ ) -> void
{
    ImGui::SetNextWindowSize( ImVec2( 360.0F, 480.0F ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowPos( ImVec2( 380.0F, 60.0F ), ImGuiCond_FirstUseEver );

    if ( ImGui::Begin( name_.c_str( ), &is_open ) )
    {
        auto const status = engine_.status( );
        auto const valid  = status.running && status.vor.valid;

        this->gui_readout( status.vor, valid );
        this->gui_instrument( status.vor, valid );
    }
    ImGui::End( );
}

auto VorIndicatorView::gui_readout( dsp::VorReading const& reading, bool const valid ) -> void
{
    if ( valid )
    {
        ImGui::Text( "Radial:  %05.1f° (from station)", reading.radial_deg );
        ImGui::Text( "Bearing: %05.1f° (to station)", wrap_360( reading.radial_deg + 180.0F ) );
    }
    else
    {
        ImGui::TextColored( error_color, "OFF: no usable VOR signal" );
        ImGui::TextDisabled(
            engine_.is_running( ) ? "Tune to a VOR or raise the gain." : "Start the radio."
        );
    }
    ImGui::TextDisabled(
        "30 Hz AM %.0f%% (nominal 30%%)   FM %.0f%% of 480 Hz",
        100.0F * reading.variable_level,
        100.0F * reading.reference_level
    );

    ImGui::SetNextItemWidth( ImGui::GetFontSize( ) * 8.0F );
    if ( ImGui::DragFloat( "Course (OBS)", &course_deg_, 0.5F, 0.0F, 0.0F, "%03.0f°" ) )
    {
        course_deg_ = wrap_360( course_deg_ );
    }
    ImGui::SameLine( );
    {
        ImGui::BeginDisabled( !valid );
        if ( ImGui::SmallButton( "Center FROM" ) )
        {
            course_deg_ = std::round( reading.radial_deg );
        }
        ImGui::SameLine( );
        if ( ImGui::SmallButton( "Center TO" ) )
        {
            course_deg_ = wrap_360( std::round( reading.radial_deg + 180.0F ) );
        }
        ImGui::EndDisabled( );
    }
}

auto VorIndicatorView::gui_instrument( dsp::VorReading const& reading, bool const valid ) -> void
{
    auto const available = ImGui::GetContentRegionAvail( );
    auto const size      = std::max( std::min( available.x, available.y ), 50.0F );
    auto const top_left  = ImGui::GetCursorScreenPos( );
    auto const center    = ImVec2( top_left.x + 0.5F * available.x, top_left.y + 0.5F * size );
    auto const radius    = 0.5F * size - 4.0F;

    ImGui::InvisibleButton( "##instrument", ImVec2( available.x, size ) );

    // Turn the card by dragging around the center, or with the scroll wheel.
    auto const& io = ImGui::GetIO( );
    if ( ImGui::IsItemActive( ) && ImGui::IsMouseDragging( ImGuiMouseButton_Left, 0.0F ) )
    {
        auto const angle = []( ImVec2 const v ) {
            return std::atan2( v.x, -v.y ) * 180.0F / std::numbers::pi_v< float32 >;
        };
        auto const now  = ImVec2( io.MousePos.x - center.x, io.MousePos.y - center.y );
        auto const prev = ImVec2( now.x - io.MouseDelta.x, now.y - io.MouseDelta.y );
        course_deg_     = wrap_360( course_deg_ - wrap_180( angle( now ) - angle( prev ) ) );
    }
    if ( ImGui::IsItemHovered( ) && io.MouseWheel != 0.0F )
    {
        course_deg_ = wrap_360( std::round( course_deg_ + io.MouseWheel ) );
    }

    auto* const draw_list = ImGui::GetWindowDrawList( );
    draw_list->AddCircleFilled( center, radius, card_color, 96 );
    draw_list->AddCircle( center, radius, bezel_color, 96, 3.0F );

    // Compass card, turned so the selected course is at the top.
    for ( auto heading = 0; heading < 360; heading += 5 )
    {
        auto const screen = static_cast< float32 >( heading ) - course_deg_;
        auto const length = heading % 30 == 0 ? 0.12F : ( heading % 10 == 0 ? 0.08F : 0.05F );
        draw_list->AddLine(
            polar( center, radius * 0.97F, screen ),
            polar( center, radius * ( 0.97F - length ), screen ),
            tick_color,
            1.5F
        );

        if ( heading % 30 == 0 )
        {
            auto const* const label = card_labels[ static_cast< std::size_t >( heading / 30 ) ];
            auto const        text  = ImGui::CalcTextSize( label );
            auto const        at    = polar( center, radius * 0.74F, screen );
            draw_list->AddText(
                ImVec2( at.x - 0.5F * text.x, at.y - 0.5F * text.y ),
                tick_color,
                label
            );
        }
    }

    // Course index at the top and its reciprocal at the bottom.
    auto const index_top = polar( center, radius * 0.99F, 0.0F );
    draw_list->AddTriangleFilled(
        ImVec2( index_top.x - 8.0F, index_top.y - 10.0F ),
        ImVec2( index_top.x + 8.0F, index_top.y - 10.0F ),
        ImVec2( index_top.x, index_top.y + 4.0F ),
        index_color
    );
    auto const course_text = std::format( "{:03.0f}", course_deg_ );
    draw_list->AddText(
        ImVec2(
            index_top.x - 0.5F * ImGui::CalcTextSize( course_text.c_str( ) ).x,
            index_top.y + 8.0F
        ),
        index_color,
        course_text.c_str( )
    );

    // Course deviation dots.
    auto const half_width = radius * 0.5F;
    for ( auto dot = -dot_count; dot <= dot_count; ++dot )
    {
        auto const x
            = center.x
            + half_width * static_cast< float32 >( dot ) / static_cast< float32 >( dot_count );
        draw_list->AddCircle( ImVec2( x, center.y ), dot == 0 ? 5.0F : 3.0F, tick_color, 12, 1.5F );
    }

    if ( !valid )
    {
        auto const flag_min = ImVec2( center.x - radius * 0.35F, center.y + radius * 0.15F );
        draw_list->AddRectFilled(
            flag_min,
            ImVec2( center.x + radius * 0.35F, center.y + radius * 0.35F ),
            flag_color,
            3.0F
        );
        auto const text = ImGui::CalcTextSize( "OFF" );
        draw_list->AddText(
            ImVec2( center.x - 0.5F * text.x, center.y + radius * 0.25F - 0.5F * text.y ),
            IM_COL32_WHITE,
            "OFF"
        );
        return;
    }

    // Pointer to the station.
    auto const to_station = reading.radial_deg + 180.0F - course_deg_;
    draw_list->AddLine(
        polar( center, radius * 0.35F, to_station ),
        polar( center, radius * 0.6F, to_station ),
        station_color,
        3.0F
    );
    auto const tip = polar( center, radius * 0.62F, to_station );
    draw_list->AddTriangleFilled(
        tip,
        polar( center, radius * 0.55F, to_station - 6.0F ),
        polar( center, radius * 0.55F, to_station + 6.0F ),
        station_color
    );

    // Deviation needle.
    auto const deviation = course_deviation( reading.radial_deg, course_deg_ );
    auto const offset = half_width * std::clamp( deviation.degrees / full_scale_deg, -1.0F, 1.0F );
    draw_list->AddLine(
        ImVec2( center.x + offset, center.y - radius * 0.6F ),
        ImVec2( center.x + offset, center.y + radius * 0.6F ),
        needle_color,
        3.0F
    );

    // TO/FROM flag.
    auto const* const flag = deviation.to ? "TO" : "FROM";
    auto const        flag_pos
        = ImVec2( center.x + radius * 0.3F, center.y + ( deviation.to ? -0.25F : 0.15F ) * radius );
    draw_list->AddText( flag_pos, index_color, flag );
}

} // namespace ltb::radio
