#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <gl/GL.h>
#include <gl/GLU.h>

#include "Wglext.h"
#include "glcorearb.h"
#include "gl_functions.h"

#include <vector>

#include "gerber_log.h"
#include "gerber_lib.h"
#include "gerber_math.h"

#include "tesselator.h"

LOG_CONTEXT("tesselator", info);

// GLU_TESS_ERRORS:
// 100151 = GLU_TESS_MISSING_BEGIN_POLYGON
// 100152 = GLU_TESS_MISSING_BEGIN_CONTOUR
// 100153 = GLU_TESS_MISSING_END_POLYGON
// 100154 = GLU_TESS_MISSING_END_CONTOUR
// 100155 = GLU_TESS_COORD_TOO_LARGE
// 100156 = GLU_TESS_NEED_COMBINE_CALLBACK
// 100157 = ?
// 100158 = ?

//////////////////////////////////////////////////////////////////////
// BOUNDARY CALLBACK PROXIES

void boundary_tesselator::begin_callback_boundary(GLenum type, GLvoid *userdata)
{
    ((boundary_tesselator *)userdata)->on_begin_boundary(type);
}
void boundary_tesselator::vertex_callback_boundary(GLvoid *vertex, GLvoid *userdata)
{
    ((boundary_tesselator *)userdata)->on_vertex_boundary(vertex);
}
void boundary_tesselator::combine_callback_boundary(GLdouble coords[3], void *d[4], GLfloat w[4], void **dataOut, GLvoid *userdata)
{
    ((boundary_tesselator *)userdata)->on_combine_boundary(coords, dataOut);
}
void boundary_tesselator::end_callback_boundary(GLvoid *userdata)
{
    ((boundary_tesselator *)userdata)->on_end_boundary();
}
void boundary_tesselator::error_callback_boundary(GLenum error, GLvoid *userdata)
{
    ((boundary_tesselator *)userdata)->on_error_boundary(error);
}
void boundary_tesselator::edge_flag_callback_boundary(GLboolean flag, GLvoid *userdata)
{
    ((boundary_tesselator *)userdata)->on_edge_flag_boundary(flag);
}

//////////////////////////////////////////////////////////////////////
// FILL CALLBACK PROXIES

void boundary_tesselator::begin_callback_fill(GLenum type, GLvoid *userdata)
{
    ((boundary_tesselator *)userdata)->on_begin_fill(type);
}
void boundary_tesselator::vertex_callback_fill(GLvoid *vertex, GLvoid *userdata)
{
    ((boundary_tesselator *)userdata)->on_vertex_fill(vertex);
}
void boundary_tesselator::combine_callback_fill(GLdouble coords[3], void *d[4], GLfloat w[4], void **dataOut, GLvoid *userdata)
{
    vert v((float)coords[0], (float)coords[1]);
    ((boundary_tesselator *)userdata)->on_combine_fill(v, (vert **)dataOut);
}
void boundary_tesselator::end_callback_fill(GLvoid *userdata)
{
    ((boundary_tesselator *)userdata)->on_end_fill();
}
void boundary_tesselator::error_callback_fill(GLenum error, GLvoid *userdata)
{
    ((boundary_tesselator *)userdata)->on_error_fill(error);
}
void boundary_tesselator::edge_flag_callback_fill(GLboolean flag, GLvoid *userdata)
{
    ((boundary_tesselator *)userdata)->on_edge_flag_fill(flag);
}

//////////////////////////////////////////////////////////////////////
// a new boundary begins

void boundary_tesselator::on_begin_boundary(GLenum type)
{
    LOG_DEBUG("BOUNDARY BEGIN: {}", type);
    boundaries.emplace_back((int)vertices.size(), 0);
}

//////////////////////////////////////////////////////////////////////
// a boundary vertex is found

void boundary_tesselator::on_vertex_boundary(GLvoid *vertex)
{
    fat_point &v = *(fat_point *)vertex;
    LOG_DEBUG("BOUNDARY VERT at {} = {},{}", vertices.size(), v.x, v.y);
    vertices.emplace_back((float)v.x, (float)v.y);
    boundaries.back().length += 1;
}

