#include "splat_loader.h"
#include "formats/decoder.h"
#include "formats/splat_decoder.h"
#include "formats/ply_decoder.h"
#include <fstream>
#include <iostream>

namespace spark {

std::unique_ptr<PackedSplats> SplatLoader::load_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "SplatLoader: Failed to open " << path << std::endl;
        return nullptr;
    }

    size_t file_size = file.tellg();
    file.seekg(0);

    std::vector<uint8_t> data(file_size);
    file.read(reinterpret_cast<char*>(data.data()), file_size);

    return load_bytes(data.data(), data.size(), SplatFileType::Auto, path);
}

std::unique_ptr<PackedSplats> SplatLoader::load_bytes(
    const uint8_t* data, size_t len,
    SplatFileType type, const std::string& filename) {

    if (type == SplatFileType::Auto) {
        type = detect_file_type(data, len, filename);
    }

    auto splats = std::make_unique<PackedSplats>();

    switch (type) {
        case SplatFileType::SPLAT: {
            SplatFormatDecoder decoder;
            decoder.push(data, len);
            decoder.finish();
            if (decoder.num_splats() > 0) {
                splats->set_data(decoder.packed_array().data(),
                                decoder.num_splats(), decoder.encoding());
            }
            break;
        }

        case SplatFileType::PLY: {
            PlyDecoder decoder;
            decoder.push(data, len);
            decoder.finish();
            if (decoder.num_splats() > 0) {
                splats->set_data(decoder.packed_array().data(),
                                decoder.num_splats(), decoder.encoding());
            }
            break;
        }

        default:
            std::cerr << "SplatLoader: Format not yet supported" << std::endl;
            return nullptr;
    }

    std::cout << "SplatLoader: Loaded " << splats->num_splats() << " splats" << std::endl;
    return splats;
}

SplatFileType SplatLoader::detect_type(const std::string& filename,
                                        const uint8_t* data, size_t len) {
    return detect_file_type(data, len, filename);
}

} // namespace spark
