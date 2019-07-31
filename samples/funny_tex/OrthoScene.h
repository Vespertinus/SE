
#ifndef __ORTHO_SCENE_H__
#define __ORTHO_SCENE_H__ 1

#include <VisualHelpers.h>


namespace FUNNY_TEX {

class OrthoScene {

        SE::Material    * pMaterial;
        SE::StaticModel * pModel;
        SE::TSceneTree  * pSceneTree;
        SE::Camera      * pCamera;
        SE::CalcDuration  oElapsed;

	public:
        struct Settings {
                SE::Camera::Settings oCamSettings;
        };

        OrthoScene(const Settings & oSettings);
        ~OrthoScene() throw();

        void Process();
};


} //namespace FUNNY_TEX

#include "OrthoScene.tcc"

#endif
