#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "defines.h"
#include "half_float.h"

namespace spark {

// Encode a float to uint8 in range [min_val, max_val]
inline uint8_t float_to_u8(float x, float min_val, float max_val) {
    float t = (x - min_val) / (max_val - min_val);
    return static_cast<uint8_t>(std::clamp(t * 255.0f, 0.0f, 255.0f));
}

// Decode uint8 to float in range [min_val, max_val]
inline float u8_to_float(uint8_t x, float min_val, float max_val) {
    return min_val + (static_cast<float>(x) / 255.0f) * (max_val - min_val);
}

// Encode scale to uint8
inline uint8_t encode_scale8(float scale, float ln_min, float ln_max) {
    if (scale <= 0.0f) return 0;
    float ln_s = std::log(scale);
    float t = (ln_s - ln_min) / (ln_max - ln_min);
    int v = static_cast<int>(std::round(t * 254.0f)) + 1;
    return static_cast<uint8_t>(std::clamp(v, 0, 255));
}

// Decode uint8 to scale
inline float decode_scale8(uint8_t v, float ln_min, float ln_max) {
    if (v == 0) return 0.0f;
    float t = static_cast<float>(v - 1) / 254.0f;
    return std::exp(ln_min + t * (ln_max - ln_min));
}

// Folded octahedral quaternion encoding: quat -> 24 bits (xy8,8 + angle8)
inline uint32_t encode_quat_oct888(const float q_xyzw[4]) {
    float qx = q_xyzw[0], qy = q_xyzw[1], qz = q_xyzw[2], qw = q_xyzw[3];
    // Ensure minimal representation
    if (qw < 0.0f) { qx = -qx; qy = -qy; qz = -qz; qw = -qw; }

    float theta = 2.0f * std::acos(std::clamp(qw, -1.0f, 1.0f));
    float half_theta = theta * 0.5f;
    float s = std::sin(half_theta);

    float ax, ay, az;
    if (std::abs(s) < 1e-6f) {
        ax = 1.0f; ay = 0.0f; az = 0.0f;
    } else {
        float inv_s = 1.0f / s;
        ax = qx * inv_s; ay = qy * inv_s; az = qz * inv_s;
    }

    // Folded octahedral mapping
    float sum = std::abs(ax) + std::abs(ay) + std::abs(az);
    if (sum < 1e-10f) sum = 1e-10f;
    float px = ax / sum, py = ay / sum;
    if (az < 0.0f) {
        float old_px = px;
        px = (1.0f - std::abs(py)) * (px >= 0.0f ? 1.0f : -1.0f);
        py = (1.0f - std::abs(old_px)) * (py >= 0.0f ? 1.0f : -1.0f);
    }

    float uf = px * 0.5f + 0.5f;
    float vf = py * 0.5f + 0.5f;
    uint32_t qu = static_cast<uint32_t>(std::clamp(std::round(uf * 255.0f), 0.0f, 255.0f));
    uint32_t qv = static_cast<uint32_t>(std::clamp(std::round(vf * 255.0f), 0.0f, 255.0f));
    uint32_t angle_int = static_cast<uint32_t>(std::clamp(std::round((theta / 3.14159265359f) * 255.0f), 0.0f, 255.0f));

    return (angle_int << 16) | (qv << 8) | qu;
}

// Decode 24-bit octahedral quaternion
inline glm::quat decode_quat_oct888(uint32_t encoded) {
    uint32_t qu = encoded & 0xFF;
    uint32_t qv = (encoded >> 8) & 0xFF;
    uint32_t angle_int = (encoded >> 16) & 0xFF;

    float uf = static_cast<float>(qu) / 255.0f;
    float vf = static_cast<float>(qv) / 255.0f;
    float fx = uf * 2.0f - 1.0f;
    float fy = vf * 2.0f - 1.0f;

    float az = 1.0f - std::abs(fx) - std::abs(fy);
    float t = std::max(-az, 0.0f);
    float ax = fx + (fx >= 0.0f ? -t : t);
    float ay = fy + (fy >= 0.0f ? -t : t);

    float len = std::sqrt(ax * ax + ay * ay + az * az);
    if (len > 0.0f) { ax /= len; ay /= len; az /= len; }

    float theta = (static_cast<float>(angle_int) / 255.0f) * 3.14159265359f;
    float half_theta = theta * 0.5f;
    float s = std::sin(half_theta);
    float w = std::cos(half_theta);

    return glm::quat(w, ax * s, ay * s, az * s);
}

// Encode a full splat into 4 x uint32 packed format
inline void encode_packed_splat(uint32_t* packed,
                                const glm::vec3& center, float opacity,
                                const glm::vec3& rgb, const glm::vec3& scale,
                                const glm::quat& quat,
                                const SplatEncoding& enc) {
    uint8_t u_rgb[3] = {
        float_to_u8(rgb.x, enc.rgb_min, enc.rgb_max),
        float_to_u8(rgb.y, enc.rgb_min, enc.rgb_max),
        float_to_u8(rgb.z, enc.rgb_min, enc.rgb_max),
    };
    uint8_t u_a = float_to_u8(opacity, 0.0f, enc.lod_opacity ? 2.0f : 1.0f);

    uint16_t cx = float_to_half(center.x);
    uint16_t cy = float_to_half(center.y);
    uint16_t cz = float_to_half(center.z);

    float q_xyzw[4] = {quat.x, quat.y, quat.z, quat.w};
    uint32_t quat_enc = encode_quat_oct888(q_xyzw);
    uint8_t quat_xy = quat_enc & 0xFF;
    uint8_t quat_v = (quat_enc >> 8) & 0xFF;
    uint8_t quat_a = (quat_enc >> 16) & 0xFF;

    uint8_t u_scale[3] = {
        encode_scale8(scale.x, enc.ln_scale_min, enc.ln_scale_max),
        encode_scale8(scale.y, enc.ln_scale_min, enc.ln_scale_max),
        encode_scale8(scale.z, enc.ln_scale_min, enc.ln_scale_max),
    };

    packed[0] = u_rgb[0] | (uint32_t(u_rgb[1]) << 8) | (uint32_t(u_rgb[2]) << 16) | (uint32_t(u_a) << 24);
    packed[1] = cx | (uint32_t(cy) << 16);
    packed[2] = cz | (uint32_t(quat_xy) << 16) | (uint32_t(quat_v) << 24);
    packed[3] = u_scale[0] | (uint32_t(u_scale[1]) << 8) | (uint32_t(u_scale[2]) << 16) | (uint32_t(quat_a) << 24);
}

// Decode packed splat center
inline glm::vec3 decode_packed_center(const uint32_t* packed) {
    return glm::vec3(
        half_to_float(packed[1] & 0xFFFF),
        half_to_float(packed[1] >> 16),
        half_to_float(packed[2] & 0xFFFF)
    );
}

// Decode packed splat opacity
inline float decode_packed_opacity(const uint32_t* packed, const SplatEncoding& enc) {
    uint8_t a = (packed[0] >> 24) & 0xFF;
    return u8_to_float(a, 0.0f, enc.lod_opacity ? 2.0f : 1.0f);
}

// Decode packed splat rgb
inline glm::vec3 decode_packed_rgb(const uint32_t* packed, const SplatEncoding& enc) {
    return glm::vec3(
        u8_to_float(packed[0] & 0xFF, enc.rgb_min, enc.rgb_max),
        u8_to_float((packed[0] >> 8) & 0xFF, enc.rgb_min, enc.rgb_max),
        u8_to_float((packed[0] >> 16) & 0xFF, enc.rgb_min, enc.rgb_max)
    );
}

// Decode packed splat scale
inline glm::vec3 decode_packed_scale(const uint32_t* packed, const SplatEncoding& enc) {
    return glm::vec3(
        decode_scale8(packed[3] & 0xFF, enc.ln_scale_min, enc.ln_scale_max),
        decode_scale8((packed[3] >> 8) & 0xFF, enc.ln_scale_min, enc.ln_scale_max),
        decode_scale8((packed[3] >> 16) & 0xFF, enc.ln_scale_min, enc.ln_scale_max)
    );
}

// Decode packed splat quaternion
inline glm::quat decode_packed_quat(const uint32_t* packed) {
    uint32_t quat_enc = ((packed[2] >> 16) & 0xFF)
                      | (((packed[2] >> 24) & 0xFF) << 8)
                      | (((packed[3] >> 24) & 0xFF) << 16);
    return decode_quat_oct888(quat_enc);
}

} // namespace spark
