#include "asr.h"

#include <cmath>
#include <string>
#include <utility>
#include <vector>

static const std::string Vertex_Shader_Source = R"(
    #version 110
    attribute vec4 position;
    attribute vec4 color;
    attribute vec4 texture_coordinates;

    uniform bool texture_enabled;
    uniform mat4 texture_transformation_matrix;

    uniform float point_size;

    uniform mat4 model_view_projection_matrix;
    varying vec4 fragment_color;
    varying vec2 fragment_texture_coordinates;
    void main()
    {
        fragment_color = color;
        if (texture_enabled) {
            vec4 transformed_texture_coordinates = texture_transformation_matrix * vec4(texture_coordinates.st, 0.0, 1.0);
            fragment_texture_coordinates = vec2(transformed_texture_coordinates);
        }
        gl_Position = model_view_projection_matrix * position;
        gl_PointSize = point_size;
    }
)";

static const std::string Fragment_Shader_Source = R"(
    #version 110
    #define TEXTURING_MODE_ADDITION            0
    #define TEXTURING_MODE_SUBTRACTION         1
    #define TEXTURING_MODE_REVERSE_SUBTRACTION 2
    #define TEXTURING_MODE_MODULATION          3
    #define TEXTURING_MODE_DECALING            4

    uniform bool texture_enabled;
    uniform int texturing_mode;
    uniform sampler2D texture_sampler;

    varying vec4 fragment_color;
    varying vec2 fragment_texture_coordinates;

    void main()
    {
        gl_FragColor = fragment_color;

        if (texture_enabled) {
            if (texturing_mode == TEXTURING_MODE_ADDITION) {
                gl_FragColor += texture2D(texture_sampler, fragment_texture_coordinates);
            } else if (texturing_mode == TEXTURING_MODE_MODULATION) {
                gl_FragColor *= texture2D(texture_sampler, fragment_texture_coordinates);
            } else if (texturing_mode == TEXTURING_MODE_DECALING) {
                vec4 texel_color = texture2D(texture_sampler, fragment_texture_coordinates);
                gl_FragColor.rgb = mix(gl_FragColor.rgb, texel_color.rgb, texel_color.a);
            } else if (texturing_mode == TEXTURING_MODE_SUBTRACTION) {
                gl_FragColor -= texture2D(texture_sampler, fragment_texture_coordinates);
            } else if (texturing_mode == TEXTURING_MODE_REVERSE_SUBTRACTION) {
                gl_FragColor = texture2D(texture_sampler, fragment_texture_coordinates) - gl_FragColor;
            }
        }
    }
)";

static std::pair<std::vector<asr::Vertex>, std::vector<unsigned int>> generate_circle_geometry_data(
    float radius,
    unsigned int segment_count)
{
    std::vector<asr::Vertex> vertices;
    std::vector<unsigned int> indices;

    float angle{0.0f};
    float angle_delta{asr::two_pi / static_cast<float>(segment_count)};

    vertices.push_back(asr::Vertex{
        0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        0.5f, 0.5f});
    float x{std::cosf(angle) * radius};
    float y{std::sinf(angle) * radius};
    float u{0.5f + std::cosf(angle) * 0.5f};
    float v{1.0f - (0.5f + std::sinf(angle) * 0.5f)};
    vertices.push_back(asr::Vertex{
        x, y, 0.0f,
        0.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        u, v});

    for (unsigned int i = 0; i < segment_count; ++i)
    {
        indices.push_back(0);
        indices.push_back(vertices.size() - 1);

        float next_x{std::cosf(angle + angle_delta) * radius};
        float next_y{std::sinf(angle + angle_delta) * radius};
        float next_u{0.5f + std::cosf(angle + angle_delta) * 0.5f};
        float next_v{1.0f - (0.5f + std::sinf(angle + angle_delta) * 0.5f)};
        vertices.push_back(asr::Vertex{
            next_x, next_y, 0.0f,
            0.0f, 0.0f, 1.0f,
            1.0f, 1.0f, 1.0f, 1.0f,
            next_u, next_v});
        indices.push_back(vertices.size() - 1);

        angle += angle_delta;
    }

    return std::make_pair(vertices, indices);
}

