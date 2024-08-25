#include "math.h"

#include "gerber_2d.h"

namespace gerber_lib
{
    namespace gerber_2d
    {
        //////////////////////////////////////////////////////////////////////

        vec2d::vec2d(double x, double y, matrix const &m) : x(x * m.A + y * m.C + m.X), y(x * m.B + y * m.D + m.Y)
        {
        }

        //////////////////////////////////////////////////////////////////////

        vec2d::vec2d(vec2d const &o, matrix const &m) : vec2d(o.x, o.y, m)
        {
        }

        //////////////////////////////////////////////////////////////////////

        matrix matrix_multiply(matrix const &l, matrix const &r)
        {
            return matrix(l.A * r.A + l.B * r.C,          //
                          l.A * r.B + l.B * r.D,          //
                          l.C * r.A + l.D * r.C,          //
                          l.C * r.B + l.D * r.D,          //
                          l.X * r.A + l.Y * r.C + r.X,    //
                          l.X * r.B + l.Y * r.D + r.Y);
        }

        //////////////////////////////////////////////////////////////////////

        matrix make_identity()
        {
            return matrix(1.0, 0.0, 0.0, 1.0, 0.0, 0.0);
        }

        //////////////////////////////////////////////////////////////////////

        matrix make_translation(vec2d const &offset)
        {
            return matrix(1.0, 0.0, 0.0, 1.0, offset.x, offset.y);
        }

        //////////////////////////////////////////////////////////////////////

        matrix make_rotation(double angle_degrees)
        {
            double radians = deg_2_rad(angle_degrees);
            double s = sin(radians);
            double c = cos(radians);
            return matrix(c, s, -s, c, 0.0, 0.0);
        }

        //////////////////////////////////////////////////////////////////////

        matrix make_scale(vec2d const &scale)
        {
            return matrix(scale.x, 0.0, 0.0, scale.y, 0.0, 0.0);
        }

        //////////////////////////////////////////////////////////////////////

        matrix make_rotate_around(double angle_degrees, vec2d const &pos)
        {
            matrix m;
            m = make_translation({ -pos.x, -pos.y });
            m = matrix_multiply(make_rotation(angle_degrees), m);
            m = matrix_multiply(make_translation(pos), m);
            return m;
        }

        //////////////////////////////////////////////////////////////////////

        vec2d transform_point(matrix const &m, vec2d const &p)
        {
            return vec2d(p.x, p.y, m);
        }

        //////////////////////////////////////////////////////////////////////

        void get_arc_extents(vec2d const &center, double radius, double start_degrees, double end_degrees, rect &extent)
        {
            // normalize the angles

            start_degrees = fmod(start_degrees, 360.0);
            if(start_degrees < 0.0) {
                start_degrees += 360.0;
            }

            end_degrees = fmod(end_degrees, 360.0);
            if(end_degrees < 0.0) {
                end_degrees += 360.0;
            }

            if(start_degrees >= end_degrees) {
                end_degrees += 360.0;
            }

            // get the endpoints of the arc

            auto arc_point = [&](double d) {
                double radians = deg_2_rad(d);
                return vec2d{ center.x + cos(radians) * radius, center.y + sin(radians) * radius };
            };

            vec2d start_point = arc_point(start_degrees);
            vec2d end_point = arc_point(end_degrees);

            // initial extents are just the start and end points

            extent.min_pos = { std::min(start_point.x, end_point.x), std::min(start_point.y, end_point.y) };
            extent.max_pos = { std::max(start_point.x, end_point.x), std::max(start_point.y, end_point.y) };

            // check if the arc goes through any cardinal points

            auto crosses_cardinal = [&](double d) {
                return start_degrees <= d && d < end_degrees;
            };

            if(crosses_cardinal(0) || crosses_cardinal(360)) {
                extent.max_pos.x = center.x + radius;
            }
            if(crosses_cardinal(90)) {
                extent.max_pos.y = center.y + radius;
            }
            if(crosses_cardinal(180)) {
                extent.min_pos.x = center.x - radius;
            }
            if(crosses_cardinal(270)) {
                extent.min_pos.y = center.y - radius;
            }
        }

    }    // namespace gerber_2d

}    // namespace gerber_lib
