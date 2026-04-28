#include "ply_decoder.h"
#include <cstring>
#include <cmath>
#include <sstream>
#include <algorithm>

namespace spark {

static constexpr float SH_C0 = 0.28209479177387814f;

PlyDecoder::PropertyType PlyDecoder::parse_type(const std::string& s) {
    if (s == "float" || s == "float32") return PropertyType::Float;
    if (s == "double" || s == "float64") return PropertyType::Double;
    if (s == "uchar" || s == "uint8") return PropertyType::UChar;
    if (s == "short" || s == "int16") return PropertyType::Short;
    if (s == "ushort" || s == "uint16") return PropertyType::UShort;
    if (s == "int" || s == "int32") return PropertyType::Int;
    if (s == "uint" || s == "uint32") return PropertyType::UInt;
    return PropertyType::Invalid;
}

int PlyDecoder::type_size(PropertyType t) {
    switch (t) {
        case PropertyType::Float: return 4;
        case PropertyType::Double: return 8;
        case PropertyType::UChar: return 1;
        case PropertyType::Short: case PropertyType::UShort: return 2;
        case PropertyType::Int: case PropertyType::UInt: return 4;
        default: return 0;
    }
}

void PlyDecoder::push(const uint8_t* data, size_t len) {
    buffer_.insert(buffer_.end(), data, data + len);
}

bool PlyDecoder::parse_header() {
    // Find "end_header\n"
    const char* terminator = "end_header\n";
    size_t term_len = std::strlen(terminator);
    if (buffer_.size() < term_len + 4) return false;

    auto it = std::search(buffer_.begin(), buffer_.end(),
                          terminator, terminator + term_len);
    if (it == buffer_.end()) return false;

    header_size_ = std::distance(buffer_.begin(), it) + term_len;

    // Parse header lines
    std::string header(buffer_.begin(), buffer_.begin() + header_size_);
    std::istringstream iss(header);
    std::string line;

    Element* current_element = nullptr;

    while (std::getline(iss, line)) {
        if (line.empty() || line[0] == '\r') continue;
        // Remove trailing \r
        if (!line.empty() && line.back() == '\r') line.pop_back();

        std::istringstream lss(line);
        std::string keyword;
        lss >> keyword;

        if (keyword == "format") {
            std::string fmt;
            lss >> fmt;
            if (fmt == "ascii") format_ = PlyFormat::ASCII;
            else if (fmt == "binary_little_endian") format_ = PlyFormat::BinaryLE;
            else if (fmt == "binary_big_endian") format_ = PlyFormat::BinaryBE;
        } else if (keyword == "element") {
            elements_.emplace_back();
            current_element = &elements_.back();
            lss >> current_element->name >> current_element->count;
        } else if (keyword == "property" && current_element) {
            std::string type_str, name;
            lss >> type_str;
            if (type_str == "list") {
                // List property (skip for now)
                std::string count_type_str, elem_type_str;
                lss >> count_type_str >> elem_type_str >> name;
                Property p;
                p.name = name;
                p.is_list = true;
                p.count_type = parse_type(count_type_str);
                p.type = parse_type(elem_type_str);
                current_element->properties.push_back(p);
            } else {
                lss >> name;
                Property p;
                p.name = name;
                p.type = parse_type(type_str);
                p.size = type_size(p.type);
                p.offset = current_element->stride;
                current_element->stride += p.size;
                current_element->properties.push_back(p);
            }
        }
    }

    header_parsed_ = true;
    return true;
}

float PlyDecoder::read_property(const uint8_t* data, const Property& prop) const {
    switch (prop.type) {
        case PropertyType::Float: {
            float v; std::memcpy(&v, data + prop.offset, 4); return v;
        }
        case PropertyType::Double: {
            double v; std::memcpy(&v, data + prop.offset, 8); return static_cast<float>(v);
        }
        case PropertyType::UChar:
            return static_cast<float>(data[prop.offset]);
        case PropertyType::Short: {
            int16_t v; std::memcpy(&v, data + prop.offset, 2); return static_cast<float>(v);
        }
        case PropertyType::UShort: {
            uint16_t v; std::memcpy(&v, data + prop.offset, 2); return static_cast<float>(v);
        }
        case PropertyType::Int: {
            int32_t v; std::memcpy(&v, data + prop.offset, 4); return static_cast<float>(v);
        }
        case PropertyType::UInt: {
            uint32_t v; std::memcpy(&v, data + prop.offset, 4); return static_cast<float>(v);
        }
        default: return 0.0f;
    }
}

void PlyDecoder::decode_binary_le() {
    // Find the "vertex" element
    const Element* vertex_elem = nullptr;
    for (const auto& elem : elements_) {
        if (elem.name == "vertex") {
            vertex_elem = &elem;
            break;
        }
    }
    if (!vertex_elem) return;

    num_splats_ = vertex_elem->count;
    packed_array_.resize(num_splats_ * 4);

    // Build property lookup
    std::unordered_map<std::string, const Property*> prop_map;
    for (const auto& p : vertex_elem->properties) {
        prop_map[p.name] = &p;
    }

    const uint8_t* base = buffer_.data() + header_size_;
    int stride = vertex_elem->stride;

    for (size_t i = 0; i < num_splats_; i++) {
        const uint8_t* row = base + i * stride;

        // Position
        float cx = 0, cy = 0, cz = 0;
        if (auto it = prop_map.find("x"); it != prop_map.end()) cx = read_property(row, *it->second);
        if (auto it = prop_map.find("y"); it != prop_map.end()) cy = read_property(row, *it->second);
        if (auto it = prop_map.find("z"); it != prop_map.end()) cz = read_property(row, *it->second);

        // Color (f_dc_* are SH degree-0 coefficients)
        float r = 0.5f, g = 0.5f, b = 0.5f;
        if (auto it = prop_map.find("f_dc_0"); it != prop_map.end()) {
            r = read_property(row, *it->second) * SH_C0 + 0.5f;
        } else if (auto it2 = prop_map.find("red"); it2 != prop_map.end()) {
            r = read_property(row, *it2->second);
            if (it2->second->type == PropertyType::UChar) r /= 255.0f;
        }
        if (auto it = prop_map.find("f_dc_1"); it != prop_map.end()) {
            g = read_property(row, *it->second) * SH_C0 + 0.5f;
        } else if (auto it2 = prop_map.find("green"); it2 != prop_map.end()) {
            g = read_property(row, *it2->second);
            if (it2->second->type == PropertyType::UChar) g /= 255.0f;
        }
        if (auto it = prop_map.find("f_dc_2"); it != prop_map.end()) {
            b = read_property(row, *it->second) * SH_C0 + 0.5f;
        } else if (auto it2 = prop_map.find("blue"); it2 != prop_map.end()) {
            b = read_property(row, *it2->second);
            if (it2->second->type == PropertyType::UChar) b /= 255.0f;
        }

        // Opacity (stored as logit in standard 3DGS PLY)
        float opacity = 1.0f;
        if (auto it = prop_map.find("opacity"); it != prop_map.end()) {
            float logit = read_property(row, *it->second);
            opacity = 1.0f / (1.0f + std::exp(-logit)); // sigmoid
        }

        // Scale (stored as log)
        float sx = 1.0f, sy = 1.0f, sz = 1.0f;
        if (auto it = prop_map.find("scale_0"); it != prop_map.end()) sx = std::exp(read_property(row, *it->second));
        if (auto it = prop_map.find("scale_1"); it != prop_map.end()) sy = std::exp(read_property(row, *it->second));
        if (auto it = prop_map.find("scale_2"); it != prop_map.end()) sz = std::exp(read_property(row, *it->second));

        // Quaternion
        float qw = 1, qx = 0, qy = 0, qz = 0;
        if (auto it = prop_map.find("rot_0"); it != prop_map.end()) qw = read_property(row, *it->second);
        if (auto it = prop_map.find("rot_1"); it != prop_map.end()) qx = read_property(row, *it->second);
        if (auto it = prop_map.find("rot_2"); it != prop_map.end()) qy = read_property(row, *it->second);
        if (auto it = prop_map.find("rot_3"); it != prop_map.end()) qz = read_property(row, *it->second);

        // Normalize quaternion
        float qlen = std::sqrt(qw*qw + qx*qx + qy*qy + qz*qz);
        if (qlen > 0) { qw /= qlen; qx /= qlen; qy /= qlen; qz /= qlen; }

        glm::vec3 center(cx, cy, cz);
        glm::vec3 rgb_v(std::clamp(r, 0.0f, 1.0f),
                        std::clamp(g, 0.0f, 1.0f),
                        std::clamp(b, 0.0f, 1.0f));
        glm::vec3 scale(sx, sy, sz);
        glm::quat quat(qw, qx, qy, qz);

        encode_packed_splat(&packed_array_[i * 4],
                           center, opacity, rgb_v, scale, quat, encoding_);
    }
}

void PlyDecoder::decode_ascii() {
    // Find the "vertex" element
    const Element* vertex_elem = nullptr;
    for (const auto& elem : elements_) {
        if (elem.name == "vertex") {
            vertex_elem = &elem;
            break;
        }
    }
    if (!vertex_elem) return;

    num_splats_ = vertex_elem->count;
    packed_array_.resize(num_splats_ * 4);

    // Property indices
    std::unordered_map<std::string, int> prop_idx;
    for (int j = 0; j < static_cast<int>(vertex_elem->properties.size()); j++) {
        prop_idx[vertex_elem->properties[j].name] = j;
    }
    int num_props = static_cast<int>(vertex_elem->properties.size());

    std::string content(buffer_.begin() + header_size_, buffer_.end());
    std::istringstream iss(content);

    for (size_t i = 0; i < num_splats_; i++) {
        std::vector<float> vals(num_props, 0.0f);
        for (int j = 0; j < num_props; j++) {
            iss >> vals[j];
        }

        auto get = [&](const std::string& name, float def = 0.0f) -> float {
            auto it = prop_idx.find(name);
            return it != prop_idx.end() ? vals[it->second] : def;
        };

        float cx = get("x"), cy = get("y"), cz = get("z");
        float r = get("f_dc_0", 0.0f) * SH_C0 + 0.5f;
        float g = get("f_dc_1", 0.0f) * SH_C0 + 0.5f;
        float b_val = get("f_dc_2", 0.0f) * SH_C0 + 0.5f;
        float logit_opacity = get("opacity", 0.0f);
        float opacity = 1.0f / (1.0f + std::exp(-logit_opacity));

        float sx = std::exp(get("scale_0")), sy = std::exp(get("scale_1")), sz = std::exp(get("scale_2"));
        float qw = get("rot_0", 1.0f), qx = get("rot_1"), qy = get("rot_2"), qz = get("rot_3");

        float qlen = std::sqrt(qw*qw + qx*qx + qy*qy + qz*qz);
        if (qlen > 0) { qw /= qlen; qx /= qlen; qy /= qlen; qz /= qlen; }

        encode_packed_splat(&packed_array_[i * 4],
                           glm::vec3(cx, cy, cz),
                           opacity,
                           glm::vec3(std::clamp(r, 0.0f, 1.0f),
                                     std::clamp(g, 0.0f, 1.0f),
                                     std::clamp(b_val, 0.0f, 1.0f)),
                           glm::vec3(sx, sy, sz),
                           glm::quat(qw, qx, qy, qz),
                           encoding_);
    }
}

void PlyDecoder::finish() {
    if (!header_parsed_) {
        if (!parse_header()) return;
    }

    if (format_ == PlyFormat::ASCII) {
        decode_ascii();
    } else {
        decode_binary_le();
    }
}

} // namespace spark
