//////////////////////////////////////////////////////////////////////

#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <windowsx.h>

#include <gl/GL.h>
#include <gl/GLU.h>

#include "Wglext.h"
#include "glcorearb.h"

#include "gl_window.h"
#include "gl_functions.h"

#include "gl_drawer.h"
#include "gerber_lib.h"
#include "gerber_log.h"

LOG_CONTEXT("gl_drawer", debug);

namespace
{
    using namespace gerber_lib;

    template <typename T> bool is_clockwise(std::vector<T> const &points)
    {
        double t = 0;
        for(size_t i = 0, n = points.size() - 1; i < points.size(); n = i++) {
            t += (points[i].x - points[n].x) * (points[i].y + points[n].y);
        }
        return t >= 0;
    }
}    // namespace

namespace gerber_3d
{
    //////////////////////////////////////////////////////////////////////

    void gl_drawer::set_gerber(gerber *g)
    {
        gerber_file = g;
        triangulator.clear();
        g->draw(*this);

        // create and fill in the GL vertex/index buffers
        vertex_array.init(*program, (GLsizei)triangulator.vertices.size());
        vertex_array.activate();
        indices_triangles.init((GLsizei)triangulator.indices.size());
        indices_triangles.activate();
        triangulator.finalize();

        LOG_DEBUG("DONE");
    }

    //////////////////////////////////////////////////////////////////////

    void gl_drawer::fill_elements(gerber_draw_element const *elements, size_t num_elements, gerber_polarity polarity, int entity_id)
    {
        double constexpr THRESHOLD = 1e-38;
        double constexpr ARC_DEGREES = 2;

        std::vector<vec2d> points;

        // add a point to the list if it's more than ## away from the previous point

        auto add_point = [&](double x, double y) {
            if(points.empty() || fabs(points.back().x - x) > THRESHOLD || fabs(points.back().y - y) > THRESHOLD) {
                points.emplace_back(x, y);
            }
        };

        auto add_arc_point = [&](gerber_draw_element const &element, double t) {
            double radians = deg_2_rad(t);
            double x = cos(radians) * element.radius + element.arc_center.x;
            double y = sin(radians) * element.radius + element.arc_center.y;
            add_point(x, y);
        };

        // create array of points, approximating arcs

        for(size_t n = 0; n < num_elements; ++n) {
            gerber_draw_element const &element = elements[n];
            switch(element.draw_element_type) {
            case draw_element_line:
                add_point(element.line_end.x, element.line_end.y);
                break;
            case draw_element_arc:
                double final_angle = element.end_degrees;
                if(element.start_degrees < element.end_degrees) {
                    for(double t = element.start_degrees; t < element.end_degrees; t += ARC_DEGREES) {
                        add_arc_point(element, t);
                        final_angle = t;
                    }
                } else {
                    for(double t = element.start_degrees; t > element.end_degrees; t -= ARC_DEGREES) {
                        add_arc_point(element, t);
                        final_angle = t;
                    }
                }
                if(final_angle != element.end_degrees) {
                    add_arc_point(element, element.end_degrees);
                }
                break;
            }
        }

        if(points.empty()) {
            return;
        }

        // if last point == first point, bin it
        if(points.back().x == points.front().x && points.back().y == points.front().y) {
            points.pop_back();
        }

        // triangulate the points

        triangulator.append(points.data(), (int)points.size(), polarity == polarity_clear ? 1 : 0);
    }

    //////////////////////////////////////////////////////////////////////

    void gl_drawer::draw()
    {
        program->use();
        vertex_array.activate();
        indices_triangles.activate();

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        for(size_t n = 0; n < triangulator.draw_calls.size(); ++n) {
            tesselator_draw_call &d = triangulator.draw_calls[n];

            uint32_t draw_color = layer_color;
            if(d.flags & 1) {
                draw_color = 0xC0000000;
            }
            program->set_color(draw_color);
            d.draw_filled();

            program->set_color(0x60ffffff);
            d.draw_outline();
        }
        //triangulator.draw_range(0, triangulator.draw_calls.size(), tesselator::draw_flag_filled | tesselator::draw_flag_outline);
    }

}    // namespace gerber_3d
