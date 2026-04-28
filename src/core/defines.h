#pragma once

#include <cstdint>
#include <cmath>

namespace spark {

// LN_SCALE_MIN..LN_SCALE_MAX define the internal scale range for Gsplats,
// covering approx 0.0001..8000 with discrete steps ~7% apart.
// Value "0" is reserved for truly flat scales (2DGS).
constexpr float LN_SCALE_MIN = -12.0f;
constexpr float LN_SCALE_MAX = 9.0f;

// Zero scale sentinel for 2DGS
constexpr float LN_SCALE_ZERO = -30.0f;

// Splat texture dimensions: 2^11 x 2^11 x 2^11
constexpr int SPLAT_TEX_WIDTH_BITS = 11;
constexpr int SPLAT_TEX_HEIGHT_BITS = 11;
constexpr int SPLAT_TEX_DEPTH_BITS = 11;
constexpr int SPLAT_TEX_LAYER_BITS = SPLAT_TEX_WIDTH_BITS + SPLAT_TEX_HEIGHT_BITS;

constexpr int SPLAT_TEX_WIDTH = 1 << SPLAT_TEX_WIDTH_BITS;   // 2048
constexpr int SPLAT_TEX_HEIGHT = 1 << SPLAT_TEX_HEIGHT_BITS; // 2048
constexpr int SPLAT_TEX_DEPTH = 1 << SPLAT_TEX_DEPTH_BITS;   // 2048
constexpr int SPLAT_TEX_MIN_HEIGHT = 1;
constexpr int SPLAT_TEX_LAYER_SIZE = SPLAT_TEX_WIDTH * SPLAT_TEX_HEIGHT;

constexpr int SPLAT_TEX_WIDTH_MASK = SPLAT_TEX_WIDTH - 1;
constexpr int SPLAT_TEX_HEIGHT_MASK = SPLAT_TEX_HEIGHT - 1;
constexpr int SPLAT_TEX_DEPTH_MASK = SPLAT_TEX_DEPTH - 1;

// File format types
enum class SplatFileType {
    Auto,
    PLY,
    SPZ,
    SPLAT,
    KSPLAT,
    PCSOGS,
    PCSOGSZIP,
    RAD,
};

// Encoding parameters for packed splats
struct SplatEncoding {
    float rgb_min = 0.0f;
    float rgb_max = 1.0f;
    float ln_scale_min = LN_SCALE_MIN;
    float ln_scale_max = LN_SCALE_MAX;
    float sh1_max = 1.0f;
    float sh2_max = 1.0f;
    float sh3_max = 1.0f;
    bool lod_opacity = false;
};

inline constexpr SplatEncoding DEFAULT_SPLAT_ENCODING{};

// Compute texture dimensions for a given number of splats
struct SplatTexSize {
    int width;
    int height;
    int depth;
    int max_splats;
};

inline SplatTexSize get_splat_tex_size(int num_splats) {
    int width = SPLAT_TEX_WIDTH;
    int height = std::max(SPLAT_TEX_MIN_HEIGHT,
                          std::min(SPLAT_TEX_HEIGHT,
                                   (num_splats + SPLAT_TEX_WIDTH - 1) / SPLAT_TEX_WIDTH));
    int depth = std::max(1, (num_splats + SPLAT_TEX_LAYER_SIZE - 1) / SPLAT_TEX_LAYER_SIZE);
    int max_splats = width * height * depth;
    return {width, height, depth, max_splats};
}

} // namespace spark
