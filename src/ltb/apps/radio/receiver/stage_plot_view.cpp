// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#include "stage_plot_view.hpp"

// project
#include "ltb/radio/dsp/fft.hpp"
#include "ltb/vlk/check.hpp"

// external
#include <imgui.h>
#include <spdlog/spdlog.h>

// standard
#include <algorithm>
#include <cmath>
#include <format>
#include <ranges>

namespace ltb::radio
{
namespace
{

constexpr auto min_size_log2   = 8;
constexpr auto max_size_log2   = 14;
constexpr auto max_plot_points = 3U * ( 1U << max_size_log2 ) + 16U;
constexpr auto ms_per_second   = 1000.0F;
constexpr auto hz_per_khz      = 1000.0F;
constexpr auto tick_target_px  = 80.0F;

constexpr auto i_color         = glm::vec4( 0.35F, 0.75F, 1.0F, 1.0F );
constexpr auto q_color         = glm::vec4( 1.0F, 0.55F, 0.3F, 1.0F );
constexpr auto magnitude_color = glm::vec4( 0.45F, 1.0F, 0.45F, 1.0F );
constexpr auto grid_color      = IM_COL32( 255, 255, 255, 28 );
constexpr auto axis_color      = IM_COL32( 255, 255, 255, 70 );
constexpr auto label_color     = IM_COL32( 210, 210, 210, 200 );
constexpr auto channel_fill    = IM_COL32( 255, 200, 0, 36 );
constexpr auto channel_line    = IM_COL32( 255, 200, 0, 170 );
constexpr auto cursor_color    = IM_COL32( 255, 255, 255, 90 );

auto to_imvec( glm::vec2 const v ) -> ImVec2
{
    return { v.x, v.y };
}

auto to_imcolor( glm::vec4 const& color ) -> ImU32
{
    return ImGui::ColorConvertFloat4ToU32( ImVec4( color.r, color.g, color.b, color.a ) );
}

// 1, 2 or 5 times a power of ten so about one tick fits in `target` units.
auto nice_step( float32 const target ) -> float32
{
    auto const magnitude  = std::pow( 10.0F, std::floor( std::log10( target ) ) );
    auto const normalized = target / magnitude;
    auto const factor     = normalized < 1.5F
                              ? 1.0F
                              : ( normalized < 3.5F ? 2.0F : ( normalized < 7.5F ? 5.0F : 10.0F ) );
    return factor * magnitude;
}

auto decimals_for( float64 const step ) -> int
{
    return std::clamp( static_cast< int >( -std::floor( std::log10( step ) ) ), 0, 9 );
}

auto size_combo( char const* const label, int& size_log2 ) -> bool
{
    auto changed = false;
    ImGui::SetNextItemWidth( ImGui::CalcTextSize( "000000" ).x + ImGui::GetFrameHeight( ) * 1.5F );
    if ( ImGui::BeginCombo( label, std::to_string( 1 << size_log2 ).c_str( ) ) )
    {
        for ( auto log2 = min_size_log2; log2 <= max_size_log2; ++log2 )
        {
            if ( ImGui::Selectable( std::to_string( 1 << log2 ).c_str( ), log2 == size_log2 ) )
            {
                size_log2 = log2;
                changed   = true;
            }
        }
        ImGui::EndCombo( );
    }
    return changed;
}

auto format_rate( float64 const rate_hz ) -> std::string
{
    return rate_hz >= 1.0e6 ? std::format( "{:g} MS/s", rate_hz / 1.0e6 )
                            : std::format( "{:g} kS/s", rate_hz / 1.0e3 );
}

struct PlotRange
{
    glm::vec2 min = glm::vec2( std::numeric_limits< float32 >::max( ) );
    glm::vec2 max = glm::vec2( std::numeric_limits< float32 >::lowest( ) );

