
#ifndef __ANIM_EVALUATOR_H__
#define __ANIM_EVALUATOR_H__ 1

#include <cstdint>
#include <cmath>
#include <algorithm>
#include <functional>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <AnimClip.h>
#include <Skeleton.h>
#include <Allocator.h>

namespace SE {

// ============================================================
// LocalPose — frame-allocator lifetime (never heap-owned)
// ============================================================

struct LocalPose {
        uint32_t   bone_count = 0;
        glm::vec3* pPos       = nullptr;
        glm::quat* pRot       = nullptr;
        glm::vec3* pScl       = nullptr;
};

// Allocate a LocalPose from the frame allocator.
inline LocalPose AllocatePose(uint32_t bone_count, FrameAllocator& alloc) {
        LocalPose p;
        p.bone_count = bone_count;
        p.pPos = reinterpret_cast<glm::vec3*>(alloc.allocate(bone_count * sizeof(glm::vec3), alignof(glm::vec3)));
        p.pRot = reinterpret_cast<glm::quat*>(alloc.allocate(bone_count * sizeof(glm::quat), alignof(glm::quat)));
        p.pScl = reinterpret_cast<glm::vec3*>(alloc.allocate(bone_count * sizeof(glm::vec3), alignof(glm::vec3)));
        return p;
}

// Fill a pose with the skeleton's bind pose (TRS from BoneData).
inline void InitBindPose(LocalPose& pose, const Skeleton& skeleton) {
        const auto& bones = skeleton.Bones();
        for (uint32_t i = 0; i < pose.bone_count && i < static_cast<uint32_t>(bones.size()); ++i) {
                pose.pPos[i] = bones[i].bindPos;
                pose.pRot[i] = bones[i].bindRot;
                pose.pScl[i] = bones[i].bindScale;
        }
}

// Zero-initialise a pose (pos=0, rot=identity, scl=1 or bind).
// Used internally by additive/layer nodes as a neutral base.
/*
inline void zeroPose(LocalPose& pose, const Skeleton* pSkel = nullptr) {
        if (pSkel) {
                const auto& bones = pSkel->Bones();
                for (uint32_t i = 0; i < pose.bone_count && i < static_cast<uint32_t>(bones.size()); ++i) {
                        pose.pPos[i] = glm::vec3(0.0f);
                        pose.pRot[i] = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                        pose.pScl[i] = bones[i].bindScale;
                }
                for (uint32_t i = static_cast<uint32_t>(bones.size()); i < pose.bone_count; ++i) {
                        pose.pPos[i] = glm::vec3(0.0f);
                        pose.pRot[i] = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                        pose.pScl[i] = glm::vec3(1.0f);
                }
        } else {
                for (uint32_t i = 0; i < pose.bone_count; ++i) {
                        pose.pPos[i] = glm::vec3(0.0f);
                        pose.pRot[i] = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                        pose.pScl[i] = glm::vec3(1.0f);
                }
        }
}
*/
// ============================================================
// Curve sampling
// ============================================================

// Sample a single CurveChannel at [time]. Writes result to [out].
inline void SampleCurve(const AnimClip::CurveChannel& ch, float time, float& out) {
        const uint32_t keyCount = static_cast<uint32_t>(ch.vTimes.size());

        if (keyCount == 0) { out = 0.0f; return; }

        if (ch.format == AnimClip::Format::ConstantF32 || keyCount == 1) {
                out = ch.vValues[0];
                return;
        }

        // clamp time to clip range
        if (time <= ch.vTimes[0])          { out = ch.vValues[0]; return; }
        if (time >= ch.vTimes[keyCount-1]) { out = ch.vValues[keyCount-1]; return; }

        // binary search for bracket [lo, hi] where times[lo] <= time < times[hi]
        uint32_t lo = 0, hi = keyCount - 1;
        while (lo < hi - 1) {
                uint32_t mid = (lo + hi) >> 1;
                if (ch.vTimes[mid] <= time) lo = mid; else hi = mid;
        }

        if (ch.format == AnimClip::Format::StepF32) {
                out = ch.vValues[lo];
                return;
        }

        // LinearF32 — simple lerp between adjacent keyframes
        if (ch.format == AnimClip::Format::LinearF32) {
                float dt = ch.vTimes[hi] - ch.vTimes[lo];
                float t  = (dt > 1e-6f) ? (time - ch.vTimes[lo]) / dt : 0.0f;
                t = std::clamp(t, 0.0f, 1.0f);
                out = ch.vValues[lo] + t * (ch.vValues[hi] - ch.vValues[lo]);
                return;
        }

        // HermiteF32 cubic interpolation
        float dt = ch.vTimes[hi] - ch.vTimes[lo];
        float t  = (dt > 1e-6f) ? (time - ch.vTimes[lo]) / dt : 0.0f;
        t = std::clamp(t, 0.0f, 1.0f);

        float t2 = t * t, t3 = t2 * t;
        float h00 =  2.0f*t3 - 3.0f*t2 + 1.0f;
        float h10 =       t3 - 2.0f*t2 + t;
        float h01 = -2.0f*t3 + 3.0f*t2;
        float h11 =       t3 -       t2;

        float tan0 = ch.vTangents.empty() ? 0.0f : ch.vTangents[lo];
        float tan1 = ch.vTangents.empty() ? 0.0f : ch.vTangents[hi];

        out = h00 * ch.vValues[lo] + h10 * tan0
                + h01 * ch.vValues[hi] + h11 * tan1;
}

// ============================================================
// Clip sampling
// ============================================================

// Sample all channels of [clip] at [time] and write (replace) into [outPose].
// Call with a pose pre-filled via InitBindPose() so that bones with no channels
// keep their bind-pose values.  Caller must call RenormalizeRotations() after.
inline void SampleClip(const AnimClip& clip, float time, LocalPose& outPose) {
        for (const auto& ch : clip.Channels()) {
                if (ch.bone_index >= outPose.bone_count) continue;

                float value = 0.0f;
                SampleCurve(ch, time, value);

                if (ch.target <= 2) {
                        // pos x/y/z
                        outPose.pPos[ch.bone_index][ch.target] = value;
                } else if (ch.target <= 6) {
                        // rot x/y/z/w  (target 3=x, 4=y, 5=z, 6=w)
                        outPose.pRot[ch.bone_index][ch.target - 3] = value;
                } else if (ch.target <= 9) {
                        // scl x/y/z  (target 7=x, 8=y, 9=z)
                        outPose.pScl[ch.bone_index][ch.target - 7] = value;
                }
        }
}

// Blend two fully-evaluated poses into [out] using slerp/lerp.
inline void BlendPoses(const LocalPose& a, const LocalPose& b, float t, LocalPose& out) {
        for (uint32_t i = 0; i < out.bone_count; ++i) {
                out.pPos[i] = glm::mix(a.pPos[i], b.pPos[i], t);
                out.pRot[i] = glm::slerp(a.pRot[i], b.pRot[i], t);
                out.pScl[i] = glm::mix(a.pScl[i], b.pScl[i], t);
        }
}

// Renormalize all quaternions in [pose] (call after blending multiple clips).
inline void RenormalizeRotations(LocalPose& pose) {
        for (uint32_t i = 0; i < pose.bone_count; ++i) {
                float len2 = glm::dot(pose.pRot[i], pose.pRot[i]);
                if (len2 > 1e-8f) {
                        pose.pRot[i] = glm::normalize(pose.pRot[i]);
                } else {
                        pose.pRot[i] = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // identity fallback
                }
        }
}

// Check and fire events in [prevTime, curTime].
// Handles loop wrap when looping==true (curTime may be < prevTime).
// callback is called for each fired event.
inline void CheckAnimEvents(
                const AnimClip&                                         clip,
                float                                                   prevTime,
                float                                                   curTime,
                bool                                                    looping,
                const std::function<void(const AnimClip::AnimEvent&)>&  callback) {

        const auto& events = clip.Events();
        if (events.empty()) return;

        auto fireRange = [&](float t0, float t1) {
                for (const auto& ev : events) {
                        if (ev.time > t0 && ev.time <= t1) callback(ev);
                }
        };

        if (!looping || curTime >= prevTime) {
                fireRange(prevTime, curTime);
        } else {
                // Loop wrap: fire prevTime→duration, then 0→curTime
                fireRange(prevTime, clip.Duration());
                fireRange(0.0f, curTime);
        }
}

} // namespace SE

#endif
