#include <experimental/string_view>
namespace SAMPLES {


Scene::Scene(const Settings & oSettings, SE::Camera & oCurCamera) : 
        oCamera(oCurCamera),
        pTex01(SE::TResourceManager::Instance().Create<SE::TTexture>("resource/texture/tst_01.tga")),
        pTex02(SE::TResourceManager::Instance().Create<SE::TTexture>("resource/texture/tst_01.png")) {
  

                pTestMesh = SE::TResourceManager::Instance().Create<SE::TMesh>("resource/mesh/tests/test_ship01.obj");
                log_d("pTestMesh: shape cnt = {}, tringles cnt = {}", pTestMesh->GetShapesCnt(), pTestMesh->GetTrianglesCnt());
                
                const glm::vec3 & center = pTestMesh->GetCenter();
                oCurCamera.SetPos(center.x, center.y - 50, center.z);
                oCurCamera.LookAt(center);
                oCurCamera.ZoomTo(pTestMesh->GetBBox());
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

        
        pTestMesh->Draw();

        glDisable(GL_TEXTURE_2D);

        pTestMesh->DrawBBox();
}


} //namespace SAMPLES 