    auto operator( )( glm::vec2 const point ) -> void
    {
        min = glm::min( min, point );
        max = glm::max( max, point );
    }
};

} // namespace

StagePlotView::StagePlotView( RadioEngine& engine, std::size_t const stage, std::string name )
    : engine_( engine )
    , stage_( stage )
    , info_( engine.stages( ).at( stage ) )
    , name_( std::move( name ) )
    , mode_( info_.kind == dsp::StageKind::Real ? Mode::Time : Mode::Spectrum )
{
}

auto StagePlotView::name( ) const -> std::string const&
{
    return name_;
}

auto StagePlotView::initialize( SubAppContext const& context ) -> utils::Result< void >
{
    viewport_ = std::make_unique< ImguiViewport >( context.gpu );
    LTB_CHECK( viewport_->initialize( {
        .frame_count = context.frame_count,
        .clear_color = glm::vec4( 0.06F, 0.06F, 0.08F, 1.0F ),
    } ) );

    lines_ = std::make_unique< PlotLinePipeline >( context.gpu, viewport_->render_pass( ) );
    LTB_CHECK(
        lines_->initialize( { .frame_count = context.frame_count, .max_points = max_plot_points } )
    );

    return utils::success( );
}

auto StagePlotView::configure_gui( uint32 const frame_index ) -> void
{
    ImGui::SetNextWindowSize( ImVec2( 560.0F, 320.0F ), ImGuiCond_FirstUseEver );
    auto const offset = 30.0F * static_cast< float32 >( stage_ );
    ImGui::SetNextWindowPos( ImVec2( 380.0F + offset, 60.0F + offset ), ImGuiCond_FirstUseEver );

    if ( ImGui::Begin( name_.c_str( ), &is_open ) )
    {
        this->gui_toolbar( );
        this->update_data( );
        this->build_traces( );

        if ( needs_fit_ )
        {
            needs_fit_ = !this->fit( true );
        }
        else if ( follow_y_ )
        {
            utils::ignore( this->fit( false ) );
        }

        if ( auto result = viewport_->show( frame_index ); !result )
        {
            spdlog::error( "Viewport failed: {}", result.error( ).debug_error_message( ) );
        }
        this->handle_input( );
        this->draw_overlay( );
    }
    ImGui::End( );
}

auto StagePlotView::record_commands( vlk::objs::FrameInfo const& frame ) -> utils::Result< void >
{
    auto lines = std::vector< PlotLine >( );
    for ( auto const& trace : traces_ )
    {
        lines.push_back( { .points = trace.points, .color = trace.color } );
    }

    LTB_CHECK( viewport_->begin_render_pass( frame ) );
    LTB_CHECK( lines_->draw( frame, camera_.clip_from_plot( ), lines ) );
    viewport_->end_render_pass( frame );

    return utils::success( );
}

auto StagePlotView::is_complex( ) const -> bool
{
    return info_.kind != dsp::StageKind::Real;
}

auto StagePlotView::is_rf( ) const -> bool
{
    return info_.kind == dsp::StageKind::RfInput || info_.kind == dsp::StageKind::Baseband;
}

auto StagePlotView::gui_toolbar( ) -> void
{
    auto const previous_mode = mode_;
    if ( ImGui::RadioButton( "Time", mode_ == Mode::Time ) )
    {
        mode_ = Mode::Time;
    }
    ImGui::SameLine( );
    if ( ImGui::RadioButton( "Spectrum", mode_ == Mode::Spectrum ) )
    {
        mode_ = Mode::Spectrum;
    }

    ImGui::SameLine( );
    ImGui::TextDisabled( "|" );
    ImGui::SameLine( );
    auto size_changed = false;
    if ( mode_ == Mode::Time )
    {
        size_changed = size_combo( "Samples", time_size_log2_ );
        if ( this->is_complex( ) )
        {
            ImGui::SameLine( );
            ImGui::Checkbox( "I", &show_i_ );
            ImGui::SameLine( );
            ImGui::Checkbox( "Q", &show_q_ );
            ImGui::SameLine( );
            ImGui::Checkbox( "|x|", &show_magnitude_ );
        }
    }
    else
    {
        size_changed = size_combo( "FFT", fft_size_log2_ );
        ImGui::SameLine( );
        ImGui::SetNextItemWidth( 80.0F );
        ImGui::SliderFloat( "Avg", &averaging_, 0.0F, 0.95F, "%.2f" );
        if ( ImGui::IsItemHovered( ) )
        {
            ImGui::SetTooltip( "Spectrum averaging (0 = none)" );
        }
    }

    ImGui::SameLine( );
    ImGui::TextDisabled( "|" );
    ImGui::SameLine( );
    ImGui::Checkbox( "Freeze", &frozen_ );
    ImGui::SameLine( );
    ImGui::Checkbox( "Follow Y", &follow_y_ );
    ImGui::SameLine( );
    if ( ImGui::Button( "Fit" ) )
    {
        needs_fit_ = true;
    }

    ImGui::SameLine( );
    ImGui::TextDisabled( "(?)" );
    if ( ImGui::BeginItemTooltip( ) )
    {
        ImGui::PushTextWrapPos( ImGui::GetFontSize( ) * 30.0F );
        ImGui::TextUnformatted( info_.description.c_str( ) );
        ImGui::Spacing( );
        ImGui::TextDisabled( "Drag: pan    Scroll: zoom time/frequency" );
        ImGui::TextDisabled( "Alt+Scroll: zoom amplitude    Ctrl+Scroll: zoom both" );
        ImGui::TextDisabled( "Double-click: fit    Right-click spectrum: tune" );
        ImGui::PopTextWrapPos( );
        ImGui::EndTooltip( );
    }

    ImGui::SameLine( );
    ImGui::TextDisabled( "%s", format_rate( info_.sample_rate_hz ).c_str( ) );

    if ( mode_ != previous_mode || size_changed )
    {
        spectrum_.clear( );
        data_version_ = std::numeric_limits< uint64 >::max( );
        needs_fit_    = true;
    }
}

auto StagePlotView::update_data( ) -> void
{
    auto const& history = engine_.history( stage_ );
    if ( frozen_ || history.version( ) == data_version_ )
    {
        return;
    }
    data_version_ = history.version( );

    auto const size_log2 = mode_ == Mode::Time ? time_size_log2_ : fft_size_log2_;
    history.copy_latest( 1UZ << static_cast< std::size_t >( size_log2 ), samples_ );

    if ( mode_ == Mode::Spectrum )
    {
        this->update_spectrum( );
    }
}

auto StagePlotView::update_spectrum( ) -> void
{
    // Wait for a full FFT's worth of samples.
    if ( !std::has_single_bit( samples_.size( ) )
         || samples_.size( ) != 1UZ << static_cast< std::size_t >( fft_size_log2_ ) )
    {
        return;
    }

    auto latest = std::vector< float32 >( );
    if ( this->is_complex( ) )
    {
        dsp::power_spectrum_db( samples_, latest );
    }
    else
    {
        auto const real = samples_
                        | std::views::transform( []( dsp::Complex const& s ) { return s.real( ); } )
                        | std::ranges::to< std::vector >( );
        dsp::power_spectrum_db( real, latest );
    }

    if ( spectrum_.size( ) != latest.size( ) )
    {
        spectrum_ = std::move( latest );
        return;
    }
    for ( auto&& [ average, value ] : std::views::zip( spectrum_, latest ) )
    {
        average = averaging_ * average + ( 1.0F - averaging_ ) * value;
    }
}

auto StagePlotView::build_traces( ) -> void
{
    traces_.clear( );
    auto const rate = static_cast< float32 >( info_.sample_rate_hz );

    if ( mode_ == Mode::Spectrum )
    {
        auto       trace    = Trace{ .points = { }, .color = i_color, .label = "Power (dB)" };
        auto const bins     = spectrum_.size( );
        auto const fft_size = this->is_complex( ) ? bins : 2UZ * ( bins - 1UZ );
        auto const bin_khz
            = rate / static_cast< float32 >( std::max( fft_size, 1UZ ) ) / hz_per_khz;
        auto const first_bin = this->is_complex( ) ? -static_cast< float32 >( bins / 2UZ ) : 0.0F;
        for ( auto i = 0UZ; i < bins; ++i )
        {
            trace.points.emplace_back(
                ( first_bin + static_cast< float32 >( i ) ) * bin_khz,
                spectrum_[ i ]
            );
        }
        traces_.push_back( std::move( trace ) );
        return;
    }

    auto const time_ms = [ rate ]( std::size_t const i ) {
        return static_cast< float32 >( i ) * ms_per_second / rate;
    };

    auto add_trace
        = [ & ]( bool const shown, glm::vec4 const& color, char const* const label, auto&& value ) {
              if ( !shown )
              {
                  return;
              }
              auto trace = Trace{ .points = { }, .color = color, .label = label };
              trace.points.reserve( samples_.size( ) );
              for ( auto i = 0UZ; i < samples_.size( ); ++i )
              {
                  trace.points.emplace_back( time_ms( i ), value( samples_[ i ] ) );
              }
              traces_.push_back( std::move( trace ) );
          };

    if ( this->is_complex( ) )
    {
        add_trace( show_magnitude_, magnitude_color, "|x|", []( dsp::Complex const& s ) {
            return std::abs( s );
        } );
        add_trace( show_q_, q_color, "Q", []( dsp::Complex const& s ) { return s.imag( ); } );
        add_trace( show_i_, i_color, "I", []( dsp::Complex const& s ) { return s.real( ); } );
    }
    else
    {
        add_trace( true, magnitude_color, "Value", []( dsp::Complex const& s ) {
            return s.real( );
        } );
    }
}

auto StagePlotView::fit( bool const x_axis ) -> bool
{
    auto range = PlotRange( );
    for ( auto const& trace : traces_ )
    {
        range = std::ranges::for_each( trace.points, range ).fun;
    }
    if ( range.min.x >= range.max.x )
    {
        return false;
    }

    auto min = camera_.min( );
    auto max = camera_.max( );
    if ( x_axis )
    {
        min.x = range.min.x;
        max.x = std::max( range.max.x, range.min.x + 1e-3F );
    }

    if ( mode_ == Mode::Spectrum )
    {
        // Ignore the deepest nulls; they hide the noise floor's shape.
        min.y = std::max( range.min.y, range.max.y - 120.0F ) - 5.0F;
        max.y = range.max.y + 10.0F;
    }
    else
    {
        auto const extent
            = std::max( { std::abs( range.min.y ), std::abs( range.max.y ), 1e-6F } ) * 1.1F;
        min.y = -extent;
        max.y = extent;
    }
    camera_.set_bounds( min, max );
    return true;
}

auto StagePlotView::handle_input( ) -> void
{
    auto const& io   = ImGui::GetIO( );
    auto const  size = viewport_->logical_size( );

    if ( viewport_->is_active( ) && ImGui::IsMouseDragging( ImGuiMouseButton_Left, 0.0F ) )
    {
        camera_.pan( { io.MouseDelta.x, io.MouseDelta.y }, size );
    }

    if ( !viewport_->is_hovered( ) )
    {
        return;
    }

    auto const cursor = glm::vec2( io.MousePos.x, io.MousePos.y ) - viewport_->logical_origin( );
    if ( io.MouseWheel != 0.0F )
    {
        auto const axes
            = io.KeyCtrl ? glm::bvec2( true, true ) : glm::bvec2( !io.KeyAlt, io.KeyAlt );
        camera_.zoom( io.MouseWheel, cursor, size, axes );
    }
    if ( ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) )
    {
        needs_fit_ = true;
    }
    if ( mode_ == Mode::Spectrum && this->is_rf( )
         && ImGui::IsMouseClicked( ImGuiMouseButton_Right ) )
    {
        this->tune_to( camera_.plot_from_screen( cursor, size ).x );
    }
}

