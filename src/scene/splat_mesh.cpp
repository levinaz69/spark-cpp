#include "splat_mesh.h"
#include "core/splat_encoding.h"
#include "core/half_float.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace spark {

glm::mat4 SplatMesh::model_matrix() const {
    glm::mat4 m = glm::translate(glm::mat4(1.0f), position);
    m *= glm::mat4_cast(rotation);
    m = glm::scale(m, scale);
    return m;
}

glm::mat4 SplatMesh::inverse_model_matrix() const {
    return glm::inverse(model_matrix());
}

void SplatMesh::build_lod() {
    if (packed_splats.num_splats() == 0) return;

    const auto& packed = packed_splats.packed_array();
    size_t n = packed_splats.num_splats();

    std::vector<glm::vec3> centers(n);
    std::vector<float> scales(n);

    for (size_t i = 0; i < n; i++) {
        centers[i] = decode_packed_center(&packed[i * 4]);
        glm::vec3 s = decode_packed_scale(&packed[i * 4], packed_splats.encoding());
        scales[i] = (s.x + s.y + s.z) / 3.0f; // average scale
    }

    auto result = QuickLod::compute(centers.data(), scales.data(), n);
    lod_tree.build(result.child_count, result.child_start, n);
}

void SplatMesh::apply_transform() {
    if (packed_splats.num_splats() == 0) return;

    glm::mat4 m = model_matrix();
    auto& packed = const_cast<std::vector<uint32_t>&>(packed_splats.packed_array());
    size_t n = packed_splats.num_splats();
    const auto& enc = packed_splats.encoding();

    for (size_t i = 0; i < n; i++) {
        glm::vec3 center = decode_packed_center(&packed[i * 4]);
        float opacity = decode_packed_opacity(&packed[i * 4], enc);
        glm::vec3 rgb = decode_packed_rgb(&packed[i * 4], enc);
        glm::vec3 s = decode_packed_scale(&packed[i * 4], enc);
        glm::quat q = decode_packed_quat(&packed[i * 4]);

        // Transform center
        glm::vec4 tc = m * glm::vec4(center, 1.0f);
        center = glm::vec3(tc);

        // Transform rotation
        q = rotation * q;

        // Transform scale
        s *= scale;

        encode_packed_splat(&packed[i * 4], center, opacity, rgb, s, q, enc);
    }

    // Reset transform
    position = glm::vec3(0.0f);
    rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    this->scale = glm::vec3(1.0f);
}

} // namespace spark
