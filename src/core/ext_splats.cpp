#include "ext_splats.h"
#include "defines.h"
#include <cstring>

namespace spark {

ExtSplats::~ExtSplats() {
    if (texture_) glDeleteTextures(1, &texture_);
}

void ExtSplats::set_centers(const glm::vec3* centers, size_t count) {
    centers_.assign(centers, centers + count);
    dirty_ = true;
}

void ExtSplats::push_center(const glm::vec3& center) {
    centers_.push_back(center);
    dirty_ = true;
}

void ExtSplats::upload_to_gpu() {
    if (!dirty_ || centers_.empty()) return;

    if (!texture_) glGenTextures(1, &texture_);

    auto tex_size = get_splat_tex_size(static_cast<int>(centers_.size()));

    // Pack centers as RGBA32F (xyz + padding)
    size_t total_texels = static_cast<size_t>(tex_size.width) * tex_size.height * tex_size.depth;
    std::vector<float> data(total_texels * 4, 0.0f);
    for (size_t i = 0; i < centers_.size(); i++) {
        data[i * 4 + 0] = centers_[i].x;
        data[i * 4 + 1] = centers_[i].y;
        data[i * 4 + 2] = centers_[i].z;
        data[i * 4 + 3] = 1.0f;
    }

    glBindTexture(GL_TEXTURE_2D_ARRAY, texture_);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA32F,
                 tex_size.width, tex_size.height, tex_size.depth,
                 0, GL_RGBA, GL_FLOAT, data.data());
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    dirty_ = false;
}

} // namespace spark
