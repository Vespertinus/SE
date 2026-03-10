#ifndef __PBR_SCENE_H__
#define __PBR_SCENE_H__

#include <VisualHelpers.h>

namespace SE {

class Scene {
        Camera     * pCamera    { nullptr };
        TSceneTree * pSceneTree { nullptr };
public:
        struct Settings { SE::Camera::Settings oCamSettings; };
        Scene(const Settings &);
        ~Scene() noexcept;
        void Process();
};

} // namespace SE

#include "Scene.tcc"
#endif
