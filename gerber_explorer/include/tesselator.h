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

struct fat_point
{
    double x, y, z;
    int index;
};

//////////////////////////////////////////////////////////////////////

struct tesselator_entity
{
    int first_outline;
    int num_outlines;
    int first_fill;
    int num_fills;
    uint32_t flags;
    int entity_id;  // redundant, strictly speaking, but handy
};

//////////////////////////////////////////////////////////////////////

struct tesselator_span
{
    int start;    // glDrawArrays(start, length) or glDrawElements(start, length)
    int length;
};

//////////////////////////////////////////////////////////////////////
// The boundary tesselator finds the boundary of a sequence of entities
// Needed for macros which consist of multiple primitives
// It only stores vertices which can be used to draw the outline with glDrawArrays
// Once the boundary is complete, a triangulation tesselator fills the interior

class GLUtesselator;

struct boundary_tesselator
{
    LOG_CONTEXT("boundary", info);

    using vert = gerber_3d::gl_vertex_solid;
    using vec2d = gerber_lib::vec2d;

    boundary_tesselator() = default;

    GLUtesselator *boundary_tess;

    std::vector<vec2d> points;

    std::list<fat_point> fat_points;

    std::vector<tesselator_entity> entities;

    std::vector<vert> vertices;
    std::vector<GLuint> indices;

    std::vector<tesselator_span> boundaries;
    std::vector<tesselator_span> fills;

    void clear();
    void new_entity(int entity_id, uint32_t flags);
    void append_points(size_t offset);
    void finish_entity();
    void finalize();

    //////////////////////////////////////////////////////////////////////
    // internals for libglu tesselator

    void on_begin_boundary(GLenum type);
    void on_vertex_boundary(GLvoid *vertex);
    void on_combine_boundary(GLdouble vertex[3], GLvoid **dataOut);
    void on_end_boundary();
    void on_error_boundary(GLenum error);
    void on_edge_flag_boundary(GLboolean flag);

    static void begin_callback_boundary(GLenum type, GLvoid *userdata);
    static void vertex_callback_boundary(GLvoid *vertex, GLvoid *userdata);
    static void combine_callback_boundary(GLdouble coords[3], void *d[4], GLfloat w[4], void **dataOut, GLvoid *userdata);
    static void end_callback_boundary(GLvoid *userdata);
    static void error_callback_boundary(GLenum error, GLvoid *userdata);
    static void edge_flag_callback_boundary(GLboolean flag, GLvoid *userdata);

    void on_begin_fill(GLenum type);
    void on_vertex_fill(GLvoid *vertex);
    void on_combine_fill(vert vertex, vert **dataOut);
    void on_end_fill();
    void on_error_fill(GLenum error);
    void on_edge_flag_fill(GLboolean flag);

    static void begin_callback_fill(GLenum type, GLvoid *userdata);
    static void vertex_callback_fill(GLvoid *vertex, GLvoid *userdata);
    static void combine_callback_fill(GLdouble coords[3], void *d[4], GLfloat w[4], void **dataOut, GLvoid *userdata);
    static void end_callback_fill(GLvoid *userdata);
    static void error_callback_fill(GLenum error, GLvoid *userdata);
    static void edge_flag_callback_fill(GLboolean flag, GLvoid *userdata);
};

