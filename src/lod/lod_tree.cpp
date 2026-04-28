#include "lod_tree.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <unordered_map>

namespace spark {

void LodTree::build(const std::vector<uint16_t>& child_count,
                     const std::vector<uint32_t>& child_start,
                     size_t num_splats) {
    child_count_ = child_count;
    child_start_ = child_start;
    level_.resize(num_splats, 0);

    // Find root nodes (those not referenced as children)
    std::vector<bool> is_child(num_splats, false);
    for (size_t i = 0; i < num_splats; i++) {
        uint32_t start = child_start_[i];
        uint16_t count = child_count_[i];
        for (uint32_t c = 0; c < count && (start + c) < num_splats; c++) {
            is_child[start + c] = true;
        }
    }

    root_count_ = 0;
    for (size_t i = 0; i < num_splats; i++) {
        if (!is_child[i]) root_count_++;
    }

    // Compute levels via BFS
    std::vector<uint32_t> queue;
    for (size_t i = 0; i < num_splats; i++) {
        if (!is_child[i]) {
            queue.push_back(static_cast<uint32_t>(i));
            level_[i] = 0;
        }
    }

    size_t front = 0;
    while (front < queue.size()) {
        uint32_t idx = queue[front++];
        uint8_t next_level = level_[idx] + 1;
        uint32_t start = child_start_[idx];
        uint16_t count = child_count_[idx];
        for (uint32_t c = 0; c < count && (start + c) < num_splats; c++) {
            uint32_t child = start + c;
            level_[child] = next_level;
            queue.push_back(child);
        }
    }
}

std::vector<uint32_t> LodTree::get_visible(const glm::vec3& camera_pos,
                                             const glm::vec3* centers,
                                             size_t num_splats,
                                             int target_count,
                                             float lod_scale) const {
    if (child_count_.empty()) {
        // No LOD tree, return all indices
        std::vector<uint32_t> all(num_splats);
        std::iota(all.begin(), all.end(), 0);
        return all;
    }

    std::vector<uint32_t> visible;
    visible.reserve(target_count);

    // BFS traversal with distance-based refinement
    std::vector<uint32_t> stack;
    std::vector<bool> is_child(num_splats, false);
    for (size_t i = 0; i < num_splats; i++) {
        uint32_t start = child_start_[i];
        uint16_t count = child_count_[i];
        for (uint32_t c = 0; c < count && (start + c) < num_splats; c++) {
            is_child[start + c] = true;
        }
    }

    for (size_t i = 0; i < num_splats; i++) {
        if (!is_child[i]) stack.push_back(static_cast<uint32_t>(i));
    }

    while (!stack.empty() && static_cast<int>(visible.size()) < target_count) {
        uint32_t idx = stack.back();
        stack.pop_back();

        visible.push_back(idx);

        // Refine if close enough
        float dist = glm::length(centers[idx] - camera_pos);
        float threshold = lod_scale * (level_[idx] + 1) * 5.0f;

        if (dist < threshold && child_count_[idx] > 0) {
            uint32_t start = child_start_[idx];
            for (uint32_t c = 0; c < child_count_[idx] && (start + c) < num_splats; c++) {
                stack.push_back(start + c);
            }
        }
    }

    return visible;
}

int LodTree::get_level(uint32_t index) const {
    if (index < level_.size()) return level_[index];
    return 0;
}

// QuickLod implementation
QuickLod::Result QuickLod::compute(const glm::vec3* centers, const float* scales,
                                     size_t num_splats, int target_levels) {
    Result result;
    result.child_count.resize(num_splats, 0);
    result.child_start.resize(num_splats, 0);
    result.ordering.resize(num_splats);
    std::iota(result.ordering.begin(), result.ordering.end(), 0);

    if (num_splats < 2) return result;

    // Sort by scale (largest first = coarsest LOD)
    std::sort(result.ordering.begin(), result.ordering.end(),
              [&](uint32_t a, uint32_t b) {
                  return scales[a] > scales[b];
              });

    // Assign LOD levels based on scale quantiles
    size_t level_size = num_splats / target_levels;
    if (level_size < 1) level_size = 1;

    for (int level = 0; level < target_levels - 1; level++) {
        size_t parent_start = level * level_size;
        size_t child_start_idx = (level + 1) * level_size;

        if (child_start_idx >= num_splats) break;

        size_t children_in_level = std::min(level_size, num_splats - child_start_idx);
        size_t children_per_parent = std::max(size_t(1), children_in_level / level_size);

        for (size_t i = 0; i < level_size && parent_start + i < num_splats; i++) {
            size_t c_start = child_start_idx + i * children_per_parent;
            size_t c_count = std::min(children_per_parent, num_splats - c_start);
            if (c_start >= num_splats) break;

            result.child_start[result.ordering[parent_start + i]] = static_cast<uint32_t>(c_start);
            result.child_count[result.ordering[parent_start + i]] = static_cast<uint16_t>(c_count);
        }
    }

    return result;
}

// TinyLod implementation
TinyLod::Result TinyLod::compute(const glm::vec3* centers, const float* scales,
                                   size_t num_splats) {
    auto qr = QuickLod::compute(centers, scales, num_splats, 4);
    Result r;
    r.child_count = std::move(qr.child_count);
    r.child_start = std::move(qr.child_start);
    r.ordering = std::move(qr.ordering);
    return r;
}

} // namespace spark
