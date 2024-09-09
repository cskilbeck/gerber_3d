//////////////////////////////////////////////////////////////////////

#pragma once

#include "gerber_lib.h"
#include "gerber_2d.h"
#include "gerber_draw.h"

#include "gl_window.h"

namespace gerber_3d
{
    //////////////////////////////////////////////////////////////////////

    struct gl_drawer : gerber_lib::gerber_draw_interface
    {
        enum draw_call_flags
        {
            draw_call_flag_clear = 1
        };

        struct draw_call
        {
            uint32_t offset;    // offset into the indices
            uint32_t length;    // # of indices
            int flags;
        };

        void set_gerber(gerber_lib::gerber *g) override;
        void fill_elements(gerber_lib::gerber_draw_element const *elements, size_t num_elements, gerber_lib::gerber_polarity polarity, int entity_id) override;

        void draw();

        gerber_lib::gerber *gerber_file;

        std::vector<gl_vertex_solid> vertices;
        std::vector<GLuint> indices;

        std::vector<draw_call> draw_calls;

        gl_vertex_array_solid vertex_array;
        gl_solid_program *program;
    };

}    // namespace gerber_3d
