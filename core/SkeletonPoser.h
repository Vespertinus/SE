
#ifndef __SKELETON_POSER_H__
#define __SKELETON_POSER_H__ 1

#include <cstdint>
#include <algorithm>
#include <utility>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <Skeleton.h>
#include <AnimEvaluator.h>

namespace SE {

// Converts a LocalPose (local-space TRS per bone) to:
//   outWorldTransforms[i] = world-space matrix for bone i
//   outSkinMatrices[i]    = worldTransforms[i] * inverseBindMatrix[i]
//
// entityWorldTransform: the mesh entity's world matrix (root bone is relative to this)
//
// Bones must be in topological order (parent_index < bone_index) as enforced at import.
// Both output arrays must be pre-allocated by the caller (boneCount entries each).
//
inline void computeWorldTransforms(
    const LocalPose&     localPose,
    const Skeleton& skeleton,
    const glm::mat4&     entityWorldTransform,
    glm::mat4*           outWorldTransforms,
    glm::mat4*           outSkinMatrices) {

        const auto&    bones = skeleton.Bones();
        const uint32_t n     = std::min(localPose.bone_count,
                        static_cast<uint32_t>(bones.size()));

        auto boneMat = [&](uint32_t i) -> glm::mat4 {
                return glm::translate(glm::mat4(1.0f), localPose.pPos[i])
                        * glm::mat4_cast(localPose.pRot[i])
                        * glm::scale(glm::mat4(1.0f), localPose.pScl[i]);
        };

        for (uint32_t i = 0; i < n; ++i) {
                glm::mat4 local = boneMat(i);
                if (bones[i].parentIndex == Skeleton::kNoParent) {
                        outWorldTransforms[i] = entityWorldTransform * local;
                } else {
                        outWorldTransforms[i] = outWorldTransforms[bones[i].parentIndex] * local;
                }
                outSkinMatrices[i] = outWorldTransforms[i] * bones[i].invBindMatrix;
        }
}

// ============================================================
// Root motion
// ============================================================

struct RootMotionConfig {
    bool extractTranslationXZ = true;
    bool extractTranslationY  = false;
    bool extractRotation      = true;
};

struct RootMotionDelta {
    glm::vec3 translation = glm::vec3(0.0f);
    glm::quat rotation    = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
};

// Extract root-bone delta TRS between prevTime and curTime from a clip.
// The root bone is bone index 0 by convention.
// Handles loop-boundary wrap when looping==true.
// Caller applies delta to the entity's Transform.
inline RootMotionDelta extractRootMotionDelta(
    const AnimClip&         clip,
    float                   prevTime,
    float                   curTime,
    bool                    looping,
    const RootMotionConfig& cfg) {

        // Sample bone 0 TRS at a given time by scanning all channels targeting bone 0.
        auto sampleRoot = [&](float t) -> std::pair<glm::vec3, glm::quat> {
                glm::vec3 pos(0.0f);
                glm::quat rot(1.0f, 0.0f, 0.0f, 0.0f);
                for (const auto& ch : clip.Channels()) {
                        if (ch.bone_index != 0) continue;
                        float val = 0.0f;
                        SampleCurve(ch, t, val);
                        if (ch.target <= 2) {
                                pos[ch.target] = val;
                        } else if (ch.target <= 6) {
                                rot[ch.target - 3] = val;
                        }
                }
                if (glm::dot(rot, rot) > 1e-8f) rot = glm::normalize(rot);
                return { pos, rot };
        };

        auto [p0, r0] = sampleRoot(prevTime);

        RootMotionDelta delta;

        if (looping && curTime < prevTime) {
                // Loop wrap: delta = (end - prevTime) + (curTime - start)
                auto [pe, re] = sampleRoot(clip.Duration());
                auto [p2, r2] = sampleRoot(curTime);

                glm::vec3 trans = (pe - p0) + p2;
                if (cfg.extractTranslationXZ) {
                        delta.translation.x = trans.x;
                        delta.translation.z = trans.z;
                }
                if (cfg.extractTranslationY) {
                        delta.translation.y = trans.y;
                }
                if (cfg.extractRotation) {
                        delta.rotation = r2 * glm::inverse(r0);
                }
        } else {
                auto [p1, r1] = sampleRoot(curTime);

                glm::vec3 trans = p1 - p0;
                if (cfg.extractTranslationXZ) {
                        delta.translation.x = trans.x;
                        delta.translation.z = trans.z;
                }
                if (cfg.extractTranslationY) {
                        delta.translation.y = trans.y;
                }
                if (cfg.extractRotation) {
                        delta.rotation = r1 * glm::inverse(r0);
                }
        }

        return delta;
}

} // namespace SE

#endif
