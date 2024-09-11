//////////////////////////////////////////////////////////////////////

#pragma once

#include "gerber_lib.h"
#include "gerber_2d.h"
#include "gerber_draw.h"

#include "gl_window.h"

#include "tesselator.h"

namespace gerber_3d
{
    //////////////////////////////////////////////////////////////////////

    struct gl_drawer : gerber_lib::gerber_draw_interface
    {
        enum draw_call_flags
        {
            draw_call_flag_clear = 1
        };

        void set_gerber(gerber_lib::gerber *g) override;
        void fill_elements(gerber_lib::gerber_draw_element const *elements, size_t num_elements, gerber_lib::gerber_polarity polarity, int entity_id) override;

        void draw(uint32_t fill_color, uint32_t clear_color, bool outline, uint32_t outline_color);

        gerber_lib::gerber *gerber_file;

        std::vector<gl_vertex_solid> vertices;
        std::vector<GLuint> triangle_indices;
        std::vector<GLuint> line_indices;

        gl_vertex_array_solid vertex_array;
        gl_index_array indices_triangles;

        tesselator triangulator{};

        gl_solid_program *program{};

        gl_drawer() = default;
    };

}    // namespace gerber_3d
