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
#include "gl_colors.h"

#include "gerber_lib.h"
#include "gerber_log.h"
#include "gerber_2d.h"
#include "gerber_draw.h"
#include "gl_base.h"

//////////////////////////////////////////////////////////////////////

namespace gerber_3d
{
    struct gl_drawer;

    //////////////////////////////////////////////////////////////////////

    struct gl_window
    {
        LOG_CONTEXT("gl_drawer", debug);

        struct gerber_layer
        {
            gl_drawer *layer{ nullptr };
            bool outline{ false };
            std::string filename;
            uint32_t fill_color;
            uint32_t clear_color;
            uint32_t outline_color;
        };

        std::list<gerber_layer *> layers;

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
        gl_index_array indices{};

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
