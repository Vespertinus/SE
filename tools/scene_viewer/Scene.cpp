#include <Global.h>
#include <GlobalTypes.h>
#include <Camera.h>
#include "Scene.h"


namespace SE {
namespace TOOLS {


Scene::Scene(const Settings & oNewSettings, SE::Camera & oCurCamera) :
        oCamera(oCurCamera),
        pSceneTree(SE::CreateResource<SE::TSceneTree>(oNewSettings.sScenePath)),
        oSettings(oNewSettings) {

        oCamera.SetPos(8, 8, 4);
        oCamera.LookAt(0.1, 0.1, 0.1);

        //TODO write bounding box class
        //BBox + Tranform -> BBox from SceneTree
        pSceneTree->Print();
}



Scene::~Scene() throw() { ;; }



void Scene::Process() {

        SE::HELPERS::DrawAxes(10);

        //glEnable(GL_TEXTURE_2D);

        TRenderState::Instance().SetViewProjection(oCamera.GetMVPMatrix());

        /*if (oSettings.vdebug) {

                pSceneTree->GetRoot()->DepthFirstWalk([](SE::TSceneTree::TSceneNode & oNode) {

                        if (oNode.GetEntityCnt() == 0) {
                                return true;
                        }

                        auto * pMesh = oNode.GetEntity<TMesh>(0);
                        pMesh->DrawBBox();

                        return true;
                });
        }*/

        pSceneTree->Draw();

        if (oSettings.vdebug) {

                pSceneTree->GetRoot()->DepthFirstWalk([](SE::TSceneTree::TSceneNode & oNode) {

                        if (oNode.GetEntityCnt() == 0) {
                                return true;
                        }

                        TRenderState::Instance().SetTransform(oNode.GetTransform().GetWorld());
                        TVisualHelpers::Instance().DrawLocalAxes();

                        return true;
                });
        }

        //glDisable(GL_TEXTURE_2D);

}


} //namespace SAMPLES
} //namespace SE




