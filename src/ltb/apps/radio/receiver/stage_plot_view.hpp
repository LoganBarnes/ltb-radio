// ///////////////////////////////////////////////////////////////////////////////////////
// A Logan Thomas Barnes project
// ///////////////////////////////////////////////////////////////////////////////////////
#pragma once

// project
#include "ltb/apps/radio/app_view.hpp"
#include "ltb/radio/imgui_viewport.hpp"
#include "ltb/radio/plot_camera.hpp"
#include "ltb/radio/plot_line_pipeline.hpp"
#include "ltb/radio/radio_engine.hpp"

// standard
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace ltb::radio
{

// Plots one stage of a radio's pipeline as a waveform or spectrum.
//
// Drag to pan, scroll to zoom time/frequency (Alt: amplitude, Ctrl: both), double-click to fit.
// Right-click a spectrum to tune to that frequency.
class StagePlotView : public AppView
{
public:
    StagePlotView( RadioEngine& engine, std::size_t stage, std::string name );

    [[nodiscard( "Const getter" )]]
    auto name( ) const -> std::string const& override;

    auto initialize( SubAppContext const& context ) -> utils::Result< void > override;
    auto configure_gui( uint32 frame_index ) -> void override;
    auto record_commands( vlk::objs::FrameInfo const& frame ) -> utils::Result< void > override;

private:
    enum class Mode
    {
        Time,
        Spectrum,
    };

    struct Trace
    {
        std::vector< glm::vec2 > points;
        glm::vec4                color;
        char const*              label;
    };

    RadioEngine&   engine_;
    std::size_t    stage_;
    dsp::StageInfo info_;
    std::string    name_;

    std::unique_ptr< ImguiViewport >    viewport_ = nullptr;
    std::unique_ptr< PlotLinePipeline > lines_    = nullptr;
    PlotCamera                          camera_   = { };

    Mode    mode_           = Mode::Time;
    bool    show_i_         = true;
    bool    show_q_         = true;
    bool    show_magnitude_ = false;
    bool    frozen_         = false;
    bool    follow_y_       = false;
    int     time_size_log2_ = 11;
    int     fft_size_log2_  = 12;
    float32 averaging_      = 0.7F;
    bool    needs_fit_      = true;
    uint64  data_version_   = std::numeric_limits< uint64 >::max( );

    std::vector< dsp::Complex > samples_  = { };
    std::vector< float32 >      spectrum_ = { };
    std::vector< Trace >        traces_   = { };

    [[nodiscard( "Const getter" )]]
    auto is_complex( ) const -> bool;

    // Complex samples whose spectrum maps to radio frequencies (so it can be tuned).
    [[nodiscard( "Const getter" )]]
    auto is_rf( ) const -> bool;

    auto gui_toolbar( ) -> void;
    auto update_data( ) -> void;
    auto update_spectrum( ) -> void;
    auto build_traces( ) -> void;
    // Returns false if there is no data to fit yet.
    auto fit( bool x_axis ) -> bool;
    auto handle_input( ) -> void;
    auto draw_overlay( ) const -> void;
    auto draw_channel_marker( ) const -> void;
    auto tune_to( float32 plot_x ) -> void;

    // Converts a plot X coordinate to text (e.g. "100.300 MHz") given the tick spacing.
    [[nodiscard( "Const computation" )]]
    auto x_label( float32 x, float32 step ) const -> std::string;

    // The absolute frequency at the center of a complex stage's spectrum.
    [[nodiscard( "Const computation" )]]
    auto spectrum_center_hz( ) const -> float64;
};

} // namespace ltb::radio
