#include "splat_pager.h"
#include <algorithm>
#include <cmath>

namespace spark {

void SplatPager::configure(const PageConfig& config) {
    config_ = config;
}

void SplatPager::build_pages(const glm::vec3* centers, size_t num_splats,
                               const LodTree* lod_tree) {
    pages_.clear();
    if (num_splats == 0) return;

    // Compute bounding box center
    glm::vec3 bbox_min(std::numeric_limits<float>::max());
    glm::vec3 bbox_max(std::numeric_limits<float>::lowest());
    for (size_t i = 0; i < num_splats; i++) {
        bbox_min = glm::min(bbox_min, centers[i]);
        bbox_max = glm::max(bbox_max, centers[i]);
    }
    glm::vec3 scene_center = (bbox_min + bbox_max) * 0.5f;

    // Compute distances from center
    std::vector<std::pair<float, uint32_t>> dist_indices(num_splats);
    for (size_t i = 0; i < num_splats; i++) {
        float dist = glm::length(centers[i] - scene_center);
        dist_indices[i] = {dist, static_cast<uint32_t>(i)};
    }

    // Create pages based on page_size
    size_t num_pages = (num_splats + config_.page_size - 1) / config_.page_size;
    pages_.resize(num_pages);

    for (size_t p = 0; p < num_pages; p++) {
        auto& page = pages_[p];
        page.base_index = static_cast<uint32_t>(p * config_.page_size);
        page.count = static_cast<uint32_t>(
            std::min(static_cast<size_t>(config_.page_size),
                     num_splats - p * config_.page_size));
        page.loaded = true;
        page.visible = true;

        // Compute distance range for this page
        page.min_distance = std::numeric_limits<float>::max();
        page.max_distance = 0.0f;
        for (uint32_t i = 0; i < page.count; i++) {
            uint32_t idx = page.base_index + i;
            float dist = glm::length(centers[idx] - scene_center);
            page.min_distance = std::min(page.min_distance, dist);
            page.max_distance = std::max(page.max_distance, dist);
        }
    }
}

void SplatPager::update(const glm::vec3& camera_pos) {
    visible_ranges_.clear();
    visible_count_ = 0;

    for (size_t p = 0; p < pages_.size(); p++) {
        auto& page = pages_[p];

        // Simple distance-based visibility
        bool should_be_visible = true;
        if (visible_count_ + page.count > static_cast<size_t>(config_.max_splats)) {
            should_be_visible = false;
        }

        bool was_visible = page.visible;
        page.visible = should_be_visible;

        if (page.visible) {
            visible_ranges_.push_back({page.base_index, page.count});
            visible_count_ += page.count;
        }

        if (was_visible != page.visible && callback_) {
            callback_(static_cast<int>(p), page.visible);
        }
    }
}

} // namespace spark
