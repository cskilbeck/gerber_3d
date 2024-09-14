//////////////////////////////////////////////////////////////////////

#include "gerber_log.h"
#include "gerber_lib.h"
#include "gerber_2d.h"
#include "gerber_util.h"
#include "gerber_settings.h"
#include "fonts.h"

#include <vector>
#include <thread>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <windowsx.h>
#include <Commdlg.h>

#include <filesystem>

#include <gl/GL.h>
#include <gl/GLU.h>
#include "Wglext.h"
#include "glcorearb.h"

#include "gl_base.h"
#include "gl_matrix.h"
#include "gl_drawer.h"
#include "gl_functions.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"

LOG_CONTEXT("gl_window", info);

namespace
{
    //////////////////////////////////////////////////////////////////////

    auto constexpr WM_SHOW_OPEN_FILE_DIALOG = WM_USER;
    auto constexpr WM_GERBER_WAS_LOADED = WM_USER + 1;
    auto constexpr WM_FIT_TO_WINDOW = WM_USER + 2;

    using vec2d = gerber_lib::gerber_2d::vec2d;

    using namespace gerber_lib;
    using namespace gerber_3d;

    long long const zoom_lerp_time_ms = 700;

    double const drag_select_offset_start_distance = 16;

    uint32_t layer_colors[] = { gl_color::white,      gl_color::green,         gl_color::dark_cyan,
                                gl_color::lime_green, gl_color::antique_white, gl_color::corn_flower_blue,
                                gl_color::gold };

    uint32_t layer_color = gl_color::red;

    //////////////////////////////////////////////////////////////////////
    // make a rectangle have a certain aspect ratio by shrinking or expanding it

    enum aspect_ratio_correction
    {
        aspect_shrink,
        aspect_expand,
    };

    rect correct_aspect_ratio(double new_aspect_ratio, rect const &r, aspect_ratio_correction correction)
    {
        bool dir = r.aspect_ratio() > new_aspect_ratio;
        if(correction == aspect_expand) {
            dir = !dir;
        }
        vec2d n = r.size().scale(0.5);
        if(dir) {
            n.x = n.y * new_aspect_ratio;
        } else {
            n.y = n.x / new_aspect_ratio;
        }
        vec2d center = r.mid_point();
        return rect{ center.subtract(n), center.add(n) };
    }

    //////////////////////////////////////////////////////////////////////
    // make a matrix which maps window rect to world coordinates
    // if aspect ratio(view_rect) != aspect_ratio(window_rect), there will be distortion

    void make_world_to_window_transform(gl_matrix result, rect const &window, rect const &view)
    {
        gl_matrix scale;
        gl_matrix origin;

        make_scale(scale, (float)(window.width() / view.width()), (float)(window.height() / view.height()));
        make_translate(origin, -(float)view.min_pos.x, -(float)view.min_pos.y);

        matrix_multiply(scale, origin, result);
    }

    //////////////////////////////////////////////////////////////////////

    bool is_clockwise(std::vector<vec2d> const &points)
    {
        double t = 0;
        for(size_t i = 0, j = points.size() - 1; i < points.size(); j = i++) {
            t += points[i].x * points[j].y - points[i].y * points[j].x;
        }
        return t >= 0;
    }

    //////////////////////////////////////////////////////////////////////

    bool is_mouse_message(UINT message)
    {
        return message >= WM_MOUSEFIRST && message <= WM_MOUSELAST;
    }

    //////////////////////////////////////////////////////////////////////

    bool is_keyboard_message(UINT message)
    {
        return message >= WM_KEYFIRST && message <= WM_KEYLAST;
    }

    //////////////////////////////////////////////////////////////////////

    vec2d pos_from_lparam(LPARAM lParam)
    {
        return vec2d{ (double)GET_X_LPARAM(lParam), (double)GET_Y_LPARAM(lParam) };
    }

};    // namespace

namespace gerber_3d
{
    //////////////////////////////////////////////////////////////////////

    void gl_window::show_3d_view()
    {
        if(layers.empty()) {
            return;
        }

        if(occ.vout.hwnd == nullptr) {
            occ.show_progress = true;
            occ.create_window(50, 50, 1600, 900);
        }

        // std::thread([&](gerber *g) {

        //    Sleep(1000);
        //    //occ.set_gerber(g);
        //    //PostMessageA(occ.vout.hwnd, WM_USER, 0, 0);

        //}, layers.front()->layer->gerber_file).detach();
    }

    //////////////////////////////////////////////////////////////////////

    void gl_window::save_settings()
    {
        using namespace gerber_util;

        clear_settings();

        save_bool("show_axes", show_axes);
        save_bool("show_extent", show_extent);
        save_bool("show_stats", show_stats);
        save_bool("show_options", show_options);
        save_uint("background_color", background_color);
        save_int("multisample_count", multisample_count);

        int index = 0;
        for(auto const layer : layers) {
            save_string(std::format("file_{}", index), layer->layer->gerber_file->filename);
            index += 1;
            if(index > 50) {
                break;
            }
        }
    }

    //////////////////////////////////////////////////////////////////////

