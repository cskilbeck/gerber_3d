#pragma once

#include <vector>
#include <string>
#include <format>
#include <algorithm>

#include "gerber_math.h"
#include "gerber_util.h"

namespace gerber_lib
{
    namespace gerber_2d
    {
        struct matrix;

        //////////////////////////////////////////////////////////////////////

        struct vec2d
        {

            double x{};
            double y{};

            vec2d() = default;

            vec2d(double x, double y) : x(x), y(y)
            {
            }

            //////////////////////////////////////////////////////////////////////

            vec2d(double x, double y, matrix const &transform_matrix);

            //////////////////////////////////////////////////////////////////////

            vec2d(vec2d const &o, matrix const &transform_matrix);

            //////////////////////////////////////////////////////////////////////

            vec2d scale(double scale) const
            {
                return { x * scale, y * scale };
            }

            //////////////////////////////////////////////////////////////////////

            vec2d add(vec2d const &v) const
            {
                return { x + v.x, y + v.y };
            }

            //////////////////////////////////////////////////////////////////////

            vec2d subtract(vec2d const &v) const
            {
                return { x - v.x, y - v.y };
            }

            //////////////////////////////////////////////////////////////////////

            vec2d multiply(vec2d const &v) const
            {
                return { x * v.x, y * v.y };
            }

            //////////////////////////////////////////////////////////////////////

            double length_squared() const
            {
                return x * x + y * y;
            }

            //////////////////////////////////////////////////////////////////////

            double length() const
            {
                return sqrt(length_squared());
            }

            //////////////////////////////////////////////////////////////////////

            std::string to_string() const
            {
                return std::format("({},{})", x, y);
            }
        };

        //////////////////////////////////////////////////////////////////////

        struct rect
        {
            vec2d min_pos{};
            vec2d max_pos{};

            //////////////////////////////////////////////////////////////////////

            std::string to_string() const
            {
                return std::format("{}..{}", min_pos.to_string(), max_pos.to_string());
            }

            //////////////////////////////////////////////////////////////////////

            rect() = default;

            //////////////////////////////////////////////////////////////////////

            rect(double x1, double y1, double x2, double y2) : min_pos(x1, y1), max_pos(x2, y2)
            {
            }

            //////////////////////////////////////////////////////////////////////

            rect(vec2d const &min, vec2d const &max) : min_pos(min), max_pos(max)
            {
            }

            //////////////////////////////////////////////////////////////////////

            bool contains(vec2d const &p) const
            {
                return p.x >= min_pos.x && p.x <= max_pos.x && p.y >= min_pos.y && p.y <= max_pos.y;
            }
        };

        //////////////////////////////////////////////////////////////////////

        struct matrix
        {
            double A,B;
            double C,D;
            double X,Y;

            //////////////////////////////////////////////////////////////////////

            matrix() = default;

            //////////////////////////////////////////////////////////////////////

            matrix(matrix const &o)
            {
                A = o.A;
                B = o.B;
                C = o.C;
                D = o.D;
                X = o.X;
                Y = o.Y;
            }

            //////////////////////////////////////////////////////////////////////

            matrix(double a, double b, double c, double d, double x, double y) : A(a), B(b), C(c), D(d), X(x), Y(y)
            {
            }

            //////////////////////////////////////////////////////////////////////

            std::string to_string() const
            {
                return std::format("MATRIX: A:{}, B:{}, C:{}, D:{}, X:{}, Y:{}", A, B, C, D, X, Y);
            }
        };

        matrix matrix_multiply(matrix const &l, matrix const &r);
        matrix make_identity();
        matrix make_translation(vec2d const &offset);
        matrix make_rotation(double angle_degrees);
        matrix make_scale(vec2d const &scale);
        matrix make_rotate_around(double angle_degrees, vec2d const &pos);
        vec2d transform_point(matrix const &m, vec2d const &p);

        //////////////////////////////////////////////////////////////////////

        template <typename T> void transform_points(matrix const &m, T &points)
        {
            for(auto &p : points) {
                p = vec2d(p.x, p.y, m);
            }
        }

        //////////////////////////////////////////////////////////////////////

        inline vec2d mid_point(rect const &r)
        {
            return vec2d{(r.min_pos.x + r.max_pos.x) / 2, (r.min_pos.y + r.max_pos.y) / 2};
        }

        //////////////////////////////////////////////////////////////////////

        inline double width(rect const &r)
        {
            return r.max_pos.x - r.min_pos.x;
        }

        //////////////////////////////////////////////////////////////////////

        inline double height(rect const &r)
        {
            return r.max_pos.y - r.min_pos.y;
        }

        //////////////////////////////////////////////////////////////////////

        inline vec2d size(rect const &r)
        {
            return vec2d{ width(r), height(r) };
        }

        //////////////////////////////////////////////////////////////////////

        void get_arc_extents(vec2d const &center, double radius, double start_degrees, double end_degrees, rect &extent);

    }    // namespace gerber_2d

}    // namespace gerber_lib

GERBER_MAKE_FORMATTER(gerber_lib::gerber_2d::vec2d);
GERBER_MAKE_FORMATTER(gerber_lib::gerber_2d::rect);
GERBER_MAKE_FORMATTER(gerber_lib::gerber_2d::matrix);

