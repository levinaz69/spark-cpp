#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <cstdint>
#include "core/defines.h"
#include "core/splat_encoding.h"

namespace spark {

// GPU-side packed splat data manager
class PackedSplats {
public:
    PackedSplats() = default;
    ~PackedSplats();

    // Non-copyable
    PackedSplats(const PackedSplats&) = delete;
    PackedSplats& operator=(const PackedSplats&) = delete;
    PackedSplats(PackedSplats&& other) noexcept;
    PackedSplats& operator=(PackedSplats&& other) noexcept;

    // Set data from decoded packed array (4 uint32 per splat)
    void set_data(const uint32_t* data, size_t num_splats,
                  const SplatEncoding& encoding = DEFAULT_SPLAT_ENCODING);

    // Add a single splat
    void push_splat(const glm::vec3& center, float opacity,
                    const glm::vec3& rgb, const glm::vec3& scale,
                    const glm::quat& quat);

    // Upload to GPU (creates/updates texture)
    void upload_to_gpu();

    // Accessors
    size_t num_splats() const { return num_splats_; }
    GLuint texture() const { return texture_; }
    int tex_width() const { return tex_size_.width; }
    int tex_height() const { return tex_size_.height; }
    int tex_depth() const { return tex_size_.depth; }
    const SplatEncoding& encoding() const { return encoding_; }
    const std::vector<uint32_t>& packed_array() const { return packed_array_; }
    bool gpu_dirty() const { return dirty_; }

private:
    void ensure_texture();

    std::vector<uint32_t> packed_array_;
    size_t num_splats_ = 0;
    SplatEncoding encoding_;
    SplatTexSize tex_size_ = {};

    GLuint texture_ = 0;
    bool dirty_ = true;
};

} // namespace spark
