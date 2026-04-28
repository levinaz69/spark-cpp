#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <cstdint>
#include <functional>
#include "core/defines.h"

namespace spark {

// SDF shape types for splat editing
enum class SdfShape {
    Sphere,
    Box,
    Cylinder,
    Plane,
};

// SDF region for selecting/modifying splats
struct SdfRegion {
    SdfShape shape = SdfShape::Sphere;
    glm::vec3 center{0.0f};
    glm::vec3 size{1.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    float smoothness = 0.0f;

    float distance(const glm::vec3& point) const;
    bool contains(const glm::vec3& point) const;
};

// Edit operation types
enum class EditOp {
    Delete,       // Remove splats inside region
    Hide,         // Set opacity to 0
    ColorTint,    // Tint splat colors
    ScaleModify,  // Modify splat scales
    Move,         // Move splats
    Duplicate,    // Duplicate selected splats
};

struct EditParams {
    EditOp op = EditOp::Delete;
    SdfRegion region;
    glm::vec3 color_tint{1.0f};
    glm::vec3 move_offset{0.0f};
    float scale_factor = 1.0f;
    float opacity_factor = 1.0f;
};

// SplatEdit: Apply SDF-based edits to packed splat arrays
class SplatEdit {
public:
    SplatEdit() = default;

    // Apply edit operation, returns number of affected splats
    uint32_t apply(uint32_t* packed_array, size_t num_splats,
                   const SplatEncoding& encoding, const EditParams& params);

    // Find splats inside a region
    std::vector<uint32_t> find_splats(const uint32_t* packed_array, size_t num_splats,
                                      const SdfRegion& region);

    // Delete splats by indices (compact the array)
    size_t delete_splats(uint32_t* packed_array, size_t num_splats,
                          const std::vector<uint32_t>& indices);
};

} // namespace spark