auto StagePlotView::draw_overlay( ) const -> void
{
    auto const origin = viewport_->logical_origin( );
    auto const size   = viewport_->logical_size( );
    if ( size.x < 1.0F || size.y < 1.0F )
    {
        return;
    }

    auto* const draw_list = ImGui::GetWindowDrawList( );
    draw_list->PushClipRect( to_imvec( origin ), to_imvec( origin + size ), true );

    auto const min  = camera_.min( );
    auto const max  = camera_.max( );
    auto const step = glm::vec2(
        nice_step( ( max.x - min.x ) * tick_target_px / size.x ),
        nice_step( ( max.y - min.y ) * tick_target_px * 0.6F / size.y )
    );
    auto const to_screen
        = [ & ]( glm::vec2 const plot ) { return origin + camera_.screen_from_plot( plot, size ); };

    if ( mode_ == Mode::Spectrum && this->is_rf( ) )
    {
        this->draw_channel_marker( );
    }

    for ( auto x = std::ceil( min.x / step.x ) * step.x; x <= max.x; x += step.x )
    {
        auto const top = to_screen( { x, max.y } );
        draw_list->AddLine( to_imvec( top ), ImVec2( top.x, top.y + size.y ), grid_color );
        draw_list->AddText(
            ImVec2( top.x + 3.0F, origin.y + size.y - ImGui::GetFontSize( ) - 2.0F ),
            label_color,
            this->x_label( x, step.x ).c_str( )
        );
    }
    for ( auto y = std::ceil( min.y / step.y ) * step.y; y <= max.y; y += step.y )
    {
        auto const left  = to_screen( { min.x, y } );
        auto const color = std::abs( y ) < 0.5F * step.y ? axis_color : grid_color;
        draw_list->AddLine( to_imvec( left ), ImVec2( left.x + size.x, left.y ), color );

        auto const unit  = mode_ == Mode::Spectrum ? " dB" : "";
        auto const label = std::format( "{:.{}f}{}", y, decimals_for( step.y ), unit );
        draw_list->AddText(
            ImVec2( origin.x + 3.0F, left.y - ImGui::GetFontSize( ) ),
            label_color,
            label.c_str( )
        );
    }

    // Legend.
    auto legend_y = origin.y + 4.0F;
    for ( auto const& trace : traces_ )
    {
        auto const width = ImGui::CalcTextSize( trace.label ).x;
        draw_list->AddText(
            ImVec2( origin.x + size.x - width - 6.0F, legend_y ),
            to_imcolor( trace.color ),
            trace.label
        );
        legend_y += ImGui::GetFontSize( );
    }

    // Cursor readout.
    if ( viewport_->is_hovered( ) )
    {
        auto const mouse = glm::vec2( ImGui::GetIO( ).MousePos.x, ImGui::GetIO( ).MousePos.y );
        auto const plot  = camera_.plot_from_screen( mouse - origin, size );
        draw_list->AddLine(
            ImVec2( mouse.x, origin.y ),
            ImVec2( mouse.x, origin.y + size.y ),
            cursor_color
        );
        draw_list->AddLine(
            ImVec2( origin.x, mouse.y ),
            ImVec2( origin.x + size.x, mouse.y ),
            cursor_color
        );

        auto const unit    = mode_ == Mode::Spectrum ? " dB" : "";
        auto const readout = std::format(
            "{}, {:.{}f}{}",
            this->x_label( plot.x, step.x * 0.01F ),
            plot.y,
            decimals_for( step.y * 0.01F ),
            unit
        );
        draw_list->AddText(
            ImVec2( mouse.x + 8.0F, mouse.y - ImGui::GetFontSize( ) - 4.0F ),
            IM_COL32_WHITE,
            readout.c_str( )
        );
    }

    draw_list->PopClipRect( );
}

