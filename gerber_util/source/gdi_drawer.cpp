
#include "gdi_drawer.h"
#include <algorithm>
#include <windowsx.h>
#include "gerber_lib.h"
#include "gerber_net.h"
#include "gerber_log.h"
#include "log_drawer.h"

#pragma comment(lib, "Gdiplus.lib")

LOG_CONTEXT("gdi_drawer", info);

namespace
{
    HINSTANCE hInstance{ NULL };
    constexpr char const *class_name = "Gerber_Util_Window_Class";

    using namespace gerber_lib;
    using namespace gerber_2d;
    using namespace Gdiplus;

    Color const debug_color{ 255, 0, 0, 0 };

    POINT get_mouse_pos(LPARAM lParam)
    {
        return POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    }
};    // namespace

namespace gerber_3d
{
    Color const gdi_drawer::background_color{ 255, 255, 240, 224 };
    Color const gdi_drawer::gerber_fill_color[2] = { Color(255, 64, 128, 32), Color(128, 64, 128, 32) };
    Color const gdi_drawer::highlight_color_fill{ 128, 0, 255, 255 };
    Color const gdi_drawer::highlight_color_clear{ 128, 255, 0, 255 };
    Color const gdi_drawer::axes_color{ 255, 0, 0, 0 };
    Color const gdi_drawer::origin_color{ 255, 255, 0, 0 };
    Color const gdi_drawer::extent_color{ 255, 64, 64, 255 };

    Color const gdi_drawer::select_outline_color{ 32, 255, 0, 0 };
    Color const gdi_drawer::select_fill_color{ 32, 64, 64, 255 };
    Color const gdi_drawer::select_whole_fill_color{ 64, 0, 255, 0 };

    Color const gdi_drawer::info_text_background_color{ 128, 0, 0, 0 };
    Color const gdi_drawer::info_text_foreground_color{ 255, 255, 255, 255 };

    //////////////////////////////////////////////////////////////////////

    void gdi_drawer::set_default_zoom()
    {
        if(gerber_file == nullptr) {
            return;
        }

        rect const &gerber_rect = gerber_file->image.info.extent;

        double gerber_width = width(gerber_rect);
        double gerber_height = height(gerber_rect);

        if(gerber_width == 0 || gerber_height == 0) {
            return;
        }

        double window_aspect = window_size.x / window_size.y;
        double gerber_aspect = gerber_width / gerber_height;    // aspect ratio of gerber

        if(gerber_aspect > window_aspect) {
            image_size_px.x = window_size.x;
            image_size_px.y = window_size.x / gerber_aspect;
            image_pos_px.x = 0;
            image_pos_px.y = (window_size.y - image_size_px.y) / 2.0;
        } else {
            image_size_px.y = window_size.y;
            image_size_px.x = window_size.y * gerber_aspect;
            image_pos_px.y = 0;
            image_pos_px.x = (window_size.x - image_size_px.x) / 2.0;
        }
    }

    //////////////////////////////////////////////////////////////////////

