#include "spark_renderer.h"
#include "core/half_float.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <iostream>
#include <cmath>

namespace spark {

// --- Camera ---

void Camera::update_projection() {
    projection = glm::perspective(glm::radians(fov), aspect, near_plane, far_plane);
}

void Camera::update_view() {
    glm::mat4 rot = glm::mat4_cast(glm::conjugate(rotation));
    glm::mat4 trans = glm::translate(glm::mat4(1.0f), -position);
    view = rot * trans;
}

glm::vec3 Camera::forward() const {
    return rotation * glm::vec3(0.0f, 0.0f, -1.0f);
}

glm::vec3 Camera::right() const {
    return rotation * glm::vec3(1.0f, 0.0f, 0.0f);
}

glm::vec3 Camera::up() const {
    return rotation * glm::vec3(0.0f, 1.0f, 0.0f);
}

// --- SparkRenderer ---

SparkRenderer::SparkRenderer() = default;

SparkRenderer::~SparkRenderer() {
    if (quad_vao_) glDeleteVertexArrays(1, &quad_vao_);
    if (quad_vbo_) glDeleteBuffers(1, &quad_vbo_);
    if (quad_ebo_) glDeleteBuffers(1, &quad_ebo_);
    if (ordering_texture_) glDeleteTextures(1, &ordering_texture_);
}

bool SparkRenderer::init(const std::string& shader_dir) {
    // Load and register shader includes
    std::string defines_src = shader_manager_.load_file(shader_dir + "/splat_defines.glsl");
    if (defines_src.empty()) {
        std::cerr << "SparkRenderer: Failed to load splat_defines.glsl" << std::endl;
        return false;
    }
    shader_manager_.register_include("splat_defines", defines_src);

    // Load shaders
    std::string vert_src = shader_manager_.load_file(shader_dir + "/splat_vertex.glsl");
    std::string frag_src = shader_manager_.load_file(shader_dir + "/splat_fragment.glsl");
    if (vert_src.empty() || frag_src.empty()) {
        std::cerr << "SparkRenderer: Failed to load shaders" << std::endl;
        return false;
    }

    // Inject defines into vertex shader (after #version line)
    auto inject_after_version = [&](std::string& src) {
        auto pos = src.find('\n');
        if (pos != std::string::npos) {
            src.insert(pos + 1, "\n" + defines_src + "\n");
        }
    };
    inject_after_version(vert_src);

    splat_program_ = shader_manager_.create_program(vert_src, frag_src, "splat");
    if (!splat_program_) {
        std::cerr << "SparkRenderer: Failed to create splat program" << std::endl;
        return false;
    }

    create_quad_geometry();
    return true;
}

void SparkRenderer::create_quad_geometry() {
    // 4 vertices for a quad centered at origin, scaled by position.xy = ±1
    float vertices[] = {
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f,
    };

    uint32_t indices[] = {
        0, 1, 2,
        0, 2, 3,
    };

    glGenVertexArrays(1, &quad_vao_);
    glGenBuffers(1, &quad_vbo_);
    glGenBuffers(1, &quad_ebo_);

    glBindVertexArray(quad_vao_);

    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad_ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void SparkRenderer::set_splats(PackedSplats* splats) {
    current_splats_ = splats;
}

void SparkRenderer::compute_readback(const Camera& camera) {
    if (!current_splats_ || current_splats_->num_splats() == 0) return;

    size_t n = current_splats_->num_splats();
    sorter16_.ensure_size(n);

    const uint32_t* packed = current_splats_->packed_array().data();
    glm::mat4 mv = camera.view;

    for (size_t i = 0; i < n; i++) {
        glm::vec3 center = decode_packed_center(packed + i * 4);
        glm::vec4 view_pos = mv * glm::vec4(center, 1.0f);

        if (view_pos.z >= 0.0f) {
            sorter16_.readback[i] = 0x7C00; // infinity (culled)
        } else {
            // Convert to float16 depth
            float depth = -view_pos.z;
            sorter16_.readback[i] = float_to_half(depth);
        }
    }
}

void SparkRenderer::sort(const Camera& camera) {
    if (!current_splats_ || current_splats_->num_splats() == 0) return;
    compute_readback(camera);
    active_splats_ = sorter16_.sort(current_splats_->num_splats());
}

void SparkRenderer::update_ordering_texture() {
    if (active_splats_ == 0) return;

    // Pack ordering into RGBA32UI texture (4 indices per texel)
    int num_texels = (active_splats_ + 3) / 4;
    // Use 4096-wide texture
    int tex_w = std::min(num_texels, 4096);
    int tex_h = (num_texels + tex_w - 1) / tex_w;

    size_t total = static_cast<size_t>(tex_w) * tex_h * 4;
    std::vector<uint32_t> ordering_data(total, 0xFFFFFFFF);
    std::memcpy(ordering_data.data(), sorter16_.ordering.data(),
                active_splats_ * sizeof(uint32_t));

    if (!ordering_texture_) {
        glGenTextures(1, &ordering_texture_);
    }

    glBindTexture(GL_TEXTURE_2D, ordering_texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI,
                 tex_w, tex_h, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT,
                 ordering_data.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    ordering_tex_width_ = tex_w;
    ordering_tex_height_ = tex_h;
}

void SparkRenderer::render(const Camera& camera, int viewport_width, int viewport_height) {
    if (!splat_program_ || !current_splats_ || active_splats_ == 0) return;

    // Ensure GPU data is uploaded
    current_splats_->upload_to_gpu();
    update_ordering_texture();

    // Setup blending
    glEnable(GL_BLEND);
    if (config.premultiplied_alpha) {
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glUseProgram(splat_program_);

    // Camera uniforms
    glm::quat view_quat = glm::conjugate(camera.rotation);
    glm::vec3 view_pos = view_quat * (-camera.position);

    auto set_uniform = [&](const char* name, auto&&... args) {
        GLint loc = shader_manager_.get_uniform(splat_program_, name);
        if (loc < 0) return;
        using T = std::tuple<std::decay_t<decltype(args)>...>;
        if constexpr (sizeof...(args) == 1) {
            auto val = std::get<0>(std::tuple(args...));
            using V = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<V, float>)
                glUniform1f(loc, val);
            else if constexpr (std::is_same_v<V, int>)
                glUniform1i(loc, val);
            else if constexpr (std::is_same_v<V, bool>)
                glUniform1i(loc, val ? 1 : 0);
        }
    };

    // vec/mat uniforms
    GLint loc;

    loc = shader_manager_.get_uniform(splat_program_, "renderSize");
    if (loc >= 0) glUniform2f(loc, static_cast<float>(viewport_width), static_cast<float>(viewport_height));

    loc = shader_manager_.get_uniform(splat_program_, "renderToViewQuat");
    if (loc >= 0) glUniform4f(loc, view_quat.x, view_quat.y, view_quat.z, view_quat.w);

    loc = shader_manager_.get_uniform(splat_program_, "renderToViewPos");
    if (loc >= 0) glUniform3f(loc, view_pos.x, view_pos.y, view_pos.z);

    loc = shader_manager_.get_uniform(splat_program_, "projectionMatrix");
    if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(camera.projection));

    loc = shader_manager_.get_uniform(splat_program_, "maxStdDev");
    if (loc >= 0) glUniform1f(loc, config.max_std_dev);

    loc = shader_manager_.get_uniform(splat_program_, "minPixelRadius");
    if (loc >= 0) glUniform1f(loc, config.min_pixel_radius);

    loc = shader_manager_.get_uniform(splat_program_, "maxPixelRadius");
    if (loc >= 0) glUniform1f(loc, config.max_pixel_radius);

    loc = shader_manager_.get_uniform(splat_program_, "minAlpha");
    if (loc >= 0) glUniform1f(loc, config.min_alpha);

    loc = shader_manager_.get_uniform(splat_program_, "clipXY");
    if (loc >= 0) glUniform1f(loc, config.clip_xy);

    loc = shader_manager_.get_uniform(splat_program_, "focalAdjustment");
    if (loc >= 0) glUniform1f(loc, config.focal_adjustment);

    loc = shader_manager_.get_uniform(splat_program_, "blurAmount");
    if (loc >= 0) glUniform1f(loc, config.blur_amount);

    loc = shader_manager_.get_uniform(splat_program_, "preBlurAmount");
    if (loc >= 0) glUniform1f(loc, config.pre_blur_amount);

    loc = shader_manager_.get_uniform(splat_program_, "falloff");
    if (loc >= 0) glUniform1f(loc, config.falloff);

    loc = shader_manager_.get_uniform(splat_program_, "lodInflate");
    if (loc >= 0) glUniform1i(loc, config.lod_inflate ? 1 : 0);

    loc = shader_manager_.get_uniform(splat_program_, "encodeLinear");
    if (loc >= 0) glUniform1i(loc, config.encode_linear ? 1 : 0);

    loc = shader_manager_.get_uniform(splat_program_, "premultipliedAlpha");
    if (loc >= 0) glUniform1i(loc, config.premultiplied_alpha ? 1 : 0);

    loc = shader_manager_.get_uniform(splat_program_, "time");
    if (loc >= 0) glUniform1f(loc, 0.0f);

    // Bind textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ordering_texture_);
    loc = shader_manager_.get_uniform(splat_program_, "ordering");
    if (loc >= 0) glUniform1i(loc, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D_ARRAY, current_splats_->texture());
    loc = shader_manager_.get_uniform(splat_program_, "splatTexture");
    if (loc >= 0) glUniform1i(loc, 1);

    // Draw instanced quads
    glBindVertexArray(quad_vao_);
    glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr, active_splats_);
    glBindVertexArray(0);

    // Cleanup state
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

} // namespace spark
