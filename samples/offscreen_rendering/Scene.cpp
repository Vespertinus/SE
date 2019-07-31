
#include <Global.h>
#include <GlobalTypes.h>
#include <Logging.h>
#include <Camera.h>
#include "Scene.h"


namespace SAMPLES {


Scene::Scene(const Settings & oSettings) :
        pTex01(SE::CreateResource<SE::TTexture>("resource/texture/tst_01.tga")),
        pTex02(SE::CreateResource<SE::TTexture>("resource/texture/tst_01.png")),
        pSceneTree(SE::CreateResource<SE::TSceneTree>("resource/scene/test-01.sesc")) {

        auto pCameraNode = pSceneTree->Create("MainCamera");
        auto res = pCameraNode->CreateComponent<SE::Camera>(oSettings.oCamSettings);
        if (res != SE::uSUCCESS) {
                throw("failed to create Camera component");
        }
        pCamera = pCameraNode->GetComponent<SE::Camera>();
        se_assert(pCamera);

        pCamera->SetPos(8, 4, 8);
        pCamera->LookAt(0.1, 0.1, 0.1);

        SE::GetSystem<SE::TRenderer>().SetCamera(pCamera);

#ifdef SE_INTERACTIVE
        res = pCameraNode->CreateComponent<SE::BasicController>();
        if (res != SE::uSUCCESS) {
                throw("failed to create BasicController component");
        }
#endif

        /*
        const glm::vec3 & center = pTestMesh->GetCenter();
        oCurCamera.SetPos(center.x, center.y - 50, center.z);
        oCurCamera.LookAt(center);
        oCurCamera.ZoomTo(pTestMesh->GetBBox());
        */

        //auto pTestNode = pSceneTree->Create("test");
        //pTestNode->SetPos(glm::vec3(0, 0, 0));
        //pTestNode->SetScale(0.75);
        //pTestNode->SetRotation(glm::vec3(0, 0, 90));
        //pTestNode->AddRenderEntity(pTestMesh);

        pSceneTree->Print();
}



Scene::~Scene() noexcept { ;; }



void Scene::Process() {

        //SE::HELPERS::DrawAxes(10);

/*
        SE::HELPERS::DrawBox(2,
                             2,
                             2,
                             4,
                             0,
                             0,
                             pTex01->GetID());

        SE::HELPERS::DrawBox(2,
                             2,
                             2,
                             8,
                             0,
                             0,
                             pTex02->GetID());
*/

}

} //namespace SAMPLES



