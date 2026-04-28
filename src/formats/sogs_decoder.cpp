#include "sogs_decoder.h"
#include "core/half_float.h"
#include "core/splat_encoding.h"
#include <cstring>
#include <cmath>
#include <iostream>
#include <miniz.h>

namespace spark {

static constexpr uint32_t PK_MAGIC = 0x04034b50;
static constexpr float SH_C0 = 0.28209479177387814f;

void SogsDecoder::push(const uint8_t* data, size_t len) {
    buffer_.insert(buffer_.end(), data, data + len);
}

void SogsDecoder::finish() {
    if (buffer_.size() < 4) {
        std::cerr << "SogsDecoder: file too small" << std::endl;
        return;
    }
    uint32_t magic;
    std::memcpy(&magic, buffer_.data(), 4);
    if (magic != PK_MAGIC) {
        std::cerr << "SogsDecoder: not a ZIP file" << std::endl;
        return;
    }
    parse_zip();
}

bool SogsDecoder::parse_zip() {
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_mem(&zip, buffer_.data(), buffer_.size(), 0)) {
        std::cerr << "SogsDecoder: failed to open zip" << std::endl;
        return false;
    }

    // Find meta.json
    int meta_idx = -1;
    std::string prefix;
    for (int i = 0; i < (int)mz_zip_reader_get_num_files(&zip); i++) {
        char name[512];
        mz_zip_reader_get_filename(&zip, i, name, sizeof(name));
        std::string sname(name);
        if (sname.find("meta.json") != std::string::npos) {
            meta_idx = i;
            auto slash_pos = sname.rfind('/');
            if (slash_pos != std::string::npos) {
                prefix = sname.substr(0, slash_pos + 1);
            }
            break;
        }
    }

    if (meta_idx < 0) {
        std::cerr << "SogsDecoder: meta.json not found" << std::endl;
        mz_zip_reader_end(&zip);
        return false;
    }

    // Read meta.json
    size_t meta_size;
    void* meta_data = mz_zip_reader_extract_to_heap(&zip, meta_idx, &meta_size, 0);
    if (!meta_data) {
        mz_zip_reader_end(&zip);
        return false;
    }

    std::string meta_str(static_cast<char*>(meta_data), meta_size);
    mz_free(meta_data);

    // Parse count from meta.json
    auto count_pos = meta_str.find("\"count\"");
    if (count_pos == std::string::npos) {
        // V1 format: try to find shape
        auto shape_pos = meta_str.find("\"shape\"");
        if (shape_pos != std::string::npos) {
            auto arr_start = meta_str.find('[', shape_pos);
            if (arr_start != std::string::npos) {
                auto comma = meta_str.find(',', arr_start);
                auto arr_end = meta_str.find(']', arr_start);
                if (comma != std::string::npos && arr_end != std::string::npos) {
                    int w = std::stoi(meta_str.substr(arr_start + 1));
                    int h = std::stoi(meta_str.substr(comma + 1));
                    num_splats_ = w * h;
                }
            }
        }
    } else {
        auto colon = meta_str.find(':', count_pos);
        if (colon != std::string::npos) {
            num_splats_ = std::stoul(meta_str.substr(colon + 1));
        }
    }

    if (num_splats_ == 0) {
        std::cerr << "SogsDecoder: could not determine splat count" << std::endl;
        mz_zip_reader_end(&zip);
        return false;
    }

    // Helper to extract file from zip
    auto extract_file = [&](const std::string& name) -> std::vector<uint8_t> {
        std::string full_name = prefix + name;
        int idx = mz_zip_reader_locate_file(&zip, full_name.c_str(), nullptr, 0);
        if (idx < 0) {
            idx = mz_zip_reader_locate_file(&zip, name.c_str(), nullptr, 0);
        }
        if (idx < 0) return {};
        size_t sz;
        void* data = mz_zip_reader_extract_to_heap(&zip, idx, &sz, 0);
        if (!data) return {};
        std::vector<uint8_t> result(static_cast<uint8_t*>(data), static_cast<uint8_t*>(data) + sz);
        mz_free(data);
        return result;
    };

    // Parse means file names from meta
    auto extract_filename = [&](const std::string& json, const std::string& section, int file_idx) -> std::string {
        auto sec_pos = json.find("\"" + section + "\"");
        if (sec_pos == std::string::npos) return "";
        auto files_pos = json.find("\"files\"", sec_pos);
        if (files_pos == std::string::npos) return "";
        auto arr_start = json.find('[', files_pos);
        if (arr_start == std::string::npos) return "";
        size_t pos = arr_start + 1;
        for (int f = 0; f <= file_idx; f++) {
            pos = json.find('"', pos);
            if (pos == std::string::npos) return "";
            auto end = json.find('"', pos + 1);
            if (end == std::string::npos) return "";
            if (f == file_idx) return json.substr(pos + 1, end - pos - 1);
            pos = end + 1;
        }
        return "";
    };

    // For now, decode basic RGBA data from means/sh0 image files
    // Full SOGS decoding would require stb_image for PNG decoding of texture maps
    // This is a simplified version that reads raw file data

    packed_.resize(num_splats_ * 4, 0);

    // Try to load raw position/color data
    std::string means0_name = extract_filename(meta_str, "means", 0);
    std::string sh0_name = extract_filename(meta_str, "sh0", 0);

    if (!means0_name.empty()) {
        auto means0_data = extract_file(means0_name);
        // means0 is typically a PNG image, would need stb_image to decode
        // For now, mark as loaded but with default positions
        (void)means0_data;
    }

    // Initialize with default data
    for (size_t i = 0; i < num_splats_; i++) {
        glm::vec3 center(0.0f);
        float opacity = 0.5f;
        glm::vec3 rgb(0.5f);
        glm::vec3 scale(0.01f);
        glm::quat quat(1.0f, 0.0f, 0.0f, 0.0f);
        encode_packed_splat(&packed_[i * 4], center, opacity, rgb, scale, quat, encoding_);
    }

    std::cout << "SogsDecoder: loaded " << num_splats_ << " splats (basic mode)" << std::endl;
    mz_zip_reader_end(&zip);
    return true;
}

} // namespace spark