    void gl_window::load_settings()
    {
        using namespace gerber_util;

        load_bool("show_axes", show_axes);
        load_bool("show_extent", show_extent);
        load_bool("show_stats", show_stats);
        load_bool("show_options", show_options);
        load_uint("background_color", background_color);
        load_int("multisample_count", multisample_count);

        std::vector<std::string> names;
        for(int index = 0; index < 50; ++index) {
            std::string filename;
            if(!load_string(std::format("file_{}", index), filename)) {
                break;
            }
            names.push_back(filename);
        }
        load_gerber_files(names);
    }

    //////////////////////////////////////////////////////////////////////

    bool gl_window::get_open_filenames(std::vector<std::string> &filenames)
    {
        // open a file name
        char filename[MAX_PATH * 20] = {};
        OPENFILENAME ofn{ 0 };
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFile = filename;
        ofn.lpstrFile[0] = '\0';
        ofn.nMaxFile = (DWORD)gerber_util::array_length(filename);
        ofn.lpstrFilter = "All files\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = NULL;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

        if(GetOpenFileNameA(&ofn)) {
            int num_files = 1;
            char const *n = ofn.lpstrFile;
            std::string first_name{ n };
            n += strlen(n) + 1;
            while(*n) {
                filenames.push_back(std::format("{}\\{}", first_name, std::string{ n }));
                n += strlen(n) + 1;
                num_files += 1;
            }
            if(num_files == 1) {
                filenames.push_back(first_name);
            }
            return true;
        }
        return false;
    }

    //////////////////////////////////////////////////////////////////////
    // must pass the names by value here!

    void gl_window::load_gerber_files(std::vector<std::string> filenames)
    {
        std::thread([=]() {
            std::vector<std::thread *> threads;
            std::vector<gl_drawer *> drawers;
            drawers.resize(50);
            int index = 0;
            for(auto const &s : filenames) {
                LOG_DEBUG("LOADING {}", s);
                threads.push_back(new std::thread(
                    [&](std::string filename, int index) {
                        gerber *g = new gerber();
                        if(g->parse_file(filename.c_str()) == ok) {
                            gl_drawer *drawer = new gl_drawer();
                            drawers[index] = drawer;
                            drawer->program = &solid_program;
                            drawer->set_gerber(g);
                        }
                    },
                    s, index));
                ++index;
            }
            LOG_DEBUG("{} threads", threads.size());
            int id = 0;
            for(auto thread : threads) {
                LOG_DEBUG("Joining thread {}", id++);
                thread->join();
                delete thread;
            }
            threads.clear();
            for(int i = 0; i < index; ++i) {
                gl_drawer *d = drawers[i];
                PostMessage(hwnd, WM_GERBER_WAS_LOADED, 0, (LPARAM)d);
            }
            PostMessage(hwnd, WM_FIT_TO_WINDOW, 0, 0);
        }).detach();
    }

    //////////////////////////////////////////////////////////////////////

    vec2d gl_window::world_pos_from_window_pos(vec2d const &p) const
    {
        vec2d scale = view_rect.size().divide(window_size);
        return vec2d{ p.x, window_size.y - p.y }.multiply(scale).add(view_rect.min_pos);
    }

    //////////////////////////////////////////////////////////////////////

    vec2d gl_window::window_pos_from_world_pos(vec2d const &p) const
    {
        vec2d scale = window_size.divide(view_rect.size());
        vec2d pos = p.subtract(view_rect.min_pos).multiply(scale);
        return { pos.x, window_size.y - pos.y };
    }

    //////////////////////////////////////////////////////////////////////

    void gl_window::fit_to_window()
    {
        if(selected_layer != nullptr) {
            zoom_to_rect(selected_layer->layer->gerber_file->image.info.extent);
        } else {
            rect all{ { FLT_MAX, FLT_MAX }, { -FLT_MAX, -FLT_MAX } };
            for(auto layer : layers) {
                all = all.union_with(layer->layer->gerber_file->image.info.extent);
            }
            zoom_to_rect(all);
        }
    }

    //////////////////////////////////////////////////////////////////////

