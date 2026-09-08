
#ifndef __ANIM_IK_H__
#define __ANIM_IK_H__ 1

#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <AnimEvaluator.h>

namespace SE {

// ============================================================
// Two-Bone IK (limb solver: foot, hand, arm reach)
// ============================================================

struct TwoBoneIKJob {
        uint32_t  rootBone;
        uint32_t  midBone;
        uint32_t  endBone;
        glm::vec3 targetPositionWorld;
        glm::vec3 poleVector         = {0.0f, 1.0f, 0.0f};  // controls knee/elbow direction
        float     weight             = 1.0f;                  // 0=pure animation, 1=pure IK
};

// Solve two-bone IK in-place.
// worldTransforms: current world-space matrices (one per bone), precomputed from pose.
// Writes rotations back into [pose] for midBone and endBone.
inline void SolveTwoBoneIK(
    LocalPose&             pose,
    const TwoBoneIKJob&    job,
    const glm::mat4*       worldTransforms) {

        glm::vec3 rootPos = glm::vec3(worldTransforms[job.rootBone][3]);
        glm::vec3 midPos  = glm::vec3(worldTransforms[job.midBone][3]);
        glm::vec3 endPos  = glm::vec3(worldTransforms[job.endBone][3]);

        float upperLen  = glm::length(midPos  - rootPos);
        float lowerLen  = glm::length(endPos  - midPos);
        float targetLen = glm::length(job.targetPositionWorld - rootPos);

        // Clamp to reachable range (epsilon margin to avoid degenerate cos)
        targetLen = std::clamp(targetLen, 0.001f, upperLen + lowerLen - 0.001f);

        // Law of cosines: find angle at mid joint
        float cosAngle = (upperLen*upperLen + lowerLen*lowerLen - targetLen*targetLen)
                / (2.0f * upperLen * lowerLen);
        cosAngle = std::clamp(cosAngle, -1.0f, 1.0f);
        float angle = std::acos(cosAngle);

        // Compute mid bone rotation using pole vector
        glm::vec3 toTarget    = job.targetPositionWorld - rootPos;
        float     toTargetLen = glm::length(toTarget);
        if (toTargetLen < 1e-6f) return;   // degenerate — skip

        glm::vec3 axis = glm::normalize(glm::cross(toTarget, job.poleVector));
        if (glm::length(axis) < 1e-6f) {
                axis = glm::vec3(1.0f, 0.0f, 0.0f);  // fallback axis
        }

        // IK rotation for mid bone (local space approximation)
        glm::quat midIK = glm::angleAxis(glm::pi<float>() - angle, glm::normalize(axis));
        pose.rot[job.midBone] = glm::slerp(pose.rot[job.midBone], midIK, job.weight);

        // Align end bone to target direction
        glm::vec3 endDir     = glm::normalize(job.targetPositionWorld - midPos);
        glm::vec3 currentDir = glm::normalize(endPos - midPos);
        if (glm::length(currentDir) < 1e-6f) return;

        glm::vec3 rotAxis    = glm::cross(currentDir, endDir);
        float     rotAxisLen = glm::length(rotAxis);
        if (rotAxisLen > 1e-6f) {
                float     cosA  = std::clamp(glm::dot(currentDir, endDir), -1.0f, 1.0f);
                glm::quat endIK = glm::angleAxis(std::acos(cosA), glm::normalize(rotAxis));
                pose.rot[job.endBone] = glm::slerp(pose.rot[job.endBone],
                                endIK * pose.rot[job.endBone],
                                job.weight);
        }
}

// ============================================================
// Look-At IK (head + spine tracking)
// ============================================================

struct LookAtIKJob {

        uint32_t  headBone;
        uint32_t  spineBones[3];                          // distribute rotation across spine
        float     spineContribution[3] = { 0.1f, 0.2f, 0.3f };
        glm::vec3 targetWorld;
        float     weight               = 1.0f;
        float     maxAngleDeg          = 60.0f;
};

inline void SolveLookAt(
    LocalPose&           pose,
    const LookAtIKJob&   job,
    const glm::mat4*     worldTransforms)
{
        glm::vec3 headPos  = glm::vec3(worldTransforms[job.headBone][3]);
        glm::vec3 forward  = glm::normalize(glm::vec3(worldTransforms[job.headBone]
                                * glm::vec4(0, 0, -1, 0)));
        glm::vec3 toTarget = glm::normalize(job.targetWorld - headPos);

        float cosAngle = std::clamp(glm::dot(forward, toTarget), -1.0f, 1.0f);
        float angle    = std::acos(cosAngle);
        angle = std::min(angle, glm::radians(job.maxAngleDeg));
        angle *= job.weight;

        if (angle < 1e-5f) return;

        glm::vec3 axis    = glm::cross(forward, toTarget);
        float     axisLen = glm::length(axis);
        if (axisLen < 1e-6f) return;
        axis = glm::normalize(axis);

        // Distribute across spine bones
        float spineTotal = job.spineContribution[0]
                + job.spineContribution[1]
                + job.spineContribution[2];
        for (int i = 0; i < 3; ++i) {
                float     contribution = angle * job.spineContribution[i];
                glm::quat spineRot     = glm::angleAxis(contribution, axis);
                pose.rot[job.spineBones[i]] = pose.rot[job.spineBones[i]] * spineRot;
        }

        // Remaining rotation applied to head
        float     headShare = 1.0f - spineTotal;
        glm::quat headRot   = glm::angleAxis(angle * std::max(0.0f, headShare), axis);
        pose.rot[job.headBone] = pose.rot[job.headBone] * headRot;
}

// ============================================================
// Foot IK (requires physics raycasts — only available with SE_PHYSICS_ENABLED)
// Config struct is always available for authoring purposes.
// ============================================================

struct FootIKConfig {

        uint32_t  pelvisBone;
        uint32_t  hipBones[2];
        uint32_t  kneeBones[2];
        uint32_t  ankleBones[2];
        glm::vec3 kneeForward[2]  = { {0, 0, 1}, {0, 0, 1} };
        float     upperLegLength  = 0.5f;
        float     lowerLegLength  = 0.5f;
        float     raycastOriginUp = 0.3f;  // raycast starts this far above ankle
        float     raycastMaxDist  = 0.6f;
        float     maxPelvisOffset = -0.3f; // negative = downward
};

} // namespace SE

#endif
