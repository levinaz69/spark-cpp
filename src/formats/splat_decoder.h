#pragma once

#include "decoder.h"
#include "core/splat_encoding.h"
#include <vector>

namespace spark {

// .splat format: 32 bytes per splat, no header
// Layout: center(3xf32) + scale(3xf32) + rgba(4xu8) + quaternion(4xu8_normalized)
// This is the "antisplat" format from antimatter15/splat

constexpr size_t ANTISPLAT_BYTES_PER_SPLAT = 32;
constexpr size_t MAX_SPLAT_CHUNK = 65536;

struct SplatFileData {
    size_t num_splats = 0;
    std::vector<uint32_t> packed_array;
    SplatEncoding encoding;
};

// Decode raw .splat file bytes into packed splat array
SplatFileData decode_splat_file(const uint8_t* data, size_t len,
                                const SplatEncoding& encoding = DEFAULT_SPLAT_ENCODING);

// Decode raw .splat file bytes into GsplatArray
class SplatFormatDecoder {
public:
    SplatFormatDecoder() = default;

    // Push raw bytes; call finish() when done
    void push(const uint8_t* data, size_t len);
    void finish();

    // Get packed output
    const std::vector<uint32_t>& packed_array() const { return packed_array_; }
    size_t num_splats() const { return num_splats_; }
    const SplatEncoding& encoding() const { return encoding_; }

    void set_encoding(const SplatEncoding& enc) { encoding_ = enc; }

private:
    std::vector<uint8_t> buffer_;
    std::vector<uint32_t> packed_array_;
    size_t num_splats_ = 0;
    SplatEncoding encoding_;
};

} // namespace spark
