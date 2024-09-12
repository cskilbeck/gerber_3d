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

    struct gl_vertex_textured
    {
        float x, y;
        float u, v;
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

        char const *program_name;    // just for debugging reference

        char const *vertex_shader_source;
        char const *fragment_shader_source;

        int get_uniform(char const *name);
        int get_attribute(char const *name);

        int compile_shader(GLuint shader_id, char const *source) const;
        int validate(GLuint param) const;
        void use() const;
        virtual int init();
        void cleanup();
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_render_target
    {
        GLuint fbo{};
        GLuint texture_id{};

        int width{};
        int height{};
        int num_samples{};

        gl_render_target() = default;

        int init(GLuint new_width, GLuint new_height, GLuint multisample_count);
        void activate() const;
        void bind();
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
    // textured, no color

    struct gl_textured_program : gl_program
    {
        GLuint sampler_location;
        GLuint color_r_location;
        GLuint color_g_location;
        GLuint color_b_location;
        GLuint num_samples_uniform{};
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

        int alloc(GLsizei vert_count, size_t vertex_size);

        virtual int init(gl_program &program, GLsizei vert_count) = 0;
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

    struct gl_vertex_array_textured : gl_vertex_array
    {
        gl_vertex_array_textured() = default;

        int position_location{ 0 };
        int tex_coord_location{ 0 };

        int init(gl_program &program, GLsizei vert_count) override;
        int activate() const override;
    };

    //////////////////////////////////////////////////////////////////////

    struct gl_texture
    {
        GLuint texture_id{};
        GLuint width{};
        GLuint height{};

        gl_texture() = default;

        int init(GLuint w, GLuint h, uint32_t *data = nullptr);
        void cleanup();
        int update(uint32_t *data);
        int activate(GLuint slot) const;
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

        int init(gl_color_program &program)
        {
            int err = vertex_array.init(program, max_verts);
            if(err != 0) {
                return err;
            }
            reset();
            return 0;
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
#define GL_CHECK(x)                                                                                         \
    do {                                                                                                    \
        x;                                                                                                  \
        GLenum __err = glGetError();                                                                        \
        if(__err != 0) {                                                                                    \
            char const *__err_text = (char const *)gluErrorString(__err);                                   \
            LOG_ERROR("ERROR {} ({}) from {} at line {} of {}", __err, __err_text, #x, __LINE__, __FILE__); \
        }                                                                                                   \
    } while(0)
#else
#define GL_CHECK(x) \
    do {            \
        x;          \
    } while(0)
#endif
