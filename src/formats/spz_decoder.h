#pragma once

#include "decoder.h"
#include <vector>
#include <cstdint>

namespace spark {

// SPZ format: Niantic gzip-compressed Gaussian splat format
// Magic: 0x5053474e ("NGSP"), versions 1-3
class SpzDecoder : public ChunkReceiver {
public:
    SpzDecoder() = default;
    void push(const uint8_t* data, size_t len) override;
    void finish() override;
    size_t num_splats() const override { return num_splats_; }
    const std::vector<uint32_t>& packed_array() const override { return packed_; }
    const SplatEncoding& encoding() const override { return encoding_; }

    int max_sh_degree() const { return max_sh_degree_; }
    const GsplatArray& gsplat_array() const { return gsplats_; }

private:
    bool decompress_gzip();
    bool parse_header();
    void decode_sections();
    void decode_centers_v1(const uint8_t* data, size_t start, size_t count);
    void decode_centers_v2(const uint8_t* data, size_t start, size_t count, uint8_t frac_bits);
    void decode_alphas(const uint8_t* data, size_t start, size_t count, bool lod);
    void decode_rgb(const uint8_t* data, size_t start, size_t count);
    void decode_scales(const uint8_t* data, size_t start, size_t count);
    void decode_quats_v12(const uint8_t* data, size_t start, size_t count);
    void decode_quats_v3(const uint8_t* data, size_t start, size_t count);
    void decode_sh(const uint8_t* data, size_t start, size_t count, int sh_deg);
    void decode_child_counts(const uint8_t* data, size_t start, size_t count);
    void decode_child_starts(const uint8_t* data, size_t start, size_t count);
    void encode_to_packed();

    std::vector<uint8_t> compressed_;
    std::vector<uint8_t> decompressed_;

    uint32_t version_ = 0;
    size_t num_splats_ = 0;
    int max_sh_degree_ = 0;
    uint8_t fractional_bits_ = 0;
    uint8_t flags_ = 0;
    bool has_lod_ = false;

    GsplatArray gsplats_;
    std::vector<uint32_t> packed_;
    SplatEncoding encoding_;
};

} // namespace spark
