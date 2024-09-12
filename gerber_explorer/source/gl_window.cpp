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

    uint32_t layer_colors[] = { color::red, color::green, color::dark_cyan, color::lime_green, color::antique_white, color::corn_flower_blue, color::gold };

    uint32_t layer_color = color::red;

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

    void make_world_to_window_transform(rect const &window, rect const &view, gl_matrix result)
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
            occ.create_window(100, 100, 700, 700);
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
    // must pass the names by value here

    void gl_window::load_gerber_files(std::vector<std::string> filenames)
    {
        std::thread([=]() {
            std::vector<std::thread *> threads;
            std::vector<gl_drawer *> drawers;
            drawers.resize(50);
            int index = 0;
            for(auto const s : filenames) {
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
            if(io.WantCaptureKeyboard && is_keyboard_message(message)) {
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
            draw();
            ValidateRect(hwnd, nullptr);
        } break;

        case WM_SHOWWINDOW:
            LOG_DEBUG("SHOWWINDOW({})", wParam);
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
            vec2d mouse_pos = pos_from_lparam(lParam);
            if((GetKeyState(VK_CONTROL) & 0x8000) != 0) {
                mouse_drag = mouse_drag_zoom_select;
                zoom_anim = false;
                drag_mouse_start_pos = mouse_pos;
                drag_rect = {};
                drag_rect_raw = {};
                SetCapture(hwnd);
            } else {
                mouse_drag = mouse_drag_maybe_select;
                zoom_anim = false;
                drag_rect = {};
                drag_mouse_start_pos = mouse_pos;
                SetCapture(hwnd);
            }
        } break;

            //////////////////////////////////////////////////////////////////////

        case WM_LBUTTONUP: {
            if(mouse_drag == mouse_drag_zoom_select) {
                vec2d mn = drag_rect.min_pos;
                vec2d mx = drag_rect.max_pos;
                rect d = rect{ mn, mx }.normalize();
                if(d.width() > 1 && d.height() > 1) {
                    zoom_to_rect({ world_pos_from_window_pos(vec2d{ mn.x, mx.y }), world_pos_from_window_pos(vec2d{ mx.x, mn.y }) });
                }
            }
            mouse_drag = mouse_drag_none;
            ReleaseCapture();
        } break;

            //////////////////////////////////////////////////////////////////////

        case WM_MBUTTONDOWN:
            mouse_drag = mouse_drag_zoom;
            zoom_anim = false;
            drag_mouse_cur_pos = pos_from_lparam(lParam);
            drag_mouse_start_pos = drag_mouse_cur_pos;
            SetCapture(hwnd);
            break;

            //////////////////////////////////////////////////////////////////////

        case WM_MBUTTONUP:
            mouse_drag = mouse_drag_none;
            ReleaseCapture();
            break;

            //////////////////////////////////////////////////////////////////////

        case WM_RBUTTONDOWN:
            mouse_drag = mouse_drag_pan;
            zoom_anim = false;
            drag_mouse_cur_pos = pos_from_lparam(lParam);
            SetCapture(hwnd);
            break;

            //////////////////////////////////////////////////////////////////////

        case WM_RBUTTONUP:
            mouse_drag = mouse_drag_none;
            ReleaseCapture();
            break;

            //////////////////////////////////////////////////////////////////////

        case WM_MOUSEMOVE:

            switch(mouse_drag) {

            case mouse_drag_pan: {
                vec2d mouse_pos = pos_from_lparam(lParam);
                vec2d new_mouse_pos = world_pos_from_window_pos(mouse_pos);
                vec2d old_mouse_pos = world_pos_from_window_pos(drag_mouse_cur_pos);
                view_rect = view_rect.offset(new_mouse_pos.subtract(old_mouse_pos).negate());
                zoom_anim = false;
                drag_mouse_cur_pos = mouse_pos;
            } break;

            case mouse_drag_zoom: {
                vec2d mouse_pos = pos_from_lparam(lParam);
                vec2d d = mouse_pos.subtract(drag_mouse_cur_pos);
                zoom_image(drag_mouse_start_pos, 1.0 + (d.x - d.y) * 0.01);
                drag_mouse_cur_pos = mouse_pos;
            } break;

            case mouse_drag_zoom_select: {
                drag_mouse_cur_pos = pos_from_lparam(lParam);
                if(drag_mouse_cur_pos.subtract(drag_mouse_start_pos).length() > 4) {
                    drag_rect_raw = rect{ drag_mouse_start_pos, drag_mouse_cur_pos }.normalize();
                    drag_rect = correct_aspect_ratio(window_rect.aspect_ratio(), drag_rect_raw, aspect_expand);
                }
            } break;

            case mouse_drag_maybe_select: {
                vec2d pos = pos_from_lparam(lParam);
                if(pos.subtract(drag_mouse_start_pos).length() > drag_select_offset_start_distance) {
                    mouse_drag = mouse_drag_select;
                    // entities_clicked.clear();
                    // highlight_entity = false;
                    drag_mouse_cur_pos = pos;
                    drag_rect = rect{ drag_mouse_start_pos, drag_mouse_cur_pos };
                    // select_entities(drag_rect, (GetKeyState(VK_SHIFT) & 0x8000) == 0);
                }
            } break;

            case mouse_drag_select: {
                drag_mouse_cur_pos = pos_from_lparam(lParam);
                drag_rect = rect{ drag_mouse_start_pos, drag_mouse_cur_pos };
                // select_entities(drag_rect, (GetKeyState(VK_SHIFT) & 0x8000) == 0);
            } break;

            case mouse_drag_none: {
                // update mouse world coordinates in a status bar or something here...
            } break;

            default:
                break;
            }
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
            break;

        case WM_PAINT:
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
            layer->fill_color = layer_colors[layers.size() % gerber_util::array_length(layer_colors)];
            layer->clear_color = color::black;
            layer->outline_color = color::white & 0x80ffffff;
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

        LOG_VERBOSE("GL Version: {}", (char const *)glGetString(GL_VERSION));
        LOG_VERBOSE("GL Vendor: {}", (char const *)glGetString(GL_VENDOR));
        LOG_VERBOSE("GL Renderer: {}", (char const *)glGetString(GL_RENDERER));
        LOG_VERBOSE("GL Shader language version: {}", (char const *)glGetString(GL_SHADING_LANGUAGE_VERSION));
        LOG_VERBOSE("GLU Version: {}", (char const *)gluGetString(GLU_VERSION));

        // init ImGui

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

        ImGui::StyleColorsDark();

        ImGui_ImplWin32_InitForOpenGL(hwnd);
        ImGui_ImplOpenGL3_Init();

        ImGui::LoadIniSettingsFromDisk("ImGui.ini");

        ImFontConfig font_cfg;
        font_cfg.FontDataOwnedByAtlas = false;
        io.Fonts->AddFontFromMemoryTTF((void *)gerber_font::Consolas_ttf, (int)gerber_font::Consolas_ttf_size, 14, &font_cfg);

        // setup shader

        if(solid_program.init() != 0) {
            LOG_ERROR("solid_program.init failed - exiting");
            return -13;
        }

        if(color_program.init() != 0) {
            LOG_ERROR("color_program.init failed - exiting");
            return -14;
        }

        overlay.init(color_program);

        load_settings();

        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_window::ui()
    {
        static bool show_stats{ false };

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

        for(int i = 0; i < (int)layers.size(); ++i) {
            bool close = false;
            gerber_layer *layer = layers[i];
            ImGui::PushID(i);
            std::string name = layer->filename.c_str();
            if(layer->selected) {
                ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(255, 255, 255, 255));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(255, 255, 255, 255));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(255, 255, 255, 255));
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 255));
            }
            bool node_open = ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_Framed);
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
                ImVec4 fill_color_f;
                ImVec4 clear_color_f;
                ImVec4 outline_color_f;
                color::to_floats(layer->fill_color, &fill_color_f.x);
                color::to_floats(layer->clear_color, &clear_color_f.x);
                color::to_floats(layer->outline_color, &outline_color_f.x);
                if(ImGui::Checkbox("Fill", &layer->fill)) {
                    if(!layer->fill && !layer->outline) {
                        layer->outline = true;
                    }
                }
                if(layer->fill) {
                    ImGui::SameLine();
                    if(ImGui::ColorEdit4("Fill", &fill_color_f.x, color_edit_flags)) {
                        layer->fill_color = color::from_floats(&fill_color_f.x);
                    }
                    ImGui::SameLine();
                    if(ImGui::ColorEdit4("Clear", &clear_color_f.x, color_edit_flags)) {
                        layer->clear_color = color::from_floats(&clear_color_f.x);
                    }
                }
                if(ImGui::Checkbox("Outline", &layer->outline)) {
                    if(!layer->fill && !layer->outline) {
                        layer->fill = true;
                    }
                }
                if(layer->outline) {
                    ImGui::SameLine();
                    if(ImGui::ColorEdit4("", &outline_color_f.x, color_edit_flags)) {
                        layer->outline_color = color::from_floats(&outline_color_f.x);
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

        if(show_options) {
            ImGui::Begin("Options", &show_options);
            ImVec4 color_f;
            color::to_floats(axes_color, &color_f.x);
            ImGui::Checkbox("Show axes", &show_axes);
            ImGui::SameLine();
            if(ImGui::ColorEdit4("Axes", &color_f.x, color_edit_flags)) {
                axes_color = color::from_floats(&color_f.x);
            }
            ImGui::Checkbox("Show extent", &show_extent);
            color::to_floats(extent_color, &color_f.x);
            ImGui::SameLine();
            if(ImGui::ColorEdit4("Extents", &color_f.x, color_edit_flags)) {
                extent_color = color::from_floats(&color_f.x);
            }
            ImGui::End();
        }

        if(show_stats) {
            ImGui::Begin("Stats");
            {
                ImGuiIO &io = ImGui::GetIO();
                ImGui::Text("(%.1f FPS (%.3f ms)", io.Framerate, 1000.0f / io.Framerate);
            }
            ImGui::End();
        }
        ImGui::Render();

        if(close_all) {
            for(auto l : layers) {
                delete l;
            }
            layers.clear();
        }
    }

    //////////////////////////////////////////////////////////////////////

    void gl_window::update_view_rect()
    {
        LOG_CONTEXT("zoomer", info);

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

    void gl_window::draw()
    {
        ui();

        // ui() may have Destroyed the hwnd

        if(!IsWindowVisible(hwnd)) {
            return;
        }

        update_view_rect();

        // setup gl viewport and transform matrix

        RECT rc;
        GetClientRect(hwnd, &rc);
        window_width = rc.right;
        window_height = rc.bottom;
        glViewport(0, 0, window_width, window_height);

        if(render_target.width != window_width || render_target.height != window_height) {
            render_target.cleanup();
            render_target.init(window_width, window_height);
        }

        gl_matrix projection_matrix;
        gl_matrix view_matrix;
        gl_matrix transform_matrix;

        make_ortho(projection_matrix, window_width, window_height);
        make_world_to_window_transform(window_rect, view_rect, view_matrix);
        matrix_multiply(projection_matrix, view_matrix, transform_matrix);

        solid_program.use();

        GL_CHECK(glUniformMatrix4fv(solid_program.transform_location, 1, true, transform_matrix));

        GL_CHECK(glClearColor(0.1f, 0.1f, 0.2f, 1.0));
        GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));

        for(size_t n = layers.size(); n != 0;) {
            gerber_layer *layer = layers[--n];
            if(!layer->hide) {
                layer->layer->draw(layer->fill, layer->fill_color, layer->clear_color, layer->outline, layer->outline_color);
            }
        }

        color_program.use();

        make_ortho(projection_matrix, window_width, -window_height);
        make_translate(view_matrix, 0, (float)-window_size.y);
        matrix_multiply(projection_matrix, view_matrix, transform_matrix);

        GL_CHECK(glUniformMatrix4fv(color_program.transform_location, 1, true, transform_matrix));

        overlay.reset();
        if(mouse_drag == mouse_drag_zoom_select) {
            overlay.add_rect(drag_rect, 0x80ffff00);
            overlay.add_rect(drag_rect_raw, 0x800000ff);
            overlay.add_outline_rect(drag_rect_raw, 0xffffffff);
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

        overlay.draw();

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SwapBuffers(window_dc);
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

}    // namespace gerber_3d