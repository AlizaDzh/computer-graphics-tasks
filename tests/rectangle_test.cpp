#include "asr.h"

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

static std::pair<std::vector<asr::Vertex>, std::vector<unsigned int>> generate_rectangle_geometry_data(
    float width, float height,
    unsigned int width_segments_count,
    unsigned int height_segments_count)
{
    std::vector<asr::Vertex> vertices;
    std::vector<unsigned int> indices;

    float half_height{height * 0.5f};
    float segment_height{height / static_cast<float>(height_segments_count)};

    float half_width{width * 0.5f};
    float segment_width{width / static_cast<float>(width_segments_count)};

    for (unsigned int i = 0; i <= height_segments_count; ++i)
    {
        float y{static_cast<float>(i) * segment_height - half_height};
        float v{1.0f - static_cast<float>(i) / static_cast<float>(height_segments_count)};
        for (unsigned int j = 0; j <= width_segments_count; ++j)
        {
            float x{static_cast<float>(j) * segment_width - half_width};
            float u{static_cast<float>(j) / static_cast<float>(width_segments_count)};
            vertices.push_back(asr::Vertex{
                x, y, 0.0f,
                0.0f, 0.0f, 1.0f,
                1.0f, 1.0f, 1.0f, 1.0f,
                u, v});
        }
    }

    for (unsigned int i = 0; i < height_segments_count; ++i)
    {
        for (unsigned int j = 0; j < width_segments_count; ++j)
        {
            unsigned int index_a{i * (width_segments_count + 1) + j};
            unsigned int index_b{index_a + 1};
            unsigned int index_c{index_a + (width_segments_count + 1)};
            unsigned int index_d{index_c + 1};

            indices.push_back(index_a);
            indices.push_back(index_b);
            indices.push_back(index_c);

            indices.push_back(index_b);
            indices.push_back(index_d);
            indices.push_back(index_c);
        }
    }

    return std::make_pair(vertices, indices);
}

static std::pair<std::vector<asr::Vertex>, std::vector<unsigned int>> generate_rectangle_edges_data(
    float width,
    float height,
    unsigned int width_segments_count,
    unsigned int height_segments_count)
{
    std::vector<asr::Vertex> vertices;
    std::vector<unsigned int> indices;

    float half_height{height * 0.5f};
    float segment_height{height / static_cast<float>(height_segments_count)};

    float half_width{width * 0.5f};
    float segment_width{width / static_cast<float>(width_segments_count)};

    for (unsigned int i = 0; i <= height_segments_count; ++i)
    {
        float y{static_cast<float>(i) * segment_height - half_height};
        float v{1.0f - static_cast<float>(i) / static_cast<float>(height_segments_count)};
        for (unsigned int j = 0; j <= width_segments_count; ++j)
        {
            float x{static_cast<float>(j) * segment_width - half_width};
            float u{static_cast<float>(j) / static_cast<float>(width_segments_count)};
            vertices.push_back(asr::Vertex{
                x, y, 0.0f,
                0.0f, 0.0f, 1.0f,
                1.0f, 1.0f, 1.0f, 1.0f,
                u, v});
        }
    }

    for (unsigned int i = 0; i < height_segments_count; ++i)
    {
        for (unsigned int j = 0; j < width_segments_count; ++j)
        {
            unsigned int index_a{i * (width_segments_count + 1) + j};
            unsigned int index_b{index_a + 1};
            unsigned int index_c{index_a + (width_segments_count + 1)};
            unsigned int index_d{index_c + 1};

            indices.push_back(index_a);
            indices.push_back(index_b);
            indices.push_back(index_b);
            indices.push_back(index_c);
            indices.push_back(index_c);
            indices.push_back(index_a);

            indices.push_back(index_b);
            indices.push_back(index_d);
            indices.push_back(index_d);
            indices.push_back(index_c);
            indices.push_back(index_c);
            indices.push_back(index_b);
        }
    }

    return std::make_pair(vertices, indices);
}

static std::pair<std::vector<asr::Vertex>, std::vector<unsigned int>> generate_rectangle_vertices_data(
    float width,
    float height,
    unsigned int width_segments_count,
    unsigned int height_segments_count)
{
    std::vector<asr::Vertex> vertices;
    std::vector<unsigned int> indices;

    float half_height{height * 0.5f};
    float segment_height{height / static_cast<float>(height_segments_count)};

    float half_width{width * 0.5f};
    float segment_width{width / static_cast<float>(width_segments_count)};

    for (unsigned int i = 0; i <= height_segments_count; ++i)
    {
        float y{static_cast<float>(i) * segment_height - half_height};
        float v{1.0f - static_cast<float>(i) / static_cast<float>(height_segments_count)};
        for (unsigned int j = 0; j <= width_segments_count; ++j)
        {
            float x{static_cast<float>(j) * segment_width - half_width};
            float u{static_cast<float>(j) / static_cast<float>(width_segments_count)};
            vertices.push_back(asr::Vertex{
                x, y, 0.0f,
                0.0f, 0.0f, 1.0f,
                1.0f, 1.0f, 1.0f, 1.0f,
                u, v});
            indices.push_back(vertices.size() - 1);
        }
    }

    return std::make_pair(vertices, indices);
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char **argv)
{
    using namespace asr;

    create_window(500, 500, "Rectangle Test on ASR Version 4.0");

    auto material = create_material(Vertex_Shader_Source, Fragment_Shader_Source);

    auto [geometry_vertices, geometry_indices] = generate_rectangle_geometry_data(1.0f, 1.0f, 5, 5);
    auto geometry = create_geometry(Triangles, geometry_vertices, geometry_indices);

    auto [edge_vertices, edge_indices] = generate_rectangle_edges_data(1.0f, 1.0f, 5, 5);
    for (auto &vertex : edge_vertices)
    {
        vertex.z -= 0.01f;
    }
    auto edges_geometry = create_geometry(Lines, edge_vertices, edge_indices);

    auto [vertices, vertex_indices] = generate_rectangle_vertices_data(1.0f, 1.0f, 5, 5);
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