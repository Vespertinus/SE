#ifndef __BALL_CONTROLLER_H__
#define __BALL_CONTROLLER_H__

#include <string>
#include <glm/vec3.hpp>

namespace SE {

class BallController {

        TSceneTree::TSceneNodeExact * pNode;
        BodyHandle                    hBody;
        glm::vec3                     vStart;
        float                         impulse { 1.0f };

        void OnUpdate (const Event & oEvent);
        void OnKeyDown(const Event & oEvent);

public:
        BallController(TSceneTree::TSceneNodeExact * pNewNode, const glm::vec3 & vStartPos);
        ~BallController() noexcept;

        void                    Enable();
        void                    Disable();
        std::string             Str() const;
        void                    DrawDebug() const;
};

} // namespace SE

#endif
