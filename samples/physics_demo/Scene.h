#ifndef __PHYSICS_DEMO_SCENE_H__
#define __PHYSICS_DEMO_SCENE_H__

#include <VisualHelpers.h>
#include <glm/vec3.hpp>

#include "PhysicsDebugController.h"

namespace SE {

class Scene {

        Camera      * pCamera    { nullptr };
        TSceneTree  * pSceneTree { nullptr };

        PhysicsDebugController oPhysDbg;

        TSceneTree::TSceneNodeExact * pBallNode { nullptr };
        TSceneTree::TSceneNodeExact * pKinNode  { nullptr };
        TSceneTree::TSceneNodeExact * pSpinNode { nullptr };

        BodyHandle  hBallBody;
        BodyHandle  hGoalBody;
        BodyHandle  hDeathBody;

        glm::vec3   vBallStart;
        float       time { 0.0f };

        void OnUpdate      (const Event & oEvent);
        void OnTriggerEnter(const Event & oEvent);
        void OnTriggerFire (const Event & oEvent);
        void OnKeyDown(const Event & oEvent);
        void ResetBall();

public:
        struct Settings { SE::Camera::Settings oCamSettings; };

        Scene(const Settings &);
        ~Scene() noexcept;
        void Process();
};

} // namespace SE

#include "Scene.tcc"
#endif