auto StagePlotView::draw_channel_marker( ) const -> void
{
    auto const& settings = engine_.settings( );
    auto const  station  = static_cast< float32 >(
        ( settings.station_frequency_hz - this->spectrum_center_hz( ) ) / hz_per_khz
    );
    auto const half_band
        = static_cast< float32 >( 0.5 * settings.channel_bandwidth_hz / hz_per_khz );

    auto const origin = viewport_->logical_origin( );
    auto const size   = viewport_->logical_size( );
    auto const left  = origin.x + camera_.screen_from_plot( { station - half_band, 0.0F }, size ).x;
    auto const right = origin.x + camera_.screen_from_plot( { station + half_band, 0.0F }, size ).x;
    auto const center = origin.x + camera_.screen_from_plot( { station, 0.0F }, size ).x;

    auto* const draw_list = ImGui::GetWindowDrawList( );
    draw_list->AddRectFilled(
        ImVec2( left, origin.y ),
        ImVec2( right, origin.y + size.y ),
        channel_fill
    );
    draw_list
        ->AddLine( ImVec2( center, origin.y ), ImVec2( center, origin.y + size.y ), channel_line );
}

auto StagePlotView::tune_to( float32 const plot_x ) -> void
{
    auto settings = engine_.settings( );

    // Keep the channel inside the received band.
    auto const limit
        = 0.45 * dsp::RadioPipeline::input_rate_hz - 0.5 * settings.channel_bandwidth_hz;
    auto const station = this->spectrum_center_hz( )
                       + static_cast< float64 >( plot_x ) * static_cast< float64 >( hz_per_khz );
    settings.station_frequency_hz = std::clamp(
        station,
        settings.center_frequency_hz - limit,
        settings.center_frequency_hz + limit
    );

    if ( auto result = engine_.apply( settings ); !result )
    {
        spdlog::error( "Tuning failed: {}", result.error( ).debug_error_message( ) );
    }
}

auto StagePlotView::x_label( float32 const x, float32 const step ) const -> std::string
{
    if ( mode_ == Mode::Time )
    {
        return std::format( "{:.{}f} ms", x, decimals_for( step ) );
    }
    if ( !this->is_rf( ) )
    {
        return std::format( "{:.{}f} kHz", x, decimals_for( step ) );
    }

    auto const frequency_hz = this->spectrum_center_hz( )
                            + static_cast< float64 >( x ) * static_cast< float64 >( hz_per_khz );
    auto const step_hz      = static_cast< float64 >( step ) * static_cast< float64 >( hz_per_khz );
    if ( frequency_hz >= 10.0e6 )
    {
        return std::format( "{:.{}f} MHz", frequency_hz / 1.0e6, decimals_for( step_hz / 1.0e6 ) );
    }
    return std::format( "{:.{}f} kHz", frequency_hz / 1.0e3, decimals_for( step_hz / 1.0e3 ) );
}

auto StagePlotView::spectrum_center_hz( ) const -> float64
{
    auto const& settings = engine_.settings( );
    return info_.kind == dsp::StageKind::RfInput ? settings.center_frequency_hz
                                                 : settings.station_frequency_hz;
}

} // namespace ltb::radio
