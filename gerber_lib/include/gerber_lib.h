//////////////////////////////////////////////////////////////////////
// Experiment:
// Use C++ 20 but
// Don't really use OOP much
// Don't give a shit about CPU performance
// Don't give a shit about memory efficiency
// Don't give a shit about compile times
// Don't give a shit about leaks
// Make the API minimal
// Make it conformant

#pragma once

#include "gerber_error.h"
#include "gerber_stats.h"
#include "gerber_image.h"
#include "gerber_state.h"
#include "gerber_level.h"
#include "gerber_reader.h"
#include "gerber_draw.h"
#include "gerber_arc.h"

namespace gerber_lib
{
    struct gerber_net;
    struct gerber_macro_parameters;

    //////////////////////////////////////////////////////////////////////

    struct gerber
    {
        static constexpr int min_aperture = 10;
        static constexpr int max_num_apertures = 9999;

        double image_scale_a{ 1.0 };
        double image_scale_b{ 1.0 };
        double image_rotation{ 0.0 };

        gerber_2d::matrix aperture_matrix;

        bool knockout_measure{ false };
        gerber_2d::vec2d knockout_limit_min{};
        gerber_2d::vec2d knockout_limit_max{};

        gerber_level knockout_level{};

        int current_net_id{};

        bool is_gerber_274d(std::string file_path)
        {
            return false;
        }

        bool is_gerber_rs274x(std::string file_path)
        {
            return true;
        }

        void update_net_bounds(gerber_2d::rect &bounds, std::vector<gerber_2d::vec2d> const &points) const;
        void update_net_bounds(gerber_2d::rect &bounds, double x, double y, double w, double h) const;
        void update_image_bounds(gerber_2d::rect &bounds, double repeat_offset_x, double repeat_offset_y, gerber_image &image);

        gerber_error_code get_aperture_points(gerber_macro_parameters const &macro, gerber_net *net, std::vector<gerber_2d::vec2d> &points);

        gerber_error_code parse_file(char const *file_path);

        gerber_error_code draw(gerber_draw_interface &drawer, int const hide_elements, int start_net_index = 0, int end_net_index = 0);
        gerber_error_code fill_region_path(gerber_draw_interface &drawer, size_t net_index, gerber_polarity polarity);
        gerber_error_code draw_linear_track(gerber_draw_interface &drawer,vec2d start, vec2d end, double width, gerber_polarity polarity);
        gerber_error_code fill_polygon(gerber_draw_interface &drawer, double diameter, int num_sides, double angle_degrees);
        gerber_error_code draw_macro(gerber_draw_interface &drawer, gerber_net *current_net, gerber_aperture *const macro_aperture);
        gerber_error_code draw_capsule(gerber_draw_interface &drawer, vec2d const &center, double width, double height, gerber_polarity polarity);
        gerber_error_code draw_arc(gerber_draw_interface &drawer, gerber_arc const &arc, double thickness, gerber_polarity polarity);
        gerber_error_code draw_circle(gerber_draw_interface &drawer, vec2d const &pos, double radius, gerber_polarity polarity);
        gerber_error_code parse_gerber_segment(gerber_net *net);

        gerber_error_code parse_g_code();
        gerber_error_code parse_d_code();
        gerber_error_code parse_tf_code();
        bool parse_m_code();

        gerber_error_code parse_rs274x(gerber_net *net);

        gerber_error_code parse_aperture_definition(gerber_aperture *aperture, gerber_image *image, double unit_scale, int *aperture_number);

        void update_knockout_measurements();

        gerber_stats stats{};
        gerber_image image{};
        gerber_state state{};
        gerber_reader reader{};

        gerber() = default;
    };

}    // namespace gerber_lib
