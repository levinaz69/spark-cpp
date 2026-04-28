#include "sh_clustering.h"
#include <cmath>
#include <algorithm>
#include <random>
#include <limits>
#include <numeric>

namespace spark {

int ShClusters::sh_components(int degree) {
    switch (degree) {
        case 1: return 3 * 3;   // 9
        case 2: return 3 * 8;   // 24
        case 3: return 3 * 15;  // 45
        default: return 0;
    }
}

void ShClusters::build(const float* coeffs, size_t num_splats, int sh_degree,
                         int num_clusters, int max_iterations) {
    components_ = sh_components(sh_degree);
    if (components_ == 0 || num_splats == 0) return;

    int k = std::min(num_clusters, static_cast<int>(num_splats));
    centroids_.resize(k * components_, 0.0f);
    labels_.resize(num_splats, 0);

    // Initialize centroids using k-means++ style
    std::mt19937 rng(42);
    std::uniform_int_distribution<size_t> dist(0, num_splats - 1);

    // First centroid: random
    size_t first = dist(rng);
    for (int c = 0; c < components_; c++) {
        centroids_[c] = coeffs[first * components_ + c];
    }

    // Remaining centroids: k-means++
    std::vector<float> min_dists(num_splats, std::numeric_limits<float>::max());
    for (int ki = 1; ki < k; ki++) {
        // Update distances
        for (size_t i = 0; i < num_splats; i++) {
            float d = 0.0f;
            for (int c = 0; c < components_; c++) {
                float diff = coeffs[i * components_ + c] - centroids_[(ki - 1) * components_ + c];
                d += diff * diff;
            }
            min_dists[i] = std::min(min_dists[i], d);
        }

        // Weighted random selection
        float total = std::accumulate(min_dists.begin(), min_dists.end(), 0.0f);
        std::uniform_real_distribution<float> fdist(0.0f, total);
        float r = fdist(rng);
        float acc = 0.0f;
        size_t chosen = 0;
        for (size_t i = 0; i < num_splats; i++) {
            acc += min_dists[i];
            if (acc >= r) { chosen = i; break; }
        }
        for (int c = 0; c < components_; c++) {
            centroids_[ki * components_ + c] = coeffs[chosen * components_ + c];
        }
    }

    // K-means iterations
    std::vector<float> new_centroids(k * components_);
    std::vector<int> counts(k);

    for (int iter = 0; iter < max_iterations; iter++) {
        bool changed = false;

        // Assign step
        for (size_t i = 0; i < num_splats; i++) {
            float best_dist = std::numeric_limits<float>::max();
            uint32_t best_k = 0;
            for (int ki = 0; ki < k; ki++) {
                float d = 0.0f;
                for (int c = 0; c < components_; c++) {
                    float diff = coeffs[i * components_ + c] - centroids_[ki * components_ + c];
                    d += diff * diff;
                }
                if (d < best_dist) {
                    best_dist = d;
                    best_k = static_cast<uint32_t>(ki);
                }
            }
            if (labels_[i] != best_k) {
                labels_[i] = best_k;
                changed = true;
            }
        }

        if (!changed) break;

        // Update step
        std::fill(new_centroids.begin(), new_centroids.end(), 0.0f);
        std::fill(counts.begin(), counts.end(), 0);

        for (size_t i = 0; i < num_splats; i++) {
            uint32_t ki = labels_[i];
            for (int c = 0; c < components_; c++) {
                new_centroids[ki * components_ + c] += coeffs[i * components_ + c];
            }
            counts[ki]++;
        }

        for (int ki = 0; ki < k; ki++) {
            if (counts[ki] > 0) {
                for (int c = 0; c < components_; c++) {
                    centroids_[ki * components_ + c] = new_centroids[ki * components_ + c] / counts[ki];
                }
            }
        }
    }
}

const float* ShClusters::centroid(uint32_t label) const {
    if (label * components_ + components_ > centroids_.size()) return nullptr;
    return &centroids_[label * components_];
}

uint32_t ShClusters::assign(const float* sh_coeffs, int components) const {
    if (centroids_.empty() || components != components_) return 0;

    float best_dist = std::numeric_limits<float>::max();
    uint32_t best = 0;
    size_t nc = centroids_.size() / components_;

    for (size_t ki = 0; ki < nc; ki++) {
        float d = 0.0f;
        for (int c = 0; c < components_; c++) {
            float diff = sh_coeffs[c] - centroids_[ki * components_ + c];
            d += diff * diff;
        }
        if (d < best_dist) {
            best_dist = d;
            best = static_cast<uint32_t>(ki);
        }
    }
    return best;
}

} // namespace spark
