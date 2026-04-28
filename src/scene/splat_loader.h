#pragma once

#include <string>
#include <vector>
#include <memory>
#include "render/packed_splats.h"
#include "core/defines.h"

namespace spark {

class SplatLoader {
public:
    // Load from file path
    static std::unique_ptr<PackedSplats> load_file(const std::string& path);

    // Load from raw bytes
    static std::unique_ptr<PackedSplats> load_bytes(
        const uint8_t* data, size_t len,
        SplatFileType type = SplatFileType::Auto,
        const std::string& filename = "");

    // Detect type from file extension or magic bytes
    static SplatFileType detect_type(const std::string& filename,
                                      const uint8_t* data = nullptr,
                                      size_t len = 0);
};

} // namespace spark
