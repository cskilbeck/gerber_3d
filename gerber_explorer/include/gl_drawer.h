//////////////////////////////////////////////////////////////////////
// OpenGL renderer for gerber explorer

#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <windowsx.h>
#include <gl/GL.h>

#include "Wglext.h"
#include "glcorearb.h"

#include "gl_functions.h"

#include "gerber_lib.h"
#include "gerber_log.h"
#include "gerber_2d.h"
#include "gerber_draw.h"

//////////////////////////////////////////////////////////////////////

namespace gerber_3d
{
    struct gl_program;
    struct gl_vertex_array;

    //////////////////////////////////////////////////////////////////////

    struct gl_program
    {
        LOG_CONTEXT("gl_program", debug);

        gl_program() = default;

        GLuint program_id{};
        GLuint vertex_shader_id{};
        GLuint fragment_shader_id{};
        GLuint projection_location{};
        GLuint color_location{};

        float projection_matrix[16] = {};

        int check_shader(GLuint shader_id) const;
        int validate(GLuint param) const;
        int init(char const *const vertex_shader, char const *const fragment_shader);
        void cleanup();

        void set_color(uint32_t color) const;
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_vertex_array
    {
        GLuint vbo_id{};
        GLuint ibo_id{};

        gl_vertex_array() = default;

        int init(gl_program &program);
        int activate(gl_program &program) const;
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_drawer : gerber_lib::gerber_draw_interface
    {
        LOG_CONTEXT("gl_drawer", debug);

        gerber_lib::gerber_error_code load_gerber_file(std::string const &filename);

        void set_gerber(gerber_lib::gerber *g) override;
        void fill_elements(gerber_lib::gerber_draw_element const *elements, size_t num_elements, gerber_lib::gerber_polarity polarity, int entity_id) override;

        std::string current_filename() const;

        using vec2d = gerber_lib::gerber_2d::vec2d;
        using rect = gerber_lib::gerber_2d::rect;
        using matrix = gerber_lib::gerber_2d::matrix;

        int window_width{};
        int window_height{};

        int create_window(int x_pos, int y_pos, int client_width, int client_height);
        LRESULT CALLBACK wnd_proc(UINT message, WPARAM wParam, LPARAM lParam);
        static LRESULT CALLBACK wnd_proc_proxy(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

        void draw();

        void cleanup()
        {
        }

        gl_program program{};
        gl_vertex_array verts{};

        HGLRC render_context{};
        HWND hwnd{};
        HDC window_dc{};
        RECT normal_rect;
        bool fullscreen{};
        bool quit{};
    };

}    // namespace gerber_3d
