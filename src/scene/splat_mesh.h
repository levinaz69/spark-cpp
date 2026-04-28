#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <memory>
#include "render/packed_splats.h"
#include "core/ext_splats.h"
#include "lod/lod_tree.h"

namespace spark {

// SplatMesh: A renderable Gaussian splat scene object with transform
class SplatMesh {
public:
    SplatMesh() = default;

    // Transform
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    glm::mat4 model_matrix() const;
    glm::mat4 inverse_model_matrix() const;

    // Splat data
    PackedSplats packed_splats;
    ExtSplats ext_splats;
    LodTree lod_tree;

    // Metadata
    std::string name;
    bool visible = true;
    float opacity_multiplier = 1.0f;

    // LOD
    void build_lod();
    int target_splat_count = -1; // -1 = show all

    // Apply transform to all splat centers
    void apply_transform();
};

} // namespace spark
