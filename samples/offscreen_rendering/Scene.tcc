
namespace SAMPLES {


Scene::Scene(const Settings & oSettings, SE::Camera & oCurCamera) : 
        oCamera(oCurCamera),
        pTex01(SE::TResourceManager::Instance().Create<SE::TTexture>("resource/texture/tst_01.tga")) {
  

                //oCamera.LookAt(5, 0, 0);
}



Scene::~Scene() throw() { ;; }



void Scene::Process() {
        

        SE::HELPERS::DrawAxes(10);

        SE::HELPERS::DrawPlane(4, 4, 
                        5, 0, 0,
                        1, 1, 1,
                        pTex01->GetID());
        //oCamera.LookAt(5, 0, 0);
}


} //namespace FUNNY_TEX



