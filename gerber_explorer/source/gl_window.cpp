//////////////////////////////////////////////////////////////////////

#include "gerber_log.h"
#include "gerber_lib.h"
#include "gerber_util.h"

#include "gl_window.h"
#include "gl_drawer.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <windowsx.h>
#include <Commdlg.h>

#include <gl/GL.h>

#include "Wglext.h"
#include "glcorearb.h"

#include "gl_window.h"
#include "gl_functions.h"
#include "polypartition.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"

#pragma comment(lib, "opengl32.lib")

// #define DRAW_TEST

namespace
{
    //////////////////////////////////////////////////////////////////////

    using vec2d = gerber_lib::gerber_2d::vec2d;
    using gl_matrix = float[16];

    using namespace gerber_lib;

    double const drag_offset_start_distance = 16;

#if defined(DRAW_TEST)
    std::vector<gerber_3d::gl_vertex> test_vertices{ { 10, 10 }, { 100, 30 }, { 80, 70 }, { 20, 60 } };
    std::vector<GLuint> test_indices{ 0, 1, 2, 0, 2, 3 };
#endif

    //////////////////////////////////////////////////////////////////////

    char const *solid_vertex_shader_source = R"#(

        #version 400

        in vec2 position;

        out vec4 fragment;

        uniform mat4 transform;
        uniform vec4 color;

        void main() {
            gl_Position = transform * vec4(position, 0.0f, 1.0f);
            fragment = color;
        }

        )#";

    //////////////////////////////////////////////////////////////////////

    char const *color_vertex_shader_source = R"#(

        #version 400

        in vec2 position;
        in vec4 color;

        out vec4 fragment;

        uniform mat4 transform;

        void main() {
            gl_Position = transform * vec4(position, 0.0f, 1.0f);
            fragment = color;
        }

        )#";

    //////////////////////////////////////////////////////////////////////

    char const *fragment_shader_source = R"#(

        #version 400

        in vec4 fragment;
        out vec4 color;

        void main() {
            color = fragment;
        }

        )#";

    //////////////////////////////////////////////////////////////////////

    void make_ortho(gl_matrix mat, int w, int h)
    {
        mat[0] = 2.0f / w;
        mat[1] = 0.0f;
        mat[2] = 0.0f;
        mat[3] = -1.0f;

        mat[4] = 0.0f;
        mat[5] = 2.0f / h;
        mat[6] = 0.0f;
        mat[7] = -1.0f;

        mat[8] = 0.0f;
        mat[9] = 0.0f;
        mat[10] = -1.0f;
        mat[11] = 0.0f;

        mat[12] = 0.0f;
        mat[13] = 0.0f;
        mat[14] = 0.0f;
        mat[15] = 1.0f;
    }

    //////////////////////////////////////////////////////////////////////

    void make_translate(gl_matrix mat, float x, float y)
    {
        mat[0] = 1.0f;
        mat[1] = 0.0f;
        mat[2] = 0.0f;
        mat[3] = x;

        mat[4] = 0.0f;
        mat[5] = 1.0f;
        mat[6] = 0.0f;
        mat[7] = y;

        mat[8] = 0.0f;
        mat[9] = 0.0f;
        mat[10] = 1.0f;
        mat[11] = 0.0f;

        mat[12] = 0.0f;
        mat[13] = 0.0f;
        mat[14] = 0.0f;
        mat[15] = 1.0f;
    }

    //////////////////////////////////////////////////////////////////////

    void make_scale(gl_matrix mat, float x_scale, float y_scale)
    {
        mat[0] = x_scale;
        mat[1] = 0.0f;
        mat[2] = 0.0f;
        mat[3] = 0.0f;

        mat[4] = 0.0f;
        mat[5] = y_scale;
        mat[6] = 0.0f;
        mat[7] = 0.0f;

        mat[8] = 0.0f;
        mat[9] = 0.0f;
        mat[10] = 1.0f;
        mat[11] = 0.0f;

        mat[12] = 0.0f;
        mat[13] = 0.0f;
        mat[14] = 0.0f;
        mat[15] = 1.0f;
    }

    //////////////////////////////////////////////////////////////////////

    void matrix_multiply(gl_matrix const a, gl_matrix const b, gl_matrix out)
    {
        for(int i = 0; i < 16; i += 4) {
            out[i + 0] = a[i] * b[0] + a[i + 1] * b[4] + a[i + 2] * b[8] + a[i + 3] * b[12];
            out[i + 1] = a[i] * b[1] + a[i + 1] * b[5] + a[i + 2] * b[9] + a[i + 3] * b[13];
            out[i + 2] = a[i] * b[2] + a[i + 1] * b[6] + a[i + 2] * b[10] + a[i + 3] * b[14];
            out[i + 3] = a[i] * b[3] + a[i + 1] * b[7] + a[i + 2] * b[11] + a[i + 3] * b[15];
        }
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

    vec2d pos_from_lparam(LPARAM lParam)
    {
        return vec2d{ (double)GET_X_LPARAM(lParam), (double)GET_Y_LPARAM(lParam) };
    }

};    // namespace

