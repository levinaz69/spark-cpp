#include "ksplat_decoder.h"
#include "core/half_float.h"
#include "core/splat_encoding.h"
#include <cstring>
#include <cmath>
#include <iostream>

namespace spark {

static constexpr size_t HEADER_BYTES = 4096;
static constexpr size_t SECTION_BYTES = 1024;

const KsplatDecoder::Compression KsplatDecoder::COMPRESSIONS[3] = {
    {12, 12, 16, 4, 4, 12, 24, 40, 44, 1},       // level 0: f32
    {6,  6,  8,  4, 2, 6,  12, 20, 24, 32767},    // level 1: f16
    {6,  6,  8,  4, 1, 6,  12, 20, 24, 32767},    // level 2: f16/u8
};

const int KsplatDecoder::SH_COMPONENTS[4] = {0, 9, 24, 45};

static uint16_t read_u16_le(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

static uint32_t read_u32_le(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

static float read_f32_le(const uint8_t* p) {
    float v;
    std::memcpy(&v, p, 4);
    return v;
}

static int16_t read_i16_le(const uint8_t* p) {
    return static_cast<int16_t>(read_u16_le(p));
}

void KsplatDecoder::push(const uint8_t* data, size_t len) {
    buffer_.insert(buffer_.end(), data, data + len);
}

void KsplatDecoder::finish() {
    if (buffer_.size() < HEADER_BYTES) {
        std::cerr << "KsplatDecoder: file too small" << std::endl;
        return;
    }

    uint8_t ver_major = buffer_[0];
    uint8_t ver_minor = buffer_[1];
    if (ver_major != 0 || ver_minor < 1) {
        std::cerr << "KsplatDecoder: unsupported version " << (int)ver_major << "." << (int)ver_minor << std::endl;
        return;
    }

    uint32_t max_section_count = read_u32_le(&buffer_[4]);
    uint32_t total_splats = read_u32_le(&buffer_[16]);
    uint16_t compression_level = read_u16_le(&buffer_[20]);
    if (compression_level > 2) {
        std::cerr << "KsplatDecoder: invalid compression level " << compression_level << std::endl;
        return;
    }

    const auto& comp = COMPRESSIONS[compression_level];
    float min_sh = read_f32_le(&buffer_[36]);
    float max_sh = read_f32_le(&buffer_[40]);
    if (min_sh == 0.0f) min_sh = -1.5f;
    if (max_sh == 0.0f) max_sh = 1.5f;

    num_splats_ = total_splats;
    packed_.resize(num_splats_ * 4, 0);

    // Decode sections
    size_t header_offset = HEADER_BYTES;
    size_t section_base = HEADER_BYTES + max_section_count * SECTION_BYTES;
    size_t total_decoded = 0;

    for (uint32_t sec = 0; sec < max_section_count; sec++) {
        if (header_offset + SECTION_BYTES > buffer_.size()) break;

        uint32_t section_splat_count = read_u32_le(&buffer_[header_offset]);
        uint32_t section_max_splat_count = read_u32_le(&buffer_[header_offset + 4]);
        uint32_t bucket_count = read_u32_le(&buffer_[header_offset + 12]);
        float bucket_block_size = read_f32_le(&buffer_[header_offset + 16]);
        uint16_t bucket_storage_size = read_u16_le(&buffer_[header_offset + 20]);
        uint32_t comp_scale_range_raw = read_u32_le(&buffer_[header_offset + 24]);
        float compression_scale_range = (comp_scale_range_raw == 0) ?
            static_cast<float>(comp.scale_range) : static_cast<float>(comp_scale_range_raw);
        uint32_t partially_filled = read_u32_le(&buffer_[header_offset + 36]);
        uint16_t sh_degree = read_u16_le(&buffer_[header_offset + 40]);
        int sh_comps = (sh_degree < 4) ? SH_COMPONENTS[sh_degree] : 0;

        size_t buckets_storage = bucket_storage_size * bucket_count + partially_filled * 4;
        int bytes_per_splat = comp.bytes_per_center + comp.bytes_per_scale
                            + comp.bytes_per_rotation + comp.bytes_per_color
                            + sh_comps * comp.bytes_per_sh_component;

        // Read bucket centers for position reconstruction
        std::vector<float> bucket_centers;
        if (bucket_count > 0 && buckets_storage > 0 && bucket_storage_size >= 12) {
            size_t bucket_data_start = section_base + bytes_per_splat * section_max_splat_count;
            bucket_centers.resize(bucket_count * 3);
            for (size_t b = 0; b < bucket_count && bucket_data_start + (b + 1) * bucket_storage_size <= buffer_.size(); b++) {
                size_t bo = bucket_data_start + b * bucket_storage_size;
                bucket_centers[b * 3 + 0] = read_f32_le(&buffer_[bo]);
                bucket_centers[b * 3 + 1] = read_f32_le(&buffer_[bo + 4]);
                bucket_centers[b * 3 + 2] = read_f32_le(&buffer_[bo + 8]);
            }
        }

        // Decode splats in this section
        for (size_t i = 0; i < section_splat_count && total_decoded < num_splats_; i++, total_decoded++) {
            size_t splat_offset = section_base + i * bytes_per_splat;
            if (splat_offset + bytes_per_splat > buffer_.size()) break;

            const uint8_t* sp = &buffer_[splat_offset];
            glm::vec3 center, rgb_vec;
            glm::vec3 scale_vec;
            glm::quat quat;
            float opacity;

            // Center
            if (compression_level == 0) {
                center.x = read_f32_le(sp);
                center.y = read_f32_le(sp + 4);
                center.z = read_f32_le(sp + 8);
            } else {
                float cx = half_to_float(read_u16_le(sp));
                float cy = half_to_float(read_u16_le(sp + 2));
                float cz = half_to_float(read_u16_le(sp + 4));
                // Add bucket center offset
                size_t bucket_idx = i / std::max(1u, (uint32_t)(section_max_splat_count / std::max(1u, (uint32_t)bucket_count)));
                if (bucket_idx < bucket_count && !bucket_centers.empty()) {
                    cx += bucket_centers[bucket_idx * 3 + 0];
                    cy += bucket_centers[bucket_idx * 3 + 1];
                    cz += bucket_centers[bucket_idx * 3 + 2];
                }
                center = glm::vec3(cx, cy, cz);
            }

            // Scale
            const uint8_t* scale_p = sp + comp.scale_offset_bytes;
            if (compression_level == 0) {
                float sx = read_f32_le(scale_p);
                float sy = read_f32_le(scale_p + 4);
                float sz = read_f32_le(scale_p + 8);
                scale_vec = glm::vec3(std::exp(sx), std::exp(sy), std::exp(sz));
            } else {
                float sx = read_i16_le(scale_p) / compression_scale_range;
                float sy = read_i16_le(scale_p + 2) / compression_scale_range;
                float sz = read_i16_le(scale_p + 4) / compression_scale_range;
                scale_vec = glm::vec3(std::exp(sx), std::exp(sy), std::exp(sz));
            }

            // Rotation
            const uint8_t* rot_p = sp + comp.rotation_offset_bytes;
            if (compression_level == 0) {
                float qx = read_f32_le(rot_p);
                float qy = read_f32_le(rot_p + 4);
                float qz = read_f32_le(rot_p + 8);
                float qw = read_f32_le(rot_p + 12);
                quat = glm::quat(qw, qx, qy, qz);
            } else {
                float qx = read_i16_le(rot_p) / 32767.0f;
                float qy = read_i16_le(rot_p + 2) / 32767.0f;
                float qz = read_i16_le(rot_p + 4) / 32767.0f;
                float qw = read_i16_le(rot_p + 6) / 32767.0f;
                quat = glm::quat(qw, qx, qy, qz);
            }
            quat = glm::normalize(quat);

            // Color + opacity
            const uint8_t* color_p = sp + comp.color_offset_bytes;
            opacity = color_p[3] / 255.0f;
            rgb_vec = glm::vec3(color_p[0] / 255.0f, color_p[1] / 255.0f, color_p[2] / 255.0f);

            encode_packed_splat(&packed_[total_decoded * 4], center, opacity, rgb_vec, scale_vec, quat, encoding_);
        }

        size_t storage = bytes_per_splat * section_max_splat_count + buckets_storage;
        section_base += storage;
        header_offset += SECTION_BYTES;
    }

    num_splats_ = total_decoded;
    packed_.resize(num_splats_ * 4);
}

} // namespace spark
