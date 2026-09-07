
#ifndef SE_TEST_POSE_ASSERT_H
#define SE_TEST_POSE_ASSERT_H

#include <cmath>
#include <cstdint>

#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <AnimEvaluator.h>

namespace se_test {

// The engine builds with -ffast-math globally, so exact float comparison is
// meaningless. These are the harness-wide tolerances.
constexpr float kPoseEps   = 1e-5f;   // pose component comparison
constexpr float kTimeEps   = 1e-4f;   // playback time / transition progress
constexpr float kWeightEps = 1e-4f;   // blend weights

// Quaternion equality up to sign: q and -q represent the same rotation and
// slerp/renormalize paths may legitimately flip the sign.
inline ::testing::AssertionResult QuatNear(const glm::quat& a, const glm::quat& b,
                                           float eps = kPoseEps) {
        const float dot = std::abs(glm::dot(a, b));
        if (dot >= 1.0f - 2.0f * eps) {
                return ::testing::AssertionSuccess();
        }
        return ::testing::AssertionFailure()
                << "quats differ: (" << a.x << "," << a.y << "," << a.z << "," << a.w
                << ") vs (" << b.x << "," << b.y << "," << b.z << "," << b.w
                << ") (|dot|=" << dot << ")";
}

inline ::testing::AssertionResult VecNear(const glm::vec3& a, const glm::vec3& b,
                                          float eps = kPoseEps) {
        const glm::vec3 vDiff = glm::abs(a - b);
        if (vDiff.x <= eps && vDiff.y <= eps && vDiff.z <= eps) {
                return ::testing::AssertionSuccess();
        }
        return ::testing::AssertionFailure()
                << "vectors differ: (" << a.x << "," << a.y << "," << a.z
                << ") vs (" << b.x << "," << b.y << "," << b.z << ")";
}

// Full-pose comparison. Both poses must describe the same skeleton.
inline ::testing::AssertionResult PoseNear(const SE::LocalPose& a, const SE::LocalPose& b,
                                           float eps = kPoseEps) {
        if (a.bone_count != b.bone_count) {
                return ::testing::AssertionFailure()
                        << "bone count differs: " << a.bone_count << " vs " << b.bone_count;
        }
        for (uint32_t i = 0; i < a.bone_count; ++i) {
                auto oPos = VecNear(a.pPos[i], b.pPos[i], eps);
                if (!oPos) return ::testing::AssertionFailure() << "bone " << i << " pos: " << oPos.message();
                auto oRot = QuatNear(a.pRot[i], b.pRot[i], eps);
                if (!oRot) return ::testing::AssertionFailure() << "bone " << i << " rot: " << oRot.message();
                auto oScl = VecNear(a.pScl[i], b.pScl[i], eps);
                if (!oScl) return ::testing::AssertionFailure() << "bone " << i << " scl: " << oScl.message();
        }
        return ::testing::AssertionSuccess();
}

// Pose against the skeleton's bind pose.
inline ::testing::AssertionResult PoseNearBind(const SE::LocalPose& a,
                                               const SE::Skeleton& oSkeleton,
                                               float eps = kPoseEps) {
        if (a.bone_count != oSkeleton.BoneCount()) {
                return ::testing::AssertionFailure()
                        << "bone count differs: " << a.bone_count << " vs " << oSkeleton.BoneCount();
        }
        const auto& vBones = oSkeleton.Bones();
        for (uint32_t i = 0; i < a.bone_count; ++i) {
                auto oPos = VecNear(a.pPos[i], vBones[i].bindPos, eps);
                if (!oPos) return ::testing::AssertionFailure() << "bone " << i << " pos: " << oPos.message();
                auto oRot = QuatNear(a.pRot[i], vBones[i].bindRot, eps);
                if (!oRot) return ::testing::AssertionFailure() << "bone " << i << " rot: " << oRot.message();
                auto oScl = VecNear(a.pScl[i], vBones[i].bindScale, eps);
                if (!oScl) return ::testing::AssertionFailure() << "bone " << i << " scl: " << oScl.message();
        }
        return ::testing::AssertionSuccess();
}

#define EXPECT_POSE_NEAR(a, b)        EXPECT_TRUE(se_test::PoseNear((a), (b)))
#define EXPECT_POSE_NEAR_BIND(a, skel) EXPECT_TRUE(se_test::PoseNearBind((a), (skel)))
#define EXPECT_VEC_NEAR(a, b)         EXPECT_TRUE(se_test::VecNear((a), (b)))
#define EXPECT_QUAT_NEAR(a, b)        EXPECT_TRUE(se_test::QuatNear((a), (b)))

} // namespace se_test

#endif // SE_TEST_POSE_ASSERT_H
