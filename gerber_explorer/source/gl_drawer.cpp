#include "gerber_log.h"
#include "gerber_lib.h"

#include "gl_drawer.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <windowsx.h>
#include <gl/GL.h>

#include "Wglext.h"
#include "glcorearb.h"

#include "gl_functions.h"
#include "polypartition.h"

#pragma comment(lib, "opengl32.lib")

namespace
{
    using vec2d = gerber_lib::gerber_2d::vec2d;
    using gl_matrix = float[16];

    struct vert
    {
        float x, y;
        uint32_t color;
    };

    std::vector<vert> test_vertices{ { 10, 10, 0xff00ffff }, { 100, 30, 0xffff00ff }, { 80, 70, 0xff0000ff }, { 20, 60, 0xff00ff00 } };
    std::vector<GLushort> test_indices{ 0, 1, 2, 0, 2, 3 };

    char const *vertex_shader_source = R"-----(

        #version 400
        in vec2 positionIn;
        in vec4 colorIn;
        out vec4 fragmentColor;

        uniform mat4 projection;

        void main() {
            gl_Position = projection * vec4(positionIn, 0.0f, 1.0f);
            fragmentColor = colorIn;
        
        })-----";

    //////////////////////////////////////////////////////////////////////

    char const *fragment_shader_source = R"-----(

        #version 400
        in vec4 fragmentColor;
        out vec4 color;

        void main() {
            color = fragmentColor;

        })-----";

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

    bool is_clockwise(std::vector<vec2d> const &points)
    {
        double t = 0;
        for(size_t i = 0, j = points.size() - 1; i < points.size(); j = i++) {
            t += points[i].x * points[j].y - points[i].y * points[j].x;
        }
        return t >= 0;
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

        projection_location = glGetUniformLocation(program_id, "projection");
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_program::cleanup()
    {
    }

    //////////////////////////////////////////////////////////////////////

    int gl_vertex_array::init(gl_program &program)
    {
        glGenBuffers(1, &vbo_id);
        glGenBuffers(1, &ibo_id);
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_vertex_array::activate(gl_program &program) const
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_id);

        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * 8192, nullptr, GL_DYNAMIC_DRAW);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vert) * 8192, nullptr, GL_DYNAMIC_DRAW);

        GLint positionLocation = glGetAttribLocation(program.program_id, "positionIn");
        GLint colorLocation = glGetAttribLocation(program.program_id, "colorIn");

        glEnableVertexAttribArray(positionLocation);
        glEnableVertexAttribArray(colorLocation);

        glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, sizeof(vert), (void *)(offsetof(vert, x)));
        glVertexAttribPointer(colorLocation, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vert), (void *)(offsetof(vert, color)));

        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    LRESULT CALLBACK gl_drawer::wnd_proc_proxy(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if(message == WM_CREATE) {
            LPCREATESTRUCTA c = reinterpret_cast<LPCREATESTRUCTA>(lParam);
            SetWindowLongPtrA(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(c->lpCreateParams));
        }
        gl_drawer *d = reinterpret_cast<gl_drawer *>(GetWindowLongPtrA(hWnd, GWLP_USERDATA));
        if(d != nullptr) {
            return d->wnd_proc(message, wParam, lParam);
        }
        return DefWindowProcA(hWnd, message, wParam, lParam);
    }

    //////////////////////////////////////////////////////////////////////

    LRESULT CALLBACK gl_drawer::wnd_proc(UINT message, WPARAM wParam, LPARAM lParam)
    {
        LRESULT result = 0;

        switch(message) {

        case WM_SIZE: {
            // draw();
            // swap();
        } break;

        case WM_LBUTTONDOWN: {
            // int x = GET_X_LPARAM(lParam);
            // int y = GET_Y_LPARAM(lParam);
            // on_left_click(x, y);
        } break;

        case WM_KEYDOWN:

            switch(wParam) {

            case VK_ESCAPE: {
                DestroyWindow(hwnd);
            } break;

            default:
                // on_key_press((int)wParam);
                break;
            }
            break;

        case WM_CLOSE:
            // free_gl_resources();
            DestroyWindow(hwnd);
            break;

        case WM_DESTROY:
            wglMakeCurrent(window_dc, NULL);
            wglDeleteContext(render_context);
            render_context = nullptr;
            ReleaseDC(hwnd, window_dc);
            PostQuitMessage(0);
            break;

        case WM_ERASEBKGND:
            break;

        case WM_PAINT: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            window_width = rc.right;
            window_height = rc.bottom;
            glViewport(0, 0, window_width, window_height);
            gl_matrix projection_matrix;
            make_ortho(projection_matrix, window_width, window_height);
            glUniformMatrix4fv(program.projection_location, 1, true, projection_matrix);
            glClearColor(0.1f, 0.2f, 0.5f, 0);
            glClear(GL_COLOR_BUFFER_BIT);

            vert *v = (vert *)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
            GLushort *i = (GLushort *)glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
            memcpy(v, test_vertices.data(), test_vertices.size() * sizeof(vert));
            memcpy(i, test_indices.data(), test_indices.size() * sizeof(GLushort));
            glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
            glUnmapBuffer(GL_ARRAY_BUFFER);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glDrawElements(GL_TRIANGLES, (GLsizei)test_indices.size(), GL_UNSIGNED_SHORT, (GLvoid *)0);

            SwapBuffers(window_dc);
            ValidateRect(hwnd, nullptr);
        } break;

        default:
            result = DefWindowProcA(hwnd, message, wParam, lParam);
        }
        return result;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_drawer::create_window(int x_pos, int y_pos, int client_width, int client_height)
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
        wcex.hIcon = LoadIcon(NULL, IDI_WINLOGO);
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

        // activate the true render context

        wglMakeCurrent(temp_dc, NULL);

        // destroy temp context and window

        wglDeleteContext(temp_render_context);
        ReleaseDC(temp_hwnd, temp_dc);
        DestroyWindow(temp_hwnd);

        // done

        wglMakeCurrent(window_dc, render_context);

        wglSwapIntervalEXT(1);

        // setup shader and vertex array

        if(program.init(vertex_shader_source, fragment_shader_source) != 0) {
            LOG_ERROR("program.init failed - exiting");
            return -13;
        }

        if(verts.init(program) != 0) {
            LOG_ERROR("verts.init failed - exiting");
            return -14;
        }

        verts.activate(program);

        // really done

        ShowWindow(hwnd, SW_SHOW);

        return 0;
    }

    void gl_drawer::set_gerber(gerber_lib::gerber *g)
    {
    }

    void gl_drawer::fill_elements(gerber_lib::gerber_draw_element const *elements, size_t num_elements, gerber_lib::gerber_polarity polarity, int entity_id)
    {
    }


}    // namespace gerber_3d