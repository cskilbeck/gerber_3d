#pragma once

namespace gerber_3d
{
    //////////////////////////////////////////////////////////////////////

    struct gl_vertex_solid
    {
        float x, y;
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_vertex_color
    {
        float x, y;
        uint32_t color;
    };

    //////////////////////////////////////////////////////////////////////
    // base gl_program - transform matrix is common to all

    struct gl_program
    {
        LOG_CONTEXT("gl_program", debug);

        GLuint program_id{};
        GLuint vertex_shader_id{};
        GLuint fragment_shader_id{};

        GLuint transform_location{};

        gl_program() = default;

        char const *vertex_shader_source;
        char const *fragment_shader_source;

        int check_shader(GLuint shader_id) const;
        int validate(GLuint param) const;
        void use() const;
        virtual int init();
        void cleanup();
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_render_target
    {
        GLuint fbo{};
        GLuint texture{};
        int width{};
        int height{};

        gl_render_target() = default;

        int init(GLuint new_width, GLuint new_height);
        void activate();
        void cleanup();
    };

    //////////////////////////////////////////////////////////////////////
    // uniform color

    struct gl_solid_program : gl_program
    {
        GLuint color_location{};

        int init() override;

        void set_color(uint32_t color) const;
    };

    //////////////////////////////////////////////////////////////////////
    // color per vertex

    struct gl_color_program : gl_program
    {
        int init() override;
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_index_array
    {
        GLuint ibo_id{ 0 };
        int num_indices{ 0 };

        gl_index_array() = default;

        int init(GLsizei index_count);
        int activate() const;

        void cleanup();
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_vertex_array
    {
        GLuint vbo_id{ 0 };
        int num_verts{ 0 };

        gl_vertex_array() = default;

        virtual int init(gl_program &program, GLsizei vert_count);
        virtual int activate() const;

        void cleanup();
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_vertex_array_solid : gl_vertex_array
    {
        gl_vertex_array_solid() = default;

        int position_location{ 0 };

        int init(gl_program &program, GLsizei vert_count) override;
        int activate() const override;
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_vertex_array_color : gl_vertex_array
    {
        gl_vertex_array_color() = default;

        int position_location{ 0 };
        int color_location{ 0 };

        int init(gl_program &program, GLsizei vert_count) override;
        int activate() const override;
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_drawlist
    {
        using rect = gerber_lib::gerber_2d::rect;
        using vec2d = gerber_lib::gerber_2d::vec2d;

        struct gl_drawlist_entry
        {
            GLenum draw_type;
            GLuint offset;
            GLuint count;
        };

        static int constexpr max_verts = 8192;

        gl_vertex_array_color vertex_array;

        std::vector<gl_vertex_color> verts;
        std::vector<gl_drawlist_entry> drawlist;

        void init(gl_color_program &program)
        {
            vertex_array.init(program, max_verts);
            reset();
        }

        void reset()
        {
            verts.clear();
            verts.reserve(max_verts);
            drawlist.clear();
        }

        void add_vertex(vec2d const &pos, uint32_t color)
        {
            if(drawlist.empty()) {
                return;
            }
            if(verts.size() >= max_verts) {
                return;
            }
            verts.emplace_back((float)pos.x, (float)pos.y, color);
            drawlist.back().count += 1;
        }

        void add_drawlist_entry(GLenum type)
        {
            drawlist.emplace_back(type, (GLuint)verts.size(), 0);
        }

        void lines()
        {
            add_drawlist_entry(GL_LINES);
        }

        void add_line(vec2d const &start, vec2d const &end, uint32_t color)
        {
            add_vertex(start, color);
            add_vertex(end, color);
        }

        void add_outline_rect(rect const &r, uint32_t color)
        {
            add_drawlist_entry(GL_LINE_STRIP);
            add_vertex(r.min_pos, color);
            add_vertex({ r.max_pos.x, r.min_pos.y }, color);
            add_vertex(r.max_pos, color);
            add_vertex({ r.min_pos.x, r.max_pos.y }, color);
            add_vertex(r.min_pos, color);
        }

        void add_rect(rect const &r, uint32_t color)
        {
            add_drawlist_entry(GL_TRIANGLE_FAN);
            add_vertex(r.min_pos, color);
            add_vertex({ r.max_pos.x, r.min_pos.y }, color);
            add_vertex(r.max_pos, color);
            add_vertex({ r.min_pos.x, r.max_pos.y }, color);
        }

        void draw();
    };

}    // namespace gerber_3d

//////////////////////////////////////////////////////////////////////

#if defined(_DEBUG)
#define GL_CHECK(x)                                                       \
    do {                                                                  \
        x;                                                                \
        GLenum __err = glGetError();                                      \
        if(__err != 0) {                                                  \
            char const *__err_text = (char const *)gluErrorString(__err); \
            LOG_ERROR("ERROR {} ({}) from {}", __err, __err_text, #x);    \
        }                                                                 \
    } while(0)
#else
#define GL_CHECK(x) \
    do {            \
        x;          \
    } while(0)
#endif