namespace gerber_3d
{
    //////////////////////////////////////////////////////////////////////

    int gl_program::check_shader(GLuint shader_id) const
    {
        GLint result;
        glGetShaderiv(shader_id, GL_COMPILE_STATUS, &result);
        if(result) {
            return 0;
        }
        GLsizei length;
        glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &length);
        if(length != 0) {
            GLchar *info_log = new GLchar[length];
            glGetShaderInfoLog(shader_id, length, &length, info_log);
            LOG_ERROR("Error in shader: {}", info_log);
            delete[] info_log;
        } else {
            LOG_ERROR("Huh? Compile error but no log?");
        }
        return -1;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_program::validate(GLuint param) const
    {
        GLint result;
        glGetProgramiv(program_id, param, &result);
        if(result) {
            return 0;
        }
        GLsizei length;
        glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &length);
        if(length != 0) {
            GLchar *info_log = new GLchar[length];
            glGetProgramInfoLog(program_id, length, &length, info_log);
            LOG_ERROR("Error in program: %s", info_log);
            delete[] info_log;
        } else if(param == GL_LINK_STATUS) {
            LOG_ERROR("glLinkProgram failed: Can not link program.");
        } else {
            LOG_ERROR("glValidateProgram failed: Can not execute shader program.");
        }
        return -1;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_program::init(char const *const vertex_shader, char const *const fragment_shader)
    {
        vertex_shader_id = glCreateShader(GL_VERTEX_SHADER);
        fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(vertex_shader_id, 1, &vertex_shader, NULL);
        glShaderSource(fragment_shader_id, 1, &fragment_shader, NULL);

        glCompileShader(vertex_shader_id);
        glCompileShader(fragment_shader_id);

        int rc = check_shader(vertex_shader_id);
        if(rc != 0) {
            return rc;
        }

        rc = check_shader(fragment_shader_id);
        if(rc != 0) {
            return rc;
        }

        program_id = glCreateProgram();

        glAttachShader(program_id, vertex_shader_id);
        glAttachShader(program_id, fragment_shader_id);

        glLinkProgram(program_id);
        rc = validate(GL_LINK_STATUS);
        if(rc != 0) {
            return rc;
        }
        glValidateProgram(program_id);
        rc = validate(GL_VALIDATE_STATUS);
        if(rc != 0) {
            return rc;
        }
        glUseProgram(program_id);

        transform_location = glGetUniformLocation(program_id, "transform");
        return on_init();
    }

    //////////////////////////////////////////////////////////////////////

