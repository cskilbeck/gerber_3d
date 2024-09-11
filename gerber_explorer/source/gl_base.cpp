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

#include "gl_base.h"
#include "gl_window.h"
#include "gl_functions.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")


namespace gerber_3d
{
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

    char const *fragment_shader_source_all = R"#(

        #version 400

        in vec4 fragment;
        out vec4 color;

        void main() {
            color = fragment;
        }

        )#";

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

    int gl_program::init()
    {
        vertex_shader_id = glCreateShader(GL_VERTEX_SHADER);
        fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(vertex_shader_id, 1, &vertex_shader_source, NULL);
        glShaderSource(fragment_shader_id, 1, &fragment_shader_source, NULL);

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
            cleanup();
            return rc;
        }
        glValidateProgram(program_id);
        rc = validate(GL_VALIDATE_STATUS);
        if(rc != 0) {
            cleanup();
            return rc;
        }
        use();
        transform_location = glGetUniformLocation(program_id, "transform");
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_program::use() const
    {
        glUseProgram(program_id);
    }

    //////////////////////////////////////////////////////////////////////

    int gl_solid_program::init()
    {
        vertex_shader_source = solid_vertex_shader_source;
        fragment_shader_source = fragment_shader_source_all;
        int err = gl_program::init();
        if(err != 0) {
            return err;
        }
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

    int gl_color_program::init()
    {
        vertex_shader_source = color_vertex_shader_source;
        fragment_shader_source = fragment_shader_source;
        return gl_program::init();
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

    int gl_index_array::init(GLsizei index_count)
    {
        glGenBuffers(1, &ibo_id);
        num_indices = index_count;
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * num_indices, nullptr, GL_DYNAMIC_DRAW);
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_index_array::activate() const
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id);
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_vertex_array::init(gl_program &program, GLsizei vert_count)
    {
        glGenBuffers(1, &vbo_id);

        num_verts = vert_count;

        glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
        glBufferData(GL_ARRAY_BUFFER, sizeof(gl_vertex_solid) * num_verts, nullptr, GL_DYNAMIC_DRAW);

        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_index_array::cleanup()
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        GLuint buffers[] = { ibo_id };
        glDeleteBuffers((GLsizei)gerber_util::array_length(buffers), buffers);

        ibo_id = 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_vertex_array::activate() const
    {
        glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_vertex_array_solid::init(gl_program &program, GLsizei vert_count)
    {
        gl_vertex_array::init(program, vert_count);
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

    int gl_vertex_array_color::init(gl_program &program, GLsizei vert_count)
    {
        gl_vertex_array::init(program, vert_count);
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
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        GLuint buffers[] = { vbo_id };
        glDeleteBuffers((GLsizei)gerber_util::array_length(buffers), buffers);

        vbo_id = 0;
    }

}    // namespace gerber_3d