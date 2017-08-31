

#ifndef __OFF_SCREEN_SCENE_H__
#define __OFF_SCREEN_SCENE_H__ 1

#include <VisualHelpers.h>

namespace SAMPLES {

class Scene {

        SE::Camera & oCamera;
        SE::TTexture        * pTex01;

        public:
        //empty settings
        struct Settings {};

        Scene(const Settings & oSettings, SE::Camera & oCurCamera);
        ~Scene() throw();

        void Process();
};


} //namespace SAMPLES

#include "Scene.tcc"

#endif

