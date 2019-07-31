


namespace SE {

Scene::Scene(const Settings & oSettings) :
        oSmallElipse(0, 0, 2, 10, 36),
        oBigElipse(0, 0, 2, 100, 36),
        pSceneTree(CreateResource<TSceneTree>("resource/scene/test-01.sesc")) {
        
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

        res = pCameraNode->CreateComponent<SE::BasicController>();
        if (res != SE::uSUCCESS) {
                throw("failed to create BasicController component");
        }

        pSceneTree->Print();
}



Scene::~Scene() noexcept { ;; }



void Scene::Process() {

        HELPERS::DrawAxes(10);
        oSmallElipse.Draw();
        oBigElipse.Draw();

}

} //namespace SE
