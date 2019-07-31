

#ifndef __SCENE_H__
#define __SCENE_H__ 1

#include <VisualHelpers.h>


namespace SE {

class Scene {

        //DEBUG code ___Start___
        HELPERS::Elipse   oSmallElipse;
        HELPERS::Elipse   oBigElipse;
        //DEBUG code ___End_____

        Camera          * pCamera;
        TSceneTree      * pSceneTree;

        public:
        //empty settings
        struct Settings {
                SE::Camera::Settings oCamSettings;
        };

        Scene(const Settings & oSettings);
        ~Scene() noexcept;

        void Process();

};

} //namespace SE

#include "Scene.tcc"

#endif
