//////////////////////////////////////////////////////////////////////

#include "gerber_log.h"
#include "gerber_lib.h"
#include "gerber_2d.h"
#include "gerber_util.h"

#include "gl_window.h"
#include "gl_drawer.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <windowsx.h>
#include <Commdlg.h>

#include <gl/GL.h>
#include <gl/GLU.h>

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

LOG_CONTEXT("gl_base", debug);

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
        GL_CHECK(glGetShaderiv(shader_id, GL_COMPILE_STATUS, &result));
        if(result) {
            return 0;
        }
        GLsizei length;
        GL_CHECK(glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &length));
        if(length != 0) {
            GLchar *info_log = new GLchar[length];
            GL_CHECK(glGetShaderInfoLog(shader_id, length, &length, info_log));
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
        GL_CHECK(glGetProgramiv(program_id, param, &result));
        if(result) {
            return 0;
        }
        GLsizei length;
        GL_CHECK(glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &length));
        if(length != 0) {
            GLchar *info_log = new GLchar[length];
            GL_CHECK(glGetProgramInfoLog(program_id, length, &length, info_log));
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
        GL_CHECK(vertex_shader_id = glCreateShader(GL_VERTEX_SHADER));
        GL_CHECK(fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER));

        GL_CHECK(glShaderSource(vertex_shader_id, 1, &vertex_shader_source, NULL));
        GL_CHECK(glShaderSource(fragment_shader_id, 1, &fragment_shader_source, NULL));

        GL_CHECK(glCompileShader(vertex_shader_id));
        GL_CHECK(glCompileShader(fragment_shader_id));

        int rc = check_shader(vertex_shader_id);
        if(rc != 0) {
            return rc;
        }

        rc = check_shader(fragment_shader_id);
        if(rc != 0) {
            return rc;
        }

        GL_CHECK(program_id = glCreateProgram());

        GL_CHECK(glAttachShader(program_id, vertex_shader_id));
        GL_CHECK(glAttachShader(program_id, fragment_shader_id));

        GL_CHECK(glLinkProgram(program_id));
        rc = validate(GL_LINK_STATUS);
        if(rc != 0) {
            cleanup();
            return rc;
        }
        GL_CHECK(glValidateProgram(program_id));
        rc = validate(GL_VALIDATE_STATUS);
        if(rc != 0) {
            cleanup();
            return rc;
        }
        use();
        GL_CHECK(transform_location = glGetUniformLocation(program_id, "transform"));
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_program::use() const
    {
        GL_CHECK(glUseProgram(program_id));
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
        GL_CHECK(color_location = glGetUniformLocation(program_id, "color"));
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_solid_program::set_color(uint32_t color) const
    {
        float a = ((color >> 24) & 0xff) / 255.0f;
        float b = ((color >> 16) & 0xff) / 255.0f;
        float g = ((color >> 8) & 0xff) / 255.0f;
        float r = ((color >> 0) & 0xff) / 255.0f;
        GL_CHECK(glUniform4f(color_location, r, g, b, a));
    }

    //////////////////////////////////////////////////////////////////////

    int gl_color_program::init()
    {
        vertex_shader_source = color_vertex_shader_source;
        fragment_shader_source = fragment_shader_source_all;
        return gl_program::init();
    }

    //////////////////////////////////////////////////////////////////////

    void gl_program::cleanup()
    {
        GL_CHECK(glDetachShader(program_id, vertex_shader_id));
        GL_CHECK(glDetachShader(program_id, fragment_shader_id));

        GL_CHECK(glDeleteShader(vertex_shader_id));
        vertex_shader_id = 0;

        GL_CHECK(glDeleteShader(fragment_shader_id));
        fragment_shader_id = 0;

        GL_CHECK(glDeleteProgram(program_id));
        program_id = 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_index_array::init(GLsizei index_count)
    {
        GL_CHECK(glGenBuffers(1, &ibo_id));
        num_indices = index_count;
        GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id));
        GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * num_indices, nullptr, GL_DYNAMIC_DRAW));
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_index_array::activate() const
    {
        GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id));
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_vertex_array::init(gl_program &program, GLsizei vert_count)
    {
        GL_CHECK(glGenBuffers(1, &vbo_id));

        num_verts = vert_count;

        GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vbo_id));
        GL_CHECK(glBufferData(GL_ARRAY_BUFFER, sizeof(gl_vertex_solid) * num_verts, nullptr, GL_DYNAMIC_DRAW));

        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_index_array::cleanup()
    {
        GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

        GLuint buffers[] = { ibo_id };
        GL_CHECK(glDeleteBuffers((GLsizei)gerber_util::array_length(buffers), buffers));

        ibo_id = 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_vertex_array::activate() const
    {
        GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vbo_id));
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_vertex_array_solid::init(gl_program &program, GLsizei vert_count)
    {
        gl_vertex_array::init(program, vert_count);
        GL_CHECK(position_location = glGetAttribLocation(program.program_id, "position"));
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_vertex_array_solid::activate() const
    {
        gl_vertex_array::activate();
        GL_CHECK(glEnableVertexAttribArray(position_location));
        GL_CHECK(glVertexAttribPointer(position_location, 2, GL_FLOAT, GL_FALSE, sizeof(gl_vertex_solid), (void *)(offsetof(gl_vertex_solid, x))));
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_vertex_array_color::init(gl_program &program, GLsizei vert_count)
    {
        gl_vertex_array::init(program, vert_count);
        GL_CHECK(position_location = glGetAttribLocation(program.program_id, "position"));
        GL_CHECK(color_location = glGetAttribLocation(program.program_id, "color"));
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_vertex_array_color::activate() const
    {
        gl_vertex_array::activate();
        GL_CHECK(glEnableVertexAttribArray(position_location));
        GL_CHECK(glEnableVertexAttribArray(color_location));
        GL_CHECK(glVertexAttribPointer(position_location, 2, GL_FLOAT, GL_FALSE, sizeof(gl_vertex_color), (void *)(offsetof(gl_vertex_color, x))));
        GL_CHECK(glVertexAttribPointer(color_location, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(gl_vertex_color), (void *)(offsetof(gl_vertex_color, color))));
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_vertex_array::cleanup()
    {
        GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));

        GLuint buffers[] = { vbo_id };
        GL_CHECK(glDeleteBuffers((GLsizei)gerber_util::array_length(buffers), buffers));

        vbo_id = 0;
    }

    //////////////////////////////////////////////////////////////////////

    int gl_render_target::init(GLuint new_width, GLuint new_height)
    {
        GL_CHECK(glGenFramebuffers(1, &fbo));
        GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, fbo));

        GL_CHECK(glGenTextures(1, &texture));
        GL_CHECK(glBindTexture(GL_TEXTURE_2D, texture));

        GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, new_width, new_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL));

        GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0));
        GLenum err = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if(err != GL_FRAMEBUFFER_COMPLETE) {
            LOG_ERROR("glCheckFramebufferStatus failed: {}", err);
            cleanup();
            return 1;
        }
        width = new_width;
        height = new_height;
        GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
        GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
        return 0;
    }

    //////////////////////////////////////////////////////////////////////

    void gl_render_target::activate()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    }

    //////////////////////////////////////////////////////////////////////

    void gl_render_target::cleanup()
    {
        GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
        GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));

        GLuint t[1] = { texture };
        GLuint f[] = { fbo };

        GL_CHECK(glDeleteTextures(1, t));
        GL_CHECK(glDeleteFramebuffers(1, f));
    }

    //////////////////////////////////////////////////////////////////////

    void gl_drawlist::draw()
    {
        vertex_array.activate();
        gl_vertex_color *v;
        GL_CHECK(v = (gl_vertex_color *)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY));
        memcpy(v, verts.data(), verts.size() * sizeof(gl_vertex_color));
        GL_CHECK(glUnmapBuffer(GL_ARRAY_BUFFER));
        GL_CHECK(glEnable(GL_BLEND));
        GL_CHECK(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
        for(auto const &d : drawlist) {
            GL_CHECK(glDrawArrays(d.draw_type, d.offset, d.count));
        }
    }
}    // namespace gerber_3d
