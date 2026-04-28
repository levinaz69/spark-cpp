#include "radix_sort.h"
#include <algorithm>

namespace spark {

// --- RadixSort16 ---

void RadixSort16::ensure_size(size_t max_splats) {
    if (readback.size() < max_splats) readback.resize(max_splats, 0);
    if (ordering.size() < max_splats) ordering.resize(max_splats, 0);
    if (buckets.size() < DEPTH_SIZE) buckets.resize(DEPTH_SIZE, 0);
}

uint32_t RadixSort16::sort(size_t num_splats) {
    // Clear buckets
    std::fill(buckets.begin(), buckets.end(), 0);

    // Count splats in each bucket
    for (size_t i = 0; i < num_splats; i++) {
        uint16_t metric = readback[i];
        if (static_cast<uint32_t>(metric) < DEPTH_INFINITY) {
            buckets[metric]++;
        }
    }

    // Compute bucket offsets (descending: farthest first)
    uint32_t active_splats = 0;
    for (int i = static_cast<int>(DEPTH_SIZE) - 2; i >= 0; i--) {
        uint32_t new_total = active_splats + buckets[i];
        buckets[i] = active_splats;
        active_splats = new_total;
    }

    // Write splat indices at correct positions
    for (size_t i = 0; i < num_splats; i++) {
        uint16_t metric = readback[i];
        if (static_cast<uint32_t>(metric) < DEPTH_INFINITY) {
            ordering[buckets[metric]] = static_cast<uint32_t>(i);
            buckets[metric]++;
        }
    }

    return active_splats;
}

// --- RadixSort32 ---

void RadixSort32::ensure_size(size_t max_splats) {
    if (readback.size() < max_splats) readback.resize(max_splats, 0);
    if (ordering.size() < max_splats) ordering.resize(max_splats, 0);
    if (scratch.size() < max_splats) scratch.resize(max_splats, 0);
    if (buckets_lo.size() < RADIX_BASE) buckets_lo.resize(RADIX_BASE, 0);
    if (buckets_hi.size() < RADIX_BASE) buckets_hi.resize(RADIX_BASE, 0);
}

uint32_t RadixSort32::sort(size_t num_splats) {
    // Tally low and high buckets
    std::fill(buckets_lo.begin(), buckets_lo.end(), 0);
    std::fill(buckets_hi.begin(), buckets_hi.end(), 0);

    for (size_t i = 0; i < num_splats; i++) {
        uint32_t key = readback[i];
        if (key < DEPTH_INFINITY) {
            uint32_t inv = ~key;
            buckets_lo[inv & 0xFFFF]++;
            buckets_hi[inv >> 16]++;
        }
    }

    // Pass 1: bucket by inv(low 16 bits) - exclusive prefix sum
    uint32_t total = 0;
    for (auto& slot : buckets_lo) {
        uint32_t cnt = slot;
        slot = total;
        total += cnt;
    }
    uint32_t active_splats = total;

    // Scatter into scratch by low bits
    for (size_t i = 0; i < num_splats; i++) {
        uint32_t key = readback[i];
        if (key < DEPTH_INFINITY) {
            uint32_t inv = ~key;
            uint32_t lo = inv & 0xFFFF;
            scratch[buckets_lo[lo]] = static_cast<uint32_t>(i);
            buckets_lo[lo]++;
        }
    }

    // Pass 2: bucket by inv(high 16 bits) - exclusive prefix sum
    uint32_t sum = 0;
    for (auto& slot : buckets_hi) {
        uint32_t cnt = slot;
        slot = sum;
        sum += cnt;
    }

    // Scatter into final ordering by high bits
    for (size_t i = 0; i < active_splats; i++) {
        uint32_t idx = scratch[i];
        uint32_t key = readback[idx];
        uint32_t inv = ~key;
        uint32_t hi = inv >> 16;
        ordering[buckets_hi[hi]] = idx;
        buckets_hi[hi]++;
    }

    return active_splats;
}

} // namespace spark
