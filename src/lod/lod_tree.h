#pragma once

#include <vector>
#include <cstdint>
#include <glm/glm.hpp>

namespace spark {

// LOD tree for hierarchical Gaussian splat rendering
// Each node has child_count children starting at child_start
class LodTree {
public:
    LodTree() = default;

    void build(const std::vector<uint16_t>& child_count,
               const std::vector<uint32_t>& child_start,
               size_t num_splats);

    // Get visible splats based on camera distance and target count
    std::vector<uint32_t> get_visible(const glm::vec3& camera_pos,
                                       const glm::vec3* centers,
                                       size_t num_splats,
                                       int target_count,
                                       float lod_scale = 1.0f) const;

    // Get LOD level for a given splat index
    int get_level(uint32_t index) const;

    bool valid() const { return !child_count_.empty(); }
    size_t num_nodes() const { return child_count_.size(); }
    size_t root_count() const { return root_count_; }

private:
    std::vector<uint16_t> child_count_;
    std::vector<uint32_t> child_start_;
    std::vector<uint8_t> level_;
    size_t root_count_ = 0;
};

// Quick LOD: simple distance-based LOD without tree structure
// Computes LOD tree from unsorted splats using spatial hashing
class QuickLod {
public:
    struct Result {
        std::vector<uint16_t> child_count;
        std::vector<uint32_t> child_start;
        std::vector<uint32_t> ordering;
    };

    static Result compute(const glm::vec3* centers, const float* scales,
                           size_t num_splats, int target_levels = 8);
};

// Tiny LOD: lightweight LOD based on scale thresholds
class TinyLod {
public:
    struct Result {
        std::vector<uint16_t> child_count;
        std::vector<uint32_t> child_start;
        std::vector<uint32_t> ordering;
    };

    static Result compute(const glm::vec3* centers, const float* scales,
                           size_t num_splats);
};

} // namespace spark
