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

}    // namespace gerber_3d
