#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>

namespace spark {

// Spherical Harmonics clustering for data compression
// Groups similar SH coefficients together using k-means style clustering
class ShClusters {
public:
    ShClusters() = default;

    // Build clusters from SH data
    // coeffs: interleaved SH coefficients (count * components_per_splat)
    void build(const float* coeffs, size_t num_splats, int sh_degree,
               int num_clusters = 256, int max_iterations = 50);

    // Look up cluster centroids for a given label
    const float* centroid(uint32_t label) const;

    // Assign a splat to its nearest cluster
    uint32_t assign(const float* coeffs, int components) const;

    // Accessors
    size_t num_clusters() const { return centroids_.size() / components_; }
    int components() const { return components_; }
    const std::vector<float>& centroids_data() const { return centroids_; }
    const std::vector<uint32_t>& labels() const { return labels_; }

    // SH degree to number of components (per RGB channel)
    static int sh_components(int degree);

private:
    std::vector<float> centroids_; // num_clusters * components
    std::vector<uint32_t> labels_; // num_splats
    int components_ = 0;
};

} // namespace spark