//////////////////////////////////////////////////////////////////////
// new vert needed, this shouldn't happen for boundaries?

void boundary_tesselator::on_combine_boundary(GLdouble vertex[3], GLvoid **dataOut)
{
    // TODO (chs): provide an option/setting for # of decimal places
    double x = gerber_lib::round_precise(vertex[0], 5);
    double y = gerber_lib::round_precise(vertex[1], 5);
    fat_point &v = fat_points.emplace_back(x, y, 0, (int)(fat_points.size() - 1));
    LOG_DEBUG("BOUNDARY COMBINE! New point at {} = {},{}", fat_points.back().index, v.x, v.y);
    *dataOut = (GLdouble *)&v.x;
}

//////////////////////////////////////////////////////////////////////
// current boundary ends

void boundary_tesselator::on_end_boundary()
{
    LOG_DEBUG("DONE");
}

//////////////////////////////////////////////////////////////////////

void boundary_tesselator::on_edge_flag_boundary(GLboolean flag)
{
    LOG_DEBUG("Edge flag {}", flag);
}

//////////////////////////////////////////////////////////////////////

void boundary_tesselator::on_error_boundary(GLenum error)
{
    LOG_ERROR("Boundary tesselator says {}", error);
}

//////////////////////////////////////////////////////////////////////

void boundary_tesselator::clear()
{
    LOG_DEBUG("BOUNDARY TESSELATOR: Clear");
    entities.clear();      // the entities (each may consist of multiple draw calls (if a macro has disconnected primitives))
    vertices.clear();      // the verts (for outlines and fills)
    indices.clear();       // the indices (for fills)
    boundaries.clear();    // spans for drawing outlines (GL_TRIANGLES)
    fills.clear();         // spans for drawing fills (GL_LINE_LOOP)
    fat_points.clear();
}

//////////////////////////////////////////////////////////////////////
// a new entity begins

void boundary_tesselator::new_entity(int entity_id, uint32_t flags)
{
    LOG_DEBUG("BOUNDARY New Entity: {}", entity_id);
    finish_entity();

    entities.emplace_back((int)boundaries.size(), 0, (int)fills.size(), 0, flags, entity_id);

    boundary_tess = gluNewTess();
    gluTessProperty(boundary_tess, GLU_TESS_BOUNDARY_ONLY, GL_TRUE);
    GLdouble rule;
    gluGetTessProperty(boundary_tess, GLU_TESS_WINDING_RULE, &rule);
    LOG_DEBUG("WINDING RULE: {}", rule);
    gluTessProperty(boundary_tess, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_NONZERO);
    gluTessCallback(boundary_tess, GLU_TESS_BEGIN_DATA, (GLvoid(CALLBACK *)())begin_callback_boundary);
    gluTessCallback(boundary_tess, GLU_TESS_VERTEX_DATA, (GLvoid(CALLBACK *)())vertex_callback_boundary);
    gluTessCallback(boundary_tess, GLU_TESS_COMBINE_DATA, (GLvoid(CALLBACK *)())combine_callback_boundary);
    gluTessCallback(boundary_tess, GLU_TESS_END_DATA, (GLvoid(CALLBACK *)())end_callback_boundary);
    gluTessCallback(boundary_tess, GLU_TESS_ERROR_DATA, (GLvoid(CALLBACK *)())error_callback_boundary);
    gluTessBeginPolygon(boundary_tess, this);
}

//////////////////////////////////////////////////////////////////////
// add a bunch of points to the boundary tesselator

void boundary_tesselator::append_points(size_t offset)
{
    // tesselate the boundary
    gluTessBeginContour(boundary_tess);
    for(size_t i = offset; i < points.size(); ++i) {
        vec2d const &v = points[i];
        fat_points.emplace_back(v.x, v.y, 0, (int)i);
        LOG_DEBUG("ADDING: {},{}", v.x, v.y);
        gluTessVertex(boundary_tess, (GLdouble *)&fat_points.back(), (GLvoid *)(&fat_points.back()));
    }
    gluTessEndContour(boundary_tess);
}

//////////////////////////////////////////////////////////////////////

