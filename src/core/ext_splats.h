#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace spark {

// Extended splats: float32 precision centers (for large scenes)
// Stored in a separate GPU texture alongside packed splats
class ExtSplats {
public:
    ExtSplats() = default;
    ~ExtSplats();

    ExtSplats(const ExtSplats&) = delete;
    ExtSplats& operator=(const ExtSplats&) = delete;

    void set_centers(const glm::vec3* centers, size_t count);
    void push_center(const glm::vec3& center);
    void upload_to_gpu();

    size_t count() const { return centers_.size(); }
    GLuint texture() const { return texture_; }
    const std::vector<glm::vec3>& centers() const { return centers_; }

    bool enabled = false;

private:
    std::vector<glm::vec3> centers_;
    GLuint texture_ = 0;
    bool dirty_ = true;
};

} // namespace spark
