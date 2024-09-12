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

#include "occ_drawer.h"

//////////////////////////////////////////////////////////////////////

namespace gerber_3d
{
    struct gl_drawer;

    //////////////////////////////////////////////////////////////////////

    struct gl_window
    {
        LOG_CONTEXT("gl_drawer", info);

        //////////////////////////////////////////////////////////////////////

        struct gerber_layer
        {
            int index;
            gl_drawer *layer{ nullptr };
            bool hide{ false };
            bool outline{ false };
            bool fill{ true };
            bool expanded{ false };
            bool selected{ false };
            std::string filename;
            uint32_t fill_color;
            uint32_t clear_color;
            uint32_t outline_color;

            bool operator<(gerber_layer const &other)
            {
                return index < other.index;
            }
        };

        //////////////////////////////////////////////////////////////////////

        struct gerber_entity
        {
            int entity_id{};                                     // index into entities
            int path_id{};                                       // index into gl_drawer->draw_calls for line_offset, line_count
            int num_paths{};                                     // # of paths in this entity
            bool fill{};                                         // fill (true) or clear (false)
            bool selected{};                                     // highlighted entities are selected when mouse released
            gerber_lib::gerber_2d::rect pixel_space_bounds{};    // screen space bounding rectangle

            gerber_entity() = default;

            gerber_entity(int entity_id, int path_id, int num_paths, bool fill)
                : entity_id(entity_id), path_id(path_id), num_paths(num_paths), fill(fill), pixel_space_bounds{}
            {
            }
        };

        //////////////////////////////////////////////////////////////////////

        std::vector<gerber_entity> entities;

        gerber_layer *selected_layer{ nullptr };

        std::vector<gerber_layer *> layers;

        using vec2d = gerber_lib::gerber_2d::vec2d;
        using rect = gerber_lib::gerber_2d::rect;
        using matrix = gerber_lib::gerber_2d::matrix;

        int create_window(int x_pos, int y_pos, int client_width, int client_height);
        LRESULT CALLBACK wnd_proc(UINT message, WPARAM wParam, LPARAM lParam);
        static LRESULT CALLBACK wnd_proc_proxy(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

        void fit_to_window();

        void zoom_to_rect(rect const &zoom_rect, double border_ratio = 1.1);
        void zoom_image(vec2d const &pos, double zoom_scale);
        vec2d window_pos_from_world_pos(vec2d const &p) const;
        vec2d world_pos_from_window_pos(vec2d const &p) const;

        void save_settings();
        void load_settings();

        void load_gerber_files(std::vector<std::string> filenames);

        void update_view_rect();
        void draw();

        void ui();

        void select_layer(gerber_layer *l);

        void cleanup()
        {
        }

        bool get_open_filenames(std::vector<std::string> &filenames);

        enum mouse_drag_action
        {
            mouse_drag_none = 0,
            mouse_drag_pan,
            mouse_drag_zoom,
            mouse_drag_zoom_select,
            mouse_drag_maybe_select,
            mouse_drag_select
        };

        mouse_drag_action mouse_drag{};

        vec2d drag_mouse_cur_pos{};
        vec2d drag_mouse_start_pos{};

        rect drag_rect_raw{};
        rect drag_rect{};

        bool show_options{ false };

        bool show_axes{ true };
        bool show_extent{ true };

        bool window_size_valid{ false };

        uint32_t axes_color{ 0x60ffffff };
        uint32_t extent_color{ 0xC000ffff };

        gl_solid_program solid_program{};
        gl_color_program color_program{};

        gl_vertex_array verts{};
        gl_index_array indices{};

        gl_drawlist overlay;

        gl_render_target render_target;

        int window_width{};
        int window_height{};

        vec2d window_size{};
        rect window_rect{};
        rect view_rect{};
        rect target_view_rect{};
        rect source_view_rect{};
        bool zoom_anim{ false };

        std::chrono::time_point<std::chrono::high_resolution_clock> target_view_time;

        HGLRC render_context{};
        HWND hwnd{};
        HDC window_dc{};
        RECT normal_rect;
        bool fullscreen{};
        bool quit{};

        void show_3d_view();

        gerber_3d::occ_drawer occ;
    };

}    // namespace gerber_3d
