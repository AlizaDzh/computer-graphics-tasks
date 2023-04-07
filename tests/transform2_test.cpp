#include "asr.h"

#include <chrono>
#include <cmath>
#include <ctime>
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

static std::pair<std::vector<asr::Vertex>, std::vector<unsigned int>> generate_sphere_geometry_data(
    float radius,
    unsigned int width_segments_count,
    unsigned int height_segments_count)
{
    std::vector<asr::Vertex> vertices;
    std::vector<unsigned int> indices;

    for (unsigned int ring = 0; ring <= height_segments_count; ++ring)
    {
        float v{static_cast<float>(ring) / static_cast<float>(height_segments_count)};
        float phi{v * asr::pi};

        for (unsigned int segment = 0; segment <= width_segments_count; ++segment)
        {
            float u{static_cast<float>(segment) / static_cast<float>(width_segments_count)};
            float theta{u * asr::two_pi};

            float cos_phi{std::cosf(phi)};
            float sin_phi{std::sinf(phi)};
            float cos_theta{std::cosf(theta)};
            float sin_theta{std::sinf(theta)};

            float y{cos_phi};
            float x{sin_phi * cos_theta};
            float z{sin_phi * sin_theta};

            vertices.push_back(asr::Vertex{
                x * radius, y * radius, z * radius,
                x, y, z,
                1.0f, 1.0f, 1.0f, 1.0f,
                1.0f - u, v});
        }
    }

    for (unsigned int ring = 0; ring < height_segments_count; ++ring)
    {
        for (unsigned int segment = 0; segment < width_segments_count; ++segment)
        {
            unsigned int index_a{ring * (width_segments_count + 1) + segment};
            unsigned int index_b{index_a + 1};

            unsigned int index_c{index_a + (width_segments_count + 1)};
            unsigned int index_d{index_c + 1};

            if (ring != 0)
            {
                indices.push_back(index_a);
                indices.push_back(index_b);
                indices.push_back(index_c);
            }

            if (ring != height_segments_count - 1)
            {
                indices.push_back(index_b);
                indices.push_back(index_d);
                indices.push_back(index_c);
            }
        }
    }

    return std::make_pair(vertices, indices);
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char **argv)
{
    using namespace asr;

    create_window("Transformation Test on ASR Version 4.0");

    auto material = create_material(Vertex_Shader_Source, Fragment_Shader_Source);

    auto [red_rect_geometry_vertices, red_rect_geometry_indices] = generate_rectangle_geometry_data(1.0f, 1.0f, 1, 1);
    for (Vertex &vertex : red_rect_geometry_vertices)
    {
        vertex.g = 0.0f;
        vertex.b = 0.0f;
    }
    auto red_rect_geometry = create_geometry(Triangles, red_rect_geometry_vertices, red_rect_geometry_indices);

    auto [pink_rect_geometry_vertices, pink_rect_geometry_indices] = generate_rectangle_geometry_data(1.0f, 1.0f, 1, 1);
    for (Vertex &vertex : pink_rect_geometry_vertices)
    {
        vertex.g = 0.5f;
        vertex.b = 0.5f;
    }
    auto pink_rect_geometry = create_geometry(Triangles, pink_rect_geometry_vertices, pink_rect_geometry_indices);

    auto [circle_geometry_vertices, circle_geometry_indices] = generate_circle_geometry_data(1.0f, 10);
    auto circle_geometry = create_geometry(Triangles, circle_geometry_vertices, circle_geometry_indices);

    auto [sphere_geometry_vertices, sphere_geometry_indices] = generate_sphere_geometry_data(1.0f, 10, 10);
    for (Vertex &vertex : sphere_geometry_vertices)
    {
        vertex.g = 0.3f;
        vertex.b = 0.3f;
    }
    auto sphere_geometry = create_geometry(Triangles, sphere_geometry_vertices, sphere_geometry_indices);

    prepare_for_rendering();

    set_material_current(&material);
    set_material_depth_test_enabled(true);

    static const float CAMERA_SPEED{6.0f};
    static const float CAMERA_ROT_SPEED{1.5f};
    static const float CAMERA_FOV{1.13f};
    static const float CAMERA_NEAR_PLANE{0.1f};
    static const float CAMERA_FAR_PLANE{100.0f};

    glm::vec3 camera_position{0.0f, 0.0f, 2.5f};
    glm::vec3 camera_rotation{0.0f, 0.0f, 0.0f};

    set_matrix_mode(Projection);
    load_perspective_projection_matrix(CAMERA_FOV, CAMERA_NEAR_PLANE, CAMERA_FAR_PLANE);

    float clock_rotation{0.0f};
    float clock_delta_angle{-0.5f};

    const float second_marks_size{0.015f};
    const float second_marks_radius{1.0f};

    const float hour_marks_size{0.04f};
    const float hour_marks_radius{1.007f};
    const float hour_marks_z_shift{0.1f};

    const float quarter_marks_size{0.1f};
    const float quarter_marks_radius{1.007f};
    const float quarter_marks_z_shift{0.05f};

    const glm::vec3 hour_hand_scale{0.62f, 0.03f, 1.0f};
    const glm::vec3 minute_hand_scale{0.72f, 0.02f, 1.0f};
    const glm::vec3 seconds_hand_scale{0.82f, 0.01f, 1.0f};

    const float hands_axis_size{0.04f};

    bool should_stop{false};
    while (!should_stop)
    {
        process_window_events(&should_stop);

        prepare_to_render_frame();

        set_matrix_mode(View);
        load_identity_matrix();
        translate_matrix(camera_position);
        rotate_matrix(camera_rotation);

        // Seconds Marks

        set_matrix_mode(Model);
        load_identity_matrix();
        rotate_matrix(glm::vec3{0.0f, clock_rotation, 0.0f});
        clock_rotation += clock_delta_angle * get_dt();

        for (int i = 0; i < 60; ++i)
        {
            float angle{static_cast<float>(i) / 60.0f * 2.0f * pi};
            float x{cosf(angle) * second_marks_radius};
            float y{sinf(angle) * second_marks_radius};

            push_matrix();
            translate_matrix(glm::vec3{x, y, 0.0f});
            scale_matrix(glm::vec3{second_marks_size});

            set_geometry_current(&circle_geometry);
            render_current_geometry();
            pop_matrix();
        }

        // Hour Marks

        for (int i = 0; i < 12; ++i)
        {
            float angle{static_cast<float>(i) / 12.0f * two_pi};
            float x{cosf(angle) * hour_marks_radius};
            float y{sinf(angle) * hour_marks_radius};

            push_matrix();
            translate_matrix(glm::vec3{x, y, hour_marks_z_shift});
            rotate_matrix(glm::vec3{0.0f, 0.0f, quarter_pi});
            scale_matrix(glm::vec3{hour_marks_size});

            set_geometry_current(&pink_rect_geometry);
            render_current_geometry();
            pop_matrix();
        }

        // Quarter Marks

        for (int i = 0; i < 4; ++i)
        {
            float angle{static_cast<float>(i) / 4.0f * two_pi};
            float x{cosf(angle) * quarter_marks_radius};
            float y{sinf(angle) * quarter_marks_radius};

            push_matrix();
            translate_matrix(glm::vec3{x, y, quarter_marks_z_shift});
            rotate_matrix(glm::vec3{0.0f, 0.0f, quarter_pi});
            scale_matrix(glm::vec3{quarter_marks_size});

            set_geometry_current(&red_rect_geometry);
            render_current_geometry();
            pop_matrix();
        }

        // Hands Axis

        push_matrix();
        scale_matrix(glm::vec3{hands_axis_size});

        set_geometry_current(&sphere_geometry);
        render_current_geometry();
        pop_matrix();

        // Hands

        time_t current_time{std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())};
        struct tm *local_time{std::localtime(&current_time)};
        float hours{static_cast<float>(local_time->tm_hour)};
        float minutes{static_cast<float>(local_time->tm_min)};
        float seconds{static_cast<float>(local_time->tm_sec)};

        // Hour Hand

        push_matrix();
        rotate_matrix(glm::vec3{0.0f, 0.0f, -hours / 12.0f * two_pi + half_pi});
        scale_matrix(hour_hand_scale);
        translate_matrix(glm::vec3{0.5f, 0.0f, 0.0f});

        set_geometry_current(&red_rect_geometry);
        render_current_geometry();
        pop_matrix();

        // Minute Hand

        push_matrix();
        rotate_matrix(glm::vec3{0.0f, 0.0f, -minutes / 60.0f * two_pi + half_pi});
        scale_matrix(minute_hand_scale);
        translate_matrix(glm::vec3{0.5f, 0.0f, 0.0f});

        set_geometry_current(&red_rect_geometry);
        render_current_geometry();
        pop_matrix();

        // Seconds Hand

        push_matrix();
        rotate_matrix(glm::vec3{0.0f, 0.0f, -seconds / 60.0f * two_pi + half_pi});
        scale_matrix(seconds_hand_scale);
        translate_matrix(glm::vec3{0.5f, 0.0f, 0.0f});

        set_geometry_current(&red_rect_geometry);
        render_current_geometry();
        pop_matrix();

        finish_frame_rendering();
    }

    destroy_geometry(sphere_geometry);
    destroy_geometry(circle_geometry);
    destroy_geometry(pink_rect_geometry);
    destroy_geometry(red_rect_geometry);

    destroy_material(material);

    destroy_window();

    return 0;
}
