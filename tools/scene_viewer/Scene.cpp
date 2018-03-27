#include <Global.h>
#include <GlobalTypes.h>
#include <Camera.h>
#include "Scene.h"


namespace SE {
namespace TOOLS {


Scene::Scene(const Settings & oSettings, SE::Camera & oCurCamera) :
        oCamera(oCurCamera),
        pSceneTree(SE::CreateResource<SE::TSceneTree>(oSettings.sScenePath)) {

                //TODO write bounding box class
                //BBox + Tranform -> BBox from SceneTree
                pSceneTree->Print();
}



Scene::~Scene() throw() { ;; }



void Scene::Process() {


        SE::HELPERS::DrawAxes(10);

        glEnable(GL_TEXTURE_2D);

        pSceneTree->Draw();

        glDisable(GL_TEXTURE_2D);

}


} //namespace SAMPLES
} //namespace SE