    void gl_window::zoom_to_rect(rect const &zoom_rect, double border_ratio)
    {
        if(window_rect.width() == 0 || window_rect.height() == 0) {
            return;
        }
        rect new_rect = correct_aspect_ratio(window_rect.aspect_ratio(), zoom_rect, aspect_expand);
        vec2d mid = new_rect.mid_point();
        vec2d siz = new_rect.size().scale(border_ratio / 2);
        target_view_rect = { mid.subtract(siz), mid.add(siz) };
        source_view_rect = view_rect;
        target_view_time = std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(zoom_lerp_time_ms);
        zoom_anim = true;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_window::zoom_image(vec2d const &pos, double zoom_scale)
    {
        // normalized position within view_rect
        vec2d zoom_pos = vec2d{ (double)pos.x, window_size.y - (double)pos.y }.divide(window_size);

        // scaled view_rect size
        vec2d new_size{ view_rect.width() / zoom_scale, view_rect.height() / zoom_scale };

        // world position of pos
        vec2d p = view_rect.min_pos.add(zoom_pos.multiply(view_rect.size()));

        // new rectangle offset from world position
        vec2d bottom_left = p.subtract(new_size.multiply(zoom_pos));
        vec2d top_right = bottom_left.add(new_size);
        view_rect = { bottom_left, top_right };
    }

    //////////////////////////////////////////////////////////////////////

    void gl_window::update_view_rect()
    {
        if(zoom_anim) {

            auto lerp = [](double d) {
                double p = 10;
                double x = d;
                double x2 = pow(x, p - 1);
                double x1 = x2 * x;    // pow(x, p)
                return 1 - (p * x2 - (p - 1) * x1);
            };

            auto now = std::chrono::high_resolution_clock::now();
            auto remaining_ms = std::chrono::duration_cast<std::chrono::milliseconds>(target_view_time - now).count();
            if(remaining_ms < 1) {
                remaining_ms = 0;
            }
            double t = (double)remaining_ms / (double)zoom_lerp_time_ms;
            double d = lerp(t);
            vec2d dmin = target_view_rect.min_pos.subtract(source_view_rect.min_pos).scale(d);
            vec2d dmax = target_view_rect.max_pos.subtract(source_view_rect.max_pos).scale(d);
            view_rect = { source_view_rect.min_pos.add(dmin), source_view_rect.max_pos.add(dmax) };
            vec2d wv = window_pos_from_world_pos(view_rect.min_pos);
            vec2d tv = window_pos_from_world_pos(target_view_rect.min_pos);
            if(wv.subtract(tv).length() <= 1) {
                LOG_DEBUG("View zoom complete");
                view_rect = target_view_rect;
                zoom_anim = false;
            }
        }
    }

    //////////////////////////////////////////////////////////////////////

    void gl_window::select_layer(gerber_layer *l)
    {
        if(selected_layer != nullptr) {
            selected_layer->selected = false;
        }
        if(l != nullptr) {
            l->selected = true;
        }
        selected_layer = l;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_window::gerber_layer::draw()
    {
        layer->draw(fill, fill_color, clear_color, outline, outline_color);
    }

    //////////////////////////////////////////////////////////////////////

    void gl_window::set_mouse_mode(gl_window::mouse_drag_action action, vec2d const &pos)
    {
        auto show_mouse = []() {
            while(ShowCursor(true) < 0) {
            }
        };

        auto hide_mouse = []() {
            while(ShowCursor(false) >= 0) {
            }
        };

        auto begin = [&]() {
            zoom_anim = false;
            show_mouse();
            SetCapture(hwnd);
        };

        switch(action) {

        case mouse_drag_none:
            zoom_anim = mouse_mode == mouse_drag_zoom_select;
            show_mouse();
            ReleaseCapture();
            break;

        case mouse_drag_pan:
            drag_mouse_start_pos = pos;
            begin();
            break;

        case mouse_drag_zoom:
            zoom_anim = false;
            hide_mouse();
            SetCapture(hwnd);
            drag_mouse_start_pos = pos;
            break;

        case mouse_drag_zoom_select:
            begin();
            drag_mouse_start_pos = pos;
            drag_rect = {};
            break;

        case mouse_drag_maybe_select:
            begin();
            drag_mouse_start_pos = pos;
            break;

        case mouse_drag_select:
            begin();
            drag_mouse_cur_pos = pos;
            drag_rect = rect{ drag_mouse_start_pos, drag_mouse_cur_pos };
            break;
        }
        mouse_mode = action;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_window::on_mouse_move(vec2d const &mouse_pos)
    {
        switch(mouse_mode) {

        case mouse_drag_pan: {
            vec2d new_mouse_pos = world_pos_from_window_pos(mouse_pos);
            vec2d old_mouse_pos = world_pos_from_window_pos(drag_mouse_start_pos);
            view_rect = view_rect.offset(new_mouse_pos.subtract(old_mouse_pos).negate());
            drag_mouse_start_pos = mouse_pos;
        } break;

        case mouse_drag_zoom: {
            vec2d d = mouse_pos.subtract(drag_mouse_start_pos);
            zoom_image(drag_mouse_start_pos, 1.0 + (d.x - d.y) * 0.01);
            drag_mouse_cur_pos = mouse_pos;
            POINT screen_pos{ (long)drag_mouse_start_pos.x, (long)drag_mouse_start_pos.y };
            ClientToScreen(hwnd, &screen_pos);
            SetCursorPos(screen_pos.x, screen_pos.y);
        } break;

        case mouse_drag_zoom_select: {
            drag_mouse_cur_pos = mouse_pos;
            if(drag_mouse_cur_pos.subtract(drag_mouse_start_pos).length() > 4) {
                drag_rect = rect{ drag_mouse_start_pos, drag_mouse_cur_pos }.normalize();
            }
        } break;

        case mouse_drag_maybe_select: {
            if(mouse_pos.subtract(drag_mouse_start_pos).length() > drag_select_offset_start_distance) {
                set_mouse_mode(mouse_drag_select, mouse_pos);
                // entities_clicked.clear();
                // highlight_entity = false;
                // select_entities(drag_rect, (GetKeyState(VK_SHIFT) & 0x8000) == 0);
            }
        } break;

        case mouse_drag_select: {
            drag_mouse_cur_pos = mouse_pos;
            drag_rect = rect{ drag_mouse_start_pos, drag_mouse_cur_pos };
            // select_entities(drag_rect, (GetKeyState(VK_SHIFT) & 0x8000) == 0);
        } break;

        case mouse_drag_none: {
            mouse_world_pos = world_pos_from_window_pos(mouse_pos);
        } break;

        default:
            break;
        }
    }

    //////////////////////////////////////////////////////////////////////

    LRESULT CALLBACK gl_window::wnd_proc_proxy(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

        if(ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam)) {
            return true;
        }

        if(ImGui::GetCurrentContext() != nullptr) {
            ImGuiIO &io = ImGui::GetIO();
            if(io.WantCaptureMouse && is_mouse_message(message)) {
                return 0;
            }
            if(io.WantCaptureKeyboard && is_keyboard_message(message) && wParam != VK_ESCAPE) {
                return 0;
            }
        }

        if(message == WM_CREATE) {
            LPCREATESTRUCTA c = reinterpret_cast<LPCREATESTRUCTA>(lParam);
            SetWindowLongPtrA(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(c->lpCreateParams));
        }
        gl_window *d = reinterpret_cast<gl_window *>(GetWindowLongPtrA(hWnd, GWLP_USERDATA));
        if(d != nullptr) {
            return d->wnd_proc(message, wParam, lParam);
        }
        return DefWindowProcA(hWnd, message, wParam, lParam);
    }

    //////////////////////////////////////////////////////////////////////

    LRESULT CALLBACK gl_window::wnd_proc(UINT message, WPARAM wParam, LPARAM lParam)
    {
        LRESULT result = 0;

        switch(message) {

        case WM_SIZING: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            draw(rc.right, rc.bottom);
            ValidateRect(hwnd, nullptr);
        } break;

        case WM_SHOWWINDOW:
            break;

        case WM_SIZE: {
            if(IsWindowVisible(hwnd)) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                vec2d new_window_size = { (double)rc.right, (double)rc.bottom };
                LOG_DEBUG("New window size: {}", new_window_size);
                if(window_size.x == 0 || window_size.y == 0) {
                    window_size = new_window_size;
                    window_rect = { { 0, 0 }, window_size };
                    view_rect = window_rect.offset(window_size.scale(-0.5));
                } else {
                    vec2d scale_factor = new_window_size.divide(window_size);
                    window_size = new_window_size;
                    window_rect = { { 0, 0 }, window_size };
                    vec2d new_view_size = view_rect.size().multiply(scale_factor);
                    view_rect.max_pos = view_rect.min_pos.add(new_view_size);
                }
                if(!window_size_valid && !layers.empty()) {
                    zoom_to_rect(layers.front()->layer->gerber_file->image.info.extent);
                }
                window_size_valid = true;
            }
            ValidateRect(hwnd, nullptr);
        } break;

        case WM_MOUSEWHEEL: {
            double scale_factor = ((int16_t)(HIWORD(wParam)) > 0) ? 1.1 : 0.9;
            POINT pos = POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pos);
            zoom_image(vec2d{ (double)pos.x, (double)pos.y }, scale_factor);
        } break;

            //////////////////////////////////////////////////////////////////////

        case WM_LBUTTONDOWN: {
            if((GetKeyState(VK_CONTROL) & 0x8000) != 0) {
                set_mouse_mode(mouse_drag_zoom_select, pos_from_lparam(lParam));
            } else {
                set_mouse_mode(mouse_drag_maybe_select, pos_from_lparam(lParam));
            }
        } break;

            //////////////////////////////////////////////////////////////////////

        case WM_LBUTTONUP: {
            if(mouse_mode == mouse_drag_zoom_select) {
                rect drag_rect_corrected = correct_aspect_ratio(window_rect.aspect_ratio(), drag_rect, aspect_expand);
                vec2d mn = drag_rect_corrected.min_pos;
                vec2d mx = drag_rect_corrected.max_pos;
                rect d = rect{ mn, mx }.normalize();
                if(d.width() > 2 && d.height() > 2) {
                    zoom_to_rect({ world_pos_from_window_pos(vec2d{ mn.x, mx.y }), world_pos_from_window_pos(vec2d{ mx.x, mn.y }) });
                }
            }
            set_mouse_mode(mouse_drag_none, {});
        } break;

            //////////////////////////////////////////////////////////////////////

        case WM_MBUTTONDOWN:
            set_mouse_mode(mouse_drag_zoom, pos_from_lparam(lParam));
            break;

            //////////////////////////////////////////////////////////////////////

        case WM_MBUTTONUP:
            set_mouse_mode(mouse_drag_none, {});
            break;

            //////////////////////////////////////////////////////////////////////

        case WM_RBUTTONDOWN:
            set_mouse_mode(mouse_drag_pan, pos_from_lparam(lParam));
            break;

            //////////////////////////////////////////////////////////////////////

        case WM_RBUTTONUP:
            set_mouse_mode(mouse_drag_none, {});
            break;

            //////////////////////////////////////////////////////////////////////

        case WM_MOUSEMOVE:

            on_mouse_move(pos_from_lparam(lParam));
            break;

        case WM_KEYDOWN:

            switch(wParam) {

            case VK_ESCAPE:
                DestroyWindow(hwnd);
                break;

            case '3':
                show_3d_view();
                break;

            default:
                break;
            }
            break;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            break;

        case WM_DESTROY:
            save_settings();
            ImGui::SaveIniSettingsToDisk("ImGui.ini");
            wglMakeCurrent(window_dc, NULL);
            wglDeleteContext(render_context);
            render_context = nullptr;
            ReleaseDC(hwnd, window_dc);
            PostQuitMessage(0);
            break;

        case WM_ERASEBKGND:
            ValidateRect(hwnd, nullptr);
            break;

        case WM_SHOW_OPEN_FILE_DIALOG: {
            std::vector<std::string> filenames;
            if(get_open_filenames(filenames)) {
                load_gerber_files(filenames);
            }
        } break;

        case WM_GERBER_WAS_LOADED: {
            gl_drawer *drawer = (gl_drawer *)lParam;
            drawer->on_finished_loading();
            gerber_layer *layer = new gerber_layer();
            layer->layer = drawer;
            layer->fill_color = layer_colors[layers.size() % gerber_util::array_length(layer_colors)] & 0x80ffffff;
            layer->clear_color = gl_color::clear;
            layer->outline_color = gl_color::white;
            layer->outline = false;
            layer->filename = std::filesystem::path(drawer->gerber_file->filename).filename().string();
            layers.push_back(layer);
        } break;

        case WM_FIT_TO_WINDOW: {
            fit_to_window();
        } break;

        default:
            result = DefWindowProcA(hwnd, message, wParam, lParam);
        }
        return result;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_window::create_window(int x_pos, int y_pos, int client_width, int client_height)
    {
        HINSTANCE instance = GetModuleHandleA(nullptr);

        static constexpr char const *class_name = "GL_CONTEXT_WINDOW_CLASS";
        static constexpr char const *window_title = "GL Window";

        // register window class

        WNDCLASSEXA wcex{};
        memset(&wcex, 0, sizeof(wcex));
        wcex.cbSize = sizeof(WNDCLASSEXA);
        wcex.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wcex.lpfnWndProc = (WNDPROC)wnd_proc_proxy;
        wcex.hInstance = instance;
        wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
        wcex.lpszClassName = class_name;

        if(!RegisterClassExA(&wcex)) {
            return -1;
        }

        // create temp render context

        HWND temp_hwnd = CreateWindowExA(0, class_name, "", 0, 0, 0, 1, 1, nullptr, nullptr, instance, nullptr);
        if(temp_hwnd == nullptr) {
            return -2;
        }

        HDC temp_dc = GetDC(temp_hwnd);
        if(temp_dc == nullptr) {
            return -3;
        }

        PIXELFORMATDESCRIPTOR temp_pixel_format_desc{};
        temp_pixel_format_desc.nSize = sizeof(PIXELFORMATDESCRIPTOR);
        temp_pixel_format_desc.nVersion = 1;
        temp_pixel_format_desc.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
        temp_pixel_format_desc.iPixelType = PFD_TYPE_RGBA;
        temp_pixel_format_desc.cColorBits = 32;
        temp_pixel_format_desc.cAlphaBits = 8;
        temp_pixel_format_desc.cDepthBits = 24;
        int temp_pixelFormat = ChoosePixelFormat(temp_dc, &temp_pixel_format_desc);
        if(temp_pixelFormat == 0) {
            return -4;
        }

        if(!SetPixelFormat(temp_dc, temp_pixelFormat, &temp_pixel_format_desc)) {
            return -5;
        }

        HGLRC temp_render_context = wglCreateContext(temp_dc);
        if(temp_render_context == nullptr) {
            return -6;
        }

        // activate temp render context so we can...

        wglMakeCurrent(temp_dc, temp_render_context);

        // ...get some opengl function pointers

        init_gl_functions();

        // now opengl functions are available, create actual window

        fullscreen = false;

        RECT rect{ x_pos, y_pos, client_width, client_height };
        DWORD style = WS_OVERLAPPEDWINDOW;
        if(!AdjustWindowRect(&rect, style, false)) {
            return -7;
        }

#if 0
        HMONITOR monitor = MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);

