#pragma once

#include "gl_base.h"

class GLUtesselator;

namespace gerber_3d
{
    //////////////////////////////////////////////////////////////////////

    struct fat_point
    {
        double x, y, z;
    };

    //////////////////////////////////////////////////////////////////////

    struct tesselator_entity
    {
        int entity_id;    // redundant, strictly speaking, but handy

        int first_outline;    // offset into outlines spans
        int first_fill;       // offset into fills spans

        uint32_t flags;    // clear/fill

        int num_outlines{};
        int num_fills{};

        gerber_lib::gerber_2d::rect bounds{};    // for picking speedup
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

    struct boundary_tesselator
    {
        LOG_CONTEXT("boundary", info);

        using vert = gerber_3d::gl_vertex_solid;
        using vec2d = gerber_lib::vec2d;
        using rect = gerber_lib::rect;

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

        void pick_entities(vec2d const &world_pos, std::list<tesselator_entity const *> &picked);
        void select_touching_entities(rect const &world_rect, std::list<tesselator_entity const *> &picked);
        void select_enclosed_entities(rect const &world_rect, std::list<tesselator_entity const *> &picked);

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

}    // namespace gerber_3d
