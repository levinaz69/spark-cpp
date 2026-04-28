#include "decoder.h"
#include <cstring>
#include <algorithm>

namespace spark {

static constexpr uint32_t PLY_MAGIC = 0x00796c70;  // "ply\0"
static constexpr uint32_t SPZ_MAGIC = 0x5053474e;  // "NGSP"
static constexpr uint32_t RAD_MAGIC = 0x30444152;  // "RAD0"

SplatFileType detect_file_type(const uint8_t* data, size_t len,
                                const std::string& filename) {
    if (len >= 4) {
        uint32_t magic;
        std::memcpy(&magic, data, 4);

        if ((magic & 0x00FFFFFF) == PLY_MAGIC) return SplatFileType::PLY;
        if (magic == SPZ_MAGIC) return SplatFileType::SPZ;
        if (magic == RAD_MAGIC) return SplatFileType::RAD;

        // Check for gzip (SPZ is gzipped)
        if ((data[0] == 0x1f) && (data[1] == 0x8b)) return SplatFileType::SPZ;

        // Check for ZIP (SOGS)
        if (data[0] == 0x50 && data[1] == 0x4b) return SplatFileType::PCSOGSZIP;
    }

    // Try file extension
    if (!filename.empty()) {
        auto dot = filename.rfind('.');
        if (dot != std::string::npos) {
            std::string ext = filename.substr(dot);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".ply") return SplatFileType::PLY;
            if (ext == ".spz") return SplatFileType::SPZ;
            if (ext == ".splat") return SplatFileType::SPLAT;
            if (ext == ".ksplat") return SplatFileType::KSPLAT;
            if (ext == ".rad") return SplatFileType::RAD;
        }
    }

    // Default: try as .splat (32 bytes per splat, no header)
    return SplatFileType::SPLAT;
}

} // namespace spark
