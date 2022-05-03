
namespace FUNNY_TEX {


OrthoScene::OrthoScene(const Settings & oSettings) :
        pSceneTree(SE::CreateResource<SE::TSceneTree>("funny_tex scene", true)) {

        using namespace SE;

        auto pPlaneMesh = CreateResource<TMesh>(GetSystem<Config>().sResourceDir + "mesh/unit_plane.sems");
        pMaterial       = CreateResource<SE::Material>(GetSystem<Config>().sResourceDir + "material/julia_fractal.semt");

        auto pPlaneNode = pSceneTree->Create("plane");

        auto res = pPlaneNode->CreateComponent<StaticModel>(pPlaneMesh, pMaterial);
        if (res != uSUCCESS) {
                throw(std::runtime_error("failed to create StaticModel"));
        }
        pModel = pPlaneNode->GetComponent<StaticModel>();
        se_assert(pModel);

        auto pCameraNode = pSceneTree->Create("MainCamera");
        res = pCameraNode->CreateComponent<SE::Camera>(oSettings.oCamSettings);
        if (res != SE::uSUCCESS) {
                throw("failed to create Camera component");
        }
        pCamera = pCameraNode->GetComponent<SE::Camera>();
        se_assert(pCamera);

        auto screen_size = GetSystem<GraphicsState>().GetScreenSize();

        pCamera->SetPos(0, 0, 10);
        pCamera->SetRotation(0, 0, 0);
        pCamera->ZoomTo(screen_size.x / 100.0f);
        //pCamera.SetZoom(0.5);

        SE::GetSystem<SE::TRenderer>().SetCamera(pCamera);

        res = pCameraNode->CreateComponent<SE::BasicController>();
        if (res != SE::uSUCCESS) {
                throw("failed to create BasicController component");
        }

        pPlaneNode->SetScale(glm::vec3(screen_size.x / 100.0f, screen_size.y / 100.0f, 1.0f));

}

OrthoScene::~OrthoScene() throw() { ;; }


void OrthoScene::Process() {

        const auto & oState = SE::GetSystem<SE::GraphicsState>().GetFrameState();

        float     t = oElapsed.Get() / 10000.0f;
        glm::vec2 CSeed;
        CSeed.x = (sin(cos(t / 10) * 10) + cos(t * 2.0) / 4.0 + sin(t * 3.0) / 6.0) * 0.8;
        CSeed.y = (cos(sin(t / 10) * 10) + sin(t * 2.0) / 4.0 + cos(t * 3.0) / 6.0) * 0.8;

        if ((oState.frame_num % 60) == 0) {
                log_d("seed: ({}, {}), t: {}, frame: {}", CSeed.x, CSeed.y, t, oState.frame_num);
        }

        static SE::StrID seed_id("CSeed");
        pMaterial->SetVariable(seed_id, CSeed);

}

} //namespace FUNNY_TEX


