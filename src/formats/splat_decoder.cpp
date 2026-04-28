#include "splat_decoder.h"
#include <cstring>
#include <cmath>
#include <algorithm>

namespace spark {

SplatFileData decode_splat_file(const uint8_t* data, size_t len,
                                const SplatEncoding& encoding) {
    SplatFileData result;
    result.encoding = encoding;

    if (len % ANTISPLAT_BYTES_PER_SPLAT != 0) {
        return result; // Invalid file
    }

    result.num_splats = len / ANTISPLAT_BYTES_PER_SPLAT;
    result.packed_array.resize(result.num_splats * 4);

    for (size_t i = 0; i < result.num_splats; i++) {
        const uint8_t* src = data + i * ANTISPLAT_BYTES_PER_SPLAT;

        // Read center (3 x float32)
        float cx, cy, cz;
        std::memcpy(&cx, src + 0, 4);
        std::memcpy(&cy, src + 4, 4);
        std::memcpy(&cz, src + 8, 4);

        // Read scale (3 x float32)
        float sx, sy, sz;
        std::memcpy(&sx, src + 12, 4);
        std::memcpy(&sy, src + 16, 4);
        std::memcpy(&sz, src + 20, 4);

        // Read RGBA (4 x uint8)
        uint8_t r = src[24], g = src[25], b = src[26], a = src[27];

        // Read quaternion (4 x uint8, normalized from 0..255 to -1..1)
        float qw = (static_cast<float>(src[28]) / 128.0f) - 1.0f;
        float qx = (static_cast<float>(src[29]) / 128.0f) - 1.0f;
        float qy = (static_cast<float>(src[30]) / 128.0f) - 1.0f;
        float qz = (static_cast<float>(src[31]) / 128.0f) - 1.0f;

        // Normalize quaternion
        float qlen = std::sqrt(qw*qw + qx*qx + qy*qy + qz*qz);
        if (qlen > 0.0f) { qw /= qlen; qx /= qlen; qy /= qlen; qz /= qlen; }

        // Convert to our format
        glm::vec3 center(cx, cy, cz);
        float opacity = static_cast<float>(a) / 255.0f;
        glm::vec3 rgb(r / 255.0f, g / 255.0f, b / 255.0f);
        glm::vec3 scale(std::exp(sx), std::exp(sy), std::exp(sz));
        glm::quat quat(qw, qx, qy, qz);

        encode_packed_splat(&result.packed_array[i * 4],
                           center, opacity, rgb, scale, quat, encoding);
    }

    return result;
}

void SplatFormatDecoder::push(const uint8_t* data, size_t len) {
    buffer_.insert(buffer_.end(), data, data + len);
}

void SplatFormatDecoder::finish() {
    auto result = decode_splat_file(buffer_.data(), buffer_.size(), encoding_);
    packed_array_ = std::move(result.packed_array);
    num_splats_ = result.num_splats;
}

} // namespace spark
