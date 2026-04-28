#pragma once

#include <glad/glad.h>
#include <vector>
#include <cstdint>
#include <glm/glm.hpp>

namespace spark {

// Readback: GPU to CPU data transfer pipeline using PBOs
class Readback {
public:
    Readback() = default;
    ~Readback();

    Readback(const Readback&) = delete;
    Readback& operator=(const Readback&) = delete;

    // Initialize PBO ring buffer for async readback
    void init(int width, int height, GLenum format = GL_RGBA, GLenum type = GL_UNSIGNED_BYTE);
    void resize(int width, int height);

    // Request readback from current framebuffer
    void request(int x = 0, int y = 0, int width = -1, int height = -1);

    // Check if data is available
    bool available() const;

    // Get the readback data (returns empty if not available)
    const std::vector<uint8_t>& data() const { return data_; }

    // Read a pixel (after data is available)
    glm::vec4 read_pixel(int x, int y) const;

    // Read depth value at a pixel
    float read_depth(int x, int y) const;

    // Request depth readback
    void request_depth(int x = 0, int y = 0, int width = -1, int height = -1);

private:
    static constexpr int NUM_PBOS = 3;

    GLuint pbos_[NUM_PBOS] = {};
    int current_pbo_ = 0;
    int read_pbo_ = -1;

    int width_ = 0;
    int height_ = 0;
    int req_x_ = 0, req_y_ = 0;
    int req_w_ = 0, req_h_ = 0;
    GLenum format_ = GL_RGBA;
    GLenum type_ = GL_UNSIGNED_BYTE;

    std::vector<uint8_t> data_;
    std::vector<float> depth_data_;
    bool initialized_ = false;
    bool pending_ = false;
    bool depth_mode_ = false;
    int frame_count_ = 0;
};

} // namespace spark
