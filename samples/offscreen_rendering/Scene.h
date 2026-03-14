
#ifndef __OFF_SCREEN_SCENE_H__
#define __OFF_SCREEN_SCENE_H__ 1

namespace SAMPLES {

class Scene {

        SE::Camera               * pCamera;
        SE::H<SE::TTexture>   hTex01;
        SE::H<SE::TTexture>   hTex02;
        SE::H<SE::TSceneTree> hSceneTree;

        public:
        struct Settings {

                SE::Camera::Settings oCamSettings;
        };

        Scene(const Settings & oSettings);
        ~Scene() noexcept;

        void Process();
};


} //namespace SAMPLES

#endif