        MONITORINFO mi = { sizeof(mi) };
        if(!GetMonitorInfoA(monitor, &mi)) {
            return -8;
        }

        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;
        int x = (mi.rcMonitor.right - mi.rcMonitor.left - width) / 2;
        int y = (mi.rcMonitor.bottom - mi.rcMonitor.top - height) / 2;
#else
        int x = rect.left;
        int y = rect.top;
        int width = rect.right - x;
        int height = rect.bottom - y;
#endif

        hwnd = CreateWindowExA(0, class_name, window_title, style, x, y, width, height, nullptr, nullptr, instance, this);
        if(hwnd == nullptr) {
            return -8;
        }

        window_dc = GetDC(hwnd);
        if(window_dc == nullptr) {
            return -9;
        }

        // create actual render context

        // clang-format off

        static constexpr int const pixel_attributes[] = {
            WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
            WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
            WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
            WGL_SWAP_METHOD_ARB, WGL_SWAP_EXCHANGE_ARB,
            WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
            WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
            WGL_COLOR_BITS_ARB, 32,
            WGL_ALPHA_BITS_ARB, 8,
            WGL_SAMPLE_BUFFERS_ARB, GL_TRUE,
            WGL_SAMPLES_ARB, 8,
            WGL_DEPTH_BITS_ARB, 24,
            0 };

