#include "raycast.h"
#include <cmath>
#include <algorithm>

namespace spark {

// Rotate vector by quaternion
static glm::vec3 quat_rotate(const glm::quat& q, const glm::vec3& v) {
    return q * v;
}

std::optional<float> raycast_ellipsoid(
    const glm::vec3& origin, const glm::vec3& dir,
    float opacity, const glm::vec3& center,
    const glm::vec3& scale, const glm::quat& quat) {

    glm::vec3 local_origin = origin - center;
    glm::quat inv_quat = glm::conjugate(quat);

    // Transform ray into ellipsoid's local space
    glm::vec3 lo = quat_rotate(inv_quat, local_origin);
    glm::vec3 ld = quat_rotate(inv_quat, dir);

    // Scale to unit sphere
    glm::vec3 inv_scale(1.0f / scale.x, 1.0f / scale.y, 1.0f / scale.z);
    lo *= inv_scale;
    ld *= inv_scale;

    // Standard ray-sphere intersection
    float a = glm::dot(ld, ld);
    float b = 2.0f * glm::dot(lo, ld);
    float c = glm::dot(lo, lo) - 1.0f;
    float discriminant = b * b - 4.0f * a * c;

    if (discriminant < 0.0f) return std::nullopt;

    float sqrt_disc = std::sqrt(discriminant);
    float t1 = (-b - sqrt_disc) / (2.0f * a);
    float t2 = (-b + sqrt_disc) / (2.0f * a);

    if (t1 > 0.0f) return t1;
    if (t2 > 0.0f) return t2;
    return std::nullopt;
}

std::vector<float> raycast_packed_splats(
    const glm::vec3& origin, const glm::vec3& dir,
    float min_opacity, float near, float far,
    const uint32_t* packed, size_t count,
    const SplatEncoding& encoding) {

    std::vector<float> distances;

    for (size_t i = 0; i < count; i++) {
        const uint32_t* p = packed + i * 4;
        float opacity = decode_packed_opacity(p, encoding);
        if (opacity < min_opacity) continue;

        glm::vec3 center = decode_packed_center(p);
        glm::vec3 scale = decode_packed_scale(p, encoding);
        glm::quat quat = decode_packed_quat(p);

        auto t = raycast_ellipsoid(origin, dir, opacity, center, scale, quat);
        if (t && *t >= near && *t <= far) {
            distances.push_back(*t);
        }
    }

    std::sort(distances.begin(), distances.end());
    return distances;
}

} // namespace spark
