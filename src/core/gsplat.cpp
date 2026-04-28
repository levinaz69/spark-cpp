#include "gsplat.h"
#include <cmath>
#include <algorithm>

namespace spark {

Gsplat Gsplat::create(glm::vec3 center, float opacity,
                      glm::vec3 rgb, glm::vec3 scales, glm::quat quat) {
    Gsplat g;
    g.center = center;
    g.opacity_h = float_to_half(opacity);
    g.rgb_h[0] = float_to_half(rgb.x);
    g.rgb_h[1] = float_to_half(rgb.y);
    g.rgb_h[2] = float_to_half(rgb.z);
    g.ln_scales_h[0] = float_to_half(std::log(scales.x));
    g.ln_scales_h[1] = float_to_half(std::log(scales.y));
    g.ln_scales_h[2] = float_to_half(std::log(scales.z));
    g.quaternion_h[0] = float_to_half(quat.x);
    g.quaternion_h[1] = float_to_half(quat.y);
    g.quaternion_h[2] = float_to_half(quat.z);
    g.quaternion_h[3] = float_to_half(quat.w);
    return g;
}

glm::vec3 Gsplat::rgb() const {
    return glm::vec3(half_to_float(rgb_h[0]),
                     half_to_float(rgb_h[1]),
                     half_to_float(rgb_h[2]));
}

glm::vec3 Gsplat::scales() const {
    return glm::vec3(std::exp(half_to_float(ln_scales_h[0])),
                     std::exp(half_to_float(ln_scales_h[1])),
                     std::exp(half_to_float(ln_scales_h[2])));
}

glm::quat Gsplat::quaternion() const {
    return glm::quat(half_to_float(quaternion_h[3]),
                     half_to_float(quaternion_h[0]),
                     half_to_float(quaternion_h[1]),
                     half_to_float(quaternion_h[2]));
}

float Gsplat::max_scale() const {
    float s0 = half_to_float(ln_scales_h[0]);
    float s1 = half_to_float(ln_scales_h[1]);
    float s2 = half_to_float(ln_scales_h[2]);
    return std::exp(std::max({s0, s1, s2}));
}

float Gsplat::feature_size() const {
    return max_scale() * opacity();
}

void Gsplat::set_rgb(glm::vec3 c) {
    rgb_h[0] = float_to_half(c.x);
    rgb_h[1] = float_to_half(c.y);
    rgb_h[2] = float_to_half(c.z);
}

void Gsplat::set_scales(glm::vec3 s) {
    ln_scales_h[0] = float_to_half(std::log(s.x));
    ln_scales_h[1] = float_to_half(std::log(s.y));
    ln_scales_h[2] = float_to_half(std::log(s.z));
}

void Gsplat::set_quaternion(glm::quat q) {
    quaternion_h[0] = float_to_half(q.x);
    quaternion_h[1] = float_to_half(q.y);
    quaternion_h[2] = float_to_half(q.z);
    quaternion_h[3] = float_to_half(q.w);
}

// SH1
void GsplatSH1::set_from_array(const float* vals) {
    for (int k = 0; k < 3; k++)
        for (int d = 0; d < 3; d++)
            data[k][d] = float_to_half(vals[k * 3 + d]);
}

void GsplatSH1::to_array(float* out) const {
    for (int k = 0; k < 3; k++)
        for (int d = 0; d < 3; d++)
            out[k * 3 + d] = half_to_float(data[k][d]);
}

// SH2
void GsplatSH2::set_from_array(const float* vals) {
    for (int k = 0; k < 5; k++)
        for (int d = 0; d < 3; d++)
            data[k][d] = float_to_half(vals[k * 3 + d]);
}

void GsplatSH2::to_array(float* out) const {
    for (int k = 0; k < 5; k++)
        for (int d = 0; d < 3; d++)
            out[k * 3 + d] = half_to_float(data[k][d]);
}

// SH3
void GsplatSH3::set_from_array(const float* vals) {
    for (int k = 0; k < 7; k++)
        for (int d = 0; d < 3; d++)
            data[k][d] = float_to_half(vals[k * 3 + d]);
}

void GsplatSH3::to_array(float* out) const {
    for (int k = 0; k < 7; k++)
        for (int d = 0; d < 3; d++)
            out[k * 3 + d] = half_to_float(data[k][d]);
}

// GsplatArray
GsplatArray::GsplatArray(int capacity, int sh_degree)
    : max_sh_degree(sh_degree) {
    splats.reserve(capacity);
    if (sh_degree >= 1) sh1.reserve(capacity);
    if (sh_degree >= 2) sh2.reserve(capacity);
    if (sh_degree >= 3) sh3.reserve(capacity);
}

void GsplatArray::push_splat(const Gsplat& splat,
                              const GsplatSH1* sh1_val,
                              const GsplatSH2* sh2_val,
                              const GsplatSH3* sh3_val) {
    splats.push_back(splat);
    if (max_sh_degree >= 1) sh1.push_back(sh1_val ? *sh1_val : GsplatSH1{});
    if (max_sh_degree >= 2) sh2.push_back(sh2_val ? *sh2_val : GsplatSH2{});
    if (max_sh_degree >= 3) sh3.push_back(sh3_val ? *sh3_val : GsplatSH3{});
}

void GsplatArray::reserve(size_t count) {
    splats.reserve(count);
    if (max_sh_degree >= 1) sh1.reserve(count);
    if (max_sh_degree >= 2) sh2.reserve(count);
    if (max_sh_degree >= 3) sh3.reserve(count);
}

void GsplatArray::resize(size_t count) {
    splats.resize(count);
    if (max_sh_degree >= 1) sh1.resize(count);
    if (max_sh_degree >= 2) sh2.resize(count);
    if (max_sh_degree >= 3) sh3.resize(count);
}

void GsplatArray::prepare_children() {
    child_count.resize(splats.size(), 0);
    child_start.resize(splats.size(), 0);
}

void GsplatArray::ensure_sh(int degree) {
    if (degree >= 1 && sh1.size() < splats.size()) {
        sh1.resize(splats.size());
        max_sh_degree = std::max(max_sh_degree, 1);
    }
    if (degree >= 2 && sh2.size() < splats.size()) {
        sh2.resize(splats.size());
        max_sh_degree = std::max(max_sh_degree, 2);
    }
    if (degree >= 3 && sh3.size() < splats.size()) {
        sh3.resize(splats.size());
        max_sh_degree = std::max(max_sh_degree, 3);
    }
}

} // namespace spark
