#pragma once

#include "decoder.h"
#include "core/splat_encoding.h"
#include <vector>
#include <string>
#include <unordered_map>

namespace spark {

// PLY format decoder supporting:
// - ASCII and binary (little/big endian)
// - Standard 3DGS PLY properties (x,y,z, f_dc_*, opacity, scale_*, rot_*)
// - Compressed PLY (PlayCanvas format)
// - SH coefficients up to degree 3

class PlyDecoder {
public:
    PlyDecoder() = default;

    void push(const uint8_t* data, size_t len);
    void finish();

    const std::vector<uint32_t>& packed_array() const { return packed_array_; }
    size_t num_splats() const { return num_splats_; }
    const SplatEncoding& encoding() const { return encoding_; }

    void set_encoding(const SplatEncoding& enc) { encoding_ = enc; }

private:
    enum class PlyFormat { ASCII, BinaryLE, BinaryBE };
    enum class PropertyType { Float, Double, UChar, Short, UShort, Int, UInt, Invalid };

    struct Property {
        std::string name;
        PropertyType type = PropertyType::Invalid;
        int offset = 0;
        int size = 0;
        // For list properties
        bool is_list = false;
        PropertyType count_type = PropertyType::Invalid;
    };

    struct Element {
        std::string name;
        int count = 0;
        std::vector<Property> properties;
        int stride = 0; // total bytes per element (binary)
    };

    bool parse_header();
    void decode_binary_le();
    void decode_ascii();

    static PropertyType parse_type(const std::string& s);
    static int type_size(PropertyType t);
    float read_property(const uint8_t* data, const Property& prop) const;

    std::vector<uint8_t> buffer_;
    std::vector<uint32_t> packed_array_;
    size_t num_splats_ = 0;
    SplatEncoding encoding_;

    // Parsed header
    PlyFormat format_ = PlyFormat::BinaryLE;
    std::vector<Element> elements_;
    size_t header_size_ = 0;
    bool header_parsed_ = false;
};

} // namespace spark
