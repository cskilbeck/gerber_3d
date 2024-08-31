
#include "gdi_drawer.h"
#include <thread>
#include <algorithm>
#include <windowsx.h>
#include "gerber_lib.h"
#include "gerber_settings.h"
#include "gerber_util.h"
#include "gerber_net.h"
#include "gerber_enums.h"
#include "gerber_aperture.h"
#include "gerber_log.h"
#include "log_drawer.h"

#include <Commdlg.h>

#pragma comment(lib, "Gdiplus.lib")

LOG_CONTEXT("gdi_drawer", info);

namespace
{
    HINSTANCE hInstance{ NULL };
    constexpr char const *class_name = "Gerber_Util_Window_Class";

    using namespace gerber_util;
    using namespace gerber_lib;
    using namespace gerber_2d;

    using Gdiplus::Color;
    using Gdiplus::Matrix;
    using Gdiplus::PointF;
    using Gdiplus::SolidBrush;

    Color const debug_color{ 255, 0, 0, 0 };

    POINT get_mouse_pos(LPARAM lParam)
    {
        return POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    }

    //////////////////////////////////////////////////////////////////////
    // this assumes CCW winding order (which GDI+ seems to use)

    bool is_point_in_polygon(PointF const *polygon_points, size_t num_polygon_points, PointF const &p)
    {
        bool inside = false;

        for(size_t i = 0, j = num_polygon_points - 1; i < num_polygon_points; j = i++) {

            PointF const &a = polygon_points[i];
            PointF const &b = polygon_points[j];

            // if the line straddles the point in the vertical direction
            if((a.Y > p.Y) != (b.Y > p.Y)) {

                // check if a horizontal line from the point crosses the line
                if((p.X < (b.X - a.X) * (p.Y - a.Y) / (b.Y - a.Y) + a.X)) {

                    inside = !inside;
                }
            }
        }
        return inside;
    }

    //////////////////////////////////////////////////////////////////////
    // make a rectangle have a certain aspect ratio by shrinking or expanding it

    enum aspect_ratio_correction
    {
        aspect_shrink,
        aspect_expand,
    };

    rect correct_aspect_ratio(double new_aspect_ratio, rect const &r, aspect_ratio_correction correction)
    {
        bool dir = aspect_ratio(r) > new_aspect_ratio;
        if(correction == aspect_expand) {
            dir = !dir;
        }
        vec2d n = size(r).scale(0.5);
        if(dir) {
            n.x = n.y * new_aspect_ratio;
        } else {
            n.y = n.x / new_aspect_ratio;
        }
        vec2d center = mid_point(r);
        return rect{ center.subtract(n), center.add(n) };
    }

    //////////////////////////////////////////////////////////////////////
    // make a matrix which maps window rect to world coordinates
    // if aspect ratio(view_rect) != aspect_ratio(window_rect), there will be distortion

    matrix world_to_window_transform_matrix(rect const &window, rect const &view)
    {
        matrix flip = matrix::scale({ 1, -1 });
        matrix offset = matrix::translate({ 0, height(window) });
        matrix origin = matrix::translate(view.min_pos.negate());
        matrix scale = matrix::scale({ window.width() / view.width(), window.height() / view.height() });

        matrix m = matrix::identity();
        m = matrix::multiply(m, origin);
        m = matrix::multiply(m, scale);
        m = matrix::multiply(m, flip);
        m = matrix::multiply(m, offset);
        return m;
    }

};    // namespace

namespace gerber_3d
{
    Color const gdi_drawer::gerber_fill_color[2] = { Color(255, 64, 128, 32), Color(128, 64, 128, 32) };
    Color const gdi_drawer::gerber_clear_color[2] = { Color(255, 255, 240, 224), Color(224, 255, 240, 224) };
    Color const gdi_drawer::highlight_color_fill{ 64, 0, 255, 255 };
    Color const gdi_drawer::highlight_color_clear{ 64, 255, 0, 255 };
    Color const gdi_drawer::axes_color{ 255, 0, 0, 0 };
    Color const gdi_drawer::origin_color{ 255, 255, 0, 0 };
    Color const gdi_drawer::extent_color{ 255, 64, 64, 255 };

    Color const gdi_drawer::select_outline_color{ 32, 255, 0, 0 };
    Color const gdi_drawer::select_fill_color{ 32, 64, 64, 255 };
    Color const gdi_drawer::select_whole_fill_color{ 64, 0, 255, 0 };

    Color const gdi_drawer::info_text_background_color{ 192, 0, 0, 0 };
    Color const gdi_drawer::info_text_foreground_color{ 255, 255, 255, 255 };