void boundary_tesselator::finish_entity()
{
    if(boundary_tess != nullptr) {

        size_t boundary_count = boundaries.size();

        LOG_DEBUG("FINISH ENTITY");

        GERBER_ASSERT(!entities.empty() && "This should never happen - there is a bug");
        gluTessEndPolygon(boundary_tess);
        gluDeleteTess(boundary_tess);
        points.clear();
        boundary_tess = nullptr;

        for(size_t j = boundary_count; j < boundaries.size(); ++j) {
            tesselator_span const &b = boundaries[j];
            GLUtesselator *tess = gluNewTess();
            gluTessProperty(tess, GLU_TESS_BOUNDARY_ONLY, GL_FALSE);
            gluTessProperty(tess, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_ODD);
            gluTessCallback(tess, GLU_TESS_BEGIN_DATA, (GLvoid(CALLBACK *)())begin_callback_fill);
            gluTessCallback(tess, GLU_TESS_VERTEX_DATA, (GLvoid(CALLBACK *)())vertex_callback_fill);
            gluTessCallback(tess, GLU_TESS_COMBINE_DATA, (GLvoid(CALLBACK *)())combine_callback_fill);
            gluTessCallback(tess, GLU_TESS_END_DATA, (GLvoid(CALLBACK *)())end_callback_fill);
            gluTessCallback(tess, GLU_TESS_ERROR_DATA, (GLvoid(CALLBACK *)())error_callback_fill);
            gluTessCallback(tess, GLU_TESS_EDGE_FLAG_DATA, (GLvoid(CALLBACK *)())edge_flag_callback_fill);
            gluTessBeginPolygon(tess, this);
            gluTessBeginContour(tess);
            int end = b.start + b.length;
            for(size_t i = b.start; i < end; ++i) {
                vert const &v = vertices[i];
                GLdouble coords[3] = { v.x, v.y, 0 };
                gluTessVertex(tess, coords, (GLvoid *)(&v));
            }
            gluTessEndContour(tess);
            gluTessEndPolygon(tess);
            gluDeleteTess(tess);
        }
        tesselator_entity &e = entities.back();
        e.num_outlines = (int)(boundaries.size() - e.first_outline);
        e.num_fills = (int)(fills.size() - e.first_fill);
    }
}

//////////////////////////////////////////////////////////////////////

void boundary_tesselator::on_begin_fill(GLenum type)
{
    LOG_DEBUG("BEGIN_FILL: {}", type);
    fills.emplace_back((int)indices.size(), 0);
}

//////////////////////////////////////////////////////////////////////

void boundary_tesselator::on_vertex_fill(GLvoid *vertex)
{
    vert *v = (vert *)vertex;
    LOG_DEBUG("ON_VERTEX_FILL: {},{} at {}", v->x, v->y, vertices.size());
    indices.push_back((int)(v - vertices.data()));
}

//////////////////////////////////////////////////////////////////////

void boundary_tesselator::on_combine_fill(vert vertex, vert **dataOut)
{
    LOG_DEBUG("ON_COMBINE_FILL: {},{} at {}", vertex.x, vertex.y, vertices.size());
    indices.push_back((int)(vertices.size()));
    vertices.push_back(vertex);
    *dataOut = &vertices.back();
}

//////////////////////////////////////////////////////////////////////

void boundary_tesselator::on_end_fill()
{
    tesselator_span &f = fills.back();
    f.length = (int)(indices.size() - f.start);
    LOG_DEBUG("ON_END_FILL: length = {}", f.length);
}

//////////////////////////////////////////////////////////////////////

void boundary_tesselator::on_edge_flag_fill(GLboolean flag)
{
    LOG_DEBUG("FILL Edge flag {}", flag);
}

//////////////////////////////////////////////////////////////////////

void boundary_tesselator::on_error_fill(GLenum error)
{
    LOG_ERROR("Fill tesselator says {}", error);
}

//////////////////////////////////////////////////////////////////////
// ok, we're done, fill in all the boundaries and upload the verts/indices to the GPU
// !!! activate the vertex_array and index_array objects before calling this...

void boundary_tesselator::finalize()
{
    finish_entity();
}
