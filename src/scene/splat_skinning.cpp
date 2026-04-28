#include "splat_skinning.h"
#include "core/splat_encoding.h"
#include "core/half_float.h"
#include <cmath>

namespace spark {

glm::mat4 Bone::local_matrix() const {
    glm::mat4 m = glm::translate(glm::mat4(1.0f), position);
    m *= glm::mat4_cast(rotation);
    m = glm::scale(m, scale);
    return m;
}

glm::fdualquat Bone::dual_quaternion() const {
    glm::quat real = rotation;
    glm::quat dual = glm::quat(0.0f,
        0.5f * (position.x * real.w + position.y * real.z - position.z * real.y),
        0.5f * (-position.x * real.z + position.y * real.w + position.z * real.x),
        0.5f * (position.x * real.y - position.y * real.x + position.z * real.w));
    return glm::fdualquat(real, dual);
}

void SplatSkinning::set_bones(const std::vector<Bone>& bones) {
    bones_ = bones;
    bone_matrices_.resize(bones.size(), glm::mat4(1.0f));
    bone_dualquats_.resize(bones.size());
    inverse_bind_.resize(bones.size(), glm::mat4(1.0f));
}

void SplatSkinning::set_bind_pose(const std::vector<Bone>& bind_pose) {
    bind_pose_ = bind_pose;

    // Compute world-space bind pose matrices
    std::vector<glm::mat4> bind_world(bind_pose.size(), glm::mat4(1.0f));
    for (size_t i = 0; i < bind_pose.size(); i++) {
        glm::mat4 local = bind_pose[i].local_matrix();
        if (bind_pose[i].parent >= 0 && bind_pose[i].parent < static_cast<int>(i)) {
            bind_world[i] = bind_world[bind_pose[i].parent] * local;
        } else {
            bind_world[i] = local;
        }
        inverse_bind_[i] = glm::inverse(bind_world[i]);
    }
}

void SplatSkinning::set_weights(const std::vector<SkinWeight>& weights) {
    weights_ = weights;
}

void SplatSkinning::update_bones(const std::vector<Bone>& posed_bones) {
    if (posed_bones.size() != bones_.size()) return;

    // Compute world-space posed matrices
    std::vector<glm::mat4> world_matrices(posed_bones.size(), glm::mat4(1.0f));
    for (size_t i = 0; i < posed_bones.size(); i++) {
        glm::mat4 local = posed_bones[i].local_matrix();
        if (posed_bones[i].parent >= 0 && posed_bones[i].parent < static_cast<int>(i)) {
            world_matrices[i] = world_matrices[posed_bones[i].parent] * local;
        } else {
            world_matrices[i] = local;
        }
        bone_matrices_[i] = world_matrices[i] * inverse_bind_[i];
    }

    // Convert to dual quaternions for blending
    for (size_t i = 0; i < posed_bones.size(); i++) {
        glm::mat4 m = bone_matrices_[i];
        glm::quat rot = glm::quat_cast(m);
        glm::vec3 pos = glm::vec3(m[3]);

        Bone b;
        b.position = pos;
        b.rotation = rot;
        bone_dualquats_[i] = b.dual_quaternion();
    }
}

void SplatSkinning::apply(const uint32_t* rest_packed, uint32_t* output_packed,
                            size_t num_splats, const SplatEncoding& encoding) {
    if (weights_.empty() || bones_.empty()) {
        if (rest_packed != output_packed) {
            std::memcpy(output_packed, rest_packed, num_splats * 4 * sizeof(uint32_t));
        }
        return;
    }

    for (size_t i = 0; i < num_splats; i++) {
        const uint32_t* rp = rest_packed + i * 4;
        uint32_t* op = output_packed + i * 4;

        glm::vec3 center = decode_packed_center(rp);
        float opacity = decode_packed_opacity(rp, encoding);
        glm::vec3 rgb = decode_packed_rgb(rp, encoding);
        glm::vec3 scale = decode_packed_scale(rp, encoding);
        glm::quat quat = decode_packed_quat(rp);

        if (i < weights_.size()) {
            const auto& w = weights_[i];

            // Dual quaternion blending
            glm::fdualquat blended(glm::quat(0, 0, 0, 0), glm::quat(0, 0, 0, 0));
            for (int j = 0; j < 4; j++) {
                if (w.weights[j] <= 0.0f) continue;
                int bi = w.bone_indices[j];
                if (bi < 0 || bi >= static_cast<int>(bone_dualquats_.size())) continue;

                auto dq = bone_dualquats_[bi];
                // Ensure same hemisphere
                if (glm::dot(blended.real, dq.real) < 0.0f) {
                    dq.real = -dq.real;
                    dq.dual = -dq.dual;
                }
                blended.real += dq.real * w.weights[j];
                blended.dual += dq.dual * w.weights[j];
            }

            // Normalize
            float len = glm::length(blended.real);
            if (len > 1e-6f) {
                blended.real /= len;
                blended.dual /= len;
            }

            // Extract rotation and translation
            glm::quat bone_rot = blended.real;
            glm::vec3 bone_trans(
                2.0f * (-blended.dual.w * blended.real.x + blended.dual.x * blended.real.w
                        - blended.dual.y * blended.real.z + blended.dual.z * blended.real.y),
                2.0f * (-blended.dual.w * blended.real.y + blended.dual.x * blended.real.z
                        + blended.dual.y * blended.real.w - blended.dual.z * blended.real.x),
                2.0f * (-blended.dual.w * blended.real.z - blended.dual.x * blended.real.y
                        + blended.dual.y * blended.real.x + blended.dual.z * blended.real.w)
            );

            center = bone_rot * center + bone_trans;
            quat = bone_rot * quat;
        }

        encode_packed_splat(op, center, opacity, rgb, scale, quat, encoding);
    }
}

} // namespace spark
