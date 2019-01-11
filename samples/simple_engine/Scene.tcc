


namespace SE {

Scene::Scene(const Settings & oSettings, Camera & oCurCamera) :
        oSmallElipse(0, 0, 2, 10, 36),
        oBigElipse(0, 0, 2, 100, 36),
        oCamera(oCurCamera),
        pSceneTree(CreateResource<TSceneTree>("resource/scene/test-01.sesc")) {

        pSceneTree->Print();
}



Scene::~Scene() throw() { ;; }



void Scene::Process() {

        TGraphicsState::Instance().SetViewProjection(oCamera.GetMVPMatrix());

        HELPERS::DrawAxes(10);
        oSmallElipse.Draw();
        oBigElipse.Draw();

}

void Scene::PostRender() {

}





} //namespace SE
