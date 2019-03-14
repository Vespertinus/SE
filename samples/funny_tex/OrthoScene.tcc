
namespace FUNNY_TEX {


OrthoScene::OrthoScene(const Settings & oSettings, SE::Camera & oCurCamera) :
        pSceneTree(SE::CreateResource<SE::TSceneTree>("funny_tex scene", true)),
        oCamera(oCurCamera) {

        using namespace SE;


        auto pPlaneMesh = CreateResource<TMesh>(GetSystem<Config>().sResourceDir + "mesh/unit_plane.sems");
        pMaterial       = CreateResource<SE::Material>(GetSystem<Config>().sResourceDir + "material/julia_fractal.semt");

        auto pPlaneNode = pSceneTree->Create("plane");

        auto res = pPlaneNode->CreateComponent<StaticModel>(true, pPlaneMesh, pMaterial);
        if (res != uSUCCESS) {
                throw(std::runtime_error("failed to create StaticModel"));
        }
        pModel = pPlaneNode->GetComponent<StaticModel>();
        se_assert(pModel);


        const auto & cam_data = oCamera.GetSettings();
        pPlaneNode->SetScale(glm::vec3(cam_data.width / 100.0f, cam_data.height / 100.0f, 1.0f));

        oCamera.SetPos(0, 0, 10);
        oCamera.SetRotation(0, 0, 0);
        oCamera.ZoomTo(cam_data.width / 100.0f);
        //oCamera.SetZoom(0.5);
}

OrthoScene::~OrthoScene() throw() { ;; }


void OrthoScene::Process() {

        SE::TGraphicsState::Instance().SetViewProjection(oCamera.GetMVPMatrix());

        float     t = oElapsed.Get() / 10000.0f;
        glm::vec2 CSeed;
        CSeed.x = (sin(cos(t / 10) * 10) + cos(t * 2.0) / 4.0 + sin(t * 3.0) / 6.0) * 0.8;
        CSeed.y = (cos(sin(t / 10) * 10) + sin(t * 2.0) / 4.0 + cos(t * 3.0) / 6.0) * 0.8;

        static SE::StrID seed_id("CSeed");
        pMaterial->SetVariable(seed_id, CSeed);

}

void OrthoScene::PostRender() {}


} //namespace FUNNY_TEX


