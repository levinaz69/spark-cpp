#include "packed_splats.h"
#include <cstring>
#include <algorithm>

namespace spark {

PackedSplats::~PackedSplats() {
    if (texture_) glDeleteTextures(1, &texture_);
}

PackedSplats::PackedSplats(PackedSplats&& other) noexcept
    : packed_array_(std::move(other.packed_array_))
    , num_splats_(other.num_splats_)
    , encoding_(other.encoding_)
    , tex_size_(other.tex_size_)
    , texture_(other.texture_)
    , dirty_(other.dirty_) {
    other.texture_ = 0;
    other.num_splats_ = 0;
}

PackedSplats& PackedSplats::operator=(PackedSplats&& other) noexcept {
    if (this != &other) {
        if (texture_) glDeleteTextures(1, &texture_);
        packed_array_ = std::move(other.packed_array_);
        num_splats_ = other.num_splats_;
        encoding_ = other.encoding_;
        tex_size_ = other.tex_size_;
        texture_ = other.texture_;
        dirty_ = other.dirty_;
        other.texture_ = 0;
        other.num_splats_ = 0;
    }
    return *this;
}

void PackedSplats::set_data(const uint32_t* data, size_t num_splats,
                             const SplatEncoding& encoding) {
    num_splats_ = num_splats;
    encoding_ = encoding;
    packed_array_.assign(data, data + num_splats * 4);
    tex_size_ = get_splat_tex_size(static_cast<int>(num_splats));
    dirty_ = true;
}

void PackedSplats::push_splat(const glm::vec3& center, float opacity,
                               const glm::vec3& rgb, const glm::vec3& scale,
                               const glm::quat& quat) {
    packed_array_.resize((num_splats_ + 1) * 4);
    encode_packed_splat(&packed_array_[num_splats_ * 4],
                       center, opacity, rgb, scale, quat, encoding_);
    num_splats_++;
    tex_size_ = get_splat_tex_size(static_cast<int>(num_splats_));
    dirty_ = true;
}

void PackedSplats::ensure_texture() {
    if (!texture_) {
        glGenTextures(1, &texture_);
    }
}

void PackedSplats::upload_to_gpu() {
    if (!dirty_ || num_splats_ == 0) return;
    ensure_texture();

    // Pad to fill the full texture
    size_t total_texels = static_cast<size_t>(tex_size_.width) * tex_size_.height * tex_size_.depth;
    std::vector<uint32_t> upload_data(total_texels * 4, 0);
    std::memcpy(upload_data.data(), packed_array_.data(),
                num_splats_ * 4 * sizeof(uint32_t));

    glBindTexture(GL_TEXTURE_2D_ARRAY, texture_);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA32UI,
                 tex_size_.width, tex_size_.height, tex_size_.depth,
                 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT, upload_data.data());
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    dirty_ = false;
}

} // namespace spark
