#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <cstdint>
#include <optional>
#include "core/defines.h"
#include "core/splat_encoding.h"

namespace spark {

// Ray-ellipsoid intersection test for a single Gaussian splat
std::optional<float> raycast_ellipsoid(
    const glm::vec3& origin, const glm::vec3& dir,
    float opacity, const glm::vec3& center,
    const glm::vec3& scale, const glm::quat& quat);

// Raycast against an array of packed splats
std::vector<float> raycast_packed_splats(
    const glm::vec3& origin, const glm::vec3& dir,
    float min_opacity, float near, float far,
    const uint32_t* packed, size_t count,
    const SplatEncoding& encoding);

} // namespace spark