        static constexpr int const context_attributes[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
            WGL_CONTEXT_MINOR_VERSION_ARB, 0,
            WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            0 };

        // clang-format on

        int pixel_format;
        UINT num_formats;
        BOOL status = wglChoosePixelFormatARB(window_dc, pixel_attributes, nullptr, 1, &pixel_format, &num_formats);
        if(!status || num_formats == 0) {
            return -10;
        }

        PIXELFORMATDESCRIPTOR pixel_format_desc{};
        DescribePixelFormat(window_dc, pixel_format, sizeof(PIXELFORMATDESCRIPTOR), &pixel_format_desc);

        if(!SetPixelFormat(window_dc, pixel_format, &pixel_format_desc)) {
            return -11;
        }

        render_context = wglCreateContextAttribsARB(window_dc, 0, context_attributes);
        if(render_context == nullptr) {
            return -12;
        }

        // destroy temp context and window

        wglMakeCurrent(temp_dc, NULL);
        wglDeleteContext(temp_render_context);
        ReleaseDC(temp_hwnd, temp_dc);
        DestroyWindow(temp_hwnd);

        // activate the true render context

        wglMakeCurrent(window_dc, render_context);
        wglSwapIntervalEXT(1);

        // draw something before the window is shown so that we don't get a flash of some random color

