#pragma once

#include "gl_base.h"

//////////////////////////////////////////////////////////////////////

struct tesselator_draw_call
{
    GLsizei count{};         // # of indices
    void const *offset{};    // byte offset into index buffer

    GLsizei line_offset{};    // offset into the vertex buffer for drawing the outline
    GLsizei line_count{};     // # of vertices for drawing the outline (glDrawArrays(GL_LINE...)

    uint32_t flags;

    void draw_filled() const;
    void draw_outline() const;
};

//////////////////////////////////////////////////////////////////////

struct tesselator
{
    LOG_CONTEXT("tesselator", info);

    enum draw_flags : uint32_t
    {
        draw_flag_filled = 1,
        draw_flag_outline = 2,
    };

    using vert = gerber_3d::gl_vertex_solid;
    using vec2d = gerber_lib::vec2d;

    tesselator();

    std::vector<vert> vertices;
    std::vector<GLuint> indices;
    std::vector<tesselator_draw_call> draw_calls;

    uint32_t current_flags{};

    // clear all the verts, indices and draw_call
    void clear();

    // triangulate some points and add necessary draw call(s)
    void append(vec2d const *points, int num_points, uint32_t flags);

    // upload verts and indices to the GPU.
    // bind the vertex and index buffer objects before calling this....!
    void finalize();

    //////////////////////////////////////////////////////////////////////
    // internals for libglu tesselator

    void on_vertex(GLvoid *vertex);
    void on_begin(GLenum type);
    void on_end();
    void on_error(GLenum error);

    static void begin_callback(GLenum type, GLvoid *userdata);
    static void vertex_callback(GLvoid *vertex, GLvoid *userdata);
    static void end_callback(GLvoid *userdata);
    static void error_callback(GLenum error, GLvoid *userdata);
    static void edge_flag_callback(GLvoid *userdata);
};
