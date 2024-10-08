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
            vec2d(double x, double y);
            vec2d(double x, double y, matrix const &transform_matrix);
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

            vec2d divide(vec2d const &v) const
            {
                return { x / v.x, y / v.y };
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

            vec2d negate() const
            {
                return { -x, -y };
            }

            //////////////////////////////////////////////////////////////////////

            std::string to_string() const
            {
                return std::format("(X:{:7.3f},Y:{:7.3f})", x, y);
            }
        };
    }    // namespace gerber_2d
}    // namespace gerber_lib
GERBER_MAKE_FORMATTER(gerber_lib::gerber_2d::vec2d);

namespace gerber_lib
{
    namespace gerber_2d
    {
        //////////////////////////////////////////////////////////////////////

        struct rect
        {
            vec2d min_pos{};
            vec2d max_pos{};

            //////////////////////////////////////////////////////////////////////

            std::string to_string() const
            {
                return std::format("(MIN:{} MAX:{})", min_pos, max_pos);
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
            // this orders min_pos, max_pos correctly

            rect normalize() const;

            //////////////////////////////////////////////////////////////////////

            bool contains(vec2d const &p) const
            {
                return p.x >= min_pos.x && p.x <= max_pos.x && p.y >= min_pos.y && p.y <= max_pos.y;
            }

            //////////////////////////////////////////////////////////////////////

            double x() const
            {
                return min_pos.x;
            }

            //////////////////////////////////////////////////////////////////////

            double y() const
            {
                return min_pos.y;
            }

            //////////////////////////////////////////////////////////////////////

            double width() const
            {
                return max_pos.x - min_pos.x;
            }

            //////////////////////////////////////////////////////////////////////

            double height() const
            {
                return max_pos.y - min_pos.y;
            }

            //////////////////////////////////////////////////////////////////////
 
            rect offset(vec2d o)
            {
                return { min_pos.add(o), max_pos.add(o) };
            }

            //////////////////////////////////////////////////////////////////////

            vec2d mid_point() const
            {
                return vec2d{ (min_pos.x + max_pos.x) / 2, (min_pos.y + max_pos.y) / 2 };
            }

            //////////////////////////////////////////////////////////////////////

            vec2d size() const
            {
                return vec2d{ width(), height() };
            }

            //////////////////////////////////////////////////////////////////////

            double aspect_ratio() const
            {
                double w = width();
                double h = height();
                if(h != 0) {
                    return w / h;
                }
                return 0;
            }
        };

        //////////////////////////////////////////////////////////////////////

        struct matrix
        {
            double A, B;
            double C, D;
            double X, Y;

            //////////////////////////////////////////////////////////////////////

            matrix() = default;

            //////////////////////////////////////////////////////////////////////

            matrix(double a, double b, double c, double d, double x, double y) : A(a), B(b), C(c), D(d), X(x), Y(y)
            {
            }

            //////////////////////////////////////////////////////////////////////

            std::string to_string() const
            {
                return std::format("MATRIX: A:{}, B:{}, C:{}, D:{}, X:{}, Y:{}", A, B, C, D, X, Y);
            }

            static matrix multiply(matrix const &l, matrix const &r);
            static matrix identity();
            static matrix translate(vec2d const &offset);
            static matrix rotate(double angle_degrees);
            static matrix scale(vec2d const &scale);
            static matrix rotate_around(double angle_degrees, vec2d const &pos);
            static matrix invert(matrix const &m);
        };

        vec2d transform_point(matrix const &m, vec2d const &p);

        //////////////////////////////////////////////////////////////////////

        template <typename T> void transform_points(matrix const &m, T &points)
        {
            for(auto &p : points) {
                p = vec2d(p.x, p.y, m);
            }
        }

        //////////////////////////////////////////////////////////////////////

        void get_arc_extents(vec2d const &center, double radius, double start_degrees, double end_degrees, rect &extent);

    }    // namespace gerber_2d

}    // namespace gerber_lib

GERBER_MAKE_FORMATTER(gerber_lib::gerber_2d::rect);
GERBER_MAKE_FORMATTER(gerber_lib::gerber_2d::matrix);
