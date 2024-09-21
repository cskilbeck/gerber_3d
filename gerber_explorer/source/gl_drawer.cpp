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

    bool is_clockwise(std::vector<vec2d> const &points, size_t start, size_t end)
    {
        double sum = 0;
        for(size_t i = start, n = end - 1; i != end; n = i++) {
            vec2d const &p1 = points[i];
            vec2d const &p2 = points[n];
            sum += (p2.x - p1.x) * (p2.y + p1.y);
        }
        return sum < 0;    // Negative sum indicates clockwise orientation

    }
}    // namespace

namespace gerber_3d
{
    //////////////////////////////////////////////////////////////////////

    void gl_drawer::set_gerber(gerber *g)
    {
        gerber_file = g;
        current_entity_id = -1;

        tesselator.clear();

        g->draw(*this);

        tesselator.new_entity(current_entity_id, current_flag);
        tesselator.finalize();
    }

    //////////////////////////////////////////////////////////////////////

    void gl_drawer::fill_elements(gerber_draw_element const *elements, size_t num_elements, gerber_polarity polarity, int entity_id)
    {
        // LOG_DEBUG("ENTITY ID {} HAS {} elements, polarity is {}", entity_id, num_elements, polarity);

        double constexpr THRESHOLD = 1e-38;
        double constexpr ARC_DEGREES = 3.6;

        uint32_t flag = polarity == polarity_clear ? 1 : 0;

        if(entity_id != current_entity_id) {
            tesselator.new_entity(entity_id, flag);
        }

        current_flag = flag;
        current_entity_id = entity_id;

        // we need these points to persist!
        std::vector<vec2d> &points = tesselator.points;
        size_t offset = points.size();

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

            case draw_element_arc: {

                double start = element.start_degrees;
                double end = element.end_degrees;

                double final_angle = end;

                if(start < end) {
                    for(double t = start; t < end; t += ARC_DEGREES) {
                        add_arc_point(element, t);
                        final_angle = t;
                    }
                } else {
                    for(double t = start; t > end; t -= ARC_DEGREES) {
                        add_arc_point(element, t);
                        final_angle = t;
                    }
                }
                if(final_angle != end) {
                    add_arc_point(element, end);
                }
            } break;
            }
        }

        if(points.size() < 3) {
            LOG_WARNING("CULLED SECTION OF ENTITY {}", entity_id);
            return;
        }

        // if last point == first point, bin it
        if(points.back().x == points.front().x && points.back().y == points.front().y) {
            points.pop_back();
        }

        // force clockwise ordering

        if(!is_clockwise(points, offset, points.size())) {
            // LOG_DEBUG("REVERSING entity {} from {} to {}", entity_id, offset, points.size());
            std::reverse(points.begin() + offset, points.end());
        }

        // triangulate the points

        tesselator.append_points(offset);
    }

    //////////////////////////////////////////////////////////////////////

    void gl_drawer::fill_entities(std::list<tesselator_entity const *> const &entities)
    {
        vertex_array.activate();
        indices_triangles.activate();

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        for(auto const e : entities) {
            int end = e->num_fills + e->first_fill;
            for(int i = e->first_fill; i < end; ++i) {
                tesselator_span const &s = tesselator.fills[i];
                glDrawElements(GL_TRIANGLES, s.length, GL_UNSIGNED_INT, (void *)(s.start * sizeof(GLuint)));
            }
        }
    }

    //////////////////////////////////////////////////////////////////////

    void gl_drawer::draw(bool fill, bool outline, bool wireframe, float outline_thickness)
    {
        program->use();
        vertex_array.activate();
        indices_triangles.activate();

        glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        uint32_t constexpr fill_cover = 0xff0000ff;
        uint32_t constexpr clear_cover = 0xff00ff00;
        uint32_t constexpr outline_cover = 0xffff0000;

        for(auto const &e : tesselator.entities) {
            if(fill) {
                glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
                glLineWidth(1.0f);
                if(e.flags & 1) {
                    program->set_color(clear_cover);
                } else {
                    program->set_color(fill_cover);
                }
                int end = e.num_fills + e.first_fill;
                for(int i = e.first_fill; i < end; ++i) {
                    tesselator_span const &s = tesselator.fills[i];
                    glDrawElements(GL_TRIANGLES, s.length, GL_UNSIGNED_INT, (void *)(s.start * sizeof(GLuint)));
                }
            }
            if(outline) {
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                glLineWidth(outline_thickness);
                program->set_color(outline_cover);
                int end = e.num_outlines + e.first_outline;
                for(int i = e.first_outline; i < end; ++i) {
                    tesselator_span const &s = tesselator.boundaries[i];
                    glDrawArrays(GL_LINE_LOOP, s.start, s.length);
                }
            }
        }
    }

    //////////////////////////////////////////////////////////////////////

    void gl_drawer::on_finished_loading()
    {
        GLsizei num_verts = (GLsizei)tesselator.vertices.size();
        GLsizei num_indices = (GLsizei)tesselator.indices.size();
        gl_vertex_solid *vtx = tesselator.vertices.data();
        GLuint *idx = tesselator.indices.data();

        if(num_verts == 0 || num_indices == 0) {
            LOG_WARNING("Huh? No shapes in gerber file {}", gerber_file->filename);
            return;
        }
        vertex_array.init(*program, num_verts);
        vertex_array.activate();
        indices_triangles.init(num_indices);
        indices_triangles.activate();

        gl_vertex_solid *v = (gl_vertex_solid *)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
        GLuint *i = (GLuint *)glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
        memcpy(v, vtx, num_verts * sizeof(gl_vertex_solid));
        memcpy(i, idx, num_indices * sizeof(GLuint));
        glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
        glUnmapBuffer(GL_ARRAY_BUFFER);
    }

}    // namespace gerber_3d
