//////////////////////////////////////////////////////////////////////
// GDI renderer for debugging the gerber parser, it's very very slow

#pragma once

#define NOMINMAX

#include <Windows.h>
#include <objidl.h>
#include <gdiplus.h>

#include "gerber_lib.h"
#include "gerber_2d.h"
#include "gerber_draw.h"

#include "occ_drawer.h"

//////////////////////////////////////////////////////////////////////

namespace gerber_3d
{
    struct gdi_drawer : gerber_lib::gerber_draw_interface
    {
        gdi_drawer() = default;

        void set_gerber(gerber_lib::gerber *g) override;
        void fill_elements(gerber_lib::gerber_draw_element const *elements, size_t num_elements, gerber_lib::gerber_polarity polarity, int entity_id) override;

        using Graphics = Gdiplus::Graphics;
        using GraphicsPath = Gdiplus::GraphicsPath;
        using Bitmap = Gdiplus::Bitmap;
        using REAL = Gdiplus::REAL;
        using Color = Gdiplus::Color;
        using Pen = Gdiplus::Pen;
        using Brush = Gdiplus::Brush;
        using Font = Gdiplus::Font;

        ULONG_PTR gdiplus_token{};

        gerber_lib::gerber_2d::vec2d image_pos_px{};    // image position on screen in pixels
        gerber_lib::gerber_2d::vec2d image_size_px{};   // image size on screen in pixels

        HWND hwnd{};
        HWND status_hwnd{};

        enum gdi_draw_mode
        {
            draw_mode_none = 0,
            draw_mode_shaded = 1,
            draw_mode_wireframe = 2,
            draw_mode_both = 3
        };

        int draw_mode{ draw_mode_shaded  | draw_mode_wireframe };

        enum mouse_drag_action
        {
            mouse_drag_none = 0,
            mouse_drag_pan,
            mouse_drag_zoom,
            mouse_drag_select
        };

        mouse_drag_action mouse_drag;

        POINT drag_mouse_cur_pos{};
        POINT drag_mouse_start_pos{};

        bool show_origin{ true };
        bool show_extent{ true };
        bool show_axes{ true };

        gerber_lib::gerber_2d::rect drag_rect_raw;
        gerber_lib::gerber_2d::rect drag_rect;

        size_t solid_color_index{ 1 };

        int elements_to_hide{ 0 };

        int highlight_entity_id{ 1 };
        bool highlight_entity{ false };

        static Color const axes_color;
        static Color const origin_color;
        static Color const extent_color;
        static Color const background_color;
        static Color const select_outline_color;
        static Color const select_fill_color;
        static Color const select_whole_fill_color;
        static Color const gerber_fill_color[2];
        static Color const highlight_color_fill;
        static Color const highlight_color_clear;
        static Color const info_text_background_color;
        static Color const info_text_foreground_color;

        gerber_lib::gerber *gerber_file{};

        Graphics *graphics{ nullptr };
        Bitmap *bitmap{ nullptr };

        Pen *debug_pen{ nullptr };
        Pen *axes_pen{ nullptr };
        Pen *origin_pen{ nullptr };
        Pen *extent_pen{ nullptr };
        Pen *select_outline_pen{ nullptr };
        Brush *select_fill_brush{ nullptr };
        Brush *select_whole_fill_brush{ nullptr };
        Brush *fill_brush[2]{ nullptr, nullptr };
        Brush *clear_brush{ nullptr };
        Brush *red_fill_brush{ nullptr };
        Brush *highlight_fill_brush{ nullptr };
        Brush *highlight_clear_brush{ nullptr };
        Brush *info_text_background_brush{ nullptr };
        Brush *info_text_foregound_brush{ nullptr };
        Font *info_text_font{ nullptr };

        gerber_lib::gerber_2d::vec2d window_size;

        void create_gdi_resources();
        void release_gdi_resources();

        void cleanup();

        struct gdi_entity
        {
            int entity_id{};
            int path_id{};
            int num_paths{};
            bool fill{};

            gdi_entity() = default;

            gdi_entity(int entity_id, int path_id, int num_paths, bool fill) : entity_id(entity_id), path_id(path_id), num_paths(num_paths), fill(fill)
            {
            }
        };

        std::vector<GraphicsPath *> gdi_paths;
        std::vector<gdi_entity> gdi_entities;

        std::vector<int> entities_clicked;
        size_t selected_entity_index;

        gerber_lib::gerber_2d::matrix get_transform_matrix();
        gerber_lib::gerber_2d::vec2d world_pos_from_window_pos(POINT const &window_pos);

        gerber_3d::occ_drawer occ;

        void on_left_click(POINT const &mouse_pos);

        void draw_all_entities();

        void draw_entity(gdi_entity const &entity, Brush *brush, Pen *pen);

        void create_window(int x, int y, int w, int h);
        void paint(HDC hdc);

        void redraw() const;

        void zoom_image(POINT const &pos, double zoom_scale);
        void set_default_zoom();

        std::string get_open_filename();

        LRESULT wnd_proc(UINT message, WPARAM wParam, LPARAM lParam);
        static LRESULT CALLBACK wnd_proc_proxy(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    };

}    // namespace gerber_3d
