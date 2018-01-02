
namespace FUNNY_TEX {


OrthoScene::OrthoScene(const Settings & oSettings, SE::Camera & oCurCamera) : 
        pTex01(SE::TResourceManager::Instance().Create<SE::TTexture>(
                                "resource/texture/_MG_1252.png",
                                SE::StoreTexture2D::Settings(false),
                                SE::OpenCVImgLoader::Settings()
                                )),
        oCamera(oCurCamera) {
  
        oCamera.SetPos(0, -50, 0);
        oCamera.LookAt(0, 0, 0);

}



OrthoScene::~OrthoScene() throw() { ;; }



void OrthoScene::Process() {

        SE::HELPERS::DrawAxes(10);

        glEnable(GL_TEXTURE_2D);

        /*
        auto & oCamSettings = oCamera.GetSettings();
        
        SE::HELPERS::DrawPlane(oCamSettings.width, oCamSettings.height,
                        -oCamSettings.width / 2, 0, -oCamSettings.height / 2,
                        1, 0, 1,
                        pTex01->GetID());
        */
        SE::HELPERS::DrawPlane(1024, 1024,
                        -512, 0, -512,
                        1, 0, 1,
                        pTex01->GetID());

        glDisable(GL_TEXTURE_2D);
}


} //namespace FUNNY_TEX


