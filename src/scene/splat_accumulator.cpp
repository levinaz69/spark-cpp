#include "splat_accumulator.h"
#include "core/splat_encoding.h"
#include <cmath>

namespace spark {

void SplatAccumulator::clear() {
    for (auto& s : splats_) {
        s = AccumSplat{};
    }
}

void SplatAccumulator::resize(size_t count) {
    splats_.resize(count);
    clear();
}

void SplatAccumulator::accumulate(size_t index, const glm::vec3& center,
                                    const glm::vec3& rgb, float opacity,
                                    const glm::vec3& scale, const glm::quat& rotation,
                                    float weight) {
    if (index >= splats_.size()) return;

    auto& s = splats_[index];
    s.center += center * weight;
    s.rgb += rgb * weight;
    s.opacity += opacity * weight;
    s.scale += scale * weight;

    // Accumulate quaternion (ensure same hemisphere)
    if (glm::dot(s.rotation, rotation) < 0.0f) {
        s.rotation += (-rotation) * weight;
    } else {
        s.rotation += rotation * weight;
    }
    s.weight += weight;
}

void SplatAccumulator::normalize() {
    for (auto& s : splats_) {
        if (s.weight > 0.0f) {
            float inv_w = 1.0f / s.weight;
            s.center *= inv_w;
            s.rgb *= inv_w;
            s.opacity *= inv_w;
            s.scale *= inv_w;
            s.rotation = glm::normalize(s.rotation);
        }
    }
}

void SplatAccumulator::to_packed(std::vector<uint32_t>& packed,
                                   const SplatEncoding& encoding) const {
    packed.resize(splats_.size() * 4);
    for (size_t i = 0; i < splats_.size(); i++) {
        const auto& s = splats_[i];
        encode_packed_splat(&packed[i * 4], s.center, s.opacity, s.rgb,
                            s.scale, s.rotation, encoding);
    }
}

} // namespace spark
