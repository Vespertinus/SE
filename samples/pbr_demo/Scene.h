#ifndef __PBR_SCENE_H__
#define __PBR_SCENE_H__

#include <VisualHelpers.h>

namespace SE {

class Scene {
        Camera     * pCamera    { nullptr };
        TSceneTree * pSceneTree { nullptr };

        std::vector<PointLight> vLights;
public:
        struct Settings { SE::Camera::Settings oCamSettings; };
        Scene(const Settings &);
        ~Scene() noexcept;
        void Process();
        void OnKeyDown(const Event & oEvent);
};

} // namespace SE

#include "Scene.tcc"
#endif
