//////////////////////////////////////////////////////////////////////

#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <windowsx.h>

#include <gl/GL.h>

#include "Wglext.h"
#include "glcorearb.h"

#include "gl_window.h"
#include "gl_functions.h"
#include "polypartition.h"

#include "gl_drawer.h"
#include "gerber_lib.h"
#include "gerber_log.h"

#include "polypartition.h"

LOG_CONTEXT("gl_drawer", debug);

namespace
{
    using namespace gerber_lib;

    bool is_clockwise(std::vector<TPPLPoint> const &points)
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

        g->draw(*this);

        // create and fill in the GL vertex/index buffers

        vertex_array.init(*program, (GLsizei)vertices.size(), (GLsizei)indices.size());
        vertex_array.activate();

        gl_vertex_solid *v = (gl_vertex_solid *)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
        memcpy(v, vertices.data(), vertices.size() * sizeof(gl_vertex_solid));
        glUnmapBuffer(GL_ARRAY_BUFFER);

        GLuint *i = (GLuint *)glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
        memcpy(i, indices.data(), indices.size() * sizeof(GLuint));
        glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);

        LOG_DEBUG("DONE");
    }

    //////////////////////////////////////////////////////////////////////

    void gl_drawer::fill_elements(gerber_draw_element const *elements, size_t num_elements, gerber_polarity polarity, int entity_id)
    {
        double constexpr THRESHOLD = 1e-20;
        double constexpr ARC_DEGREES = 2;

        std::vector<TPPLPoint> points;

        // add a point to the list if it's more than ## away from the previous point

        int index = 0;
        auto add_point = [&](double x, double y) {
            if(points.empty() || fabs(points.back().x - x) > THRESHOLD || fabs(points.back().y - y) > THRESHOLD) {
                points.emplace_back(x, y, index++);
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

        // reverse points if they're clockwise

        if(is_clockwise(points)) {
            std::reverse(points.begin(), points.end());
            for(int i = 0; i < (int)points.size(); ++i) {
                points[i].id = i;
            }
        }

        // if last point == first point, bin it
        if(points.back().x == points.front().x && points.back().y == points.front().y) {
            points.pop_back();
        }

        // triangulate the points

        TPPLPoly poly;
        poly.Init((long)points.size());
        memcpy(poly.GetPoints(), points.data(), sizeof(TPPLPoint) * points.size());

        TPPLPolyList triangles;
        triangles.clear();

        TPPLPartition part;
        if(!part.Triangulate_MONO(&poly, &triangles)) {
            LOG_ERROR("Can't triangulate...?");
            // add a rectangle to show where the problem is?
            return;
        }

        // add verts and indices to the vert/index arrays

        uint32_t index_offset = (uint32_t)indices.size();
        uint32_t vert_offset = (uint32_t)vertices.size();

        for(auto const &point : points) {
            vertices.emplace_back((float)point.x, (float)point.y);
        }

        for(auto const &t : triangles) {
            if(t.GetNumPoints() != 3) {
                LOG_ERROR("Huh?");
            } else {
                for(int n = 0; n < 3; ++n) {
                    indices.push_back(t.GetPoint(n).id + vert_offset);
                }
            }
        }

        // register the draw call for this element

        uint32_t length = (uint32_t)(indices.size() - index_offset);
        draw_calls.emplace_back(index_offset, length, polarity == polarity_clear ? draw_call_flag_clear : 0);
    }

    //////////////////////////////////////////////////////////////////////

    void gl_drawer::draw()
    {
        if(draw_calls.empty()) {
            return;
        }

        vertex_array.activate();

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        for(auto const &d : draw_calls) {
            uint32_t color = 0x4000ff00;
            if(d.flags & draw_call_flag_clear) {
                color = 0x40000000;
            }
            program->set_color(color);
            glDrawElements(GL_TRIANGLES, d.length, GL_UNSIGNED_INT, (GLvoid *)(d.offset * sizeof(GLuint)));
        }
    }

}    // namespace gerber_3d
