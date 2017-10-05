

#ifndef __ORTHO_SCENE_H__
#define __ORTHO_SCENE_H__ 1

#include <VisualHelpers.h>


namespace FUNNY_TEX {

class OrthoScene {

        SE::TTexture        * pTex01;


	public:
        //empty settings
        struct Settings {};

        OrthoScene(const Settings & oSettings, SE::Camera & oCurCamera);
        ~OrthoScene() throw();

        void Process();
};


} //namespace FUNNY_TEX

#include "OrthoScene.tcc"

#endif
