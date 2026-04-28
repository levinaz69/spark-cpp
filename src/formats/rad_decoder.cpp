#include "rad_decoder.h"
#include "core/half_float.h"
#include "core/splat_encoding.h"
#include <cstring>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <miniz.h>

// Minimal JSON parsing for RAD metadata (avoid nlohmann dependency)
#include <sstream>

namespace spark {

static uint32_t read_u32_le(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

static uint64_t read_u64_le(const uint8_t* p) {
    return static_cast<uint64_t>(read_u32_le(p)) | (static_cast<uint64_t>(read_u32_le(p + 4)) << 32);
}

static size_t roundup8(size_t s) { return (s + 7) & ~size_t(7); }

void RadDecoder::push(const uint8_t* data, size_t len) {
    buffer_.insert(buffer_.end(), data, data + len);
}

void RadDecoder::finish() {
    if (buffer_.size() < 8) {
        std::cerr << "RadDecoder: file too small" << std::endl;
        return;
    }

    uint32_t magic = read_u32_le(&buffer_[0]);
    if (magic == RAD_MAGIC) {
        parse_rad_meta();
    } else if (magic == RAD_CHUNK_MAGIC) {
        parse_single_chunk();
    } else {
        std::cerr << "RadDecoder: unknown magic 0x" << std::hex << magic << std::endl;
    }
}

std::vector<uint8_t> RadDecoder::decompress_gz(const uint8_t* data, size_t len) {
    mz_ulong dest_len = static_cast<mz_ulong>(len * 8);
    std::vector<uint8_t> result(dest_len);

    // Try raw inflate
    mz_stream stream{};
    stream.next_in = data;
    stream.avail_in = static_cast<unsigned>(len);
    stream.next_out = result.data();
    stream.avail_out = static_cast<unsigned>(dest_len);

    if (mz_inflateInit2(&stream, -MZ_DEFAULT_WINDOW_BITS) != MZ_OK) {
        // Try with zlib header
        stream = {};
        stream.next_in = data;
        stream.avail_in = static_cast<unsigned>(len);
        stream.next_out = result.data();
        stream.avail_out = static_cast<unsigned>(dest_len);
        if (mz_inflateInit(&stream) != MZ_OK) return {};
    }

    int status = mz_inflate(&stream, MZ_FINISH);
    if (status == MZ_BUF_ERROR) {
        mz_inflateEnd(&stream);
        dest_len *= 4;
        result.resize(dest_len);
        stream = {};
        stream.next_in = data;
        stream.avail_in = static_cast<unsigned>(len);
        stream.next_out = result.data();
        stream.avail_out = static_cast<unsigned>(dest_len);
        mz_inflateInit2(&stream, -MZ_DEFAULT_WINDOW_BITS);
        status = mz_inflate(&stream, MZ_FINISH);
    }

    result.resize(stream.total_out);
    mz_inflateEnd(&stream);
    return result;
}

// Simplified JSON field extraction helpers
static std::string json_string(const std::string& json, const std::string& key) {
    auto pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos);
    if (pos == std::string::npos) return "";
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return "";
    auto end = json.find('"', pos + 1);
    if (end == std::string::npos) return "";
    return json.substr(pos + 1, end - pos - 1);
}

static int64_t json_int(const std::string& json, const std::string& key, int64_t def = 0) {
    auto pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return def;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return def;
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    try { return std::stoll(json.substr(pos)); } catch (...) { return def; }
}

static float json_float(const std::string& json, const std::string& key, float def = 0.0f) {
    auto pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return def;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return def;
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    try { return std::stof(json.substr(pos)); } catch (...) { return def; }
}

static bool json_bool(const std::string& json, const std::string& key, bool def = false) {
    auto pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return def;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return def;
    pos++;
    while (pos < json.size() && (json[pos] == ' ')) pos++;
    return json.substr(pos, 4) == "true";
}

static RadDecoder::PropEncoding parse_encoding(const std::string& s) {
    if (s == "f32") return RadDecoder::PropEncoding::F32;
    if (s == "f16") return RadDecoder::PropEncoding::F16;
    if (s == "f32_lebytes") return RadDecoder::PropEncoding::F32_LeBytes;
    if (s == "f16_lebytes") return RadDecoder::PropEncoding::F16_LeBytes;
    if (s == "r8") return RadDecoder::PropEncoding::R8;
    if (s == "r8_delta") return RadDecoder::PropEncoding::R8_Delta;
    if (s == "s8") return RadDecoder::PropEncoding::S8;
    if (s == "s8_delta") return RadDecoder::PropEncoding::S8_Delta;
    if (s == "ln_0r8") return RadDecoder::PropEncoding::Ln0R8;
    if (s == "ln_f16") return RadDecoder::PropEncoding::LnF16;
    if (s == "oct88r8") return RadDecoder::PropEncoding::Oct88R8;
    if (s == "u16") return RadDecoder::PropEncoding::U16;
    if (s == "u32") return RadDecoder::PropEncoding::U32;
    return RadDecoder::PropEncoding::F32;
}

static RadDecoder::PropName parse_prop_name(const std::string& s) {
    if (s == "center") return RadDecoder::PropName::Center;
    if (s == "alpha") return RadDecoder::PropName::Alpha;
    if (s == "rgb") return RadDecoder::PropName::Rgb;
    if (s == "scales") return RadDecoder::PropName::Scales;
    if (s == "orientation") return RadDecoder::PropName::Orientation;
    if (s == "sh1") return RadDecoder::PropName::Sh1;
    if (s == "sh2") return RadDecoder::PropName::Sh2;
    if (s == "sh3") return RadDecoder::PropName::Sh3;
    if (s == "child_count") return RadDecoder::PropName::ChildCount;
    if (s == "child_start") return RadDecoder::PropName::ChildStart;
    return RadDecoder::PropName::Center;
}

static int prop_dims(RadDecoder::PropName name) {
    switch (name) {
        case RadDecoder::PropName::Center: return 3;
        case RadDecoder::PropName::Alpha: return 1;
        case RadDecoder::PropName::Rgb: return 3;
        case RadDecoder::PropName::Scales: return 3;
        case RadDecoder::PropName::Orientation: return 4;
        case RadDecoder::PropName::Sh1: return 9;
        case RadDecoder::PropName::Sh2: return 15;
        case RadDecoder::PropName::Sh3: return 21;
        case RadDecoder::PropName::ChildCount: return 1;
        case RadDecoder::PropName::ChildStart: return 1;
        default: return 1;
    }
}

std::vector<float> RadDecoder::decode_property_f32(const uint8_t* data, size_t bytes,
                                                     PropEncoding enc, int dims, size_t count,
                                                     float min_val, float max_val) {
    std::vector<float> result(dims * count, 0.0f);

    switch (enc) {
        case PropEncoding::F32: {
            for (size_t i = 0; i < count; i++) {
                for (int d = 0; d < dims; d++) {
                    size_t idx = (count * d + i) * 4;
                    if (idx + 4 <= bytes) {
                        float v;
                        std::memcpy(&v, data + idx, 4);
                        result[i * dims + d] = v;
                    }
                }
            }
            break;
        }
        case PropEncoding::F16: {
            for (size_t i = 0; i < count; i++) {
                for (int d = 0; d < dims; d++) {
                    size_t idx = (count * d + i) * 2;
                    if (idx + 2 <= bytes) {
                        uint16_t h = static_cast<uint16_t>(data[idx]) | (static_cast<uint16_t>(data[idx+1]) << 8);
                        result[i * dims + d] = half_to_float(h);
                    }
                }
            }
            break;
        }
        case PropEncoding::R8: {
            float range = max_val - min_val;
            for (size_t i = 0; i < count; i++) {
                for (int d = 0; d < dims; d++) {
                    size_t idx = count * d + i;
                    if (idx < bytes) {
                        result[i * dims + d] = data[idx] / 255.0f * range + min_val;
                    }
                }
            }
            break;
        }
        case PropEncoding::Ln0R8: {
            for (size_t i = 0; i < count; i++) {
                for (int d = 0; d < dims; d++) {
                    size_t idx = count * d + i;
                    if (idx < bytes) {
                        float ln_val = data[idx] / 255.0f * (LN_SCALE_MAX - LN_SCALE_MIN) + LN_SCALE_MIN;
                        result[i * dims + d] = std::exp(ln_val);
                    }
                }
            }
            break;
        }
        case PropEncoding::LnF16: {
            for (size_t i = 0; i < count; i++) {
                for (int d = 0; d < dims; d++) {
                    size_t idx = (count * d + i) * 2;
                    if (idx + 2 <= bytes) {
                        uint16_t h = static_cast<uint16_t>(data[idx]) | (static_cast<uint16_t>(data[idx+1]) << 8);
                        result[i * dims + d] = std::exp(half_to_float(h));
                    }
                }
            }
            break;
        }
        case PropEncoding::Oct88R8: {
            // Octahedral encoded quaternion - return xyzw
            for (size_t i = 0; i < count; i++) {
                size_t idx = i * 3;
                if (idx + 3 <= bytes) {
                    uint32_t encoded = data[idx] | (data[idx+1] << 8) | (data[idx+2] << 16);
                    glm::quat q = decode_quat_oct888(encoded);
                    result[i * 4 + 0] = q.x;
                    result[i * 4 + 1] = q.y;
                    result[i * 4 + 2] = q.z;
                    result[i * 4 + 3] = q.w;
                }
            }
            break;
        }
        default:
            // F32_LeBytes, F16_LeBytes, S8, S8_Delta, R8_Delta
            // Simplified fallback: treat as raw bytes
            for (size_t i = 0; i < count * dims && i * 4 + 4 <= bytes; i++) {
                float v;
                std::memcpy(&v, data + i * 4, 4);
                result[i] = v;
            }
            break;
    }
    return result;
}

bool RadDecoder::parse_rad_meta() {
    if (buffer_.size() < 8) return false;
    uint32_t meta_len = read_u32_le(&buffer_[4]);
    size_t meta_end = 8 + roundup8(meta_len);
    if (buffer_.size() < meta_end) return false;

    std::string meta_json(buffer_.begin() + 8, buffer_.begin() + 8 + meta_len);
    num_splats_ = static_cast<size_t>(json_int(meta_json, "count"));
    max_sh_degree_ = static_cast<int>(json_int(meta_json, "maxSh"));
    has_lod_ = json_bool(meta_json, "lodTree");

    if (has_lod_) encoding_.lod_opacity = true;

    gsplats_.resize(num_splats_);
    packed_.resize(num_splats_ * 4, 0);

    // Parse chunks
    size_t offset = meta_end;
    while (offset + 8 <= buffer_.size()) {
        uint32_t chunk_magic = read_u32_le(&buffer_[offset]);
        if (chunk_magic != RAD_CHUNK_MAGIC) break;

        uint32_t chunk_meta_len = read_u32_le(&buffer_[offset + 4]);
        size_t chunk_meta_end = offset + 8 + roundup8(chunk_meta_len);
        if (chunk_meta_end + 8 > buffer_.size()) break;

        std::string chunk_json(buffer_.begin() + offset + 8,
                               buffer_.begin() + offset + 8 + chunk_meta_len);

        size_t base = static_cast<size_t>(json_int(chunk_json, "base"));
        size_t count = static_cast<size_t>(json_int(chunk_json, "count"));
        uint64_t payload_bytes = static_cast<uint64_t>(json_int(chunk_json, "payloadBytes"));

        size_t payload_start = chunk_meta_end + 8; // skip payload_bytes field
        size_t chunk_end = payload_start + static_cast<size_t>(payload_bytes);

        // Parse properties from chunk JSON
        std::vector<ChunkProperty> props;
        size_t prop_pos = chunk_json.find("\"properties\"");
        if (prop_pos != std::string::npos) {
            size_t arr_start = chunk_json.find('[', prop_pos);
            if (arr_start != std::string::npos) {
                size_t obj_pos = arr_start;
                while ((obj_pos = chunk_json.find('{', obj_pos + 1)) != std::string::npos) {
                    size_t obj_end = chunk_json.find('}', obj_pos);
                    if (obj_end == std::string::npos) break;
                    std::string prop_json = chunk_json.substr(obj_pos, obj_end - obj_pos + 1);

                    ChunkProperty cp;
                    cp.offset = static_cast<uint64_t>(json_int(prop_json, "offset"));
                    cp.bytes = static_cast<uint64_t>(json_int(prop_json, "bytes"));
                    cp.property = parse_prop_name(json_string(prop_json, "property"));
                    cp.enc = parse_encoding(json_string(prop_json, "encoding"));
                    cp.gz_compressed = json_string(prop_json, "compression") == "gz";
                    cp.min_val = json_float(prop_json, "min", 0.0f);
                    cp.max_val = json_float(prop_json, "max", 1.0f);
                    props.push_back(cp);

                    obj_pos = obj_end;
                }
            }
        }

        if (payload_start + payload_bytes <= buffer_.size()) {
            decode_chunk_properties(&buffer_[payload_start],
                                     static_cast<size_t>(payload_bytes), props, base, count);
        }

        offset = chunk_end;
        // Align to 8 bytes
        offset = roundup8(offset);
    }

    // Encode gsplats to packed format
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

    return true;
}

bool RadDecoder::parse_single_chunk() {
    // Single chunk RAD file (starts with RADC magic)
    // Reuse the same logic as multi-chunk but with offset 0
    // Wrap as if it's a single chunk inside parse_rad_meta
    // For simplicity, prepend a synthetic RAD header
    std::vector<uint8_t> synth;
    // Magic
    uint32_t magic = RAD_MAGIC;
    synth.resize(4);
    std::memcpy(synth.data(), &magic, 4);

    // Minimal JSON meta
    std::string meta = R"({"version":1,"type":"gsplat","count":0,"allChunkBytes":0,"chunks":[]})";
    uint32_t meta_len = static_cast<uint32_t>(meta.size());
    synth.resize(synth.size() + 4);
    std::memcpy(&synth[4], &meta_len, 4);
    synth.insert(synth.end(), meta.begin(), meta.end());
    // pad to 8
    while (synth.size() % 8 != 0) synth.push_back(0);

    // Append the original buffer (single chunk)
    synth.insert(synth.end(), buffer_.begin(), buffer_.end());
    buffer_ = std::move(synth);
    return parse_rad_meta();
}

void RadDecoder::decode_chunk_properties(const uint8_t* payload, size_t payload_size,
                                          const std::vector<ChunkProperty>& props,
                                          size_t base, size_t count) {
    for (const auto& prop : props) {
        if (prop.offset + prop.bytes > payload_size) continue;

        const uint8_t* prop_data = payload + prop.offset;
        size_t prop_bytes = static_cast<size_t>(prop.bytes);

        std::vector<uint8_t> decompressed;
        if (prop.gz_compressed) {
            decompressed = decompress_gz(prop_data, prop_bytes);
            prop_data = decompressed.data();
            prop_bytes = decompressed.size();
        }

        int dims = prop_dims(prop.property);
        auto values = decode_property_f32(prop_data, prop_bytes, prop.enc, dims, count,
                                           prop.min_val, prop.max_val);

        for (size_t i = 0; i < count && (base + i) < num_splats_; i++) {
            size_t idx = base + i;
            auto& s = gsplats_.splats[idx];

            switch (prop.property) {
                case PropName::Center:
                    s.center = glm::vec3(values[i*3], values[i*3+1], values[i*3+2]);
                    break;
                case PropName::Alpha:
                    s.opacity_h = float_to_half(values[i]);
                    break;
                case PropName::Rgb:
                    s.rgb_h[0] = float_to_half(values[i*3]);
                    s.rgb_h[1] = float_to_half(values[i*3+1]);
                    s.rgb_h[2] = float_to_half(values[i*3+2]);
                    break;
                case PropName::Scales:
                    s.ln_scales_h[0] = float_to_half(std::log(values[i*3]));
                    s.ln_scales_h[1] = float_to_half(std::log(values[i*3+1]));
                    s.ln_scales_h[2] = float_to_half(std::log(values[i*3+2]));
                    break;
                case PropName::Orientation:
                    s.quaternion_h[0] = float_to_half(values[i*4]);
                    s.quaternion_h[1] = float_to_half(values[i*4+1]);
                    s.quaternion_h[2] = float_to_half(values[i*4+2]);
                    s.quaternion_h[3] = float_to_half(values[i*4+3]);
                    break;
                default:
                    break;
            }
        }
    }
}

} // namespace spark
