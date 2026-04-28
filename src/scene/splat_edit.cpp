#include "splat_edit.h"
#include "core/splat_encoding.h"
#include "core/half_float.h"
#include <cmath>
#include <algorithm>

namespace spark {

float SdfRegion::distance(const glm::vec3& point) const {
    // Transform point to local space
    glm::vec3 local = glm::conjugate(rotation) * (point - center);

    switch (shape) {
        case SdfShape::Sphere: {
            return glm::length(local) - size.x;
        }
        case SdfShape::Box: {
            glm::vec3 d = glm::abs(local) - size;
            return glm::length(glm::max(d, glm::vec3(0.0f)))
                 + std::min(std::max(d.x, std::max(d.y, d.z)), 0.0f);
        }
        case SdfShape::Cylinder: {
            float d_xz = glm::length(glm::vec2(local.x, local.z)) - size.x;
            float d_y = std::abs(local.y) - size.y;
            return std::max(d_xz, d_y);
        }
        case SdfShape::Plane: {
            return local.y; // plane at y=0 in local space
        }
    }
    return 0.0f;
}

bool SdfRegion::contains(const glm::vec3& point) const {
    return distance(point) <= smoothness;
}

uint32_t SplatEdit::apply(uint32_t* packed_array, size_t num_splats,
                           const SplatEncoding& encoding, const EditParams& params) {
    uint32_t affected = 0;

    for (size_t i = 0; i < num_splats; i++) {
        uint32_t* p = packed_array + i * 4;
        glm::vec3 center = decode_packed_center(p);

        if (!params.region.contains(center)) continue;
        affected++;

        switch (params.op) {
            case EditOp::Hide: {
                float opacity = decode_packed_opacity(p, encoding);
                opacity *= params.opacity_factor;
                glm::vec3 rgb = decode_packed_rgb(p, encoding);
                glm::vec3 scale = decode_packed_scale(p, encoding);
                glm::quat quat = decode_packed_quat(p);
                encode_packed_splat(p, center, opacity, rgb, scale, quat, encoding);
                break;
            }
            case EditOp::ColorTint: {
                float opacity = decode_packed_opacity(p, encoding);
                glm::vec3 rgb = decode_packed_rgb(p, encoding);
                rgb *= params.color_tint;
                glm::vec3 scale = decode_packed_scale(p, encoding);
                glm::quat quat = decode_packed_quat(p);
                encode_packed_splat(p, center, opacity, rgb, scale, quat, encoding);
                break;
            }
            case EditOp::ScaleModify: {
                float opacity = decode_packed_opacity(p, encoding);
                glm::vec3 rgb = decode_packed_rgb(p, encoding);
                glm::vec3 scale = decode_packed_scale(p, encoding);
                scale *= params.scale_factor;
                glm::quat quat = decode_packed_quat(p);
                encode_packed_splat(p, center, opacity, rgb, scale, quat, encoding);
                break;
            }
            case EditOp::Move: {
                center += params.move_offset;
                float opacity = decode_packed_opacity(p, encoding);
                glm::vec3 rgb = decode_packed_rgb(p, encoding);
                glm::vec3 scale = decode_packed_scale(p, encoding);
                glm::quat quat = decode_packed_quat(p);
                encode_packed_splat(p, center, opacity, rgb, scale, quat, encoding);
                break;
            }
            default:
                break;
        }
    }

    return affected;
}

std::vector<uint32_t> SplatEdit::find_splats(const uint32_t* packed_array, size_t num_splats,
                                               const SdfRegion& region) {
    std::vector<uint32_t> result;
    for (size_t i = 0; i < num_splats; i++) {
        glm::vec3 center = decode_packed_center(packed_array + i * 4);
        if (region.contains(center)) {
            result.push_back(static_cast<uint32_t>(i));
        }
    }
    return result;
}

size_t SplatEdit::delete_splats(uint32_t* packed_array, size_t num_splats,
                                  const std::vector<uint32_t>& indices) {
    if (indices.empty()) return num_splats;

    std::vector<bool> to_delete(num_splats, false);
    for (uint32_t idx : indices) {
        if (idx < num_splats) to_delete[idx] = true;
    }

    size_t write_pos = 0;
    for (size_t i = 0; i < num_splats; i++) {
        if (!to_delete[i]) {
            if (write_pos != i) {
                packed_array[write_pos * 4 + 0] = packed_array[i * 4 + 0];
                packed_array[write_pos * 4 + 1] = packed_array[i * 4 + 1];
                packed_array[write_pos * 4 + 2] = packed_array[i * 4 + 2];
                packed_array[write_pos * 4 + 3] = packed_array[i * 4 + 3];
            }
            write_pos++;
        }
    }
    return write_pos;
}

} // namespace spark
