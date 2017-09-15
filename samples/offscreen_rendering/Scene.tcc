
namespace SAMPLES {


Scene::Scene(const Settings & oSettings, SE::Camera & oCurCamera) : 
        oCamera(oCurCamera),
        pTex01(SE::TResourceManager::Instance().Create<SE::TTexture>("resource/texture/tst_01.tga")),
        pTex02(SE::TResourceManager::Instance().Create<SE::TTexture>("resource/texture/tst_01.png")) {
  

                pTestMesh = SE::TResourceManager::Instance().Create<SE::TMesh>("resource/Maya/proto_full_body.obj");
                printf("Scene::Scene: pTestMesh: shape cnt = %u, tringles cnt = %u\n", pTestMesh->GetShapesCnt(), pTestMesh->GetTrianglesCnt());
                
                pTestMesh2 = SE::TResourceManager::Instance().Create<SE::TMesh>("resource/mesh/tests/test_ship01.obj");

                Setup();

                const glm::vec3 & center = pTestMesh->GetCenter();
                //oCurCamera.SetPos(5, 1, 1);
                //oCurCamera.SetPos(-0.512439, -100.100000, 12.530633);
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

        for (auto shape_ind : vOtherIndexes) {
                pTestMesh->Draw(shape_ind);
        }
        pTestMesh->Draw(vBodyIndexes[0]);
        
        pTestMesh2->Draw();

        glDisable(GL_TEXTURE_2D);

        pTestMesh->DrawBBox();
        pTestMesh2->DrawBBox();
}


void Scene::Setup() {

        SE::TMesh::TShapesInfo vInfo = pTestMesh->GetShapesInfo();
        size_t pos = 0;

        for (auto item : vInfo) {
                const std::string & sName = std::get<1>(item);
                //printf("shape: ind = %u, name = '%s'\n", std::get<0>(item), std::get<1>(item).c_str());
                pos = sName.find("sticker_body");
                if (pos != std::string::npos) {
                        vBodyIndexes.emplace_back(std::get<0>(item));
                }
                else {
                        vOtherIndexes.emplace_back(std::get<0>(item));
                }

        }
        printf("Scene::Setup: got %zu body shapes, and %zu other shapes\n", vBodyIndexes.size(), vOtherIndexes.size());
}



} //namespace SAMPLES 