static std::pair<std::vector<asr::Vertex>, std::vector<unsigned int>> generate_circle_edges_data(
    float radius,
    unsigned int segment_count)
{
    std::vector<asr::Vertex> vertices;
    std::vector<unsigned int> indices;

    float angle{0.0f};
    float angle_delta{asr::two_pi / static_cast<float>(segment_count)};

    vertices.push_back(asr::Vertex{
        0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        0.5f, 0.5f});
    float x{std::cosf(angle) * radius};
    float y{std::sinf(angle) * radius};
    float u{0.5f + std::cosf(angle) * 0.5f};
    float v{1.0f - (0.5f + std::sinf(angle) * 0.5f)};
    vertices.push_back(asr::Vertex{
        x, y, 0.0f,
        0.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        u, v});

    for (unsigned int i = 0; i < segment_count; ++i)
    {
        indices.push_back(0);
        indices.push_back(vertices.size() - 1);
        indices.push_back(vertices.size() - 1);

        float next_x{std::cosf(angle + angle_delta) * radius};
        float next_y{std::sinf(angle + angle_delta) * radius};
        float next_u{0.5f + std::cosf(angle + angle_delta) * 0.5f};
        float next_v{1.0f - (0.5f + std::sinf(angle + angle_delta) * 0.5f)};
        vertices.push_back(asr::Vertex{
            next_x, next_y, 0.0f,
            0.0f, 0.0f, 1.0f,
            1.0f, 1.0f, 1.0f, 1.0f,
            next_u, next_v});
        indices.push_back(vertices.size() - 1);

        angle += angle_delta;
    }

    return std::make_pair(vertices, indices);
}

static std::pair<std::vector<asr::Vertex>, std::vector<unsigned int>> generate_circle_vertices_data(
    float radius,
    unsigned int segment_count)
{
    std::vector<asr::Vertex> vertices;
    std::vector<unsigned int> indices;

    float angle{0.0f};
    float angle_delta{asr::two_pi / static_cast<float>(segment_count)};

    vertices.push_back(asr::Vertex{
        0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        0.5f, 0.5f});
    indices.push_back(0);
    float x{std::cosf(angle) * radius};
    float y{std::sinf(angle) * radius};
    float u{0.5f + std::cosf(angle) * 0.5f};
    float v{1.0f - (0.5f + std::sinf(angle) * 0.5f)};
    vertices.push_back(asr::Vertex{
        x, y, 0.0f,
        0.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        u, v});
    indices.push_back(1);

    for (unsigned int i = 0; i < segment_count; ++i)
    {
        float next_x{std::cosf(angle + angle_delta) * radius};
        float next_y{std::sinf(angle + angle_delta) * radius};
        float next_u{0.5f + std::cosf(angle + angle_delta) * 0.5f};
        float next_v{1.0f - (0.5f + std::sinf(angle + angle_delta) * 0.5f)};
        vertices.push_back(asr::Vertex{
            next_x, next_y, 0.0f,
            0.0f, 0.0f, 1.0f,
            1.0f, 1.0f, 1.0f, 1.0f,
            next_u, next_v});
        indices.push_back(vertices.size() - 1);

        angle += angle_delta;
    }

    return std::make_pair(vertices, indices);
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char **argv)
{
    using namespace asr;

    create_window(500, 500, "Circle Test on ASR Version 4.0");

    auto material = create_material(Vertex_Shader_Source, Fragment_Shader_Source);

    auto [geometry_vertices, geometry_indices] = generate_circle_geometry_data(0.5f, 10);
    auto geometry = create_geometry(Triangles, geometry_vertices, geometry_indices);

    auto [edge_vertices, edge_indices] = generate_circle_edges_data(0.5f, 10);
    for (auto &vertex : edge_vertices)
    {
        vertex.z -= 0.01f;
    }
    auto edges_geometry = create_geometry(Lines, edge_vertices, edge_indices);

    auto [vertices, vertex_indices] = generate_circle_vertices_data(0.5f, 10);
    for (auto &vertex : vertices)
    {
        vertex.z -= 0.02f;
        vertex.r = 1.0f;
        vertex.g = 0.0f;
        vertex.b = 0.0f;
    }
    auto vertices_geometry = create_geometry(Points, vertices, vertex_indices);

    auto image = read_image_file("data/images/uv_test.png");
    auto texture = create_texture(image);

    prepare_for_rendering();

    set_material_current(&material);
    set_material_line_width(3.0f);
    set_material_point_size(10.0f);

    bool should_stop{false};
    while (!should_stop)
    {
        process_window_events(&should_stop);

        prepare_to_render_frame();

        set_texture_current(&texture);
        set_geometry_current(&geometry);
        render_current_geometry();

        set_texture_current(nullptr);
        set_geometry_current(&edges_geometry);
        render_current_geometry();
        set_geometry_current(&vertices_geometry);
        render_current_geometry();

        finish_frame_rendering();
    }

    destroy_texture(texture);

    destroy_geometry(geometry);
    destroy_geometry(edges_geometry);
    destroy_geometry(vertices_geometry);

    destroy_material(material);

    destroy_window();

    return 0;
}