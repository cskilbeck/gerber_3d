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

#include "tesselator.h"

//////////////////////////////////////////////////////////////////////

void tesselator_draw_call::draw_filled() const
{
    glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, offset);
}

//////////////////////////////////////////////////////////////////////

void tesselator_draw_call::draw_outline() const
{
    glDrawArrays(GL_LINE_STRIP, line_offset, line_count);
}

//////////////////////////////////////////////////////////////////////

tesselator::tesselator()
{
    LOG_DEBUG("tesselator::tesselator");
}

void tesselator::clear()
{
    vertices.clear();
    draw_calls.clear();
    indices.clear();
}

//////////////////////////////////////////////////////////////////////

void tesselator::begin_callback(GLenum type, GLvoid *userdata)
{
    ((tesselator *)userdata)->on_begin(type);
}

//////////////////////////////////////////////////////////////////////

void tesselator::vertex_callback(GLvoid *vertex, GLvoid *userdata)
{
    ((tesselator *)userdata)->on_vertex(vertex);
}

//////////////////////////////////////////////////////////////////////

void tesselator::combine_callback(GLdouble coords[3], void *d[4], GLfloat w[4], void **dataOut, GLvoid *userdata)
{
    vert v((float)coords[0], (float)coords[1]);
    ((tesselator *)userdata)->on_combine(v, (vert **)dataOut);
}

//////////////////////////////////////////////////////////////////////

void tesselator::end_callback(GLvoid *userdata)
{
    ((tesselator *)userdata)->on_end();
}

//////////////////////////////////////////////////////////////////////

void tesselator::error_callback(GLenum error, GLvoid *userdata)
{
    ((tesselator *)userdata)->on_error(error);
}

//////////////////////////////////////////////////////////////////////

void tesselator::on_vertex(GLvoid *vertex)
{
    draw_calls.back().count += 1;
    GLuint index = (GLuint)((vert *)vertex - vertices.data());
    indices.push_back(index);
}

//////////////////////////////////////////////////////////////////////

void tesselator::on_combine(vert vertex, vert **dataOut)
{
    vertices.push_back(vertex);
    draw_calls.back().count += 1;
    GLuint index = (GLuint)(vertices.size() - 1);
    indices.push_back(index);
    *dataOut = &vertices[index];
}

//////////////////////////////////////////////////////////////////////

void tesselator::on_begin(GLenum type)
{
    draw_calls.emplace_back(0, (void *)(indices.size() * sizeof(GLuint)), 0, 0, current_flags);
}

//////////////////////////////////////////////////////////////////////

void tesselator::on_end()
{
}

//////////////////////////////////////////////////////////////////////

void tesselator::on_error(GLenum error)
{
}

//////////////////////////////////////////////////////////////////////
// if this callback is supplied, and does nothing, it generates
// triangles only, rather than fans, strips and triangles, so
// we end up with a single draw call which is probably better
// because the vertex cache will deal and we don't have loads
// of draw calls to issue from the CPU, although I haven't
// measured anything, this is still an assumption...

void tesselator::edge_flag_callback(GLvoid *userdata)
{
}

//////////////////////////////////////////////////////////////////////

void tesselator::append(vec2d const *points, int num_points, uint32_t flags)
{
    int vertex_offset = (int)vertices.size();
    current_flags = flags;

    for(int i = 0; i < num_points; ++i) {
        vec2d const &p = points[i];
        vertices.emplace_back((float)p.x, (float)p.y);
    }

    GLUtesselator *tess = gluNewTess();

    gluTessCallback(tess, GLU_TESS_BEGIN_DATA, (GLvoid(CALLBACK *)())begin_callback);
    gluTessCallback(tess, GLU_TESS_VERTEX_DATA, (GLvoid(CALLBACK *)())vertex_callback);
    gluTessCallback(tess, GLU_TESS_COMBINE_DATA, (GLvoid(CALLBACK *)())combine_callback);
    gluTessCallback(tess, GLU_TESS_END_DATA, (GLvoid(CALLBACK *)())end_callback);
    gluTessCallback(tess, GLU_TESS_ERROR_DATA, (GLvoid(CALLBACK *)())error_callback);
    gluTessCallback(tess, GLU_TESS_EDGE_FLAG_DATA, (GLvoid(CALLBACK *)())edge_flag_callback);

    gluTessBeginPolygon(tess, this);
    gluTessBeginContour(tess);

    for(size_t i = 0; i < num_points; ++i) {
        vert const &v = vertices[i + vertex_offset];
        GLdouble coords[3] = { v.x, v.y, 0 };
        gluTessVertex(tess, coords, (GLvoid *)(&v));
    }
    gluTessEndContour(tess);
    gluTessEndPolygon(tess);
    gluDeleteTess(tess);

    // duplicate first point for GL_LINE_STRIP, the filled one doesn't use this
    vec2d const &p = points[0];
    vertices.emplace_back((float)p.x, (float)p.y);

    // setup outline drawer
    tesselator_draw_call &draw_call = draw_calls.back();
    draw_call.line_count = (int)(vertices.size() - vertex_offset);
    draw_call.line_offset = vertex_offset;
}

//////////////////////////////////////////////////////////////////////

void tesselator::finalize()
{
    if(!draw_calls.empty() && !vertices.empty() && !indices.empty()) {

        vert *v = (vert *)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
        GLuint *i = (GLuint *)glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
        memcpy(v, vertices.data(), vertices.size() * sizeof(vert));
        memcpy(i, indices.data(), indices.size() * sizeof(GLuint));
        glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
        glUnmapBuffer(GL_ARRAY_BUFFER);
    }
}
