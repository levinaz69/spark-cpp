#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace spark {

// 16-bit radix sort for float16 depth values (back-to-front)
class RadixSort16 {
public:
    void ensure_size(size_t max_splats);
    uint32_t sort(size_t num_splats);

    std::vector<uint16_t> readback;
    std::vector<uint32_t> ordering;

private:
    static constexpr uint32_t DEPTH_INFINITY = 0x7C00;
    static constexpr size_t DEPTH_SIZE = DEPTH_INFINITY + 1;

    std::vector<uint32_t> buckets;
};

// 32-bit radix sort for float32 depth values (back-to-front, 2-pass)
class RadixSort32 {
public:
    void ensure_size(size_t max_splats);
    uint32_t sort(size_t num_splats);

    std::vector<uint32_t> readback;
    std::vector<uint32_t> ordering;

private:
    static constexpr uint32_t DEPTH_INFINITY = 0x7F800000;
    static constexpr size_t RADIX_BASE = 1 << 16;

    std::vector<uint32_t> buckets_lo;
    std::vector<uint32_t> buckets_hi;
    std::vector<uint32_t> scratch;
};

} // namespace spark