        GL_CHECK(glViewport(0, 0, client_width, client_width));
        GL_CHECK(glClearColor(0, 0, 0, 1));
        GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));

        GL_CHECK(SwapBuffers(window_dc));

        // info some GL stuff

        LOG_INFO("GL Version: {}", (char const *)glGetString(GL_VERSION));
        LOG_INFO("GL Vendor: {}", (char const *)glGetString(GL_VENDOR));
        LOG_INFO("GL Renderer: {}", (char const *)glGetString(GL_RENDERER));
        LOG_INFO("GL Shader language version: {}", (char const *)glGetString(GL_SHADING_LANGUAGE_VERSION));
        LOG_INFO("GLU Version: {}", (char const *)gluGetString(GLU_VERSION));

        // init ImGui

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

#if defined(IMGUI_HAS_DOCK)
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#endif

        ImGui::StyleColorsDark();

        ImGui_ImplWin32_InitForOpenGL(hwnd);
        ImGui_ImplOpenGL3_Init();

        ImGui::LoadIniSettingsFromDisk("ImGui.ini");

        ImFontConfig font_cfg;
        font_cfg.FontDataOwnedByAtlas = false;
        io.Fonts->AddFontFromMemoryTTF((void *)gerber_font::Consolas_ttf, (int)gerber_font::Consolas_ttf_size, 14, &font_cfg);

        // setup shaders

        if(solid_program.init() != 0) {
            LOG_ERROR("solid_program.init failed - exiting");
            return -13;
        }

        if(color_program.init() != 0) {
            LOG_ERROR("color_program.init failed - exiting");
            return -14;
        }

        if(textured_program.init() != 0) {
            LOG_ERROR("textured_program.init failed - exiting");
            return -15;
        }

        if(overlay.init(color_program) != 0) {
            LOG_ERROR("overlay.init failed");
            return -16;
        }

        fullscreen_blit_verts.init(textured_program, 3);

        // load ini file (and maybe kick off a bunch of file loading threads)

        load_settings();

        glGetInternalformativ(GL_TEXTURE_2D_MULTISAMPLE, GL_RGBA, GL_SAMPLES, 1, &max_multisamples);

        LOG_INFO("MAX GL Multisamples: {}", max_multisamples);

        if(multisample_count > max_multisamples) {
            multisample_count = max_multisamples;
        }

        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_window::ui(int wide, int high)
    {
        auto color_edit_flags = ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip;

        bool close_all = false;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Gerber Explorer", nullptr, ImGuiWindowFlags_MenuBar);
        if(ImGui::BeginMenuBar()) {
            if(ImGui::BeginMenu("File")) {
                if(ImGui::MenuItem("Open", nullptr, nullptr)) {
                    PostMessage(hwnd, WM_SHOW_OPEN_FILE_DIALOG, 0, 0);
                }
                ImGui::MenuItem("Stats", nullptr, &show_stats);
                ImGui::MenuItem("Options", nullptr, &show_options);
                if(ImGui::MenuItem("Close all", nullptr, nullptr)) {
                    close_all = true;
                }
                ImGui::Separator();
                if(ImGui::MenuItem("Exit", "Esc", nullptr)) {
                    DestroyWindow(hwnd);
                    return;
                }
                ImGui::EndMenu();
            }
            if(ImGui::BeginMenu("View")) {
                if(ImGui::MenuItem("Fit to window", nullptr, nullptr, !layers.empty())) {
                    fit_to_window();
                }
                ImGui::MenuItem("Show Axes", nullptr, &show_axes);
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        for(size_t i = 0; i < layers.size(); ++i) {
            bool close = false;
            gerber_layer *layer = layers[i];
            std::string name = layer->filename.c_str();
            if(layer->selected) {
                ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(255, 255, 255, 255));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(255, 255, 255, 255));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(255, 255, 255, 255));
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 255));
            }
            ImGui::PushID((int)i);
            bool node_open = ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_OpenOnArrow);
            if(layer->selected) {
                ImGui::PopStyleColor();
                ImGui::PopStyleColor();
                ImGui::PopStyleColor();
                ImGui::PopStyleColor();
            }
            if(ImGui::IsItemClicked() && node_open == layer->expanded) {
                if(layer->selected) {
                    select_layer(nullptr);
                } else {
                    select_layer(layer);
                }
            }
            layer->expanded = node_open;
            if(ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("TREENODE_PAYLOAD", &layer->index, sizeof(int));
                ImGui::EndDragDropSource();
            }
            ImGui::Indent();
            if(ImGui::BeginDragDropTarget()) {
                if(ImGuiPayload const *payload = ImGui::AcceptDragDropPayload("TREENODE_PAYLOAD", ImGuiDragDropFlags_None)) {
                    int index = *(int const *)payload->Data;
                    auto it = layers.begin() + index;
                    auto node = *it;
                    layers.erase(it);
                    layers.insert(layers.begin() + i, node);
                    int new_index = 0;
                    for(auto const &l : layers) {
                        l->index = new_index++;
                        if(l->selected) {
                            select_layer(l);
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }
            ImGui::Unindent();
            if(node_open) {
                ImGui::Checkbox("Hide", &layer->hide);
                ImGui::SameLine();
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 100);
                if(ImGui::Button("Close")) {
                    close = true;
                }
                ImGui::PopStyleVar();
                gl_color::float4 fill_color_f(layer->fill_color);
                gl_color::float4 clear_color_f(layer->clear_color);
                gl_color::float4 outline_color_f(layer->outline_color);
                if(ImGui::Checkbox("Fill", &layer->fill)) {
                    if(!layer->fill && !layer->outline) {
                        layer->outline = true;
                    }
                }
                if(layer->fill) {
                    ImGui::SameLine();
                    if(ImGui::ColorEdit4("Fill", fill_color_f, color_edit_flags)) {
                        layer->fill_color = gl_color::from_floats(fill_color_f);
                    }
                    ImGui::SameLine();
                    if(ImGui::ColorEdit4("Clear", clear_color_f, color_edit_flags)) {
                        layer->clear_color = gl_color::from_floats(clear_color_f);
                    }
                }
                if(ImGui::Checkbox("Outline", &layer->outline)) {
                    if(!layer->fill && !layer->outline) {
                        layer->fill = true;
                    }
                }
                if(layer->outline) {
                    ImGui::SameLine();
                    if(ImGui::ColorEdit4("", outline_color_f, color_edit_flags)) {
                        layer->outline_color = gl_color::from_floats(outline_color_f);
                    }
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
            if(close) {
                gerber_layer *l = layers[i];
                if(l->selected) {
                    select_layer(nullptr);
                }
                layers.erase(layers.begin() + i);
            }
        }
        ImGui::End();

        ImGui::Begin("Options", &show_options);
        {
            gl_color::float4 color_f(axes_color);
            ImGui::Checkbox("Show axes", &show_axes);
            ImGui::SameLine();
            if(ImGui::ColorEdit4("", color_f, color_edit_flags)) {
                axes_color = gl_color::from_floats(color_f);
            }
            ImGui::SetNextItemWidth(150);
            ImGui::Checkbox("Show extent", &show_extent);
            color_f = extent_color;
            ImGui::SameLine();
            if(ImGui::ColorEdit4("", color_f, color_edit_flags)) {
                extent_color = gl_color::from_floats(color_f);
            }
            color_f = background_color;
            if(ImGui::ColorEdit3("Background", color_f, color_edit_flags)) {
                background_color = gl_color::from_floats(color_f);
            }
            ImGui::Checkbox("Wireframe", &wireframe);
            ImGui::SetNextItemWidth(100);
            ImGui::SliderInt("Multisamples", &multisample_count, 1, max_multisamples);
        }
        ImGui::End();

        ImGui::Begin("Stats", &show_stats);
        {
            // ImGuiIO &io = ImGui::GetIO();
            // ImGui::Text("(%.1f FPS (%.3f ms)", io.Framerate, 1000.0f / io.Framerate);
            char const *imp_mm = imperial ? "IN" : "MM";
            if(ImGui::Button(imp_mm)) {
                imperial = !imperial;
            }
            double scale = 1.0;
            if(imperial) {
                scale = 1.0 / 25.4;
            }
            ImGui::Text("X: %14.8f", mouse_world_pos.x * scale);
            ImGui::Text("Y: %14.8f", mouse_world_pos.y * scale);
        }
        ImGui::End();

        ImGui::Render();

        if(close_all) {
            for(auto l : layers) {
                delete l;
            }
            layers.clear();
        }
    }

    //////////////////////////////////////////////////////////////////////

    void gl_window::draw(int width_pixels, int height_pixels)
    {
        window_width = width_pixels;
        window_height = height_pixels;

        if(overlay.vertex_array.vbo_id == 0) {
            return;
        }

        if(window_width == 0 || window_height == 0) {
            return;
        }

        GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));

        ui(width_pixels, height_pixels);

        if(!IsWindowVisible(hwnd)) {
            return;
        }

        render();

        // Draw ImGui on top of everything else

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SwapBuffers(window_dc);
    }

    //////////////////////////////////////////////////////////////////////

    void gl_window::render()
    {
        GL_CHECK(glViewport(0, 0, window_width, window_height));

        gl_color::float4 back_col(background_color);

        GL_CHECK(glClearColor(back_col[0], back_col[1], back_col[2], back_col[3]));
        GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));

        // setup gl viewport and transform matrix

        if(render_target.width != window_width || render_target.height != window_height || render_target.num_samples != multisample_count) {
            render_target.cleanup();
            render_target.init(window_width, window_height, multisample_count);
        }

        // draw the gerber layers

        update_view_rect();

        gl_matrix projection_matrix_invert_y;
        gl_matrix projection_matrix;
        gl_matrix view_matrix;
        gl_matrix world_transform_matrix;
        gl_matrix screen_matrix;

        // make a 1:1 screen matrix

        make_ortho(projection_matrix_invert_y, window_width, -window_height);
        make_translate(view_matrix, 0, (float)-window_size.y);
        matrix_multiply(projection_matrix_invert_y, view_matrix, screen_matrix);

        // make world to window matrix

        make_ortho(projection_matrix, window_width, window_height);
        make_world_to_window_transform(view_matrix, window_rect, view_rect);
        matrix_multiply(projection_matrix, view_matrix, world_transform_matrix);

        gl_vertex_textured quad[3] = { { 0, 0, 0, 0 }, { (float)window_size.x * 2, 0, 2, 0 }, { 0, (float)window_size.y * 2, 0, 2 } };
        fullscreen_blit_verts.activate();
        gl_vertex_textured *v;
        GL_CHECK(v = (gl_vertex_textured *)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY));
        memcpy(v, quad, 3 * sizeof(gl_vertex_textured));
        GL_CHECK(glUnmapBuffer(GL_ARRAY_BUFFER));

        glEnable(GL_BLEND);

        for(size_t n = layers.size(); n != 0;) {

            gerber_layer *layer = layers[--n];

            if(!layer->hide) {

                // draw the gerber layer into the render texture

                render_target.activate();
                solid_program.use();
                GL_CHECK(glUniformMatrix4fv(solid_program.transform_location, 1, true, world_transform_matrix));
                GL_CHECK(glClearColor(0, 0, 0, 0));
                GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));

                if(wireframe) {
                    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                } else {
                    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                }

                layer->draw();

                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

                // draw the render texture to the window

                GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));

                render_target.bind();

                textured_program.use();

                fullscreen_blit_verts.activate();

                gl_color::float4 fill(layer->fill_color);
                gl_color::float4 clear(layer->clear_color);
                gl_color::float4 outline(layer->outline_color);

                glUniform4fv(textured_program.color_r_location, 1, fill);
                glUniform4fv(textured_program.color_g_location, 1, clear);
                glUniform4fv(textured_program.color_b_location, 1, outline);

                glUniformMatrix4fv(textured_program.transform_location, 1, true, projection_matrix);

                glUniform1i(textured_program.num_samples_uniform, render_target.num_samples);

                glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
                glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
                glDrawArrays(GL_TRIANGLES, 0, 3);
            }
        }

        // create overlay drawlist (axes, extents etc)

        overlay.reset();
        if(mouse_mode == mouse_drag_zoom_select) {
            rect drag_rect_corrected = correct_aspect_ratio(window_rect.aspect_ratio(), drag_rect, aspect_expand);
            overlay.add_rect(drag_rect_corrected, 0x80ffff00);
            overlay.add_rect(drag_rect, 0x800000ff);
            overlay.add_outline_rect(drag_rect, 0xffffffff);
        }

        vec2d origin = window_pos_from_world_pos({ 0, 0 });

        if(show_axes) {    // show_axes
            overlay.lines();
            overlay.add_line({ 0, origin.y }, { window_size.x, origin.y }, axes_color);
            overlay.add_line({ origin.x, 0 }, { origin.x, window_size.y }, axes_color);
        }

        if(show_extent && selected_layer != nullptr) {
            rect const &extent = selected_layer->layer->gerber_file->image.info.extent;
            rect s{ window_pos_from_world_pos(extent.min_pos), window_pos_from_world_pos(extent.max_pos) };
            overlay.add_outline_rect(s, extent_color);
        }

        if(mouse_mode == mouse_drag_select) {
            rect f{ drag_mouse_start_pos, drag_mouse_cur_pos };
            uint32_t color = 0x60ff8020;
            if(f.min_pos.x > f.max_pos.x) {
                color = 0x6080ff20;
            }
            overlay.add_rect(f, color);
            overlay.add_outline_rect(f, 0xffffffff);
        }

        // draw overlay

        color_program.use();

        GL_CHECK(glUniformMatrix4fv(color_program.transform_location, 1, true, screen_matrix));

        overlay.draw();
    }

}    // namespace gerber_3d
