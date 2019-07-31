
#ifndef __OFF_SCREEN_SCENE_H__
#define __OFF_SCREEN_SCENE_H__ 1

namespace SAMPLES {

class Scene {

        SE::Camera      * pCamera;
        SE::TTexture    * pTex01;
        SE::TTexture    * pTex02;
        SE::TSceneTree  * pSceneTree;

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

