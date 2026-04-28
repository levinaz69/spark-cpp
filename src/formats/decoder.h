#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include "core/defines.h"

namespace spark {

struct SplatInit {
    size_t num_splats = 0;
    int max_sh_degree = 0;
    bool lod_tree = false;
};

struct SplatProps {
    const float* center = nullptr;
    const float* opacity = nullptr;
    const float* rgb = nullptr;
    const float* scale = nullptr;
    const float* quat = nullptr;
    const float* sh1 = nullptr;
    const float* sh2 = nullptr;
    const float* sh3 = nullptr;
};

// Interface for receiving decoded splat data
class SplatReceiver {
public:
    virtual ~SplatReceiver() = default;
    virtual void init_splats(const SplatInit& init) {}
    virtual void finish() {}

    virtual void set_batch(size_t base, size_t count, const SplatProps& props) = 0;
    virtual void set_center(size_t base, size_t count, const float* center) = 0;
    virtual void set_opacity(size_t base, size_t count, const float* opacity) = 0;
    virtual void set_rgb(size_t base, size_t count, const float* rgb) = 0;
    virtual void set_rgba(size_t base, size_t count, const float* rgba) = 0;
    virtual void set_scale(size_t base, size_t count, const float* scale) = 0;
    virtual void set_quat(size_t base, size_t count, const float* quat) = 0;

    virtual void set_sh1(size_t base, size_t count, const float* sh1) {}
    virtual void set_sh2(size_t base, size_t count, const float* sh2) {}
    virtual void set_sh3(size_t base, size_t count, const float* sh3) {}
};

// Interface for streaming chunk decoders
class ChunkDecoder {
public:
    virtual ~ChunkDecoder() = default;
    virtual void push(const uint8_t* data, size_t len) = 0;
    virtual void finish() = 0;
    virtual bool is_done() const = 0;
};

// Auto-detect file type from magic bytes
SplatFileType detect_file_type(const uint8_t* data, size_t len,
                                const std::string& filename = "");

} // namespace spark
