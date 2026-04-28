#include "readback.h"
#include <cstring>
#include <algorithm>

namespace spark {

Readback::~Readback() {
    if (initialized_) {
        glDeleteBuffers(NUM_PBOS, pbos_);
    }
}

void Readback::init(int width, int height, GLenum format, GLenum type) {
    width_ = width;
    height_ = height;
    format_ = format;
    type_ = type;

    glGenBuffers(NUM_PBOS, pbos_);

    int channels = (format == GL_RGBA) ? 4 : (format == GL_RGB) ? 3 : 1;
    int type_size = (type == GL_FLOAT) ? 4 : 1;
    size_t buffer_size = static_cast<size_t>(width) * height * channels * type_size;

    for (int i = 0; i < NUM_PBOS; i++) {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos_[i]);
        glBufferData(GL_PIXEL_PACK_BUFFER, buffer_size, nullptr, GL_STREAM_READ);
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    data_.resize(buffer_size);
    initialized_ = true;
}

void Readback::resize(int width, int height) {
    if (width == width_ && height == height_) return;
    if (initialized_) glDeleteBuffers(NUM_PBOS, pbos_);
    initialized_ = false;
    init(width, height, format_, type_);
}

void Readback::request(int x, int y, int width, int height) {
    if (!initialized_) return;

    req_x_ = x;
    req_y_ = y;
    req_w_ = (width < 0) ? width_ : width;
    req_h_ = (height < 0) ? height_ : height;
    depth_mode_ = false;

    // Read previous PBO data if available
    if (frame_count_ >= NUM_PBOS && read_pbo_ >= 0) {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos_[read_pbo_]);
        void* ptr = glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
        if (ptr) {
            std::memcpy(data_.data(), ptr, data_.size());
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
            pending_ = false;
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    }

    // Request new readback
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos_[current_pbo_]);
    glReadPixels(req_x_, req_y_, req_w_, req_h_, format_, type_, nullptr);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    read_pbo_ = current_pbo_;
    current_pbo_ = (current_pbo_ + 1) % NUM_PBOS;
    frame_count_++;
    pending_ = true;
}

void Readback::request_depth(int x, int y, int width, int height) {
    if (!initialized_) return;

    req_x_ = x;
    req_y_ = y;
    req_w_ = (width < 0) ? width_ : width;
    req_h_ = (height < 0) ? height_ : height;
    depth_mode_ = true;

    size_t depth_size = static_cast<size_t>(req_w_) * req_h_;
    depth_data_.resize(depth_size);

    glReadPixels(req_x_, req_y_, req_w_, req_h_, GL_DEPTH_COMPONENT, GL_FLOAT, depth_data_.data());
    pending_ = false;
}

bool Readback::available() const {
    return initialized_ && !pending_;
}

glm::vec4 Readback::read_pixel(int x, int y) const {
    if (data_.empty() || !available()) return glm::vec4(0.0f);

    int channels = (format_ == GL_RGBA) ? 4 : (format_ == GL_RGB) ? 3 : 1;
    int idx = (y * req_w_ + x) * channels;
    if (idx < 0 || idx + channels > static_cast<int>(data_.size())) return glm::vec4(0.0f);

    if (type_ == GL_UNSIGNED_BYTE) {
        return glm::vec4(
            data_[idx] / 255.0f,
            channels > 1 ? data_[idx + 1] / 255.0f : 0.0f,
            channels > 2 ? data_[idx + 2] / 255.0f : 0.0f,
            channels > 3 ? data_[idx + 3] / 255.0f : 1.0f
        );
    }
    return glm::vec4(0.0f);
}

float Readback::read_depth(int x, int y) const {
    if (depth_data_.empty()) return 1.0f;
    int idx = y * req_w_ + x;
    if (idx < 0 || idx >= static_cast<int>(depth_data_.size())) return 1.0f;
    return depth_data_[idx];
}

} // namespace spark
