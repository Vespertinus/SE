
#include <Global.h>
#include <GlobalTypes.h>
#include <Camera.h>
#include "Scene.h"


namespace SAMPLES {


Scene::Scene(const Settings & oSettings, SE::Camera & oCurCamera) :
        oCamera(oCurCamera),
        pTex01(SE::CreateResource<SE::TTexture>("resource/texture/tst_01.tga")),
        pTex02(SE::CreateResource<SE::TTexture>("resource/texture/tst_01.png")),
        pSceneTree(SE::CreateResource<SE::TSceneTree>("resource/scene/test-01.sesc")) {


                //external material: false
                SE::MeshSettings        oMeshSettings {0};
                SE::OBJLoader::Settings oLoaderSettings;
                //skip normals loading:
                oLoaderSettings.skip_normals   = false;
                oLoaderSettings.oTex2DSettings = SE::StoreTexture2D::Settings(false);

                //flip texture coordinates: 0 original, 1 u flip, 2 v flip
                oLoaderSettings.mShapesOptions.emplace("ship_Cube", SE::OBJLoader::Settings::ShapeSettings{0});

                pTestMesh = SE::TResourceManager::Instance().Create<SE::TMesh>("resource/mesh/tests/test_ship01.obj",
                                                                               oLoaderSettings,
                                                                               oMeshSettings);

                log_d("pTestMesh: shape cnt = {}, tringles cnt = {}", pTestMesh->GetShapesCnt(), pTestMesh->GetTrianglesCnt());

                const glm::vec3 & center = pTestMesh->GetCenter();
                oCurCamera.SetPos(center.x, center.y - 50, center.z);
                oCurCamera.LookAt(center);
                oCurCamera.ZoomTo(pTestMesh->GetBBox());


                auto pTestNode = pSceneTree->Create("test");
                //pTestNode->SetPos(glm::vec3(0, 0, 0));
                //pTestNode->SetScale(0.75);
                //pTestNode->SetRotation(glm::vec3(0, 0, 90));
                pTestNode->AddRenderEntity(pTestMesh);

                pSceneTree->Print();
}



Scene::~Scene() throw() { ;; }



void Scene::Process() {


        SE::HELPERS::DrawAxes(10);

        glEnable(GL_TEXTURE_2D);

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


        pSceneTree->Draw();

        glDisable(GL_TEXTURE_2D);

        pTestMesh->DrawBBox();
}


} //namespace SAMPLES



