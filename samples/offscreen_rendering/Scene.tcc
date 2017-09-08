
namespace SAMPLES {


Scene::Scene(const Settings & oSettings, SE::Camera & oCurCamera) : 
        oCamera(oCurCamera),
        pTex01(SE::TResourceManager::Instance().Create<SE::TTexture>("resource/texture/tst_01.tga")) {
  
                oCurCamera.SetPos(5, 1, 1);    
                oCurCamera.LookAt(0.01, 0.01, 0.01);

//                SE::TMesh * pTestMesh = SE::TResourceManager::Instance().Create<SE::TMesh>("resource/mesh/pony-cartoon/source/Pony_cartoon.obj");
//                printf("Scene::Scene: pTestMesh: shape cnt = %u, tringles cnt = %u\n", pTestMesh->GetShapesCnt(), pTestMesh->GetTrianglesCnt());
}



Scene::~Scene() throw() { ;; }



void Scene::Process() {
        

        SE::HELPERS::DrawAxes(10);

        glEnable(GL_TEXTURE_2D);
/*
        SE::HELPERS::DrawPlane(4, 4, 
                        5, 0, 0,
                        1, 1, 1,
                        pTex01->GetID());
*/        
        SE::HELPERS::DrawBox(2,
                             2,
                             2,
                             0,
                             0,
                             0,
                             pTex01->GetID());


        glDisable(GL_TEXTURE_2D);
}


} //namespace FUNNY_TEX



