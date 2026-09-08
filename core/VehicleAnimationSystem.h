
#ifndef __VEHICLE_ANIMATION_SYSTEM_H__
#define __VEHICLE_ANIMATION_SYSTEM_H__ 1

#include <cstdint>
#include <algorithm>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <AnimEvaluator.h>

namespace SE {

struct VehiclePhysicsState {

        struct WheelState {
                float angularVelocity  = 0.0f;  // rad/s
                float suspensionLength = 0.3f;  // current compression (metres)
        };
        WheelState wheels[4];
        float steeringAngle            = 0.0f;  // radians (front wheels)
        float lateralAcceleration      = 0.0f;  // m/s² (used for body roll)
        float longitudinalAcceleration = 0.0f;  // m/s² (used for body pitch)
};

struct VehicleSkeletonConfig {
        uint32_t bodyBone;
        uint32_t wheelBones[4];
        uint32_t suspensionBones[4];
        uint32_t frontLeftWheelBone;
        uint32_t frontRightWheelBone;
        float    restSuspensionLength = 0.3f;
        float    maxSuspensionTravel  = 0.15f;
};

class VehicleAnimationSystem {

        struct VehicleState {
                float wheelAngles[4] = {};  // accumulated rotation (radians)
        };
        std::unordered_map<uintptr_t, VehicleState> mStates;

public:
        // Update a vehicle's LocalPose from physics state each frame.
        // entityKey: unique ID per vehicle entity (use pointer cast to uintptr_t).
        void Update(
                        uintptr_t                    entityKey,
                        const VehiclePhysicsState&   phys,
                        const VehicleSkeletonConfig& cfg,
                        LocalPose&                   pose,
                        float                        dt)
        {
                auto& vs = mStates[entityKey];

                // Wheel rotation — integrate angular velocity from physics
                for (int i = 0; i < 4; ++i) {
                        vs.wheelAngles[i] += phys.wheels[i].angularVelocity * dt;
                        pose.pRot[cfg.wheelBones[i]] =
                                glm::angleAxis(vs.wheelAngles[i], glm::vec3(1.0f, 0.0f, 0.0f));
                }

                // Suspension compression — translate bone down proportionally
                for (int i = 0; i < 4; ++i) {
                        float compression = 1.0f - (phys.wheels[i].suspensionLength
                                        / cfg.restSuspensionLength);
                        compression = std::clamp(compression, 0.0f, 1.0f);
                        pose.pPos[cfg.suspensionBones[i]].y = -compression * cfg.maxSuspensionTravel;
                }

                // Steering — front wheel yaw
                glm::quat steer = glm::angleAxis(phys.steeringAngle, glm::vec3(0.0f, 1.0f, 0.0f));
                pose.pRot[cfg.frontLeftWheelBone]  = steer;
                pose.pRot[cfg.frontRightWheelBone] = steer;

                // Body roll from lateral acceleration; pitch from longitudinal
                float roll  = std::clamp(-phys.lateralAcceleration      * 0.008f,
                                -glm::radians(4.0f), glm::radians(4.0f));
                float pitch = std::clamp( phys.longitudinalAcceleration * 0.006f,
                                -glm::radians(3.0f), glm::radians(3.0f));
                pose.pRot[cfg.bodyBone] = glm::quat(glm::vec3(pitch, 0.0f, roll));
        }

        // Remove per-vehicle state when vehicle is destroyed.
        void RemoveVehicle(uintptr_t entityKey) {
                mStates.erase(entityKey);
        }
};

} // namespace SE

#endif // __VEHICLE_ANIMATION_SYSTEM_H__
