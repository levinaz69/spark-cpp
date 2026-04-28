#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>
#include <functional>
#include "lod/lod_tree.h"

namespace spark {

// SplatPager: manages LOD-based streaming/paging of splat data
// Pages in/out splat chunks based on camera distance and target budget
class SplatPager {
public:
    SplatPager() = default;

    struct PageConfig {
        int max_splats = 1000000;     // Maximum visible splats
        float page_distance = 100.0f; // Distance for page transitions
        float hysteresis = 0.1f;      // Prevent thrashing
        int page_size = 65536;        // Splats per page
    };

    struct Page {
        uint32_t base_index = 0;
        uint32_t count = 0;
        float min_distance = 0.0f;
        float max_distance = 0.0f;
        bool loaded = false;
        bool visible = false;
    };

    void configure(const PageConfig& config);
    void build_pages(const glm::vec3* centers, size_t num_splats,
                     const LodTree* lod_tree = nullptr);

    // Update visibility based on camera position
    void update(const glm::vec3& camera_pos);

    // Get visible splat ranges
    struct VisibleRange {
        uint32_t start;
        uint32_t count;
    };
    const std::vector<VisibleRange>& visible_ranges() const { return visible_ranges_; }
    size_t visible_count() const { return visible_count_; }

    // Callback for page load/unload events
    using PageCallback = std::function<void(int page_index, bool loaded)>;
    void set_callback(PageCallback cb) { callback_ = cb; }

private:
    PageConfig config_;
    std::vector<Page> pages_;
    std::vector<VisibleRange> visible_ranges_;
    size_t visible_count_ = 0;
    PageCallback callback_;
};

} // namespace spark
