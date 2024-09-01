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

        gerber_lib::gerber_error_code load_gerber_file(std::string const &filename);

        void set_gerber(gerber_lib::gerber *g) override;
        void fill_elements(gerber_lib::gerber_draw_element const *elements, size_t num_elements, gerber_lib::gerber_polarity polarity, int entity_id) override;

        std::string current_filename() const;

        using Graphics = Gdiplus::Graphics;
        using GraphicsPath = Gdiplus::GraphicsPath;
        using Bitmap = Gdiplus::Bitmap;
        using REAL = Gdiplus::REAL;
        using Color = Gdiplus::Color;
        using Pen = Gdiplus::Pen;
        using Brush = Gdiplus::Brush;
        using Font = Gdiplus::Font;
        using RectF = Gdiplus::RectF;
        using PointF = Gdiplus::PointF;

        using vec2d = gerber_lib::gerber_2d::vec2d;
        using rect = gerber_lib::gerber_2d::rect;
        using matrix = gerber_lib::gerber_2d::matrix;

        ULONG_PTR gdiplus_token{};

        void save_settings() const;
        void load_settings();

        rect view_rect;

        HWND hwnd{};
        HWND status_hwnd{};

        enum gdi_draw_mode
        {
            draw_mode_none = 0,
            draw_mode_shaded = 1,
            draw_mode_wireframe = 2,
            draw_mode_both = 3
        };

        int draw_mode{ draw_mode_shaded | draw_mode_wireframe };

        enum mouse_drag_action
        {
            mouse_drag_none = 0,
            mouse_drag_pan,
            mouse_drag_zoom,
            mouse_drag_zoom_select,
            mouse_drag_select
        };

        mouse_drag_action mouse_drag;

        vec2d drag_mouse_cur_pos{};
        vec2d drag_mouse_start_pos{};

        bool show_origin{ true };
        bool show_extent{ true };
        bool show_axes{ true };

        rect drag_rect_raw;
        rect drag_rect;

        size_t solid_color_index{ 1 };

        int elements_to_hide{ 0 };

        int highlight_entity_id{ 1 };
        bool highlight_entity{ false };

        static Color const axes_color;
        static Color const origin_color;
        static Color const extent_color;
        static Color const zoom_select_outline_color;
        static Color const zoom_select_fill_color;
        static Color const zoom_select_whole_fill_color;
        static Color const gerber_fill_color[2];
        static Color const gerber_clear_color[2];
        static Color const highlight_color_fill;
        static Color const highlight_color_clear;
        static Color const info_text_background_color;
        static Color const info_text_foreground_color;
        static Color const select_color[3];
        static Color const wireframe_color;

        gerber_lib::gerber *gerber_file{};

        Graphics *graphics{ nullptr };
        Bitmap *bitmap{ nullptr };

        Pen *debug_pen{ nullptr };
        Pen *axes_pen{ nullptr };
        Pen *origin_pen{ nullptr };
        Pen *extent_pen{ nullptr };
        Pen *select_outline_pen{ nullptr };
        Pen *thin_pen{ nullptr };
        Pen *select_pen[3]{ nullptr };
        Pen *wireframe_pen{ nullptr };
        Brush *select_fill_brush{ nullptr };
        Brush *select_whole_fill_brush{ nullptr };
        Brush *fill_brush[2]{ nullptr, nullptr };
        Brush *clear_brush[2]{ nullptr };
        Brush *red_fill_brush{ nullptr };
        Brush *highlight_fill_brush{ nullptr };
        Brush *highlight_clear_brush{ nullptr };
        Brush *info_text_background_brush{ nullptr };
        Brush *info_text_foregound_brush{ nullptr };
        Font *info_text_font{ nullptr };

        vec2d window_size;
        rect window_rect;

        void create_gdi_resources();
        void release_gdi_resources();

        void cleanup();

        struct gdi_entity
        {
            int entity_id{};               // index into gdi_entities
            int path_id{};                 // index into gdi_paths
            int num_paths{};               // # of paths in this entity
            bool fill{};                   // fill (true) or clear (false)
            bool selected{};               // highlighted entities are selected when mouse released
            RectF pixel_space_bounds{};    // screen space bounding rectangle

            gdi_entity() = default;

            gdi_entity(int entity_id, int path_id, int num_paths, bool fill)
                : entity_id(entity_id), path_id(path_id), num_paths(num_paths), fill(fill), pixel_space_bounds{}
            {
            }
        };

        std::vector<GraphicsPath *> gdi_paths;
        std::vector<gdi_entity> gdi_entities;
        std::vector<std::vector<PointF>> gdi_point_lists;

        std::vector<int> selected_entities;

        std::vector<int> entities_clicked;
        size_t selected_entity_index;

        matrix world_to_window_matrix;

        vec2d world_pos_from_window_pos(vec2d const &window_pos) const;
        vec2d window_pos_from_world_pos(vec2d const &world_pos) const;

        gerber_3d::occ_drawer occ;

        void on_left_click(vec2d const &mouse_pos);

        void draw_all_entities();
        void draw_selected_entities();

        void draw_entity(gdi_entity const &entity, Brush *brush, Pen *pen);

        void create_window(int x, int y, int w, int h);
        void paint(HDC hdc);

        void redraw() const;

        void zoom_image(vec2d const &pos, double zoom_scale);
        void zoom_to_rect(gerber_lib::rect const &zoom_rect, double border_ratio = 1.1);

        std::string get_open_filename();

        void select_entities(rect const &r);

        LRESULT wnd_proc(UINT message, WPARAM wParam, LPARAM lParam);
        static LRESULT CALLBACK wnd_proc_proxy(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    };

}    // namespace gerber_3d
