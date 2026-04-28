#include "spz_decoder.h"
#include "core/half_float.h"
#include "core/splat_encoding.h"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <miniz.h>

namespace spark {

static constexpr uint32_t SPZ_MAGIC = 0x5053474e; // "NGSP"
static constexpr float SH_C0 = 0.28209479177387814f;

static uint16_t read_u16_le(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

static uint32_t read_u32_le(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

static int32_t read_i24_le(const uint8_t* p) {
    int32_t v = static_cast<int32_t>(p[0]) | (static_cast<int32_t>(p[1]) << 8)
              | (static_cast<int32_t>(p[2]) << 16);
    if (v & 0x800000) v |= 0xFF000000; // sign extend
    return v;
}

void SpzDecoder::push(const uint8_t* data, size_t len) {
    compressed_.insert(compressed_.end(), data, data + len);
}

void SpzDecoder::finish() {
    if (!decompress_gzip()) {
        std::cerr << "SpzDecoder: gzip decompression failed" << std::endl;
        return;
    }
    if (!parse_header()) return;
    decode_sections();
    encode_to_packed();
}

bool SpzDecoder::decompress_gzip() {
    if (compressed_.size() < 10) return false;

    // Skip gzip header
    if (compressed_[0] != 0x1f || compressed_[1] != 0x8b || compressed_[2] != 8)
        return false;

    uint8_t flags = compressed_[3];
    size_t pos = 10;
    if (flags & 0x04) { // FEXTRA
        if (pos + 2 > compressed_.size()) return false;
        uint16_t xlen = read_u16_le(&compressed_[pos]);
        pos += 2 + xlen;
    }
    if (flags & 0x08) { // FNAME
        while (pos < compressed_.size() && compressed_[pos] != 0) pos++;
        pos++;
    }
    if (flags & 0x10) { // FCOMMENT
        while (pos < compressed_.size() && compressed_[pos] != 0) pos++;
        pos++;
    }
    if (flags & 0x02) pos += 2; // FHCRC

    if (pos >= compressed_.size()) return false;

    // Decompress using miniz raw inflate
    mz_ulong src_len = static_cast<mz_ulong>(compressed_.size() - pos - 8); // exclude 8-byte gzip footer
    mz_ulong dest_len = static_cast<mz_ulong>(src_len * 8);
    decompressed_.resize(dest_len);

    int status = mz_uncompress2(decompressed_.data(), &dest_len,
                                &compressed_[pos], &src_len);
    if (status == MZ_BUF_ERROR) {
        dest_len = static_cast<mz_ulong>(src_len * 32);
        decompressed_.resize(dest_len);
        src_len = static_cast<mz_ulong>(compressed_.size() - pos - 8);
        status = mz_uncompress2(decompressed_.data(), &dest_len,
                                &compressed_[pos], &src_len);
    }

    // If mz_uncompress2 doesn't work with raw deflate, try raw inflate
    if (status != MZ_OK) {
        dest_len = static_cast<mz_ulong>(compressed_.size() * 32);
        decompressed_.resize(dest_len);
        mz_stream stream{};
        stream.next_in = &compressed_[pos];
        stream.avail_in = static_cast<unsigned>(compressed_.size() - pos - 8);
        stream.next_out = decompressed_.data();
        stream.avail_out = static_cast<unsigned>(dest_len);

        if (mz_inflateInit2(&stream, -MZ_DEFAULT_WINDOW_BITS) != MZ_OK)
            return false;

        status = mz_inflate(&stream, MZ_FINISH);
        dest_len = stream.total_out;
        mz_inflateEnd(&stream);

        if (status != MZ_STREAM_END && status != MZ_OK)
            return false;
    }

    decompressed_.resize(dest_len);
    return true;
}

bool SpzDecoder::parse_header() {
    if (decompressed_.size() < 16) return false;

    uint32_t magic = read_u32_le(&decompressed_[0]);
    if (magic != SPZ_MAGIC) {
        std::cerr << "SpzDecoder: invalid magic 0x" << std::hex << magic << std::endl;
        return false;
    }

    version_ = read_u32_le(&decompressed_[4]);
    if (version_ < 1 || version_ > 3) {
        std::cerr << "SpzDecoder: unsupported version " << version_ << std::endl;
        return false;
    }

    num_splats_ = read_u32_le(&decompressed_[8]);
    max_sh_degree_ = decompressed_[12];
    fractional_bits_ = decompressed_[13];
    flags_ = decompressed_[14];
    has_lod_ = (flags_ & 0x80) != 0;

    if (has_lod_) {
        encoding_.lod_opacity = true;
    }

    gsplats_.resize(num_splats_);
    return true;
}

void SpzDecoder::decode_sections() {
    const uint8_t* data = decompressed_.data();
    size_t offset = 16;
    size_t n = num_splats_;

    // Centers
    if (version_ == 1) {
        size_t bytes = n * 6;
        if (offset + bytes <= decompressed_.size()) {
            decode_centers_v1(data + offset, 0, n);
            offset += bytes;
        }
    } else {
        size_t bytes = n * 9;
        if (offset + bytes <= decompressed_.size()) {
            decode_centers_v2(data + offset, 0, n, fractional_bits_);
            offset += bytes;
        }
    }

    // Alphas (1 byte each)
    if (offset + n <= decompressed_.size()) {
        decode_alphas(data + offset, 0, n, has_lod_);
        offset += n;
    }

    // RGB (3 bytes each)
    if (offset + n * 3 <= decompressed_.size()) {
        decode_rgb(data + offset, 0, n);
        offset += n * 3;
    }

    // Scales (3 bytes each)
    if (offset + n * 3 <= decompressed_.size()) {
        decode_scales(data + offset, 0, n);
        offset += n * 3;
    }

    // Quaternions
    size_t quat_bytes = (version_ == 3) ? 4 : 3;
    if (offset + n * quat_bytes <= decompressed_.size()) {
        if (version_ == 3)
            decode_quats_v3(data + offset, 0, n);
        else
            decode_quats_v12(data + offset, 0, n);
        offset += n * quat_bytes;
    }

    // SH coefficients
    if (max_sh_degree_ > 0) {
        int sh_components = 3 * ((max_sh_degree_ == 1) ? 3 : (max_sh_degree_ == 2) ? 8 : 15);
        size_t sh_bytes = n * sh_components;
        if (offset + sh_bytes <= decompressed_.size()) {
            decode_sh(data + offset, 0, n, max_sh_degree_);
            offset += sh_bytes;
        }
    }

    // LOD extension
    if (has_lod_) {
        // child_count (2 bytes each)
        if (offset + n * 2 <= decompressed_.size()) {
            decode_child_counts(data + offset, 0, n);
            offset += n * 2;
        }
        // child_start (4 bytes each)
        if (offset + n * 4 <= decompressed_.size()) {
            decode_child_starts(data + offset, 0, n);
            offset += n * 4;
        }
    }
}

void SpzDecoder::decode_centers_v1(const uint8_t* data, size_t start, size_t count) {
    for (size_t i = 0; i < count; i++) {
        const uint8_t* p = data + i * 6;
        float x = half_to_float(read_u16_le(p));
        float y = half_to_float(read_u16_le(p + 2));
        float z = half_to_float(read_u16_le(p + 4));
        gsplats_.splats[start + i].center = glm::vec3(x, y, z);
    }
}

void SpzDecoder::decode_centers_v2(const uint8_t* data, size_t start, size_t count, uint8_t frac_bits) {
    float frac = static_cast<float>(1u << frac_bits);
    for (size_t i = 0; i < count; i++) {
        const uint8_t* p = data + i * 9;
        float x = read_i24_le(p) / frac;
        float y = read_i24_le(p + 3) / frac;
        float z = read_i24_le(p + 6) / frac;
        gsplats_.splats[start + i].center = glm::vec3(x, y, z);
    }
}

void SpzDecoder::decode_alphas(const uint8_t* data, size_t start, size_t count, bool lod) {
    float scale = lod ? 2.0f : 1.0f;
    for (size_t i = 0; i < count; i++) {
        float opacity = data[i] / 255.0f * scale;
        gsplats_.splats[start + i].opacity_h = float_to_half(opacity);
    }
}

void SpzDecoder::decode_rgb(const uint8_t* data, size_t start, size_t count) {
    float color_scale = SH_C0 / 0.15f;
    for (size_t i = 0; i < count; i++) {
        const uint8_t* p = data + i * 3;
        float r = (p[0] / 255.0f - 0.5f) * color_scale + 0.5f;
        float g = (p[1] / 255.0f - 0.5f) * color_scale + 0.5f;
        float b = (p[2] / 255.0f - 0.5f) * color_scale + 0.5f;
        gsplats_.splats[start + i].rgb_h[0] = float_to_half(r);
        gsplats_.splats[start + i].rgb_h[1] = float_to_half(g);
        gsplats_.splats[start + i].rgb_h[2] = float_to_half(b);
    }
}

void SpzDecoder::decode_scales(const uint8_t* data, size_t start, size_t count) {
    for (size_t i = 0; i < count; i++) {
        const uint8_t* p = data + i * 3;
        float sx = std::exp(p[0] / 16.0f - 10.0f);
        float sy = std::exp(p[1] / 16.0f - 10.0f);
        float sz = std::exp(p[2] / 16.0f - 10.0f);
        gsplats_.splats[start + i].ln_scales_h[0] = float_to_half(std::log(sx));
        gsplats_.splats[start + i].ln_scales_h[1] = float_to_half(std::log(sy));
        gsplats_.splats[start + i].ln_scales_h[2] = float_to_half(std::log(sz));
    }
}

void SpzDecoder::decode_quats_v12(const uint8_t* data, size_t start, size_t count) {
    for (size_t i = 0; i < count; i++) {
        const uint8_t* p = data + i * 3;
        float qx = p[0] / 127.5f - 1.0f;
        float qy = p[1] / 127.5f - 1.0f;
        float qz = p[2] / 127.5f - 1.0f;
        float qw = std::sqrt(std::max(0.0f, 1.0f - (qx*qx + qy*qy + qz*qz)));
        gsplats_.splats[start + i].quaternion_h[0] = float_to_half(qx);
        gsplats_.splats[start + i].quaternion_h[1] = float_to_half(qy);
        gsplats_.splats[start + i].quaternion_h[2] = float_to_half(qz);
        gsplats_.splats[start + i].quaternion_h[3] = float_to_half(qw);
    }
}

void SpzDecoder::decode_quats_v3(const uint8_t* data, size_t start, size_t count) {
    for (size_t i = 0; i < count; i++) {
        uint32_t comp = read_u32_le(data + i * 4);
        uint32_t largest_idx = comp >> 30;
        uint32_t remaining = comp;
        constexpr uint32_t value_mask = (1u << 9) - 1;
        constexpr float max_val = 0.7071067811865476f; // 1/sqrt(2)

        float q[4] = {0, 0, 0, 0};
        float sum_sq = 0.0f;
        for (int j = 3; j >= 0; j--) {
            if (static_cast<uint32_t>(j) != largest_idx) {
                float value = static_cast<float>(remaining & value_mask);
                bool sign = ((remaining >> 9) & 1) != 0;
                remaining >>= 10;
                float v = max_val * (value / static_cast<float>(value_mask));
                if (sign) v = -v;
                q[j] = v;
                sum_sq += v * v;
            }
        }
        float sq = 1.0f - sum_sq;
        q[largest_idx] = sq > 0.0f ? std::sqrt(sq) : 0.0f;

        gsplats_.splats[start + i].quaternion_h[0] = float_to_half(q[0]);
        gsplats_.splats[start + i].quaternion_h[1] = float_to_half(q[1]);
        gsplats_.splats[start + i].quaternion_h[2] = float_to_half(q[2]);
        gsplats_.splats[start + i].quaternion_h[3] = float_to_half(q[3]);
    }
}

void SpzDecoder::decode_sh(const uint8_t* data, size_t start, size_t count, int sh_deg) {
    int sh_components = 3 * ((sh_deg == 1) ? 3 : (sh_deg == 2) ? 8 : 15);
    gsplats_.ensure_sh(sh_deg);

    for (size_t i = 0; i < count; i++) {
        const uint8_t* p = data + i * sh_components;
        // SH1: 3x3 coefficients
        if (sh_deg >= 1) {
            for (int d = 0; d < 3; d++) {
                for (int k = 0; k < 3; k++) {
                    float v = (p[k * 3 + d] - 128.0f) / 128.0f;
                    gsplats_.sh1[start + i].data[k][d] = float_to_half(v);
                }
            }
        }
        // SH2: 5x3 coefficients
        if (sh_deg >= 2) {
            for (int d = 0; d < 3; d++) {
                for (int k = 0; k < 5; k++) {
                    float v = (p[9 + k * 3 + d] - 128.0f) / 128.0f;
                    gsplats_.sh2[start + i].data[k][d] = float_to_half(v);
                }
            }
        }
        // SH3: 7x3 coefficients
        if (sh_deg >= 3) {
            for (int d = 0; d < 3; d++) {
                for (int k = 0; k < 7; k++) {
                    float v = (p[24 + k * 3 + d] - 128.0f) / 128.0f;
                    gsplats_.sh3[start + i].data[k][d] = float_to_half(v);
                }
            }
        }
    }
}

void SpzDecoder::decode_child_counts(const uint8_t* data, size_t start, size_t count) {
    gsplats_.child_count.resize(num_splats_);
    for (size_t i = 0; i < count; i++) {
        gsplats_.child_count[start + i] = read_u16_le(data + i * 2);
    }
}

void SpzDecoder::decode_child_starts(const uint8_t* data, size_t start, size_t count) {
    gsplats_.child_start.resize(num_splats_);
    for (size_t i = 0; i < count; i++) {
        gsplats_.child_start[start + i] = read_u32_le(data + i * 4);
    }
}

void SpzDecoder::encode_to_packed() {
    packed_.resize(num_splats_ * 4);
    for (size_t i = 0; i < num_splats_; i++) {
        const auto& s = gsplats_.splats[i];
        glm::vec3 center = s.center;
        float opacity = half_to_float(s.opacity_h);
        glm::vec3 rgb(half_to_float(s.rgb_h[0]), half_to_float(s.rgb_h[1]), half_to_float(s.rgb_h[2]));
        glm::vec3 scale(std::exp(half_to_float(s.ln_scales_h[0])),
                        std::exp(half_to_float(s.ln_scales_h[1])),
                        std::exp(half_to_float(s.ln_scales_h[2])));
        glm::quat quat(half_to_float(s.quaternion_h[3]),
                        half_to_float(s.quaternion_h[0]),
                        half_to_float(s.quaternion_h[1]),
                        half_to_float(s.quaternion_h[2]));
        encode_packed_splat(&packed_[i * 4], center, opacity, rgb, scale, quat, encoding_);
    }
}

} // namespace spark
