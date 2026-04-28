#pragma once

#include "decoder.h"
#include <vector>
#include <cstdint>
#include <string>

namespace spark {

// SOGS/PCSOGS format: ZIP-based Gaussian splat format with PNG textures
// Uses image-based means, scales, quaternions, SH coefficients
class SogsDecoder : public ChunkReceiver {
public:
    SogsDecoder() = default;
    void push(const uint8_t* data, size_t len) override;
    void finish() override;
    size_t num_splats() const override { return num_splats_; }
    const std::vector<uint32_t>& packed_array() const override { return packed_; }
    const SplatEncoding& encoding() const override { return encoding_; }

private:
    bool parse_zip();

    std::vector<uint8_t> buffer_;
    std::vector<uint32_t> packed_;
    size_t num_splats_ = 0;
    SplatEncoding encoding_;
};

} // namespace spark
