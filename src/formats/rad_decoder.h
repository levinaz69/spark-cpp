#pragma once

#include "decoder.h"
#include <vector>
#include <cstdint>
#include <string>

namespace spark {

// RAD format: Spark's native chunk-based Gaussian splat format
// Magic: 0x30444152 ('RAD0'), chunks: 0x43444152 ('RADC')
// Supports multiple encoding schemes (f32, f16, r8, etc.) with gz compression
class RadDecoder : public ChunkReceiver {
public:
    RadDecoder() = default;
    void push(const uint8_t* data, size_t len) override;
    void finish() override;
    size_t num_splats() const override { return num_splats_; }
    const std::vector<uint32_t>& packed_array() const override { return packed_; }
    const SplatEncoding& encoding() const override { return encoding_; }

    static constexpr uint32_t RAD_MAGIC = 0x30444152;
    static constexpr uint32_t RAD_CHUNK_MAGIC = 0x43444152;

    enum class PropEncoding {
        F32, F16, F32_LeBytes, F16_LeBytes,
        R8, R8_Delta, S8, S8_Delta,
        Ln0R8, LnF16, Oct88R8, U16, U32,
    };

    enum class PropName {
        Center, Alpha, Rgb, Scales, Orientation,
        Sh1, Sh2, Sh3, ChildCount, ChildStart,
        Sh1Code, Sh2Code, Sh3Code, ShLabel,
    };

    struct ChunkProperty {
        uint64_t offset = 0;
        uint64_t bytes = 0;
        PropName property = PropName::Center;
        PropEncoding enc = PropEncoding::F32;
        bool gz_compressed = false;
        float min_val = 0.0f;
        float max_val = 1.0f;
    };

private:
    bool parse_rad_meta();
    bool parse_single_chunk();
    void decode_chunk_properties(const uint8_t* payload, size_t payload_size,
                                  const std::vector<ChunkProperty>& props,
                                  size_t base, size_t count);
    std::vector<float> decode_property_f32(const uint8_t* data, size_t bytes,
                                            PropEncoding enc, int dims, size_t count,
                                            float min_val, float max_val);
    std::vector<uint8_t> decompress_gz(const uint8_t* data, size_t len);

    std::vector<uint8_t> buffer_;
    std::vector<uint32_t> packed_;
    size_t num_splats_ = 0;
    int max_sh_degree_ = 0;
    bool has_lod_ = false;
    SplatEncoding encoding_;
    GsplatArray gsplats_;
};

} // namespace spark
