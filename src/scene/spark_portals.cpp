#include "spark_portals.h"
#include <algorithm>

namespace spark {

bool Portal::contains(const glm::vec3& point) const {
    glm::vec3 local = glm::conjugate(rotation) * (point - position);
    return glm::length(local / scale) <= radius;
}

void SparkPortals::add_portal(const Portal& portal) {
    portals_.push_back(portal);
}

void SparkPortals::remove_portal(const std::string& name) {
    portals_.erase(std::remove_if(portals_.begin(), portals_.end(),
                   [&](const Portal& p) { return p.name == name; }),
                   portals_.end());
}

Portal* SparkPortals::find_portal(const std::string& name) {
    for (auto& p : portals_) {
        if (p.name == name) return &p;
    }
    return nullptr;
}

Portal* SparkPortals::check_trigger(const glm::vec3& position) {
    for (auto& p : portals_) {
        if (p.active && p.contains(position)) return &p;
    }
    return nullptr;
}

bool SparkPortals::teleport(const Portal& portal, const glm::vec3& current_pos,
                              const glm::quat& current_rot, TeleportResult& result) {
    // Compute relative offset from portal entry
    glm::vec3 local_offset = glm::conjugate(portal.rotation) * (current_pos - portal.position);

    // Transform to target space
    result.position = portal.target_position + portal.target_rotation * local_offset;

    // Transform rotation: remove source rotation, apply target rotation
    glm::quat relative_rot = glm::conjugate(portal.rotation) * current_rot;
    result.rotation = portal.target_rotation * relative_rot;

    result.scene_name = portal.target_scene;

    if (callback_) callback_(result);
    return true;
}

} // namespace spark
