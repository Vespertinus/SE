
#ifndef __MOVING_ACTORS_SCENE_TCC__
#define __MOVING_ACTORS_SCENE_TCC__ 1

#include "Scene.h"

#include <Global.h>
#include <GlobalTypes.h>
#include <Camera.h>
#include <Light.h>
#include <CommonEvents.h>
#include <InputEvents.h>
#include <InputCodes.h>
#include <InputManager.h>
#include <GraphicsState.h>
#include <StaticModel.h>
#include <PhysicsTypes.h>
#include <RigidBody.h>
#include <AudioTypes.h>
#include <SoundEventSystem.h>
#include <SoundContextTypes.h>
#include <SoundContextTypes.tcc>
#include <MeshBuilder.h>
#include <MeshGen.h>
#include <VertexLayout.h>
#include <TextureBuilder.h>
#include <TriggerVolume.h>
#include <ui/UISystem.h>
#include <ui/UITypes.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>

#include <Animator.h>
#include <CharacterController.h>
#include <CharacterDebugger.h>
#include <CharacterMovementSystem.h>
#include <CharacterAnimationSystem.h>
#include <InputMappingContext.h>
#include <InputState.h>
#include <SprintStamina.h>
#include <AIBrain.h>
#include <MovementEvents.h>
#include <hsm/StateMachine.h>
#include <hsm/StateMachineDebugger.h>
#include <EntityManager.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/trigonometric.hpp>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cmath>