    //////////////////////////////////////////////////////////////////////

    void gdi_drawer::zoom_to_rect(rect const &zoom_rect, double border_ratio)
    {
        rect new_rect = correct_aspect_ratio(aspect_ratio(window_rect), zoom_rect, aspect_expand);
        vec2d mid = mid_point(new_rect);
        vec2d siz = size(new_rect).scale(border_ratio / 2);
        view_rect = { mid.subtract(siz), mid.add(siz) };
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code gdi_drawer::load_gerber_file(std::string const &filename)
    {
        if(!filename.empty()) {
            std::thread(
                [this](std::string filename) {
                    std::unique_ptr<gerber> new_gerber{ std::make_unique<gerber>() };
                    CHECK(new_gerber->parse_file(filename.c_str()));
                    PostMessage(hwnd, WM_USER, 0, (LPARAM)new_gerber.release());
                    return ok;
                },
                filename)
                .detach();
        }
        return gerber_lib::ok;
    }

    //////////////////////////////////////////////////////////////////////

    std::string gdi_drawer::current_filename() const
    {
        if(gerber_file != nullptr) {
            return gerber_file->filename;
        }
        return std::string{};
    }

    //////////////////////////////////////////////////////////////////////

    void gdi_drawer::set_gerber(gerber *g)
    {
        if(gerber_file != nullptr) {
            delete gerber_file;
        }

        gerber_file = g;

        if(gerber_file != nullptr) {

            cleanup();
            zoom_to_rect(gerber_file->image.info.extent);

            gerber_file->draw(*this);
        }
        redraw();
    }

    //////////////////////////////////////////////////////////////////////

    LRESULT CALLBACK gdi_drawer::wnd_proc_proxy(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if(message == WM_CREATE) {
            LPCREATESTRUCTA c = reinterpret_cast<LPCREATESTRUCTA>(lParam);
            SetWindowLongPtrA(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(c->lpCreateParams));
        }
        gdi_drawer *d = reinterpret_cast<gdi_drawer *>(GetWindowLongPtrA(hWnd, GWLP_USERDATA));
        if(d != nullptr) {
            return d->wnd_proc(message, wParam, lParam);
        }
        return DefWindowProcA(hWnd, message, wParam, lParam);
    }

    //////////////////////////////////////////////////////////////////////

    vec2d gdi_drawer::world_pos_from_window_pos(vec2d const &window_pos) const
    {
        return vec2d{ window_pos, matrix::invert(get_transform_matrix()) };
    }

    //////////////////////////////////////////////////////////////////////

    vec2d gdi_drawer::world_pos_from_window_pos(POINT const &window_pos) const
    {
        return world_pos_from_window_pos(vec2d{ (double)window_pos.x, (double)window_pos.y });
    }

    //////////////////////////////////////////////////////////////////////

    vec2d gdi_drawer::window_pos_from_world_pos(vec2d const &world_pos) const
    {
        return vec2d{ world_pos, get_transform_matrix() };
    }

    //////////////////////////////////////////////////////////////////////

    void gdi_drawer::zoom_image(POINT const &pos, double zoom_scale)
    {
        // normalized position within view_rect
        vec2d zoom_pos = vec2d{ (double)pos.x, window_size.y - (double)pos.y }.divide(window_size);

        // scaled view_rect size
        vec2d new_size{ view_rect.width() / zoom_scale, view_rect.height() / zoom_scale };

        // world position of pos
        vec2d p = view_rect.min_pos.add(zoom_pos.multiply(size(view_rect)));

        // new rectangle offset from world position
        vec2d bottom_left = p.subtract(new_size.multiply(zoom_pos));
        vec2d top_right = bottom_left.add(new_size);
        view_rect = { bottom_left, top_right };
        redraw();
    }

    //////////////////////////////////////////////////////////////////////

    void gdi_drawer::on_left_click(POINT const &mouse_pos)
    {
        LOG_CONTEXT("pick", info);

        if(gerber_file == nullptr) {
            return;
        }

        PointF gdi_mouse_pos{ (REAL)mouse_pos.x, (REAL)mouse_pos.y };

        // Build list of entities under that point
        std::vector<int> entities;
        int entity_id = 0;

        LOG_DEBUG("There are {} entities", gdi_entities.size());

        for(auto const &entity : gdi_entities) {

            if(entity.pixel_space_bounds.Contains(gdi_mouse_pos)) {

                LOG_DEBUG("      Checking entity {} ({} paths)", entity.entity_id, entity.num_paths);

                int path_id = entity.path_id;

                for(int n = 0; n < entity.num_paths; ++n) {

                    std::vector<PointF> const &gdi_points = gdi_point_lists[path_id];

                    LOG_DEBUG("          Path {} has {} points", path_id, gdi_points.size());

                    if(is_point_in_polygon(gdi_points.data(), gdi_points.size(), gdi_mouse_pos)) {

                        LOG_DEBUG("              -> ENTITY {} is visible via PATH {}", entity_id, path_id);
                        entities.push_back(entity_id);
                        break;
                    }
                    path_id += 1;
                }
            }
            entity_id += 1;
        }

        // Clicked on something?
        highlight_entity = !entities.empty();

        if(highlight_entity) {

            if(entities != entities_clicked) {
                // Different set, save the new list and reset the cycling index
                entities_clicked = entities;
                selected_entity_index = entities_clicked.size() - 1;
            } else {
                // Same set, cycle to the next entity down
                selected_entity_index = std::min(selected_entity_index - 1, entities_clicked.size() - 1);
            }
            highlight_entity_id = entities_clicked[selected_entity_index];
        }
    }

    //////////////////////////////////////////////////////////////////////

    LRESULT gdi_drawer::wnd_proc(UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch(message) {

            //////////////////////////////////////////////////////////////////////

        case WM_CREATE: {
            Gdiplus::GdiplusStartupInput gdiplusStartupInput;
            GdiplusStartup(&gdiplus_token, &gdiplusStartupInput, NULL);
            load_settings();
        } break;


            //////////////////////////////////////////////////////////////////////

        case WM_SHOWWINDOW:
            create_gdi_resources();
            break;

            //////////////////////////////////////////////////////////////////////

        case WM_ERASEBKGND:
            break;

            //////////////////////////////////////////////////////////////////////

        case WM_KEYDOWN:

            switch(LOWORD(wParam)) {

            case 'W':
                draw_mode ^= (int)draw_mode_wireframe;
                if(draw_mode == 0) {
                    draw_mode = draw_mode_shaded;
                }
                redraw();
                break;

            case 'S':
                draw_mode ^= (int)draw_mode_shaded;
                if(draw_mode == 0) {
                    draw_mode = draw_mode_wireframe;
                }
                redraw();
                break;

            case 'D':
                if(gerber_file != nullptr && highlight_entity) {
                    log_drawer logger;
                    logger.set_gerber(gerber_file);
                    gerber_file->draw(logger);
                }
                break;

            case 27:
                DestroyWindow(hwnd);
                break;

            case 'F':
                if(gerber_file != nullptr) {
                    if(highlight_entity) {
                        RectF const &r = gdi_entities[highlight_entity_id].world_space_bounds;
                        rect zoom_rect{ (REAL)r.X, (REAL)r.Y, (REAL)(r.X + r.Width), (REAL)(r.Y + r.Height) };
                        zoom_to_rect(zoom_rect);
                    } else {
                        zoom_to_rect(gerber_file->image.info.extent);
                    }
                    redraw();
                }
                break;

            case 'O':
                show_origin = !show_origin;
                redraw();
                break;

            case 'E':
                show_extent = !show_extent;
                redraw();
                break;

            case 'A':
                show_axes = !show_axes;
                redraw();
                break;

            case 'T':
                solid_color_index = (++solid_color_index) % _countof(fill_brush);
                redraw();
                break;

            case 'N':
                highlight_entity = !highlight_entity;
                redraw();
                break;

            case 'L': {
                std::string filename = get_open_filename();
                if(!filename.empty()) {
                    LOG_INFO("{}", filename);
                    load_gerber_file(filename);
                }
            } break;

            case 'R': {
                std::string filename = current_filename();
                if(!filename.empty()) {
                    LOG_INFO("RELOADING {}", filename);
                    load_gerber_file(filename);
                }
            } break;

            case VK_LEFT:
                if(highlight_entity) {
                    highlight_entity_id = std::max(0, highlight_entity_id - 1);
                    redraw();
                }
                break;

            case VK_RIGHT:
                if(highlight_entity) {
                    highlight_entity_id = std::min((int)gdi_entities.size() - 1, highlight_entity_id + 1);
                    redraw();
                }
                break;

            case '3':
                if(occ.vout.hwnd == nullptr) {
                    occ.show_progress = true;
                    occ.create_window(100, 100, 700, 700);
                }
                std::thread([&]() {
                    occ.set_gerber(gerber_file);
                    PostMessageA(occ.vout.hwnd, WM_USER, 0, 0);
                }).detach();
                break;
            }
            break;

            //////////////////////////////////////////////////////////////////////
            // A gerber file was loaded

        case WM_USER: {
            set_gerber((gerber *)lParam);
        } break;

            //////////////////////////////////////////////////////////////////////

        case WM_CLOSE:
            DestroyWindow(hwnd);
            break;

            //////////////////////////////////////////////////////////////////////
            // window resize - update the world view rectangle accordingly

        case WM_SIZE: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            vec2d new_window_size = { (double)rc.right, (double)rc.bottom };
            vec2d scale_factor = new_window_size.divide(window_size);
            window_size = new_window_size;
            window_rect = { { 0, 0 }, window_size };
            vec2d old_view_size = size(view_rect);
            vec2d new_view_size = old_view_size.multiply(scale_factor);
            view_rect = { view_rect.min_pos, view_rect.min_pos.add(new_view_size) };
            redraw();
        } break;

            //////////////////////////////////////////////////////////////////////

        case WM_MOUSEWHEEL: {
            double scale_factor = ((int16_t)(HIWORD(wParam)) > 0) ? 1.1 : 0.9;
            POINT mouse_pos = get_mouse_pos(lParam);
            ScreenToClient(hwnd, &mouse_pos);
            zoom_image(mouse_pos, scale_factor);
        } break;

            //////////////////////////////////////////////////////////////////////

        case WM_LBUTTONDOWN: {
            POINT mouse_pos = get_mouse_pos(lParam);
            if((GetKeyState(VK_CONTROL) & 0x8000) != 0) {
                mouse_drag = mouse_drag_select;
                drag_mouse_start_pos = mouse_pos;
                drag_rect = {};
                SetCapture(hwnd);
            } else {
                on_left_click(mouse_pos);
            }
        } break;

            //////////////////////////////////////////////////////////////////////

        case WM_LBUTTONUP: {

            if(mouse_drag == mouse_drag_select) {
                vec2d mn = drag_rect.min_pos;
                vec2d mx = drag_rect.max_pos;
                view_rect = { world_pos_from_window_pos(vec2d{ mn.x, mx.y }), world_pos_from_window_pos(vec2d{ mx.x, mn.y }) };
            }
            mouse_drag = mouse_drag_none;
            redraw();
            ReleaseCapture();
        } break;

            //////////////////////////////////////////////////////////////////////

        case WM_MBUTTONDOWN:
            mouse_drag = mouse_drag_zoom;
            drag_mouse_cur_pos = get_mouse_pos(lParam);
            drag_mouse_start_pos = drag_mouse_cur_pos;
            SetCapture(hwnd);
            break;

            //////////////////////////////////////////////////////////////////////

        case WM_MBUTTONUP:
            mouse_drag = mouse_drag_none;
            ReleaseCapture();
            redraw();
            break;

            //////////////////////////////////////////////////////////////////////

        case WM_RBUTTONDOWN:
            mouse_drag = mouse_drag_pan;
            drag_mouse_cur_pos = get_mouse_pos(lParam);
            SetCapture(hwnd);
            break;

            //////////////////////////////////////////////////////////////////////

        case WM_RBUTTONUP:
            mouse_drag = mouse_drag_none;
            ReleaseCapture();
            redraw();
            break;

            //////////////////////////////////////////////////////////////////////

        case WM_MOUSEMOVE:

            switch(mouse_drag) {

            case mouse_drag_pan: {
                POINT mouse_pos = get_mouse_pos(lParam);
                vec2d new_mouse_pos = world_pos_from_window_pos(mouse_pos);
                vec2d old_mouse_pos = world_pos_from_window_pos(drag_mouse_cur_pos);
                view_rect = view_rect.offset(new_mouse_pos.subtract(old_mouse_pos).negate());
                drag_mouse_cur_pos = mouse_pos;
                redraw();
            } break;

            case mouse_drag_zoom: {
                POINT mouse_pos = get_mouse_pos(lParam);
                double dx = (double)(mouse_pos.x - drag_mouse_cur_pos.x);
                double dy = (double)(mouse_pos.y - drag_mouse_cur_pos.y);
                zoom_image(drag_mouse_start_pos, 1.0 + (dx - dy) * 0.01);
                drag_mouse_cur_pos = mouse_pos;
                redraw();
            } break;

            case mouse_drag_select: {
                drag_mouse_cur_pos = get_mouse_pos(lParam);

                if(drag_mouse_cur_pos.x != drag_mouse_start_pos.x && drag_mouse_cur_pos.y != drag_mouse_start_pos.y) {

                    double x = std::min((double)drag_mouse_start_pos.x, (double)drag_mouse_cur_pos.x);
                    double y = std::min((double)drag_mouse_start_pos.y, (double)drag_mouse_cur_pos.y);
                    double w = std::max((double)drag_mouse_start_pos.x, (double)drag_mouse_cur_pos.x) - x;
                    double h = std::max((double)drag_mouse_start_pos.y, (double)drag_mouse_cur_pos.y) - y;

                    drag_rect_raw.min_pos = { x, y };
                    drag_rect_raw.max_pos = drag_rect_raw.min_pos.add({ w, h });
                    drag_rect = correct_aspect_ratio(aspect_ratio(window_rect), drag_rect_raw, aspect_expand);
                    redraw();
                }
            } break;

            case mouse_drag_none: {
                // work out the world coordinate of the mouse position
                // vec2d pos = world_pos_from_window_pos(get_mouse_pos(lParam));
                // LOG_INFO("POS: {:9.5f},{:9.5f}mm {:9.5f},{:9.5f}in ", pos.x, pos.y, pos.x / 25.4, pos.y / 25.4);
            } break;

            default:
                break;
            }
            break;

            //////////////////////////////////////////////////////////////////////

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            paint(hdc);
            EndPaint(hwnd, &ps);
        } break;

            //////////////////////////////////////////////////////////////////////

        case WM_DESTROY:
            cleanup();
            Gdiplus::GdiplusShutdown(gdiplus_token);
            gdiplus_token = 0;
            hwnd = nullptr;
            save_settings();
            PostQuitMessage(0);
            break;

            //////////////////////////////////////////////////////////////////////

        default:
            return DefWindowProcA(hwnd, message, wParam, lParam);
        }
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void gdi_drawer::save_settings() const
    {
        save_bool("show_axes", show_axes);
        save_bool("show_extent", show_extent);
        save_bool("show_origin", show_origin);
        save_int("draw_mode", draw_mode);
    }

