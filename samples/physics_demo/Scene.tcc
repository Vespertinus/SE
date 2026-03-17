
#include <cmath>

#include "MeshBuilders.h"
#include "../pbr_demo/SphereBuilder.h"


/* PhysicsDebugController:
- P — toggle pause
- N — step one frame (only when paused)
*/

namespace SE {

// ---------------------------------------------------------------------------
// Helper: 1×1 pixel procedural texture
// ---------------------------------------------------------------------------
static H<TTexture> CreateTex1x1pd(const char * name, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {

        uint8_t pixels[4] = { r, g, b, a };
        TextureStock oStock;
        oStock.raw_image        = pixels;
        oStock.raw_image_size   = 4;
        oStock.format           = GL_RGBA;
        oStock.internal_format  = GL_RGBA8;
        oStock.width            = 1;
        oStock.height           = 1;
        return CreateResource<TTexture>(name, oStock);
}

// ---------------------------------------------------------------------------
// Scene constructor
// ---------------------------------------------------------------------------
Scene::Scene(const Settings & oSettings) :
        pSceneTree(CreateRawResource<TSceneTree>("physics_demo", true)) {

        // -----------------------------------------------------------------------
        // Shared meshes
        // -----------------------------------------------------------------------
        auto hSphereMesh = CreateSphereMesh(16, 32);                    // unit sphere (radius 1)
        auto hBoxMesh    = CreateBoxMesh({1.0f, 1.0f, 1.0f}, "box");   // unit box (half 1,1,1)
        (void)CreatePlaneMesh(1.0f, 1.0f, "plane");        // unit plane (1×1) — registered for future use

        // -----------------------------------------------------------------------
        // Shared 1×1 textures
        // -----------------------------------------------------------------------
        auto hWhiteTex  = CreateTex1x1pd("pd_white",   255, 255, 255, 255);
        auto hNormalTex = CreateTex1x1pd("pd_normal",  128, 128, 255, 255);
        auto hSpecTex   = CreateTex1x1pd("pd_spec",      0, 255, 255, 255);

        // -----------------------------------------------------------------------
        // Materials
        // -----------------------------------------------------------------------
        auto LoadMat = [&](const char * path) -> H<Material> {
                auto hMat = CreateResource<Material>(path);
                auto * pMat = GetResource(hMat);
                pMat->SetTexture(TextureUnit::DIFFUSE,  hWhiteTex);
                pMat->SetTexture(TextureUnit::NORMAL,   hNormalTex);
                pMat->SetTexture(TextureUnit::SPECULAR, hSpecTex);
                return hMat;
        };

        auto hMatStone = LoadMat("resource/material/pbr_rough_dielectric.semt");  // floor / ramp / walls
        auto hMatWood  = LoadMat("resource/material/pbr_rough_metal.semt");       // static platforms
        auto hMatBall  = LoadMat("resource/material/pbr_demo.semt");              // marble (gold)
        auto hMatGoal  = LoadMat("resource/material/pbr_emissive.semt");          // goal (blue glow)
        auto hMatKin   = LoadMat("resource/material/pbr_smooth_dielectric.semt"); // kinematic platform

        // -----------------------------------------------------------------------
        // Ball start position
        // -----------------------------------------------------------------------
        vBallStart = { -8.0f, 3.0f, 0.0f };

        // -----------------------------------------------------------------------
        // Static floor (half 10×0.5×5, scale node to match)
        // -----------------------------------------------------------------------
        {
                const glm::vec3 oHalf { 10.0f, 0.5f, 5.0f };
                const glm::vec3 oPos  {  0.0f, -0.5f, 0.0f };

                auto pNode = pSceneTree->Create("Floor");
                pNode->SetPos(oPos);
                pNode->SetScale(oHalf);           // unit box scaled to half-extents
                pNode->CreateComponent<StaticModel>(hBoxMesh, hMatStone);

                RigidBodyDesc desc;
                desc.oCollider.type         = ColliderDesc::Box;
                desc.oCollider.vHalfExtents = oHalf;
                desc.vInitialPosition       = oPos;
                desc.is_static              = true;
                pNode->CreateComponent<RigidBody>(desc);
        }

        // -----------------------------------------------------------------------
        // Start platform (elevated, static)
        // -----------------------------------------------------------------------
        {
                const glm::vec3 oHalf { 2.0f, 0.5f, 2.0f };
                const glm::vec3 oPos  { -8.0f, 0.5f, 0.0f };

                auto pNode = pSceneTree->Create("StartPlatform");
                pNode->SetPos(oPos);
                pNode->SetScale(oHalf);
                pNode->CreateComponent<StaticModel>(hBoxMesh, hMatWood);

                RigidBodyDesc desc;
                desc.oCollider.type         = ColliderDesc::Box;
                desc.oCollider.vHalfExtents = oHalf;
                desc.vInitialPosition       = oPos;
                desc.is_static              = true;
                pNode->CreateComponent<RigidBody>(desc);
        }

        // -----------------------------------------------------------------------
        // Angled ramp (static, tilted ~20° around Z to connect platform to floor)
        // -----------------------------------------------------------------------
        {
                const glm::vec3 oHalf { 3.0f, 0.2f, 2.0f };
                const glm::vec3 oPos  { -4.5f, 0.5f, 0.0f };

                auto pNode = pSceneTree->Create("Ramp");
                pNode->SetPos(oPos);
                pNode->SetScale(oHalf);
                pNode->SetRotation(glm::vec3(0.0f, 0.0f, -20.0f));   // tilt 20° downward toward +X
                pNode->CreateComponent<StaticModel>(hBoxMesh, hMatStone);

                RigidBodyDesc desc;
                desc.oCollider.type         = ColliderDesc::Box;
                desc.oCollider.vHalfExtents = oHalf;
                desc.vInitialPosition       = oPos;
                // Initial rotation for physics: 20° around -Z
                desc.qInitialRotation       = glm::angleAxis(glm::radians(-20.0f), glm::vec3(0,0,1));
                desc.is_static              = true;
                pNode->CreateComponent<RigidBody>(desc);
        }

        // -----------------------------------------------------------------------
        // Elevated mid-platform (static)
        // -----------------------------------------------------------------------
        {
                const glm::vec3 oHalf { 2.0f, 0.25f, 2.0f };
                const glm::vec3 oPos  { 1.0f, 1.0f, 0.0f };

                auto pNode = pSceneTree->Create("MidPlatform");
                pNode->SetPos(oPos);
                pNode->SetScale(oHalf);
                pNode->CreateComponent<StaticModel>(hBoxMesh, hMatWood);

                RigidBodyDesc desc;
                desc.oCollider.type         = ColliderDesc::Box;
                desc.oCollider.vHalfExtents = oHalf;
                desc.vInitialPosition       = oPos;
                desc.is_static              = true;
                pNode->CreateComponent<RigidBody>(desc);
        }

        // -----------------------------------------------------------------------
        // Goal platform (static, emissive)
        // -----------------------------------------------------------------------
        {
                const glm::vec3 oHalf { 2.0f, 0.25f, 2.0f };
                const glm::vec3 oPos  { 11.0f, 1.0f, 0.0f };

                auto pNode = pSceneTree->Create("GoalPlatform");
                pNode->SetPos(oPos);
                pNode->SetScale(oHalf);
                pNode->CreateComponent<StaticModel>(hBoxMesh, hMatGoal);

                RigidBodyDesc desc;
                desc.oCollider.type         = ColliderDesc::Box;
                desc.oCollider.vHalfExtents = oHalf;
                desc.vInitialPosition       = oPos;
                desc.is_static              = true;
                pNode->CreateComponent<RigidBody>(desc);
        }

        // -----------------------------------------------------------------------
        // Ball (dynamic)
        // -----------------------------------------------------------------------
        {
                auto hBallNode = pSceneTree->Create("Ball");
                pBallNode = hBallNode.get();
                pBallNode->SetPos(vBallStart);
                pBallNode->SetScale(glm::vec3(0.5f));   // sphere mesh radius 1 → visual radius 0.5
                pBallNode->CreateComponent<StaticModel>(hSphereMesh, hMatBall);

                RigidBodyDesc desc;
                desc.oCollider.type     = ColliderDesc::Sphere;
                desc.oCollider.radius   = 0.5f;
                desc.vInitialPosition   = vBallStart;
                desc.mass               = 1.0f;
                desc.friction           = 0.5f;
                desc.restitution        = 0.2f;
                desc.linear_damping     = 0.1f;
                desc.angular_damping    = 0.1f;
                pBallNode->CreateComponent<RigidBody>(desc);

                hBallBody = pBallNode->GetComponent<RigidBody>()->GetHandle();

                pBallNode->CreateComponent<BallController>(vBallStart);
        }

        // -----------------------------------------------------------------------
        // Kinematic oscillating platform
        // -----------------------------------------------------------------------
        {
                const glm::vec3 oHalf { 1.5f, 0.2f, 1.5f };
                const glm::vec3 oBase { 5.5f, 2.5f, 0.0f };

                auto hKinNode = pSceneTree->Create("KinPlatform");
                pKinNode = hKinNode.get();
                pKinNode->SetPos(oBase);
                pKinNode->SetScale(oHalf);
                pKinNode->CreateComponent<StaticModel>(hBoxMesh, hMatKin);

                RigidBodyDesc desc;
                desc.oCollider.type         = ColliderDesc::Box;
                desc.oCollider.vHalfExtents = oHalf;
                desc.vInitialPosition       = oBase;
                desc.is_kinematic           = true;
                pKinNode->CreateComponent<RigidBody>(desc);
        }

        // -----------------------------------------------------------------------
        // Kinematic spinning bar obstacle
        // -----------------------------------------------------------------------
        {
                const glm::vec3 oHalf { 2.0f, 0.15f, 0.3f };
                const glm::vec3 oPos  { 8.5f, 2.0f,  0.0f };

                auto hSpinNode = pSceneTree->Create("SpinBar");
                pSpinNode = hSpinNode.get();
                pSpinNode->SetPos(oPos);
                pSpinNode->SetScale(oHalf);
                pSpinNode->CreateComponent<StaticModel>(hBoxMesh, hMatKin);

                RigidBodyDesc desc;
                desc.oCollider.type         = ColliderDesc::Box;
                desc.oCollider.vHalfExtents = oHalf;
                desc.vInitialPosition       = oPos;
                desc.is_kinematic           = true;
                pSpinNode->CreateComponent<RigidBody>(desc);
        }

        // -----------------------------------------------------------------------
        // Goal trigger zone (invisible — no StaticModel)
        // -----------------------------------------------------------------------
        {
                const glm::vec3 oHalf { 1.5f, 1.0f, 1.5f };
                const glm::vec3 oPos  { 11.0f, 2.0f, 0.0f };

                auto pNode = pSceneTree->Create("GoalTrigger");
                pNode->SetPos(oPos);

                RigidBodyDesc desc;
                desc.oCollider.type         = ColliderDesc::Box;
                desc.oCollider.vHalfExtents = oHalf;
                desc.vInitialPosition       = oPos;
                desc.is_trigger             = true;
                desc.is_static              = true;
                pNode->CreateComponent<RigidBody>(desc);

                hGoalBody = pNode->GetComponent<RigidBody>()->GetHandle();
        }

        // -----------------------------------------------------------------------
        // Death zone trigger (wide box far below)
        // -----------------------------------------------------------------------
        {
                const glm::vec3 oHalf { 50.0f, 1.0f, 50.0f };
                const glm::vec3 oPos  {  0.0f, -8.0f, 0.0f };

                auto pNode = pSceneTree->Create("DeathZone");
                pNode->SetPos(oPos);

                RigidBodyDesc desc;
                desc.oCollider.type         = ColliderDesc::Box;
                desc.oCollider.vHalfExtents = oHalf;
                desc.vInitialPosition       = oPos;
                desc.is_trigger             = true;
                desc.is_static              = true;
                pNode->CreateComponent<RigidBody>(desc);

                hDeathBody = pNode->GetComponent<RigidBody>()->GetHandle();
        }

        // -----------------------------------------------------------------------
        // Camera
        // -----------------------------------------------------------------------
        {
                auto pCamNode = pSceneTree->Create("Camera");
                pCamNode->CreateComponent<Camera>(oSettings.oCamSettings);
                pCamera = pCamNode->GetComponent<Camera>();
                pCamera->SetPos(vBallStart.x, vBallStart.y + 6.0f, vBallStart.z + 10.0f);
                pCamera->LookAt(vBallStart);
                GetSystem<TRenderer>().SetCamera(pCamera);

                //pCamNode->CreateComponent<BasicController>(); //DEBUG
        }

        // -----------------------------------------------------------------------
        // Lights
        // -----------------------------------------------------------------------
        {
                DirLight oDir;
                oDir.direction = glm::normalize(glm::vec3(-1.0f, -2.0f, -1.0f));
                oDir.intensity = 0.6f;
                GetSystem<TRenderer>().SetDirLight(oDir);

                // Warm light above start area
                PointLight oP1;
                oP1.position  = { -8.0f, 6.0f, 0.0f };
                oP1.color     = {  0.6f, 0.8f, 1.0f };
                oP1.intensity = 3.0f;
                oP1.radius    = 10.0f;
                GetSystem<TRenderer>().AddPointLight(oP1);

                // Glowing goal light
                PointLight oP2;
                oP2.position  = { 11.0f, 5.0f, 0.0f };
                oP2.color     = {  0.3f, 0.5f, 1.0f };
                oP2.intensity = 4.0f;
                oP2.radius    = 8.0f;
                GetSystem<TRenderer>().AddPointLight(oP2);
        }

        // -----------------------------------------------------------------------
        // Subscribe to events
        // -----------------------------------------------------------------------
        {
                auto & oEM = GetSystem<EventManager>();
                oEM.AddListener<EUpdate,               &Scene::OnUpdate>      (this);
                oEM.AddListener<EPhysicsTriggerEnter,  &Scene::OnTriggerEnter>(this);
                oEM.AddListener<EKeyDown,              &Scene::OnKeyDown>(this);
        }

        pSceneTree->Print();
        
        //oPhysDbg.SetPaused(true);
}

// ---------------------------------------------------------------------------
Scene::~Scene() noexcept {

        auto & oEM = GetSystem<EventManager>();
        oEM.RemoveListener<EUpdate,              &Scene::OnUpdate>      (this);
        oEM.RemoveListener<EPhysicsTriggerEnter, &Scene::OnTriggerEnter>(this);
        oEM.RemoveListener<EKeyDown,             &Scene::OnKeyDown>(this);
}

// ---------------------------------------------------------------------------
void Scene::OnUpdate(const Event & oEvent) {

        time += oEvent.Get<EUpdate>().last_frame_time;
        //log_d("OnUpdate: frame time: {}", time);
}

void Scene::OnKeyDown(const Event & oEvent) {

        auto   key = oEvent.Get<EKeyDown>().key;

        if (key == Keys::T) {
                pSceneTree->Print();
        }
}

// ---------------------------------------------------------------------------
void Scene::OnTriggerEnter(const Event & oEvent) {

        const auto & oEv = oEvent.Get<EPhysicsTriggerEnter>();
        if (oEv.hTrigger == hGoalBody || oEv.hTrigger == hDeathBody) {
                ResetBall();
        }
}

// ---------------------------------------------------------------------------
void Scene::ResetBall() {

        log_d("ResetBall");
        auto & oPS = GetSystem<PhysicsSystem>();
        oPS.Teleport(hBallBody, vBallStart, glm::quat{1.0f, 0.0f, 0.0f, 0.0f});
        oPS.SetLinearVelocity(hBallBody, glm::vec3{0.0f});
}

// ---------------------------------------------------------------------------
void Scene::Process() {

        // Kinematic oscillating platform
        {
                const float  speed  = 1.2f;
                const float  range  = 2.0f;
                const glm::vec3 oBase { 5.5f, 2.5f, 0.0f };
                pKinNode->SetPos(oBase + glm::vec3(std::sin(time * speed) * range, 0.0f, 0.0f));
        }
        // Kinematic spinning bar
        {
                pSpinNode->SetRotation(glm::vec3(0.0f, time * 90.0f, 0.0f));
        }

        // Camera tracking
        {
                //TODO pCamera as ball child node
                const glm::vec3 oBallPos = pBallNode->GetTransform().GetWorldPos();
                pCamera->SetPos(oBallPos.x, oBallPos.y + 6.0f, oBallPos.z + 10.0f);//
                pCamera->LookAt(oBallPos);
        }

        // Debug: collider wireframes + grid
        {
                /*
                pSceneTree->GetRoot()->DepthFirstWalk([](SE::TSceneTree::TSceneNodeExact & oNode) {
                        oNode.DrawDebug();
                        return true;
                });*/
                //GetSystem<DebugRenderer>().DrawGrid(pSceneTree->GetRoot()->GetTransform());
                GetSystem<PhysicsSystem>().DrawDebug();
        }
}

} // namespace SE
