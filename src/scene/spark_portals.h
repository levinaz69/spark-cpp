#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>
#include <functional>

namespace spark {

// Portal: teleportation point between scenes or locations
struct Portal {
    std::string name;
    std::string target_scene;
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
    float radius = 1.0f;

    glm::vec3 target_position{0.0f};
    glm::quat target_rotation{1.0f, 0.0f, 0.0f, 0.0f};

    bool active = true;
    bool visible = true;

    // Check if a point is inside the portal trigger zone
    bool contains(const glm::vec3& point) const;
};

// SparkPortals: manages portal/teleportation system
class SparkPortals {
public:
    SparkPortals() = default;

    void add_portal(const Portal& portal);
    void remove_portal(const std::string& name);
    Portal* find_portal(const std::string& name);

    // Check if position triggers any portal
    Portal* check_trigger(const glm::vec3& position);

    // Compute teleportation transform
    struct TeleportResult {
        glm::vec3 position;
        glm::quat rotation;
        std::string scene_name;
    };
    bool teleport(const Portal& portal, const glm::vec3& current_pos,
                  const glm::quat& current_rot, TeleportResult& result);

    const std::vector<Portal>& portals() const { return portals_; }

    using TeleportCallback = std::function<void(const TeleportResult&)>;
    void set_callback(TeleportCallback cb) { callback_ = cb; }

private:
    std::vector<Portal> portals_;
    TeleportCallback callback_;
};

} // namespace spark
