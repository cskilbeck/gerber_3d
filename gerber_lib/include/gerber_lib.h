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
#include "gerber_entity.h"
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

        int accuracy_decimal_places{ 6 };

        int current_net_id{};

        gerber_stats stats{};
        gerber_image image{};
        gerber_state state{};
        gerber_reader reader{};

        std::vector<gerber_entity> entities;

        bool is_gerber_274d(std::string file_path)
        {
            return false;
        }

        bool is_gerber_rs274x(std::string file_path)
        {
            return true;
        }

        void cleanup();

        void update_net_bounds(gerber_2d::rect &bounds, std::vector<gerber_2d::vec2d> const &points) const;
        void update_net_bounds(gerber_2d::rect &bounds, double x, double y, double w, double h) const;
        void update_image_bounds(gerber_2d::rect &bounds, double repeat_offset_x, double repeat_offset_y, gerber_image &cur_image);

        gerber_error_code get_aperture_points(gerber_macro_parameters const &macro, gerber_net *net, std::vector<gerber_2d::vec2d> &points);

        gerber_error_code parse_file(char const *file_path);

        gerber_error_code draw(gerber_draw_interface &drawer) const;
        gerber_error_code fill_region_path(gerber_draw_interface &drawer, size_t net_index, gerber_polarity polarity) const;

        gerber_error_code draw_linear_interpolation(gerber_draw_interface &drawer, gerber_net *net, gerber_aperture *aperture) const;
        gerber_error_code draw_linear_circle(gerber_draw_interface &drawer, gerber_net *net, gerber_aperture *aperture) const;
        gerber_error_code draw_linear_rectangle(gerber_draw_interface &drawer, gerber_net *net, gerber_aperture *aperture) const;
        gerber_error_code draw_macro(gerber_draw_interface &drawer, gerber_net *net, gerber_aperture *const macro_aperture) const;
        gerber_error_code draw_capsule(gerber_draw_interface &drawer, gerber_net *net, double width, double height) const;
        gerber_error_code draw_arc(gerber_draw_interface &drawer, gerber_net *net, double thickness) const;
        gerber_error_code draw_circle(gerber_draw_interface &drawer, gerber_net *net, vec2d const &pos, double radius) const;
        gerber_error_code draw_rectangle(gerber_draw_interface &drawer, gerber_net *net, rect const &r) const;

        gerber_error_code fill_polygon(gerber_draw_interface &drawer, double diameter, int num_sides, double angle_degrees) const;

        gerber_error_code parse_gerber_segment(gerber_net *net);

        gerber_error_code parse_g_code();
        gerber_error_code parse_d_code();
        gerber_error_code parse_tf_code();
        bool parse_m_code();

        gerber_error_code parse_rs274x(gerber_net *net);

        gerber_error_code parse_aperture_definition(gerber_aperture *aperture, gerber_image *cur_image, double unit_scale);

        void update_knockout_measurements();

        gerber() = default;

        ~gerber() = default;
    };

}    // namespace gerber_lib
