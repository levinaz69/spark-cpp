#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <cstdint>
#include "half_float.h"

namespace spark {

// Core Gaussian Splat data structure using half-float storage
struct Gsplat {
    glm::vec3 center{0.0f};
    uint16_t opacity_h = 0;  // half float
    uint16_t rgb_h[3] = {};  // half float [R, G, B]
    uint16_t ln_scales_h[3] = {}; // half float [x, y, z] (log scale)
    uint16_t quaternion_h[4] = {}; // half float [x, y, z, w]

    static Gsplat create(glm::vec3 center, float opacity,
                         glm::vec3 rgb, glm::vec3 scales, glm::quat quat);

    float opacity() const { return half_to_float(opacity_h); }
    glm::vec3 rgb() const;
    glm::vec3 scales() const;
    glm::quat quaternion() const;
    float max_scale() const;
    float feature_size() const;

    void set_center(glm::vec3 c) { center = c; }
    void set_opacity(float o) { opacity_h = float_to_half(o); }
    void set_rgb(glm::vec3 c);
    void set_scales(glm::vec3 s);
    void set_quaternion(glm::quat q);
};

// Spherical Harmonics coefficients
struct GsplatSH1 {
    uint16_t data[3][3] = {}; // 3 coefficients × RGB (half float)
    void set_from_array(const float* vals);
    void to_array(float* out) const;
};

struct GsplatSH2 {
    uint16_t data[5][3] = {}; // 5 coefficients × RGB
    void set_from_array(const float* vals);
    void to_array(float* out) const;
};

struct GsplatSH3 {
    uint16_t data[7][3] = {}; // 7 coefficients × RGB
    void set_from_array(const float* vals);
    void to_array(float* out) const;
};

// Dynamic array of Gsplats with optional SH and LOD data
class GsplatArray {
public:
    std::vector<Gsplat> splats;
    std::vector<GsplatSH1> sh1;
    std::vector<GsplatSH2> sh2;
    std::vector<GsplatSH3> sh3;
    int max_sh_degree = 0;

    // LOD tree
    std::vector<uint16_t> child_count;
    std::vector<uint32_t> child_start;

    GsplatArray() = default;
    GsplatArray(int capacity, int sh_degree);

    size_t len() const { return splats.size(); }
    bool empty() const { return splats.empty(); }

    void push_splat(const Gsplat& splat,
                    const GsplatSH1* sh1_val = nullptr,
                    const GsplatSH2* sh2_val = nullptr,
                    const GsplatSH3* sh3_val = nullptr);

    void reserve(size_t count);
    void resize(size_t count);
    void prepare_children();
};

} // namespace spark