    int gl_solid_program::on_init()
    {
        color_location = glGetUniformLocation(program_id, "color");
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_solid_program::set_color(uint32_t color) const
    {
        float a = ((color >> 24) & 0xff) / 255.0f;
        float b = ((color >> 16) & 0xff) / 255.0f;
        float g = ((color >> 8) & 0xff) / 255.0f;
        float r = ((color >> 0) & 0xff) / 255.0f;
        glUniform4f(color_location, r, g, b, a);
    }

    //////////////////////////////////////////////////////////////////////

    void gl_program::cleanup()
    {
        glDetachShader(program_id, vertex_shader_id);
        glDetachShader(program_id, fragment_shader_id);

        glDeleteShader(vertex_shader_id);
        vertex_shader_id = 0;

        glDeleteShader(fragment_shader_id);
        fragment_shader_id = 0;

        glDeleteProgram(program_id);
        program_id = 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_vertex_array::init(gl_program &program, GLsizei vert_count, GLsizei index_count)
    {
        glGenBuffers(1, &vbo_id);
        glGenBuffers(1, &ibo_id);

        num_verts = vert_count;
        num_indices = index_count;

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_id);

        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * num_indices, nullptr, GL_DYNAMIC_DRAW);
        glBufferData(GL_ARRAY_BUFFER, sizeof(gl_vertex_solid) * num_verts, nullptr, GL_DYNAMIC_DRAW);

        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_vertex_array::activate() const
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_vertex_array_solid::init(gl_program &program, GLsizei vert_count, GLsizei index_count)
    {
        gl_vertex_array::init(program, vert_count, index_count);
        position_location = glGetAttribLocation(program.program_id, "position");
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_vertex_array_solid::activate() const
    {
        gl_vertex_array::activate();
        glEnableVertexAttribArray(position_location);
        glVertexAttribPointer(position_location, 2, GL_FLOAT, GL_FALSE, sizeof(gl_vertex_solid), (void *)(offsetof(gl_vertex_solid, x)));
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_vertex_array_color::init(gl_program &program, GLsizei vert_count, GLsizei index_count)
    {
        gl_vertex_array::init(program, vert_count, index_count);
        position_location = glGetAttribLocation(program.program_id, "position");
        color_location = glGetAttribLocation(program.program_id, "color");
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_vertex_array_color::activate() const
    {
        gl_vertex_array::activate();
        glEnableVertexAttribArray(position_location);
        glEnableVertexAttribArray(color_location);
        glVertexAttribPointer(position_location, 2, GL_FLOAT, GL_FALSE, sizeof(gl_vertex_color), (void *)(offsetof(gl_vertex_color, x)));
        glVertexAttribPointer(color_location, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(gl_vertex_color), (void *)(offsetof(gl_vertex_color, color)));
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_vertex_array::cleanup()
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        GLuint buffers[] = { vbo_id, ibo_id };
        glDeleteBuffers(2, buffers);
    }

    //////////////////////////////////////////////////////////////////////

    void gl_window::zoom_to_rect(rect const &zoom_rect, double border_ratio)
    {
        rect new_rect = correct_aspect_ratio(window_rect.aspect_ratio(), zoom_rect, aspect_expand);
        vec2d mid = new_rect.mid_point();
        vec2d siz = new_rect.size().scale(border_ratio / 2);
        view_rect = { mid.subtract(siz), mid.add(siz) };
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

    LRESULT CALLBACK gl_window::wnd_proc_proxy(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

        if(ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam)) {
            return true;
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

        case WM_SIZE: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            vec2d new_window_size = { (double)rc.right, (double)rc.bottom };
            vec2d scale_factor = new_window_size.divide(window_size);
            window_size = new_window_size;
            window_rect = { { 0, 0 }, window_size };
            vec2d new_view_size = view_rect.size().multiply(scale_factor);
            view_rect.max_pos = view_rect.min_pos.add(new_view_size);
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
                drag_mouse_start_pos = mouse_pos;
                drag_rect = {};
                SetCapture(hwnd);
            } else {
                // on_left_click(mouse_pos, (GetKeyState(VK_SHIFT) & 0x8000) != 0);
                mouse_drag = mouse_drag_maybe_select;
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
                    view_rect = { world_pos_from_window_pos(vec2d{ mn.x, mx.y }), world_pos_from_window_pos(vec2d{ mx.x, mn.y }) };
                }
            }
            mouse_drag = mouse_drag_none;
            ReleaseCapture();
        } break;

            //////////////////////////////////////////////////////////////////////

        case WM_MBUTTONDOWN:
            mouse_drag = mouse_drag_zoom;
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
                if(pos.subtract(drag_mouse_start_pos).length() > drag_offset_start_distance) {
                    mouse_drag = mouse_drag_select;
                    //entities_clicked.clear();
                    //highlight_entity = false;
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

            default:
                break;
            }
            break;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            break;

        case WM_DESTROY:
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

        case WM_USER: {
            std::string filename = get_open_filename();
            if(!filename.empty()) {
                gerber *g = new gerber();
                if(g->parse_file(filename.c_str()) == ok) {
                    gl_drawer *drawer = new gl_drawer();
                    drawer->program = &solid_program;
                    drawer->set_gerber(g);
                    layers.push_back(drawer);
                    zoom_to_rect(g->image.info.extent);
                }
            }
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

        // init ImGui

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;    // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;     // Enable Gamepad Controls

        ImGui::StyleColorsDark();

        ImGui_ImplWin32_InitForOpenGL(hwnd);
        ImGui_ImplOpenGL3_Init();

        ImGui::LoadIniSettingsFromDisk("ImGui.ini");

        // setup shader

        if(solid_program.init(solid_vertex_shader_source, fragment_shader_source) != 0) {
            LOG_ERROR("program.init failed - exiting");
            return -13;
        }

#if defined(DRAW_TEST)
        if(verts.init(program, (GLsizei)test_vertices.size(), (GLsizei)test_indices.size()) != 0) {
            LOG_ERROR("verts.init failed - exiting");
            return -14;
        }
#endif

        // really done

        ShowWindow(hwnd, SW_SHOW);

        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_window::ui()
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ImGui::Begin("Gerber Explorer", nullptr, ImGuiWindowFlags_MenuBar);
        if(ImGui::BeginMenuBar()) {
            if(ImGui::BeginMenu("File")) {
                if(ImGui::MenuItem("Open", nullptr, nullptr)) {
                    PostMessage(hwnd, WM_USER, 0, 0);
                }
                if(ImGui::MenuItem("Exit", "Esc", nullptr)) {
                    DestroyWindow(hwnd);
                    return;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
        ImGuiIO &io = ImGui::GetIO();
        ImGui::Text("(%.1f FPS (%.3f ms)", io.Framerate, 1000.0f / io.Framerate);
        ImGui::End();
        ImGui::Render();
    }

    //////////////////////////////////////////////////////////////////////

    void gl_window::draw()
    {
        ui();

        // ui() may have Destroyed the hwnd

        if(!IsWindowVisible(hwnd)) {
            return;
        }

        // setup gl viewport and transform matrix

        RECT rc;
        GetClientRect(hwnd, &rc);
        window_width = rc.right;
        window_height = rc.bottom;
        glViewport(0, 0, window_width, window_height);

        gl_matrix projection_matrix;
        gl_matrix view_matrix;
        gl_matrix transform_matrix;

        make_ortho(projection_matrix, window_width, window_height);
        make_world_to_window_transform(window_rect, view_rect, view_matrix);
        matrix_multiply(projection_matrix, view_matrix, transform_matrix);

        glUniformMatrix4fv(solid_program.transform_location, 1, true, transform_matrix);

        glClearColor(0.1f, 0.2f, 0.5f, 0);
        glClear(GL_COLOR_BUFFER_BIT);

        for(auto layer : layers) {
            layer->draw();
        }

#if defined(DRAW_TEST)
        verts.activate(program);
        program.set_color(0xffffff00);
        using vert_type = decltype(test_vertices)::value_type;
        using index_type = decltype(test_indices)::value_type;
        vert_type *v = (vert_type *)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
        index_type *i = (index_type *)glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
        memcpy(v, test_vertices.data(), test_vertices.size() * sizeof(vert_type));
        memcpy(i, test_indices.data(), test_indices.size() * sizeof(index_type));
        glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
        glUnmapBuffer(GL_ARRAY_BUFFER);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        glDrawElements(GL_TRIANGLES, (GLsizei)test_indices.size(), GL_UNSIGNED_INT, (GLvoid *)0);
#endif

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SwapBuffers(window_dc);
    }

    //////////////////////////////////////////////////////////////////////

    std::string gl_window::get_open_filename()
    {
        // open a file name
        char filename[MAX_PATH] = {};
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
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

        if(GetOpenFileNameA(&ofn)) {
            return filename;
        }
        return std::string{};
    }


}    // namespace gerber_3d