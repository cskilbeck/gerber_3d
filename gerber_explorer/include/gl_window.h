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
    struct gl_drawer;

    //////////////////////////////////////////////////////////////////////

    struct gl_vertex_solid
    {
        float x, y;
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_vertex_color
    {
        float x, y;
        uint32_t color;
    };

    //////////////////////////////////////////////////////////////////////
    // base gl_program - transform matrix is common to all

    struct gl_program
    {
        LOG_CONTEXT("gl_program", debug);

        gl_program() = default;

        GLuint program_id{};
        GLuint vertex_shader_id{};
        GLuint fragment_shader_id{};

        GLuint transform_location{};

        int check_shader(GLuint shader_id) const;
        int validate(GLuint param) const;
        int init(char const *const vertex_shader, char const *const fragment_shader);
        void cleanup();

        virtual int on_init() = 0;
    };

    //////////////////////////////////////////////////////////////////////
    // uniform color

    struct gl_solid_program : gl_program
    {
        GLuint color_location{};

        int on_init() override;

        void set_color(uint32_t color) const;
    };

    //////////////////////////////////////////////////////////////////////
    // color per vertex

    struct gl_color_program : gl_program
    {
        int on_init() override
        {
            return 0;
        }
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_vertex_array
    {
        GLuint vbo_id{ 0 };
        GLuint ibo_id{ 0 };
        int num_verts{ 0 };
        int num_indices{ 0 };

        gl_vertex_array() = default;

        virtual int init(gl_program &program, GLsizei vert_count, GLsizei index_count);
        virtual int activate() const;

        void cleanup();
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_vertex_array_solid : gl_vertex_array
    {
        gl_vertex_array_solid() = default;

        int position_location{ 0 };
        
        int init(gl_program &program, GLsizei vert_count, GLsizei index_count) override;
        int activate() const override;
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_vertex_array_color : gl_vertex_array
    {
        gl_vertex_array_color() = default;

        int position_location{ 0 };
        int color_location{ 0 };

        int init(gl_program &program, GLsizei vert_count, GLsizei index_count) override;
        int activate() const override;
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_window
    {
        LOG_CONTEXT("gl_drawer", debug);

        std::list<gl_drawer *> layers;

        using vec2d = gerber_lib::gerber_2d::vec2d;
        using rect = gerber_lib::gerber_2d::rect;
        using matrix = gerber_lib::gerber_2d::matrix;

        int create_window(int x_pos, int y_pos, int client_width, int client_height);
        LRESULT CALLBACK wnd_proc(UINT message, WPARAM wParam, LPARAM lParam);
        static LRESULT CALLBACK wnd_proc_proxy(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

        void zoom_to_rect(rect const &zoom_rect, double border_ratio = 1.1);
        void zoom_image(vec2d const &pos, double zoom_scale);
        vec2d world_pos_from_window_pos(vec2d const &p) const;

        void draw();

        void ui();

        void cleanup()
        {
        }

        std::string get_open_filename();

        enum mouse_drag_action
        {
            mouse_drag_none = 0,
            mouse_drag_pan,
            mouse_drag_zoom,
            mouse_drag_zoom_select,
            mouse_drag_maybe_select,
            mouse_drag_select
        };

        mouse_drag_action mouse_drag;

        vec2d drag_mouse_cur_pos{};
        vec2d drag_mouse_start_pos{};

        rect drag_rect_raw;
        rect drag_rect;

        gl_solid_program solid_program{};
        gl_color_program color_program{};

        gl_vertex_array verts{};

        int window_width{};
        int window_height{};

        vec2d window_size;
        rect window_rect;
        rect view_rect;

        HGLRC render_context{};
        HWND hwnd{};
        HDC window_dc{};
        RECT normal_rect;
        bool fullscreen{};
        bool quit{};
    };

}    // namespace gerber_3d