    //////////////////////////////////////////////////////////////////////

    void gdi_drawer::load_settings()
    {
        load_bool("show_axes", show_axes);
        load_bool("show_extent", show_extent);
        load_bool("show_origin", show_origin);
        load_int("draw_mode", draw_mode);
    }

    //////////////////////////////////////////////////////////////////////

    void gdi_drawer::create_window(int x, int y, int w, int h)
    {
        if(hInstance == NULL) {

            hInstance = GetModuleHandle(NULL);

            WNDCLASSA wndClass{};
            wndClass.style = CS_HREDRAW | CS_VREDRAW;
            wndClass.lpfnWndProc = wnd_proc_proxy;
            wndClass.hInstance = hInstance;
            wndClass.hIcon = LoadIconA(NULL, IDI_APPLICATION);
            wndClass.hCursor = LoadCursorA(NULL, IDC_ARROW);
            wndClass.hbrBackground = NULL;
            wndClass.lpszClassName = class_name;

            RegisterClassA(&wndClass);
        }

        RECT rc;
        SetRect(&rc, x, y, x + w, y + h);
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, false);
        x = rc.left;
        y = rc.top;
        w = rc.right - rc.left;
        h = rc.bottom - rc.top;

        hwnd = CreateWindowExA(0, class_name, "Gerber Explorer", WS_OVERLAPPEDWINDOW, x, y, w, h, NULL, NULL, hInstance, this);

        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
    }

    //////////////////////////////////////////////////////////////////////
    // entity_id is guaranteed to increase monotonically

    void gdi_drawer::fill_elements(gerber_draw_element const *elements, size_t num_elements, gerber_polarity polarity, int entity_id)
    {
        GraphicsPath *p = new GraphicsPath();
        gdi_paths.push_back(p);

        bool fill = polarity == polarity_dark || polarity == polarity_positive;

        //        entity_id -= 1;
        if(gdi_entities.empty() || gdi_entities.back().entity_id != entity_id) {
            gdi_entities.emplace_back(entity_id, (int)(gdi_paths.size() - 1llu), 1, fill);
        } else {
            gdi_entities.back().num_paths += 1;
        }

        // auto &entity = gdi_entities.back();

        for(size_t n = 0; n < num_elements; ++n) {

            gerber_draw_element const &e = elements[n];

            switch(elements[n].draw_element_type) {

            case draw_element_arc: {
                p->AddArc((REAL)(e.arc_center.x - e.radius), (REAL)(e.arc_center.y - e.radius), (REAL)e.radius * 2, (REAL)e.radius * 2, (REAL)e.start_degrees,
                          (REAL)(e.end_degrees - e.start_degrees));
            } break;

            case draw_element_line: {
                p->AddLine((REAL)e.line_start.x, (REAL)e.line_start.y, (REAL)e.line_end.x, (REAL)e.line_end.y);
            } break;
            }

            gdi_entity &entity = gdi_entities.back();

            if(entity.world_space_bounds.IsEmptyArea()) {
                p->GetBounds(&entity.world_space_bounds);
            } else {
                RectF path_bounds;
                p->GetBounds(&path_bounds);
                entity.world_space_bounds.Union(entity.world_space_bounds, entity.world_space_bounds, path_bounds);
            }
        }
        p->CloseAllFigures();
    }

    //////////////////////////////////////////////////////////////////////

    void gdi_drawer::redraw() const
    {
        InvalidateRect(hwnd, nullptr, false);
    }

    //////////////////////////////////////////////////////////////////////

    void gdi_drawer::create_gdi_resources()
    {
        UINT sx = (UINT)window_size.x;
        UINT sy = (UINT)window_size.y;

        if(bitmap != nullptr && bitmap->GetWidth() == sx && bitmap->GetHeight() == sy) {
            return;
        }

        release_gdi_resources();

        bitmap = new Bitmap(sx, sy);
        graphics = Graphics::FromImage(bitmap);
        extent_pen = new Pen(extent_color, 0);
        axes_pen = new Pen(axes_color, 0);
        debug_pen = new Pen(debug_color, 2);
        origin_pen = new Pen(origin_color, 0);
        thin_pen = new Pen(Color{ 255, 0, 0, 0 }, 1);
        fill_brush[0] = new SolidBrush(gerber_fill_color[0]);
        fill_brush[1] = new SolidBrush(gerber_fill_color[1]);
        clear_brush[0] = new SolidBrush(gerber_clear_color[0]);
        clear_brush[1] = new SolidBrush(gerber_clear_color[1]);
        red_fill_brush = new SolidBrush(Color(64, 255, 0, 0));
        highlight_fill_brush = new SolidBrush(highlight_color_fill);
        highlight_clear_brush = new SolidBrush(highlight_color_clear);
        select_outline_pen = new Pen(select_outline_color, 0);
        select_fill_brush = new SolidBrush(select_fill_color);
        select_whole_fill_brush = new SolidBrush(select_whole_fill_color);
        info_text_background_brush = new SolidBrush(info_text_background_color);
        info_text_foregound_brush = new SolidBrush(info_text_foreground_color);
        info_text_font = new Font(L"Consolas", 12.0f, Gdiplus::FontStyleRegular);
    }

    //////////////////////////////////////////////////////////////////////

    void gdi_drawer::release_gdi_resources()
    {
        LOG_CONTEXT("GDI_Drawer", debug);

        auto safe_delete = [this](auto &x) {
            if(x != nullptr) {
                delete x;
                x = nullptr;
            }
        };

        safe_delete(bitmap);
        safe_delete(graphics);
        safe_delete(fill_brush[0]);
        safe_delete(fill_brush[1]);
        safe_delete(clear_brush[0]);
        safe_delete(clear_brush[1]);
        safe_delete(red_fill_brush);
        safe_delete(highlight_fill_brush);
        safe_delete(highlight_clear_brush);
        safe_delete(extent_pen);
        safe_delete(debug_pen);
        safe_delete(axes_pen);
        safe_delete(origin_pen);
        safe_delete(thin_pen);
        safe_delete(select_outline_pen);
        safe_delete(select_fill_brush);
        safe_delete(select_whole_fill_brush);
        safe_delete(info_text_background_brush);
        safe_delete(info_text_foregound_brush);
        safe_delete(info_text_font);
    }

    //////////////////////////////////////////////////////////////////////

    void gdi_drawer::draw_entity(gdi_entity const &entity, Brush *brush, Pen *pen)
    {
        int path_id = entity.path_id;
        int last_path = entity.num_paths + path_id;
        for(; path_id != last_path; ++path_id) {

            GraphicsPath *path = gdi_paths[path_id];
            if(brush != nullptr) {
                graphics->FillPath(brush, path);
            }
            if(pen != nullptr != 0) {
                graphics->DrawPath(pen, path);
            }
        }
    }

    //////////////////////////////////////////////////////////////////////

    void gdi_drawer::draw_all_entities()
    {
        for(auto const &entity : gdi_entities) {

            Brush *brush{ nullptr };
            if((draw_mode & draw_mode_shaded) != 0) {
                if(entity.fill) {
                    brush = fill_brush[solid_color_index];
                } else {
                    brush = clear_brush[solid_color_index];
                }
            }

            Pen *pen{ nullptr };
            if((draw_mode & draw_mode_wireframe) != 0) {
                pen = axes_pen;
            }
            draw_entity(entity, brush, pen);
        }
    }

    //////////////////////////////////////////////////////////////////////

    matrix gdi_drawer::get_transform_matrix() const
    {
        if(gerber_file == nullptr) {
            return matrix::identity();
        }
        return world_to_window_transform_matrix(window_rect, view_rect);
    }

    //////////////////////////////////////////////////////////////////////

    void gdi_drawer::paint(HDC hdc)
    {
        LOG_CONTEXT("paint", debug);

        if(gerber_file == nullptr) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            FillRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
            return;
        }

        create_gdi_resources();

        graphics->ResetTransform();

        graphics->FillRectangle(clear_brush[0], 0, 0, (INT)window_size.x, (INT)window_size.y);

        matrix mat = get_transform_matrix();

        Matrix gdi_matrix((REAL)mat.A, (REAL)mat.B, (REAL)mat.C, (REAL)mat.D, (REAL)mat.X, (REAL)mat.Y);
        graphics->SetTransform(&gdi_matrix);

        graphics->SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);

        // draw the whole thing

        draw_all_entities();

        // get screen-space polygons and bounding rect for each path

        for(auto &p : gdi_point_lists) {
            p.clear();
        }
        gdi_point_lists.clear();

        for(auto &entity : gdi_entities) {

            int path_id = entity.path_id;
            int last_path = entity.num_paths + path_id;
            entity.pixel_space_bounds = RectF{ 0, 0, 0, 0 };
            for(; path_id != last_path; ++path_id) {
                std::unique_ptr<GraphicsPath> flattened_path(gdi_paths[path_id]->Clone());
                flattened_path->Flatten(&gdi_matrix, 2);    // 2 pixel tolerance to reduce the # of points on curves
                int num_points = flattened_path->GetPointCount();
                gdi_point_lists.push_back(std::vector<PointF>());
                gdi_point_lists.back().resize(num_points);
                flattened_path->GetPathPoints(gdi_point_lists.back().data(), num_points);
                RectF bounds;
                gdi_paths[path_id]->GetBounds(&bounds, &gdi_matrix, nullptr);
                if(entity.pixel_space_bounds.IsEmptyArea()) {
                    entity.pixel_space_bounds = bounds;
                } else {
                    RectF::Union(entity.pixel_space_bounds, entity.pixel_space_bounds, bounds);
                }
            }
        }

        // gerber_file->draw(*this, elements_to_hide);

        // draw just the highlighted net (if there is one)
        if(highlight_entity) {
            gdi_entity const &highlighted_entity = gdi_entities[highlight_entity_id];
            Brush *highlight_brush = highlight_clear_brush;
            if(highlighted_entity.fill) {
                highlight_brush = highlight_fill_brush;
            }
            draw_entity(highlighted_entity, highlight_brush, axes_pen);
        }

        graphics->SetSmoothingMode(Gdiplus::SmoothingModeNone);

        if(show_extent) {
            rect const &gerber_rect = gerber_file->image.info.extent;
            double gerber_width = width(gerber_rect);
            double gerber_height = height(gerber_rect);

            // transform is still active so extent is just the gerber rect
            REAL x = (REAL)gerber_rect.min_pos.x;
            REAL y = (REAL)gerber_rect.min_pos.y;
            REAL w = (REAL)gerber_width;
            REAL h = (REAL)gerber_height;
            graphics->DrawRectangle(extent_pen, x, y, w, h);
        }

        graphics->ResetTransform();

        if(show_origin || show_axes) {

            vec2d org = transform_point(mat, { 0, 0 });
            float x = (float)org.x;
            float y = (float)org.y;

            if(show_origin) {
                float s = 10;
                graphics->DrawLine(origin_pen, PointF(x - s, y - s), PointF(x + s, y + s));
                graphics->DrawLine(origin_pen, PointF(x + s, y - s), PointF(x - s, y + s));
            }

            if(show_axes) {
                graphics->DrawLine(axes_pen, PointF(0, y), PointF((REAL)window_size.x, y));
                graphics->DrawLine(axes_pen, PointF(x, 0), PointF(x, (REAL)window_size.y));
            }
        }

        if(mouse_drag == mouse_drag_select) {

            graphics->FillRectangle(select_fill_brush, (INT)drag_rect_raw.min_pos.x, (INT)drag_rect_raw.min_pos.y, (INT)width(drag_rect_raw) + 1,
                                    (INT)height(drag_rect_raw) + 1);

            graphics->DrawRectangle(select_outline_pen, (INT)drag_rect.min_pos.x, (INT)drag_rect.min_pos.y, (INT)width(drag_rect), (INT)height(drag_rect));

            graphics->FillRectangle(select_whole_fill_brush, (INT)drag_rect.min_pos.x + 1, (INT)drag_rect.min_pos.y + 1, (INT)width(drag_rect) - 1,
                                    (INT)height(drag_rect) - 1);
        }

        if(highlight_entity) {

            using namespace gerber_lib;

            gerber_entity &entity = gerber_file->entities[highlight_entity_id];

#if defined(DRAW_SELECTION_BOUNDING_BOXES)
            graphics->SetTransform(&gdi_matrix);
            gdi_entity const &highlighted_entity = gdi_entities[highlight_entity_id];
            graphics->DrawRectangle(origin_pen, highlighted_entity.bounds);
            graphics->ResetTransform();
#endif

            std::string line;
            if(entity.line_number_begin != entity.line_number_end) {
                line = std::format("Lines {} to {}", entity.line_number_begin, entity.line_number_end);
            } else {
                line = std::format("Line {}", entity.line_number_begin);
            }

            gerber_net const *net = gerber_file->image.nets[entity.net_index];

            std::string net_info;

            std::string aperture_info;
            auto f = gerber_file->image.apertures.find(net->aperture);
            if(f == gerber_file->image.apertures.end()) {
                aperture_info = "None";
            } else {
                gerber_aperture *aperture = f->second;
                aperture_info = std::format("D{} - {}", aperture->aperture_number, aperture->get_description());
                net_info = std::format("At {}", net->end);

                // net->aperture_state should be aperture_state_flash at this point...
            }

            std::string draw;
            if(net->aperture_state == aperture_state_flash) {
                draw = "Flash";
            } else {
                draw = std::format("{}", net->interpolation_method);
                switch(net->interpolation_method) {
                case interpolation_linear:
                    net_info = std::format("From {}\n  To {}", net->start, net->end);
                    break;
                case interpolation_clockwise_circular:
                case interpolation_counterclockwise_circular:
                    net_info = std::format("At {}\nRadius {:5.3f}\nFrom {:5.1f}\n  To {:5.1f}", net->circle_segment.pos, net->circle_segment.size.x,
                                           net->circle_segment.start_angle, net->circle_segment.end_angle);
                    break;
                case interpolation_region_start:
                    net_info = std::format("{} points", net->num_region_points);
                    break;
                }
            }

            std::string text = std::format("Entity {:4d} {}\n"    //
                                           "Aperture {}\n"        //
                                           "Draw {}\n"            //
                                           "{}\n"                 //
                                           "Polarity {}\n"        //
                                           ,
                                           highlight_entity_id, line,    //
                                           aperture_info,                //
                                           draw,                         //
                                           net_info,                     //
                                           net->level->polarity);

            std::wstring wide_text = utf16_from_utf8(text);

            PointF origin{ 10, 10 };
            RectF bounding_box;
            graphics->MeasureString(wide_text.c_str(), (INT)text.size(), info_text_font, origin, &bounding_box);
            bounding_box.Width += 10;

            graphics->FillRectangle(info_text_background_brush, bounding_box);

            graphics->DrawString(wide_text.c_str(), (INT)text.size(), info_text_font, origin, info_text_foregound_brush);
        }

        Graphics *window_graphics = Gdiplus::Graphics::FromHDC(hdc);
        window_graphics->DrawImage(bitmap, 0, 0);
    }

    //////////////////////////////////////////////////////////////////////

    void gdi_drawer::cleanup()
    {
        for(auto &p : gdi_point_lists) {
            p.clear();
        }
        gdi_point_lists.clear();

        for(auto p : gdi_paths) {
            delete p;
        }
        gdi_paths.clear();
        gdi_entities.clear();
        entities_clicked.clear();
        selected_entity_index = 0;
        highlight_entity = false;

        release_gdi_resources();
    }

    //////////////////////////////////////////////////////////////////////

    std::string gdi_drawer::get_open_filename()
    {
        // open a file name
        char filename[MAX_PATH] = {};
        OPENFILENAME ofn{ 0 };
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFile = filename;
        ofn.lpstrFile[0] = '\0';
        ofn.nMaxFile = (DWORD)array_length(filename);
        ofn.lpstrFilter = "All files\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = NULL;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

        if(GetOpenFileNameA(&ofn)) {
            return filename;
        }
        return std::string{};
    }

}    // namespace gerber_3d