    void gdi_drawer::set_gerber(gerber *g, int hide_elements)
    {
        gerber_file = g;
        set_default_zoom();
        elements_to_hide = hide_elements;
        while(!gdi_paths.empty()) {
            delete gdi_paths.back();
            gdi_paths.pop_back();
        }
        gdi_entities.clear();
        gerber_file->draw(*this, elements_to_hide);
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

    void gdi_drawer::zoom_image(POINT const &pos, double zoom_scale)
    {
        RECT rc;
        GetClientRect(hwnd, &rc);
        vec2d mouse_pos{ (double)pos.x, (double)(rc.bottom - pos.y) };
        vec2d relative_mouse_pos{ (mouse_pos.x - image_pos_px.x) / image_size_px.x, (mouse_pos.y - image_pos_px.y) / image_size_px.y };

        image_size_px = image_size_px.multiply({ zoom_scale, zoom_scale });

        // make it so image_size_px is at least ## pixels big in smallest dimension
        double smallest = std::max(1.0, std::min(image_size_px.x, image_size_px.y));

        double ratio = 16.0 / smallest;

        if(ratio > 1.0) {
            image_size_px.x *= ratio;
            image_size_px.y *= ratio;
        }

        relative_mouse_pos = relative_mouse_pos.multiply(image_size_px);
        image_pos_px = { mouse_pos.x - relative_mouse_pos.x, mouse_pos.y - relative_mouse_pos.y };
        redraw();
    }

    //////////////////////////////////////////////////////////////////////

    void gdi_drawer::on_left_click(POINT const &mouse_pos)
    {
        LOG_CONTEXT("pick", info);

        if(gerber_file == nullptr) {
            return;
        }

        // Get click position in world coordinates
        vec2d pos{ (double)mouse_pos.x, (double)mouse_pos.y };
        matrix transform = get_transform_matrix();
        vec2d img{ pos.x, pos.y, invert_matrix(transform) };

        // Scale the transform because the default pen width is 1.0 which is a whole mm
        // With scale(100,100), picking accuracy is 10 microns.
        // Note the gdi_point will also get scaled by the transform so no need to scale the input point
        Matrix m;
        m.Scale(100, 100);
        graphics->SetTransform(&m);

        // Build list of entities under that point
        std::vector<int> entities;
        int entity_id = 1;

        PointF gdi_point((REAL)img.x, (REAL)img.y);

        for(auto const &entity : gdi_entities) {

            int path_id = entity.path_id;

            for(int n = 0; n < entity.num_paths; ++n) {

                GraphicsPath *path = gdi_paths[path_id];

                if(path->IsVisible(gdi_point, graphics)) {
                    LOG_DEBUG("ENTITY {} is visible via PATH {}", entity_id, path_id);
                    entities.push_back(entity_id);
                    break;
                }
                path_id += 1;
            }
            entity_id += 1;
        }

        // Clicked on nothing?
        if(entities.empty()) {
            LOG_DEBUG("No entites found at {},{}", mouse_pos.x, mouse_pos.y);
            highlight_entity = false;
            return;
        }

        // Clicked on the same set of entities?
        bool same{ false };
        if(entities.size() == entities_clicked.size()) {
            bool different{ false };
            for(size_t i = 0; i < entities.size(); ++i) {
                if(entities[i] != entities_clicked[i]) {
                    different = true;
                    break;
                }
            }
            same = !different;
        }

        // Different set, reset the cycling index
        if(!same) {
            entities_clicked = entities;
            selected_entity_index = entities_clicked.size() - 1;
            highlight_entity = true;
            highlight_entity_id = entities_clicked[selected_entity_index];
        } else {
            // Same set, cycle to the next entity
            if(selected_entity_index != 0) {
                selected_entity_index -= 1;
            } else {
                selected_entity_index = entities_clicked.size() - 1;
            }
            highlight_entity = true;
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
                    gerber_file->draw(logger, elements_to_hide);
                }
                break;

            case 'Q':
            case 27:
                release_gdi_resources();
                DestroyWindow(hwnd);
                break;

            case 'F':
                set_default_zoom();
                redraw();
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

            case VK_LEFT:
                if(highlight_entity) {
                    highlight_entity_id = std::max(1, highlight_entity_id - 1);
                    redraw();
                }
                break;

            case VK_RIGHT:
                if(highlight_entity) {
                    highlight_entity_id = std::min((int)gdi_entities.size(), highlight_entity_id + 1);
                    redraw();
                }
                break;
            }
            break;

            //////////////////////////////////////////////////////////////////////

        case WM_CLOSE:
            DestroyWindow(hwnd);
            break;

            //////////////////////////////////////////////////////////////////////

        case WM_SIZE:
            redraw();
            break;

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

                if(width(drag_rect) != 0 && height(drag_rect) != 0) {

                    LOG_CONTEXT("LMBUP", debug);

                    // get position of drag rectangle in mm
                    rect const &gerber_rect = gerber_file->image.info.extent;

                    double gerber_width = width(gerber_rect);
                    double gerber_height = height(gerber_rect);

                    vec2d scale{ image_size_px.x / gerber_width, image_size_px.y / gerber_height };

                    matrix m = make_translation({ -image_pos_px.x, -image_pos_px.y });
                    m = matrix_multiply(m, make_scale({ 1 / scale.x, 1 / scale.y }));
                    m = matrix_multiply(m, make_translation({ gerber_rect.min_pos.x, gerber_rect.min_pos.y }));

                    vec2d min(drag_rect.min_pos, m);
                    vec2d max(drag_rect.max_pos, m);

                    double l = (drag_rect.min_pos.x - image_pos_px.x) / image_size_px.x;
                    double t = (drag_rect.min_pos.y - image_pos_px.y) / image_size_px.y;

                    double r = (drag_rect.max_pos.x - image_pos_px.x) / image_size_px.x;
                    double b = (drag_rect.max_pos.y - image_pos_px.y) / image_size_px.y;

                    double w = r - l;
                    double h = b - t;

                    LOG_INFO("{:5.3f},{:5.3f}-{:5.3f},{:5.3f} ({:5.3f}x{:5.3f})", l, t, r, b, w, h);

                    image_size_px = { window_size.x / w, window_size.y / h };

                    // look in zoom_image for how to center it on the center of the select box...

                    POINT center_pos{ (INT)(min.x + max.x / 2), (INT)(min.y + max.y / 2) };
                    zoom_image(center_pos, 1.0f);
                }
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
                double dx = (double)(mouse_pos.x - drag_mouse_cur_pos.x);
                double dy = (double)(mouse_pos.y - drag_mouse_cur_pos.y);
                image_pos_px = image_pos_px.add({ dx, -dy });
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

                    //// calculate coordinates in gerber space...
                    //{
                    //    rect const &gerber_rect = gerber_file->image.info.extent;
                    //    double gerber_width = width(gerber_rect);
                    //    double gerber_height = height(gerber_rect);
                    //    vec2d scale{ image_size_px.x / gerber_width, image_size_px.y / gerber_height };
                    //    matrix m = make_translation({ -image_pos_px.x, -image_pos_px.y });
                    //    m = matrix_multiply(m, make_scale({ 1 / scale.x, 1 / scale.y }));
                    //    m = matrix_multiply(m, make_translation({ gerber_rect.min_pos.x, gerber_rect.min_pos.y }));
                    //    vec2d mouse{ (double)drag_mouse_cur_pos.x, (double)drag_mouse_cur_pos.y, m };
                    //    LOG_CONTEXT("MOUSE", info);
                    //    LOG_INFO("{}", mouse);
                    //}

                    double x = std::min((double)drag_mouse_start_pos.x, (double)drag_mouse_cur_pos.x);
                    double y = std::min((double)drag_mouse_start_pos.y, (double)drag_mouse_cur_pos.y);
                    double w = std::max((double)drag_mouse_start_pos.x, (double)drag_mouse_cur_pos.x) - x;
                    double h = std::max((double)drag_mouse_start_pos.y, (double)drag_mouse_cur_pos.y) - y;

                    drag_rect_raw.min_pos = { x, y };
                    drag_rect_raw.max_pos = drag_rect_raw.min_pos.add({ w, h });

                    double window_aspect = window_size.x / window_size.y;
                    double drag_aspect = (double)w / (double)h;

                    if(drag_aspect > window_aspect) {
                        x += w / 2;
                        w = h * window_aspect;
                        x -= w / 2;
                    } else {
                        y += h / 2;
                        h = w / window_aspect;
                        y -= h / 2;
                    }

                    drag_rect.min_pos = { x, y };
                    drag_rect.max_pos = drag_rect.min_pos.add({ w, h });
                    redraw();
                }
            } break;

            case mouse_drag_none:
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
            release_gdi_resources();
            Gdiplus::GdiplusShutdown(gdiplus_token);
            hwnd = nullptr;
            PostQuitMessage(0);
            break;

            //////////////////////////////////////////////////////////////////////

        default:
            return DefWindowProcA(hwnd, message, wParam, lParam);
        }
        return 0;
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

        hwnd = CreateWindowExA(0, class_name, "Gerber Util", WS_OVERLAPPEDWINDOW, x, y, w, h, NULL, NULL, hInstance, this);

        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
    }

    //////////////////////////////////////////////////////////////////////
    // entity_id is guaranteed to increase monotonically

    void gdi_drawer::fill_elements(gerber_draw_element const *elements, size_t num_elements, gerber_polarity polarity, int entity_id)
    {
        entity_id -= 1;
        if((int)gdi_entities.size() <= entity_id) {
            bool fill = polarity == polarity_dark || polarity == polarity_positive;
            gdi_entities.emplace_back((int)gdi_paths.size(), 1, fill);
        } else {
            gdi_entities[entity_id].num_paths += 1;
        }

        GraphicsPath *p = new GraphicsPath();
        gdi_paths.push_back(p);

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
        RECT client_rect;
        GetClientRect(hwnd, &client_rect);

        if(bitmap != nullptr && bitmap->GetWidth() == (UINT)client_rect.right && bitmap->GetHeight() == (UINT)client_rect.bottom) {
            return;
        }

        release_gdi_resources();

        window_size = { (double)client_rect.right, (double)client_rect.bottom };

        bitmap = new Bitmap(client_rect.right, client_rect.bottom);
        graphics = Graphics::FromImage(bitmap);
        extent_pen = new Pen(extent_color, 0);
        axes_pen = new Pen(axes_color, 0);
        debug_pen = new Pen(debug_color, 2);
        origin_pen = new Pen(origin_color, 0);
        fill_brush[0] = new SolidBrush(gerber_fill_color[0]);
        fill_brush[1] = new SolidBrush(gerber_fill_color[1]);
        clear_brush = new SolidBrush(background_color);
        red_fill_brush = new SolidBrush(Color(64, 255, 0, 0));
        highlight_fill_brush = new SolidBrush(highlight_color_fill);
        highlight_clear_brush = new SolidBrush(highlight_color_clear);
        select_outline_pen = new Pen(select_outline_color, 0);
        select_fill_brush = new SolidBrush(select_fill_color);
        select_whole_fill_brush = new SolidBrush(select_whole_fill_color);
        info_text_background_brush = new SolidBrush(info_text_background_color);
        info_text_foregound_brush = new SolidBrush(info_text_foreground_color);
        info_text_font = new Font(L"Consolas", 10.0f, Gdiplus::FontStyleRegular);
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
        safe_delete(clear_brush);
        safe_delete(red_fill_brush);
        safe_delete(highlight_fill_brush);
        safe_delete(highlight_clear_brush);
        safe_delete(extent_pen);
        safe_delete(debug_pen);
        safe_delete(axes_pen);
        safe_delete(origin_pen);
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
                    brush = clear_brush;
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

    matrix gdi_drawer::get_transform_matrix()
    {
        if(gerber_file == nullptr) {
            return make_identity();
        }
        rect const &gerber_rect = gerber_file->image.info.extent;
        double gerber_width = width(gerber_rect);
        double gerber_height = height(gerber_rect);
        vec2d scale{ image_size_px.x / gerber_width, image_size_px.y / gerber_height };

#if 0
        // upside down (not flipped)
        matrix mat = make_translation({ -gerber_rect.min_pos.x, -gerber_rect.min_pos.y });
        mat = matrix_multiply(mat, make_scale({ scale.x, scale.y }));
        return matrix_multiply(mat, make_translation({ image_pos_px.x, image_pos_px.y }));
#else
        // right way up (vertically flipped)
        matrix mat = make_translation({ -gerber_rect.min_pos.x, -gerber_rect.min_pos.y });
        mat = matrix_multiply(mat, make_scale({ scale.x, -scale.y }));
        return matrix_multiply(mat, make_translation({ image_pos_px.x, window_size.y - image_pos_px.y }));
#endif
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

        graphics->FillRectangle(clear_brush, 0, 0, (INT)window_size.x, (INT)window_size.y);

        matrix mat = get_transform_matrix();

        Matrix gdi_matrix((REAL)mat.A, (REAL)mat.B, (REAL)mat.C, (REAL)mat.D, (REAL)mat.X, (REAL)mat.Y);
        graphics->SetTransform(&gdi_matrix);

        graphics->SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);

        // draw the whole thing

        draw_all_entities();

        // gerber_file->draw(*this, elements_to_hide);

        // draw just the highlighted net (if there is one)
        if(highlight_entity) {
            gdi_entity const &highlighted_entity = gdi_entities[highlight_entity_id - 1llu];
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

            gerber_entity &entity = gerber_file->entities[highlight_entity_id - 1];

            std::wstring line;
            if(entity.line_number_begin != entity.line_number_end) {
                line = std::format(L"Lines {} to {}", entity.line_number_begin, entity.line_number_end);
            } else {
                line = std::format(L"Line {}", entity.line_number_begin);
            }

            std::wstring text = std::format(L"Entity {:4d} {}", highlight_entity_id, line);

            PointF origin{ 10, 10 };
            RectF bounding_box;
            graphics->MeasureString(text.c_str(), (INT)text.size(), info_text_font, origin, &bounding_box);

            graphics->FillRectangle(info_text_background_brush, bounding_box);

            graphics->DrawString(text.c_str(), (INT)text.size(), info_text_font, origin, info_text_foregound_brush);
        }

        Graphics *window_graphics = Gdiplus::Graphics::FromHDC(hdc);
        window_graphics->DrawImage(bitmap, 0, 0);
    }

    //////////////////////////////////////////////////////////////////////

    void gdi_drawer::cleanup()
    {
        if(hwnd != nullptr) {
            release_gdi_resources();
        }
    }

}    // namespace gerber_3d