namespace SE {

// ---------------------------------------------------------------------------
// Local mesh creation helpers
// ---------------------------------------------------------------------------

namespace {

H<TMesh> MakeBoxMesh(glm::vec3 half, const char* name) {
        MeshBuilder oBuilder(VertexLayout::PosNormTanUV());
        MeshGen::Box(oBuilder, half);
        return oBuilder.Upload(name);
}

H<TMesh> MakeSphereMesh(float radius, int latDiv, int lonDiv, const char* name) {
        MeshBuilder oBuilder(VertexLayout::PosNormTanUV());
        MeshGen::Sphere(oBuilder, radius, latDiv, lonDiv);
        return oBuilder.Upload(name);
}

// First node in the subtree (root included) holding a component of type T.
// Robust against prefab child renames, unlike hard-coded full-name lookups.
template <class TComponent>
TSceneTree::TSceneNode FindNodeWithComponent(TSceneTree::TSceneNode pRoot) {
        TSceneTree::TSceneNode pFound;
        if (!pRoot) return pFound;
        pRoot->DepthFirstWalk([&pFound](auto& oNode) -> bool {
                if (oNode.template GetComponent<TComponent>()) {
                        pFound = oNode.shared_from_this();
                        return false;  // stop the walk
                }
                return true;
        });
        return pFound;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// MakeStaticBox — level geometry helper
// ---------------------------------------------------------------------------

void Scene::MakeStaticBox(const char* name, glm::vec3 vPos, glm::vec3 vHalf,
                          glm::vec3 vRotDeg, H<Material> hMat) {
        auto pNode = pSceneTree->Create(name);
        pNode->SetPos(vPos);
        pNode->SetScale(vHalf);
        if (glm::length(vRotDeg) > 0.001f)
                pNode->SetRotation(vRotDeg);
        pNode->CreateComponent<StaticModel>(hBoxMesh, hMat);

        RigidBodyDesc desc;
        desc.oCollider.type         = ColliderDesc::Box;
        desc.oCollider.vHalfExtents = vHalf;
        desc.vInitialPosition       = vPos;
        if (glm::length(vRotDeg) > 0.001f)
                desc.qInitialRotation = glm::quat(glm::radians(vRotDeg));
        desc.is_static = true;
        pNode->CreateComponent<RigidBody>(desc);
}

// ---------------------------------------------------------------------------
// MakeOrb — collectible pickup
// ---------------------------------------------------------------------------

void Scene::MakeOrb(int index, glm::vec3 vPos) {
        char name[32];
        std::snprintf(name, sizeof(name), "Orb_%d", index);

        auto pNode = pSceneTree->Create(name);
        pNode->SetPos(vPos);
        pNode->SetScale(glm::vec3{0.35f});
        vOrbs[index] = pNode;
        orbs_spawned = true;

        pNode->CreateComponent<StaticModel>(hSphereMesh, hOrbMat);
}

// ---------------------------------------------------------------------------
// SpawnPlayer
// ---------------------------------------------------------------------------

void Scene::SpawnPlayer(const Settings& oSettings) {
        pPlayerNode = pSceneTree->Create("Player");
        pPlayerNode->SetPos(glm::vec3{0.f, 1.5f, 0.f});

        // Input
        {
                auto res = pPlayerNode->CreateComponent<InputState>();
                se_assert(res == uSUCCESS);
                pPlayerInput = pPlayerNode->GetComponent<InputState>();
        }
        {
                auto res = pPlayerNode->CreateComponent<InputMappingContext>();
                se_assert(res == uSUCCESS);
                auto* pCtx = pPlayerNode->GetComponent<InputMappingContext>();

                pCtx->vBindings.push_back({InputSource::KEY_DOWN, InputAction::MOVE_AXIS_Y,  1.f, Keys::W});
                pCtx->vBindings.push_back({InputSource::KEY_DOWN, InputAction::MOVE_AXIS_Y, -1.f, Keys::S});
                pCtx->vBindings.push_back({InputSource::KEY_DOWN, InputAction::MOVE_AXIS_X, -1.f, Keys::A});
                pCtx->vBindings.push_back({InputSource::KEY_DOWN, InputAction::MOVE_AXIS_X,  1.f, Keys::D});
                pCtx->vBindings.push_back({InputSource::KEY_PRESS, InputAction::JUMP_PRESSED, 1.f, Keys::SPACE});
                pCtx->vBindings.push_back({InputSource::KEY_DOWN,  InputAction::JUMP_HELD,    1.f, Keys::SPACE});
                // SDL_SCANCODE_LSHIFT = 225
                pCtx->vBindings.push_back({InputSource::SCANCODE_DOWN, InputAction::SPRINT_HELD, 1.f, 225});
        }

        // Sprint stamina
        {
                auto res = pPlayerNode->CreateComponent<SprintStamina>();
                se_assert(res == uSUCCESS);
                pPlayerStamina = pPlayerNode->GetComponent<SprintStamina>();
        }

        // Physics character controller
        {
                CharacterController::Desc ccDesc;
                ccDesc.capsule_radius      = 0.4f;
                ccDesc.capsule_half_height = 0.9f;
                ccDesc.max_speed           = 5.0f;
                ccDesc.sprint_multiplier   = 1.8f;
                ccDesc.acceleration        = 20.0f;
                ccDesc.deceleration        = 30.0f;
                ccDesc.air_control         = 0.25f;
                ccDesc.jump_impulse        = 6.5f;
                ccDesc.coyote_time         = 0.12f;
                ccDesc.jump_buffer_time    = 0.10f;

                auto res = pPlayerNode->CreateComponent<CharacterController>(ccDesc);
                se_assert(res == uSUCCESS);
                pPlayerCC = pPlayerNode->GetComponent<CharacterController>();
        }

        // High-level player behavior state machine (Normal / Ragdoll).
        // This is the player side of the behavior/intent layer — it gates input and
        // flips the ragdoll seam; it does NOT drive animation (the AnimGraph does).
        {
                StateMachine::Desc smDesc;
                smDesc.definition_path = oSettings.sPlayerHSMPath.c_str();
                auto res = pPlayerNode->CreateComponent<StateMachine>(smDesc);
                if (res != uSUCCESS)
                        log_w("MovingActors: player StateMachine failed (err={})", res);
                pPlayerSM = pPlayerNode->GetComponent<StateMachine>();
        }

        // Instantiate the character prefab directly under the player capsule node, so the
        // visual is a real child of the moving body (no separate tree, no cross-tree move).
        {
                auto pInst = pSceneTree->Instantiate(oSettings.sCharScenePath, pPlayerNode);
                if (pInst) {
                        const std::string base = pInst->GetFullName();
                        pCharVisualNode = pSceneTree->FindFullName(StrID(base + "|ManArmature"));
                        // Animator lookup by component search — survives prefab child renames.
                        if (auto pAnimNode = FindNodeWithComponent<Animator>(pInst))
                                pPlayerAnimator = pAnimNode->GetComponent<Animator>();
                        if (!pCharVisualNode)
                                log_w("MovingActors: ManArmature not found under '{}'", base);
                } else {
                        log_w("MovingActors: failed to instantiate char prefab '{}'", oSettings.sCharScenePath);
                }
        }

        // Wire the locomotion→animation bridge (the only link between the two layers).
        if (pPlayerCC && pPlayerAnimator)
                GetSystem<CharacterAnimationSystem>().Register(pPlayerNode, pPlayerCC, pPlayerAnimator);

        v_cam_target_smooth = pPlayerNode->GetTransform().GetWorldPos()
                            + glm::vec3{0.f, 1.4f, 0.f};
}

// ---------------------------------------------------------------------------
// SpawnNPC — called by EntityManager template instantiator
// ---------------------------------------------------------------------------

TSceneTree::TSceneNodeWeak Scene::SpawnNPC(const SpawnIntent& intent) {
        SpawnRequest req;
        req.vTranslation = intent.vTranslation;
        req.name         = intent.name.empty() ? "NPC" : intent.name;

        req.fnInitializer = [this](TSceneTree::TSceneNodeExact& oNode) {
                // Input contract
                oNode.CreateComponent<InputState>();

                // Behavior decision maker (Idle/Wander/Chase) + its executor (AIBrain).
                StateMachine::Desc smDesc;
                smDesc.definition_path = sNPCHSMPath.c_str();
                oNode.CreateComponent<StateMachine>(smDesc);

                oNode.CreateComponent<AIBrain>();
                if (auto* pBrain = oNode.GetComponent<AIBrain>())
                        pBrain->SetTarget(pPlayerNode);

                // Physics
                CharacterController::Desc ccDesc;
                ccDesc.capsule_radius      = 0.4f;
                ccDesc.capsule_half_height = 0.9f;
                ccDesc.max_speed           = 3.5f;
                ccDesc.sprint_multiplier   = 1.0f;
                ccDesc.acceleration        = 15.0f;
                ccDesc.deceleration        = 25.0f;
                ccDesc.jump_impulse        = 0.0f;
                oNode.CreateComponent<CharacterController>(ccDesc);
        };

        auto pWeak = GetSystem<EntityManager>().Spawn(req);

        if (auto pNode = pWeak.lock()) {
                pNode->SetPos(intent.vTranslation);
                GetSystem<CharacterDebugger>().Register(pWeak);
                GetSystem<StateMachineDebugger>().Register(pWeak);

                // Instantiate the NPC visual prefab directly under the NPC capsule node.
                NPCEntry entry;
                entry.pNode = pWeak;
                entry.pCC   = pNode->GetComponent<CharacterController>();

                auto pInst = pSceneTree->Instantiate(sNPCScenePath, pNode);
                if (pInst) {
                        const std::string base = pInst->GetFullName();
                        entry.pVisualNode = pSceneTree->FindFullName(StrID(base + "|Armature"));
                        // Animator lookup by component search — survives prefab child renames.
                        if (auto pAnimNode = FindNodeWithComponent<Animator>(pInst))
                                entry.pAnimator = pAnimNode->GetComponent<Animator>();
                        if (!entry.pVisualNode)
                                log_w("MovingActors: Armature not found under '{}'", base);
                } else {
                        log_w("MovingActors: failed to instantiate NPC prefab '{}'", sNPCScenePath);
                }

                // Wire the locomotion→animation bridge for this NPC.
                if (entry.pCC && entry.pAnimator)
                        GetSystem<CharacterAnimationSystem>().Register(pWeak, entry.pCC, entry.pAnimator);

                vNPCs.push_back(std::move(entry));
        }

        return pWeak;
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

Scene::Scene(const Settings& oSettings) :
        hSceneTree(CreateResource<TSceneTree>("MovingActorsScene", true)) {

        pSceneTree = GetResource(hSceneTree);
        se_assert(pSceneTree);

        sNPCScenePath = oSettings.sNPCScenePath;
        sNPCHSMPath   = oSettings.sNPCHSMPath;

        // --- EntityManager ---
        GetSystem<EntityManager>().SetSceneTree(pSceneTree);
        GetSystem<EntityManager>().SetTemplateInstantiator(
                [this](const SpawnIntent& intent) -> TSceneTree::TSceneNodeWeak {
                        return SpawnNPC(intent);
                });

        // --- Shared meshes ---
        hBoxMesh    = MakeBoxMesh({1.f, 1.f, 1.f}, "level_box");
        hSphereMesh = MakeSphereMesh(1.f, 12, 24, "orb_sphere");

        // --- 1×1 neutral textures ---
        auto hWhite  = TextureBuilder(1, 1).Fill(255, 255, 255).Upload("ma_white");
        auto hNormal = TextureBuilder(1, 1).Fill(128, 128, 255).Upload("ma_normal");
        auto hSpec   = TextureBuilder(1, 1).Fill(  0, 255, 255).Upload("ma_spec");

        auto SetupMat = [&](H<Material> hMat) {
                if (auto* pMat = GetResource(hMat)) {
                        pMat->SetTexture(TextureUnit::DIFFUSE,  hWhite);
                        pMat->SetTexture(TextureUnit::NORMAL,   hNormal);
                        pMat->SetTexture(TextureUnit::SPECULAR, hSpec);
                }
                return hMat;
        };

        auto hMatGround   = SetupMat(CreateResource<Material>("resource/material/pbr_rough_dielectric.semt"));
        auto hMatPlatform = SetupMat(CreateResource<Material>("resource/material/pbr_rough_metal.semt"));
        auto hMatRamp     = SetupMat(CreateResource<Material>("resource/material/pbr_smooth_dielectric.semt"));

        hOrbMat = CreateResource<Material>("resource/material/pbr_emissive.semt");

        // --- Camera ---
        pCameraNode = pSceneTree->Create("MainCamera");
        {
                auto res = pCameraNode->CreateComponent<Camera>(oSettings.oCamSettings);
                se_assert(res == uSUCCESS);
        }
        pCamera = pCameraNode->GetComponent<Camera>();
        GetSystem<TRenderer>().SetCamera(pCamera);

        // --- Directional light ---
#if SE_DEFERRED_RENDERER
        {
                DirLight oDir;
                oDir.direction = glm::normalize(glm::vec3(-0.5f, -1.f, -0.5f));
                oDir.intensity = 0.9f;
                GetSystem<TRenderer>().SetDirLight(oDir);
        }
#endif

        // --- Level geometry ---
        // Ground
        MakeStaticBox("Ground",  { 0.f, -0.2f,  0.f}, {25.f, 0.2f, 25.f}, {0,0,0}, hMatGround);
        // Perimeter walls
        MakeStaticBox("WallN",   { 0.f,  0.5f,  25.f}, {25.f, 0.5f, 0.25f}, {0,0,0}, hMatGround);
        MakeStaticBox("WallS",   { 0.f,  0.5f, -25.f}, {25.f, 0.5f, 0.25f}, {0,0,0}, hMatGround);
        MakeStaticBox("WallE",   { 25.f, 0.5f,  0.f},  {0.25f, 0.5f, 25.f}, {0,0,0}, hMatGround);
        MakeStaticBox("WallW",   {-25.f, 0.5f,  0.f},  {0.25f, 0.5f, 25.f}, {0,0,0}, hMatGround);
        // Low platform (y=2) + ramp
        MakeStaticBox("Plat_Lo", { 8.f, 2.f,   6.f}, {4.f, 0.3f, 4.f}, {0,0,0},    hMatPlatform);
        MakeStaticBox("Ramp_Lo", { 4.2f, 1.1f, 6.f}, {3.5f, 0.25f, 3.5f}, {0,0,-14.f}, hMatRamp);
        // Mid platform (y=4) + ramp
        MakeStaticBox("Plat_Mi", {-6.f, 4.f,   4.f}, {4.f, 0.3f, 4.f}, {0,0,0},    hMatPlatform);
        MakeStaticBox("Ramp_Mi", { 1.f, 3.1f,  5.f}, {4.f, 0.25f, 3.5f}, {0,0,-17.f}, hMatRamp);
        // High platform (y=6) + ramp
        MakeStaticBox("Plat_Hi", { 5.f, 6.f,  -8.f}, {4.f, 0.3f, 4.f}, {0,0,0},    hMatPlatform);
        MakeStaticBox("Ramp_Hi", {-1.f, 5.1f, -6.f}, {4.f, 0.25f, 3.5f}, {0,0,-19.f}, hMatRamp);
        // Extra stepping stones
        MakeStaticBox("Step_A",  {-8.f,  1.f,  -8.f}, {2.f, 0.3f, 2.f}, {0,0,0}, hMatPlatform);
        MakeStaticBox("Step_B",  {-12.f, 1.5f, -12.f},{2.f, 0.3f, 2.f}, {0,0,0}, hMatPlatform);
        MakeStaticBox("Step_C",  {-16.f, 2.f,  -8.f}, {2.f, 0.3f, 2.f}, {0,0,0}, hMatPlatform);

        // --- Orbs ---
        MakeOrb(0, {  5.f, 0.5f,  12.f});   // ground
        MakeOrb(1, {-10.f, 0.5f,   8.f});   // ground
        MakeOrb(2, { -5.f, 0.5f, -14.f});   // ground
        MakeOrb(3, {  8.f, 2.7f,   6.f});   // low platform
        MakeOrb(4, { -6.f, 4.7f,   4.f});   // mid platform
        MakeOrb(5, {  5.f, 6.7f,  -8.f});   // high platform
        MakeOrb(6, {-16.f, 2.7f,  -8.f});   // Step_C

        // --- Sound cues ---
        GetSystem<SoundEventSystem>().LoadCues("resource/sound_cue/character.secl");
        GetSystem<SoundEventSystem>().LoadCues("resource/sound_cue/ambient.secl");
        {
                SoundEventContext oAmbient;
                GetSystem<SoundEventSystem>().Post("ambient", oAmbient);
        }

        // --- Player ---
        SpawnPlayer(oSettings);

        // --- HUD (RmlUi) ---
        if (auto* pCtx = GetSystem<UISystem>().GetContext()) {
                Rml::DataModelConstructor ctor = pCtx->CreateDataModel("moving_actors_hud");

                ctor.BindFunc("orbs_collected", [this](Rml::Variant& v) { v = orbs_collected; });
                ctor.BindFunc("orbs_total",     [this](Rml::Variant& v) { v = kOrbCount; });
                ctor.BindFunc("timer_str",      [this](Rml::Variant& v) { v = Rml::String(timer_str); });
                // Preformatted "NN%" — data-style-width sets the property from the
                // variant string, so the unit must be part of the value.
                ctor.BindFunc("stamina_pct",    [this](Rml::Variant& v) {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "%d%%",
                                      static_cast<int>(pPlayerStamina ? pPlayerStamina->stamina * 100.f : 100.f));
                        v = Rml::String(buf);
                });
                ctor.BindFunc("is_complete",   [this](Rml::Variant& v) { v = game_complete; });
                ctor.BindFunc("complete_str",  [this](Rml::Variant& v) { v = Rml::String(complete_str); });

                hHUD = ctor.GetModelHandle();

                auto hud_doc = GetSystem<UISystem>().GetDocumentManager().Load("ui/moving_actors_hud.rml");
                if (auto* pDoc = GetSystem<UISystem>().GetDocumentManager().Get(hud_doc))
                        pDoc->Show();
        }

        // --- Debug tools ---
        GetSystem<CharacterDebugger>().SetSceneTree(pSceneTree);
        GetSystem<StateMachineDebugger>().SetSceneTree(pSceneTree);

        // --- Event listeners ---
        auto& em = GetSystem<EventManager>();
        em.AddListener<EUpdate,          &Scene::OnUpdate>         (this);
        em.AddListener<EInputUpdate,     &Scene::OnInputUpdate>    (this);
        em.AddListener<EMouseMove,       &Scene::OnMouseMove>      (this);
        em.AddListener<EKeyDown,         &Scene::OnKeyDown>        (this);
        em.AddListener<EMouseButtonDown, &Scene::OnMouseButtonDown>(this);
        em.AddListener<EMouseWheel,      &Scene::OnMouseWheel>     (this);
        em.AddListener<ECharacterJumped, &Scene::OnCharacterJumped>(this);
        em.AddListener<ECharacterLanded, &Scene::OnCharacterLanded>(this);
        em.AddListener<EEntitySpawned,   &Scene::OnEntitySpawned>  (this);
        em.AddListener<EStateMachineEvent, &Scene::OnStateMachineEvent>(this);

        // Lock mouse and start
        GetSystem<InputManager>().SetMouseVisible(false);
        GetSystem<InputManager>().SetMouseGrabbed(true);
        mouse_locked = true;
        game_running = true;

        UpdateCamera(0.f);
        log_i("MovingActors: collect {} orbs to win!", kOrbCount);
}

// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------

Scene::~Scene() noexcept {
        auto& em = GetSystem<EventManager>();
        em.RemoveListener<EUpdate,          &Scene::OnUpdate>         (this);
        em.RemoveListener<EInputUpdate,     &Scene::OnInputUpdate>    (this);
        em.RemoveListener<EMouseMove,       &Scene::OnMouseMove>      (this);
        em.RemoveListener<EKeyDown,         &Scene::OnKeyDown>        (this);
        em.RemoveListener<EMouseButtonDown, &Scene::OnMouseButtonDown>(this);
        em.RemoveListener<EMouseWheel,      &Scene::OnMouseWheel>     (this);
        em.RemoveListener<ECharacterJumped, &Scene::OnCharacterJumped>(this);
        em.RemoveListener<ECharacterLanded, &Scene::OnCharacterLanded>(this);
        em.RemoveListener<EEntitySpawned,   &Scene::OnEntitySpawned>  (this);
        em.RemoveListener<EStateMachineEvent, &Scene::OnStateMachineEvent>(this);

        if (hHUD)
                GetSystem<UISystem>().GetContext()->RemoveDataModel("moving_actors_hud");

        GetSystem<InputManager>().SetMouseVisible(true);
        GetSystem<InputManager>().SetMouseGrabbed(false);
}

// ---------------------------------------------------------------------------
// Process
// ---------------------------------------------------------------------------

void Scene::Process() {}

// ---------------------------------------------------------------------------
// OnInputUpdate — rotate camera-relative move_axis to world-space.
// Fires after InputMappingContext flushes WASD to InputState.
// ---------------------------------------------------------------------------

void Scene::OnInputUpdate(const Event& /*e*/) {
        if (!pPlayerInput) return;

        static const StrID sStateNormal("Normal");

        // High-level player state gates intent: no movement/jump while not Normal
        // (e.g. ragdolled). The behavior layer overrides raw input here.
        if (pPlayerSM && pPlayerSM->GetCurrentState() != sStateNormal) {
                pPlayerInput->move_axis    = {0.f, 0.f};
                pPlayerInput->jump_pressed = false;
                pPlayerInput->jump_held    = false;
                return;
        }

        const float yaw_rad = glm::radians(cam_yaw);
        const float c = std::cos(yaw_rad);
        const float s = std::sin(yaw_rad);
        const glm::vec2 raw = pPlayerInput->move_axis;
        // raw.x = A/D (strafe), raw.y = W/S (forward).
        // The orbit camera sits at +offset = (sin yaw, cos yaw) from the target and looks
        // back at it, so "into the screen" (forward) is (-sin yaw, -cos yaw) and screen-right
        // is (cos yaw, -sin yaw). Map camera-relative intent onto world XZ accordingly so W
        // moves away from the camera and D moves to its right.
        pPlayerInput->move_axis = { raw.x * c - raw.y * s,
                                   -raw.x * s - raw.y * c };
}

// ---------------------------------------------------------------------------
// OnMouseMove — update orbit yaw/pitch when mouse is grabbed
// ---------------------------------------------------------------------------

void Scene::OnMouseMove(const Event& e) {
        if (!mouse_locked) return;
        const auto& ev = e.Get<EMouseMove>();
        cam_yaw   += ev.delta.x * 0.25f;
        cam_pitch  = glm::clamp(cam_pitch - ev.delta.y * 0.25f, -10.f, 65.f);
}

// ---------------------------------------------------------------------------
// OnUpdate
// ---------------------------------------------------------------------------

void Scene::OnUpdate(const Event& e) {
        const float dt = e.Get<EUpdate>().last_frame_time;

        if (game_running && !game_complete)
                game_time += dt;

        // Auto-spawn first NPC after 8 s
        if (!first_npc_spawned) {
                npc_spawn_timer -= dt;
                if (npc_spawn_timer <= 0.f) {
                        first_npc_spawned = true;
                        SpawnIntent intent;
                        intent.prefab_id    = "npc";
                        intent.name         = "NPC_Wander";
                        intent.vTranslation = {-5.f, 1.5f, -5.f};
                        GetSystem<EntityManager>().RequestSpawn(intent);
                }
        }

        CheckOrbProximity();
        UpdateStamina(dt);
        UpdateFootstepTimers(dt);
        UpdateCharFacing(dt);
        // Animation params are now driven by CharacterAnimationSystem (the
        // locomotion→animation mapper), not the scene.
        UpdateCamera(dt);
        UpdateCameraShake(dt);
        UpdateHUDBindings();
}

// ---------------------------------------------------------------------------
// Camera
// ---------------------------------------------------------------------------

void Scene::UpdateCamera(float dt) {
        if (!pPlayerNode || !pCamera) return;

        const glm::vec3 target_pos = pPlayerNode->GetTransform().GetWorldPos()
                                   + glm::vec3{0.f, 1.4f, 0.f};
        // Track the character tightly so it stays centred in view. Exponential smoothing
        // here left a steady-state lag of ~speed/k metres while moving, which read as the
        // character continuously drifting away from the camera's focus point. The capsule
        // is already updated at the physics rate, so following it directly is smooth.
        v_cam_target_smooth = target_pos;

        const float yaw_rad   = glm::radians(cam_yaw);
        const float pitch_rad = glm::radians(cam_pitch);
        const glm::vec3 offset{
                cam_dist * std::cos(pitch_rad) * std::sin(yaw_rad),
                cam_dist * std::sin(pitch_rad),
                cam_dist * std::cos(pitch_rad) * std::cos(yaw_rad)
        };
        const glm::vec3 cam_pos = v_cam_target_smooth + offset + v_shake_offset;

        // FOV lerp: widen on sprint
        const bool sprinting = pPlayerCC
                && pPlayerInput && pPlayerInput->sprint_held
                && pPlayerStamina && pPlayerStamina->stamina > 0.01f;
        const float target_fov = sprinting ? 70.f : 60.f;
        cam_fov_current += (target_fov - cam_fov_current) * glm::clamp(5.f * dt, 0.f, 1.f);
        // Snap once converged: an exponential lerp never reaches the target exactly,
        // so without this every frame would re-dirty the camera (ECameraProjChanged
        // → full cluster-grid rebuild) long after the transition visually ended.
        if (glm::abs(target_fov - cam_fov_current) < 0.01f) cam_fov_current = target_fov;
        pCamera->SetFOV(cam_fov_current);

        pCamera->SetPos(cam_pos.x, cam_pos.y, cam_pos.z);
        pCamera->LookAt(v_cam_target_smooth);
}

// ---------------------------------------------------------------------------
// Sprint stamina
// ---------------------------------------------------------------------------

void Scene::UpdateStamina(float dt) {
        if (!pPlayerStamina || !pPlayerInput) return;

        auto& s = *pPlayerStamina;
        if (pPlayerInput->sprint_held && s.stamina > 0.f) {
                s.stamina -= s.drain_rate * dt;
                if (s.stamina <= 0.f) {
                        s.stamina = 0.f;
                        pPlayerInput->sprint_held = false;
                }
        } else {
                s.stamina = std::min(s.stamina + s.recovery_rate * dt, 1.f);
        }
}

// ---------------------------------------------------------------------------
// Footstep timers (player + NPCs)
// ---------------------------------------------------------------------------

void Scene::UpdateFootstepTimers(float dt) {
        // Player
        if (pPlayerCC && pPlayerNode) {
                const float speed = glm::length(pPlayerCC->GetVelocity());
                if (speed >= 0.5f && pPlayerCC->IsGrounded()) {
                        step_timer += dt;
                        if (step_timer >= 0.45f / speed) {
                                step_timer = 0.f;
                                CharacterSoundContext ctx;
                                ctx.vPosition = pPlayerNode->GetTransform().GetWorldPos();
                                ctx.speed     = speed;
                                ctx.weight    = 80.f;
                                ctx.surface   = SurfaceType::CONCRETE;
                                GetSystem<SoundEventSystem>().Post("character.footstep.concrete", ctx);
                        }
                } else {
                        step_timer = 0.f;
                }
        }

        // NPCs
        if (vNPCs.empty()) return;   // nothing spawned — skip the loop and the prune pass
        for (auto& entry : vNPCs) {
                auto pNode = entry.pNode.lock();
                if (!pNode || !entry.pCC) continue;
                const float speed = glm::length(entry.pCC->GetVelocity());
                if (speed >= 0.5f && entry.pCC->IsGrounded()) {
                        entry.step_timer += dt;
                        if (entry.step_timer >= 0.45f / speed) {
                                entry.step_timer = 0.f;
                                CharacterSoundContext ctx;
                                ctx.vPosition = pNode->GetTransform().GetWorldPos();
                                ctx.speed     = speed;
                                ctx.weight    = 70.f;
                                ctx.surface   = SurfaceType::STONE;
                                GetSystem<SoundEventSystem>().Post("character.footstep.stone", ctx);
                        }
                } else {
                        entry.step_timer = 0.f;
                }
        }

        // Clean up expired NPC entries
        vNPCs.erase(
                std::remove_if(vNPCs.begin(), vNPCs.end(),
                        [](const NPCEntry& e) { return e.pNode.expired(); }),
                vNPCs.end());
}

// ---------------------------------------------------------------------------
// Orb proximity pickup
// ---------------------------------------------------------------------------

void Scene::CheckOrbProximity() {
        if (!orbs_spawned || !pPlayerNode || orbs_remaining == 0 || game_complete) return;

        const glm::vec3 player_pos = pPlayerNode->GetTransform().GetWorldPos();

        for (int i = 0; i < kOrbCount; ++i) {
                auto pOrb = vOrbs[i].lock();
                if (!pOrb) continue;

                const float dist = glm::length(player_pos - pOrb->GetTransform().GetWorldPos());
                if (dist < 1.2f) {
                        pOrb->Disable();
                        vOrbs[i].reset();
                        ++orbs_collected;
                        --orbs_remaining;

                        CharacterSoundContext ctx;
                        ctx.vPosition = player_pos;
                        GetSystem<SoundEventSystem>().Post("character.interact", ctx);

                        log_i("MovingActors: orb {} collected ({}/{})", i, orbs_collected, kOrbCount);

                        if (orbs_remaining == 0) {
                                game_complete = true;
                                const int mins  = static_cast<int>(game_time) / 60;
                                const int secs  = static_cast<int>(game_time) % 60;
                                const int tenth = static_cast<int>(game_time * 10.f) % 10;
                                std::snprintf(complete_str, sizeof(complete_str),
                                              "Completed!  %02d:%02d.%d", mins, secs, tenth);
                                log_i("MovingActors: all orbs collected! Time: {:.1f}s", game_time);
                        }
                }
        }
}

// ---------------------------------------------------------------------------
// HUD
// ---------------------------------------------------------------------------

void Scene::UpdateHUDBindings() {
        if (!hHUD) return;

        const int mins  = static_cast<int>(game_time) / 60;
        const int secs  = static_cast<int>(game_time) % 60;
        const int tenth = static_cast<int>(game_time * 10.f) % 10;
        std::snprintf(timer_str, 32, "%02d:%02d.%d", mins, secs, tenth);

        const int stamina_pct = static_cast<int>(pPlayerStamina ? pPlayerStamina->stamina * 100.f : 100.f);

        // Only flush bindings when something the HUD shows actually changed.
        if (std::strcmp(timer_str, hud_timer_prev) == 0 &&
            orbs_collected == hud_orbs_prev &&
            stamina_pct    == hud_stamina_prev &&
            game_complete  == hud_complete_prev)
                return;

        std::strncpy(hud_timer_prev, timer_str, sizeof(hud_timer_prev));
        hud_timer_prev[sizeof(hud_timer_prev) - 1] = '\0';
        hud_orbs_prev     = orbs_collected;
        hud_stamina_prev  = stamina_pct;
        hud_complete_prev = game_complete;

        hHUD.DirtyAllVariables();
}

// ---------------------------------------------------------------------------
// Camera shake
// ---------------------------------------------------------------------------

void Scene::UpdateCameraShake(float dt) {
        if (shake_timer <= 0.f) {
                v_shake_offset = {0.f, 0.f, 0.f};
                return;
        }
        shake_timer -= dt;
        const float t   = shake_timer / 0.3f;
        const float mag = shake_magnitude * t;
        v_shake_offset.x = mag * std::sin(shake_timer * 40.f);
        v_shake_offset.y = mag * std::cos(shake_timer * 28.f) * 0.5f;
        v_shake_offset.z = 0.f;
}

// ---------------------------------------------------------------------------
// Event handlers
// ---------------------------------------------------------------------------

void Scene::OnKeyDown(const Event& e) {
        const auto& ev = e.Get<EKeyDown>();

        if (ev.key == Keys::ESCAPE) {
                if (mouse_locked) {
                        GetSystem<InputManager>().SetMouseVisible(true);
                        GetSystem<InputManager>().SetMouseGrabbed(false);
                        mouse_locked = false;
                } else {
                        GetSystem<EventManager>().TriggerEvent(EQuit{});
                }
        }

        // R — toggle player ragdoll (showcases the HSM-driven pose-source seam)
        if (ev.key == Keys::R)
                TogglePlayerRagdoll();
}

void Scene::OnMouseButtonDown(const Event& e) {
        const auto& ev = e.Get<EMouseButtonDown>();

        if (ev.button == MouseB::LEFT && !mouse_locked) {
                GetSystem<InputManager>().SetMouseVisible(false);
                GetSystem<InputManager>().SetMouseGrabbed(true);
                mouse_locked = true;
        }

        // Right-click while playing → spawn extra NPC (event-driven EntityManager demo)
        if (ev.button == MouseB::RIGHT && mouse_locked && game_running) {
                SpawnIntent intent;
                intent.prefab_id    = "npc";
                intent.name         = "NPC_Extra";
                intent.vTranslation = pPlayerNode
                        ? pPlayerNode->GetTransform().GetWorldPos() + glm::vec3{3.f, 0.f, 0.f}
                        : glm::vec3{0.f, 1.5f, 0.f};
                GetSystem<EntityManager>().RequestSpawn(intent);
        }
}

void Scene::OnMouseWheel(const Event& e) {
        const auto& ev = e.Get<EMouseWheel>();
        cam_dist = glm::clamp(cam_dist - ev.delta * 0.5f, 2.0f, 20.0f);
}

void Scene::OnCharacterJumped(const Event& e) {
        const auto& ev = e.Get<ECharacterJumped>();
        if (ev.pActor.lock().get() != pPlayerNode.get()) return;

        CharacterSoundContext ctx;
        ctx.vPosition = pPlayerNode->GetTransform().GetWorldPos();
        ctx.impact    = 0.5f;
        GetSystem<SoundEventSystem>().Post("character.jump", ctx);
        // The "jump" animation trigger is fired by CharacterAnimationSystem.
}

void Scene::OnCharacterLanded(const Event& e) {
        const auto& ev = e.Get<ECharacterLanded>();
        if (ev.pActor.lock().get() != pPlayerNode.get()) return;

        const float airborne = pPlayerCC ? pPlayerCC->GetTimeAirborne() : 0.f;
        CharacterSoundContext ctx;
        ctx.vPosition = pPlayerNode->GetTransform().GetWorldPos();
        ctx.impact    = glm::clamp(airborne * 0.8f, 0.f, 1.f);
        GetSystem<SoundEventSystem>().Post(
                airborne > 0.8f ? "character.land.hard" : "character.land.soft", ctx);

        // Camera shake proportional to fall
        shake_magnitude = glm::clamp(airborne - 0.3f, 0.f, 1.f) * 0.15f;
        if (shake_magnitude > 0.001f)
                shake_timer = 0.3f;
        // The "land" animation trigger is fired by CharacterAnimationSystem.
}

void Scene::OnEntitySpawned(const Event& /*e*/) {
        // Refresh debugger registrations when new entities arrive
        GetSystem<CharacterDebugger>().SetSceneTree(pSceneTree);
        GetSystem<StateMachineDebugger>().SetSceneTree(pSceneTree);
}

// ---------------------------------------------------------------------------
// UpdateCharFacing — smooth yaw rotation applied to the visual node only
// ---------------------------------------------------------------------------

void Scene::UpdateCharFacing(float dt) {
        auto doFacing = [dt](TSceneTree::TSceneNode pVisual,
                             CharacterController*   pCC,
                             float&                 facing_deg) {
                if (!pVisual || !pCC) return;
                const glm::vec3 vel = pCC->GetVelocity();
                const glm::vec2 v2d = {vel.x, vel.z};
                if (glm::length(v2d) > 0.5f) {
                        float target = glm::degrees(std::atan2(v2d.x, v2d.y));
                        float diff   = target - facing_deg;
                        while (diff >  180.f) diff -= 360.f;
                        while (diff < -180.f) diff += 360.f;
                        facing_deg += diff * glm::clamp(10.f * dt, 0.f, 1.f);
                }
                pVisual->SetRotation(glm::vec3{0.f, facing_deg, 0.f});
        };

        doFacing(pCharVisualNode, pPlayerCC, char_facing_deg);
        for (auto& e : vNPCs) {
                if (e.pNode.expired()) continue;
                doFacing(e.pVisualNode, e.pCC, e.facing_deg);
        }
}

// ---------------------------------------------------------------------------
// TogglePlayerRagdoll — drives the player HSM, which in turn flips the seam
// ---------------------------------------------------------------------------

void Scene::TogglePlayerRagdoll() {
        if (!pPlayerSM) return;
        // Fire the trigger appropriate to the current state; the StateMachine
        // transitions and posts OnRagdollEnter/OnRagdollExit, handled below.
        if (pPlayerSM->GetCurrentState() == StrID("Normal"))
                pPlayerSM->GetParams().SetTrigger(StrID("ragdoll"));
        else
                pPlayerSM->GetParams().SetTrigger(StrID("recover"));
}

// ---------------------------------------------------------------------------
// OnStateMachineEvent — react to high-level behavior state changes.
// The player Ragdoll state owns the pose-source seam via the CharacterController.
// ---------------------------------------------------------------------------

void Scene::OnStateMachineEvent(const Event& e) {
        const auto& ev = e.Get<EStateMachineEvent>();
        if (ev.pNode.lock().get() != pPlayerNode.get()) return;
        if (!pPlayerCC) return;

        if (ev.event_name == StrID("OnRagdollEnter"))
                pPlayerCC->SetRagdoll(true);
        else if (ev.event_name == StrID("OnRagdollExit"))
                pPlayerCC->SetRagdoll(false);
}

} // namespace SE

#endif
