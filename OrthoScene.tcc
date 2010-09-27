
namespace FUNNY_TEX {


OrthoScene::OrthoScene(const Settings & oSettings) : 
  pTex01(SE::TResourceManager::Instance().Create<SE::TTexture>("resource/texture/tst_01.tga")) {
  

}
OrthoScene::~OrthoScene() throw() { ;; }



void OrthoScene::Process() {

  SE::HELPERS::DrawAxes(10);

  SE::HELPERS::DrawPlane(4, 4, 
                     -1, 2, 0.25,
                     1, 1, 1,
                     pTex01->GetID());
}


} //namespace FUNNY_TEX


