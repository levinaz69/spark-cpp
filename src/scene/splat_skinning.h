#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/dual_quaternion.hpp>
#include <vector>
#include <cstdint>
#include <string>
#include "core/defines.h"

namespace spark {

// Bone for skeletal animation
struct Bone {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
    int parent = -1;
    std::string name;

    glm::mat4 local_matrix() const;
    glm::fdualquat dual_quaternion() const;
};

// Skin weight for a splat (up to 4 bone influences)
struct SkinWeight {
    int bone_indices[4] = {0, 0, 0, 0};
    float weights[4] = {1.0f, 0.0f, 0.0f, 0.0f};
};

// SplatSkinning: skeletal animation for Gaussian splats using dual quaternion blending
class SplatSkinning {
public:
    SplatSkinning() = default;

    // Setup
    void set_bones(const std::vector<Bone>& bones);
    void set_bind_pose(const std::vector<Bone>& bind_pose);
    void set_weights(const std::vector<SkinWeight>& weights);

    // Update bone transforms (call each frame)
    void update_bones(const std::vector<Bone>& posed_bones);

    // Apply skinning to packed splat array (CPU-side)
    void apply(const uint32_t* rest_packed, uint32_t* output_packed,
               size_t num_splats, const SplatEncoding& encoding);

    // Accessors
    const std::vector<Bone>& bones() const { return bones_; }
    size_t num_bones() const { return bones_.size(); }

private:
    std::vector<Bone> bones_;
    std::vector<Bone> bind_pose_;
    std::vector<SkinWeight> weights_;
    std::vector<glm::mat4> bone_matrices_;
    std::vector<glm::fdualquat> bone_dualquats_;
    std::vector<glm::mat4> inverse_bind_;
};

} // namespace spark
