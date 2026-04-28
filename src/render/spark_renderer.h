#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <memory>
#include "packed_splats.h"
#include "shader/shader_manager.h"
#include "sort/radix_sort.h"

namespace spark {

struct Camera {
    glm::mat4 projection{1.0f};
    glm::mat4 view{1.0f};
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    float fov = 60.0f;
    float near_plane = 0.1f;
    float far_plane = 1000.0f;
    float aspect = 1.0f;

    void update_projection();
    void update_view();
    glm::vec3 forward() const;
    glm::vec3 right() const;
    glm::vec3 up() const;
};

struct SparkRendererConfig {
    float max_std_dev = 2.83f; // sqrt(8)
    float min_pixel_radius = 0.0f;
    float max_pixel_radius = 512.0f;
    float min_alpha = 0.5f / 255.0f;
    float clip_xy = 1.4f;
    float focal_adjustment = 1.0f;
    float blur_amount = 0.0f;
    float pre_blur_amount = 0.0f;
    float falloff = 1.0f;
    bool premultiplied_alpha = true;
    bool encode_linear = false;
    bool lod_inflate = false;
};

class SparkRenderer {
public:
    SparkRenderer();
    ~SparkRenderer();

    bool init(const std::string& shader_dir);

    // Add a splat mesh to render
    void set_splats(PackedSplats* splats);

    // Render all splats
    void render(const Camera& camera, int viewport_width, int viewport_height);

    // Sort splats by depth from camera
    void sort(const Camera& camera);

    SparkRendererConfig config;

private:
    void create_quad_geometry();
    void update_ordering_texture();
    void compute_readback(const Camera& camera);

    ShaderManager shader_manager_;
    GLuint splat_program_ = 0;

    // Quad geometry (2 triangles)
    GLuint quad_vao_ = 0;
    GLuint quad_vbo_ = 0;
    GLuint quad_ebo_ = 0;

    // Ordering texture (sorted indices)
    GLuint ordering_texture_ = 0;
    int ordering_tex_width_ = 0;
    int ordering_tex_height_ = 0;

    PackedSplats* current_splats_ = nullptr;
    RadixSort16 sorter16_;
    uint32_t active_splats_ = 0;
};

} // namespace spark
