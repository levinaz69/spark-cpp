#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <cstdint>
#include "core/defines.h"

namespace spark {

// SplatAccumulator: blends/accumulates multiple splat contributions
class SplatAccumulator {
public:
    SplatAccumulator() = default;

    struct AccumSplat {
        glm::vec3 center{0.0f};
        glm::vec3 rgb{0.0f};
        float opacity = 0.0f;
        glm::vec3 scale{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        float weight = 0.0f;
    };

    void clear();
    void resize(size_t count);
    void accumulate(size_t index, const glm::vec3& center, const glm::vec3& rgb,
                    float opacity, const glm::vec3& scale, const glm::quat& rotation,
                    float weight = 1.0f);
    void normalize();

    // Export to packed format
    void to_packed(std::vector<uint32_t>& packed, const SplatEncoding& encoding) const;

    size_t count() const { return splats_.size(); }
    const AccumSplat& get(size_t i) const { return splats_[i]; }

private:
    std::vector<AccumSplat> splats_;
};

} // namespace spark
