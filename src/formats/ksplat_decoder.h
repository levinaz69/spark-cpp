#pragma once

#include "decoder.h"
#include <vector>
#include <cstdint>

namespace spark {

// KSPLAT format decoder (compression levels 0-2)
class KsplatDecoder : public ChunkReceiver {
public:
    KsplatDecoder() = default;
    void push(const uint8_t* data, size_t len) override;
    void finish() override;
    size_t num_splats() const override { return num_splats_; }
    const std::vector<uint32_t>& packed_array() const override { return packed_; }
    const SplatEncoding& encoding() const override { return encoding_; }

private:
    struct Compression {
        int bytes_per_center;
        int bytes_per_scale;
        int bytes_per_rotation;
        int bytes_per_color;
        int bytes_per_sh_component;
        int scale_offset_bytes;
        int rotation_offset_bytes;
        int color_offset_bytes;
        int sh_offset_bytes;
        uint32_t scale_range;
    };

    static const Compression COMPRESSIONS[3];
    static const int SH_COMPONENTS[4];

    std::vector<uint8_t> buffer_;
    std::vector<uint32_t> packed_;
    size_t num_splats_ = 0;
    SplatEncoding encoding_;
};

} // namespace spark
