
#ifdef SE_IMPL

// Xlib `Bool` guard — see App.h (this TU includes graphics headers first).
#pragma push_macro("Bool")
#undef Bool

#include "Scene.h"

#include <Logging.h>

#include <EventManager.h>
#include <CommonEvents.h>
#include <InputEvents.h>
#include <InputCodes.h>
#include <InputMappingContext.h>

#include <StaticModel.h>
#include <Animator.h>
#include <InputState.h>
#include <CharacterController.h>
#include <PhysicsSystem.h>
#include <hsm/StateMachine.h>

#include <world/WorldSystem.h>
#include <world/WorldAsset.h>
#include <terrain/Terrain.h>
#include <terrain/TerrainSystem.h>
#include <ai/BlackboardSystem.h>
#include <nav/NavigationSystem.h>
#include <nav/NavigationDebugger.h>
#include <nav/CrowdDebugger.h>
#include <world/StreamingDebugger.h>
#include <terrain/TerrainDebugger.h>
#include <world/SpatialIndexDebugger.h>
#include <spawner/SpawnerDebugger.h>
#include <spawner/SpawnerSystem.h>
#include <ai/BlackboardInspector.h>
#include <CharacterAnimationSystem.h>
#include <EntityTemplateSystem.h>
#include <EntityManager.h>

#include <SoundEventSystem.h>
#include <SoundContextTypes.h>

#include <Camera.h>
#include <Light.h>
#include <InputManager.h>

#include <algorithm>
#include <cmath>

namespace SE {

namespace {

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

Scene::Scene(const Settings & oSettings)
        : hSceneTree(CreateResource<TSceneTree>("WorldDemoScene", true)) {

        pSceneTree = GetResource(hSceneTree);
        sWorldPath      = oSettings.sWorldPath;
        sCharScenePath  = oSettings.sCharScenePath;
        sPlayerHSMPath  = oSettings.sPlayerHSMPath;

        auto & oEM = GetSystem<EventManager>();
        oEM.AddListener<EUpdate,           &Scene::OnUpdate>(this);
        oEM.AddListener<EMouseMove,        &Scene::OnMouseMove>(this);
        oEM.AddListener<EKeyDown,          &Scene::OnKeyDown>(this);
        oEM.AddListener<EMouseButtonUp,    &Scene::OnMouseButtonUp>(this);
        oEM.AddListener<EMouseWheel,       &Scene::OnMouseWheel>(this);
        oEM.AddListener<ETriggerFired,     &Scene::OnTriggerFired>(this);
        oEM.AddListener<ESubLevelActivated,   &Scene::OnSubLevelActivated>(this);
        oEM.AddListener<ESubLevelDeactivated, &Scene::OnSubLevelDeactivated>(this);

        // --- camera ---
        pCameraNode = pSceneTree->Create("MainCamera");
        pCameraNode->CreateComponent<Camera>(oSettings.oCamSettings);
        pCamera = pCameraNode->GetComponent<Camera>();
        pCameraNode->SetPos({ 0.0f, 14.0f, 40.0f });
        GetSystem<TRenderer>().SetCamera(pCamera);

        // --- lighting ---
#if SE_DEFERRED_RENDERER
        {
                oDirLight.direction = glm::normalize(glm::vec3(-0.45f, -1.0f, -0.35f));
                oDirLight.intensity = 2.6f;
                // A tight frustum that follows the player beats a huge static
                // one: 70 m half-extent ≈ 6.8 cm per 2048² shadow-map texel;
                // UpdateCamera re-anchors the frustum every frame.
                oDirLight.shadow_ortho = 70.f;
                oDirLight.shadow_far   = 500.f;
                GetSystem<TRenderer>().SetDirLight(oDirLight);

                // Optional image-based lighting (scene_viewer flags): absent
                // files simply leave the DirLight-only look. Cubemaps need the
                // cube store strategy — the CreateResource default is Store2D,
                // which silently fails on .ktx cubemaps.
                if (!oSettings.sIblIrrPath.empty() && !oSettings.sIblLdPath.empty()) {
                        auto hIrr = CreateResource<TTexture>(oSettings.sIblIrrPath,
                                        StoreTextureCubeMap::Settings{});
                        auto hLd  = CreateResource<TTexture>(oSettings.sIblLdPath,
                                        StoreTextureCubeMap::Settings{});
                        if (hIrr.IsValid() && hLd.IsValid()) {
                                GetSystem<TRenderer>().SetIBL(hIrr, hLd);
                                GetSystem<TRenderer>().SetIBLScale(oSettings.ibl_scale);
                                GetSystem<TRenderer>().SetIBLRotation(oSettings.ibl_rotation);
                                log_i("world_demo: IBL on ('{}', scale {}, rot {})",
                                      oSettings.sIblIrrPath, oSettings.ibl_scale, oSettings.ibl_rotation);
                        } else {
                                log_w("world_demo: IBL textures failed to load ('{}', '{}')",
                                      oSettings.sIblIrrPath, oSettings.sIblLdPath);
                        }
                }
        }
#endif

        // --- world + terrain grid ---
        auto & oWorlds = GetSystem<WorldSystem>();
        oWorlds.SetSceneTree(pSceneTree);
        if (!oWorlds.LoadWorld(sWorldPath)) {
                log_e("world_demo: failed to load world '{}'", sWorldPath);
        }

        // the render-side terrain node joins the (deduped) world-grid registration
        pTerrainNode = pSceneTree->Create("TerrainGrid");
        {
                Terrain::SDesc oDesc;
                oDesc.sManifest   = "resource/terrain/world_demo/world_demo.seterrain";
                oDesc.b_grid_mode = true;
                pTerrainNode->CreateComponent<Terrain>(oDesc);
        }

        // the inn interior and the crypt are Manual — remember their slots for
        // the door interactions; the tower hosts the crypt entrance
        if (auto * pAsset = oWorlds.GetWorldAsset()) {
                const auto & vLevels = pAsset->GetSubLevels();
                for (uint32_t i = 0; i < vLevels.size(); ++i) {
                        if (vLevels[i].path.find("wd_inn_interior") != std::string::npos) {
                                inn_level_index = i;
                        } else if (vLevels[i].path.find("wd_crypt") != std::string::npos) {
                                crypt_level_index = i;
                        } else if (vLevels[i].path.find("wd_tower") != std::string::npos) {
                                tower_level_index = i;
                        }
                }
        }

        // Populations snap spawns to the navmesh through this probe; without
        // it SpawnerSystem falls back to the raw sampled position.
        GetSystem<SpawnerSystem>().SetNavProbe([](const glm::vec3 & vPos, glm::vec3 & vOut) {
                return GetSystem<NavigationSystem>()
                        .ProjectToNavMesh(StrID("default"), vPos, 4.0f, vOut);
        });

        // Cap navmesh queries: a BT whose goal is unreachable re-issues MoveTo
        // on its 0.2 s tick; the budget keeps a pathological world from
        // monopolizing a core with findPath work.
        NavigationSystem::SConfig oNavCfg;
        oNavCfg.max_queries_per_frame = 64;
        // findNearestPoly fallback extent; SetAgentPath pads per-agent extents
        // beyond this, but 1 m was too tight for off-mesh reseeds near tile seams
        oNavCfg.query_radius = 2.5f;
        GetSystem<NavigationSystem>().SetConfig(oNavCfg);

        // --- NPC templates + spawn route ---
        LoadNpcTemplates();
        GetSystem<EntityManager>().SetSceneTree(pSceneTree);
        GetSystem<EntityManager>().SetTemplateInstantiator(
                [this](const SpawnIntent & intent) -> TSceneTree::TSceneNodeWeak {
                        return SpawnNpcEntity(intent);
                });

        // --- player (spawns into the world; its focus drives streaming) ---
        SpawnPlayer(oSettings);
        // matches the world's load_radius (96): a focus radius > 0 overrides
        // it, so this must move with build_village.py's streaming block.
        hPlayerFocus = oWorlds.RegisterFocus(pPlayerNode, 96.0f, 0.0f);

        // page the initial ring in before the first frame (loading-screen seam)
        oWorlds.FlushStreaming(5000.f, &Scene::FlushPump);

        // --- ambient sound (moving_actors pattern) ---
        GetSystem<SoundEventSystem>().LoadCues("resource/sound_cue/ambient.secl");
        GetSystem<SoundEventSystem>().LoadCues("resource/sound_cue/character.secl");
        GetSystem<SoundEventSystem>().LoadCues("resource/sound_cue/village.secl");
        {
                SoundEventContext oAmbient;
                GetSystem<SoundEventSystem>().Post("ambient", oAmbient);
        }

        // BT debugger needs the scene to find the brains
        GetSystem<BehaviorTreeDebugger>().SetSceneTree(pSceneTree);

        // All panels render as tabs of the single F3 window — their
        // standalone windows stay disabled (and only the ACTIVE tab costs
        // per-frame work, instead of eight windows drawing simultaneously).
        GetSystem<StreamingDebugger>().SetTabbed(true);
        GetSystem<TerrainDebugger>().SetTabbed(true);
        GetSystem<SpatialIndexDebugger>().SetTabbed(true);
        GetSystem<SpawnerDebugger>().SetTabbed(true);
        GetSystem<NavigationDebugger>().SetTabbed(true);
        GetSystem<CrowdDebugger>().SetTabbed(true);
        GetSystem<BehaviorTreeDebugger>().SetTabbed(true);
        GetSystem<BlackboardInspector>().SetTabbed(true);

        // grab the mouse from the start (moving_actors feel); RMB or ESC
        // releases it for ImGui interaction
        {
                auto & oInput = GetSystem<InputManager>();
                oInput.SetMouseGrabbed(true);
                oInput.SetMouseVisible(false);
                mouse_locked = true;
        }

        log_i("world_demo: scene ready ('{}')", sWorldPath);
}

// ---------------------------------------------------------------------------

Scene::~Scene() noexcept {

        auto & oEM = GetSystem<EventManager>();
        oEM.RemoveListener<EUpdate,          &Scene::OnUpdate>(this);
        oEM.RemoveListener<EMouseMove,       &Scene::OnMouseMove>(this);
        oEM.RemoveListener<EKeyDown,         &Scene::OnKeyDown>(this);
        oEM.RemoveListener<EMouseButtonUp,   &Scene::OnMouseButtonUp>(this);
        oEM.RemoveListener<EMouseWheel,      &Scene::OnMouseWheel>(this);
        oEM.RemoveListener<ETriggerFired,    &Scene::OnTriggerFired>(this);
        oEM.RemoveListener<ESubLevelActivated,   &Scene::OnSubLevelActivated>(this);
        oEM.RemoveListener<ESubLevelDeactivated, &Scene::OnSubLevelDeactivated>(this);

        pCharVisualNode.reset();
        pPlayerNode.reset();
        pCameraNode.reset();

        GetSystem<WorldSystem>().SetSceneTree(nullptr);
        GetSystem<BehaviorTreeDebugger>().SetSceneTree(nullptr);

        // The scene owns its tree: release it here, not from the manager's dtor
        // inside ~Engine, where the renderer this teardown calls back into is
        // already destroyed. The destroy is deferred; ~Application flushes it.
        DestroyResource(hSceneTree);
}

// ---------------------------------------------------------------------------

void Scene::LoadNpcTemplates() {

        auto & oETS = GetSystem<TEntityTemplateSystem>();
        oETS.SetSceneTree(pSceneTree);

        for (const char* szTemplate : { "villager_a", "villager_b", "farmer", "guard" }) {
                const std::string sPath = fmt::format("resource/world/village_gen/{}.setm", szTemplate);
                if (const auto res = oETS.Load(sPath); res != uSUCCESS) {
                        log_e("world_demo: NPC template '{}' failed (err={})", sPath, res);
                }
        }
}

// ---------------------------------------------------------------------------

void Scene::SpawnPlayer(const Settings& oSettings) {

        pPlayerNode = pSceneTree->Create("Player");
        // feet on the village plateau (height 2.0 m in the baked heightmap);
        // OnUpdate one-shot snaps to the real surface once terrain tiles page in
        pPlayerNode->SetPos(glm::vec3{0.f, 2.05f, 24.f});   // south end of the main street

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

                pCtx->vBindings.push_back({InputSource::KEY_DOWN,      InputAction::MOVE_AXIS_Y,   1.f, Keys::W});
                pCtx->vBindings.push_back({InputSource::KEY_DOWN,      InputAction::MOVE_AXIS_Y,  -1.f, Keys::S});
                pCtx->vBindings.push_back({InputSource::KEY_DOWN,      InputAction::MOVE_AXIS_X,  -1.f, Keys::A});
                pCtx->vBindings.push_back({InputSource::KEY_DOWN,      InputAction::MOVE_AXIS_X,   1.f, Keys::D});
                pCtx->vBindings.push_back({InputSource::KEY_PRESS,     InputAction::JUMP_PRESSED,  1.f, Keys::SPACE});
                pCtx->vBindings.push_back({InputSource::KEY_DOWN,      InputAction::JUMP_HELD,     1.f, Keys::SPACE});
                // SDL_SCANCODE_LSHIFT = 225
                pCtx->vBindings.push_back({InputSource::SCANCODE_DOWN, InputAction::SPRINT_HELD, 1.f, 225});

                // rotate the raw device axes into camera space inside the
                // context's own listener — a separate EInputUpdate listener
                // would race the flush by listener registration order (the
                // IMC would overwrite the rotated axes with raw ones)
                pCtx->post_process = SA::delegate<void (InputState &)>::create<
                                Scene, &Scene::ApplyInputFrame>(this);
        }

        // Physics character controller.
        // The capsule must fit the kit's 2.1 m door openings (Door_1_* leaves
        // are ~2.1 m tall in 3.12 m walls): 2*(0.65 + 0.35) = 2.0 m.
        {
                CharacterController::Desc ccDesc;
                ccDesc.capsule_radius      = 0.35f;
                ccDesc.capsule_half_height = 0.65f;
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

        // High-level player behavior state machine (Normal / Ragdoll)
        {
                StateMachine::Desc smDesc;
                smDesc.definition_path = sPlayerHSMPath.c_str();
                auto res = pPlayerNode->CreateComponent<StateMachine>(smDesc);
                if (res != uSUCCESS)
                        log_w("world_demo: player StateMachine failed (err={})", res);
                pPlayerSM = pPlayerNode->GetComponent<StateMachine>();
        }

        // Visual prefab as a child of the moving capsule
        {
                auto pInst = pSceneTree->Instantiate(sCharScenePath, pPlayerNode);
                if (pInst) {
                        const std::string base = pInst->GetFullName();
                        pCharVisualNode = pSceneTree->FindFullName(StrID(base + "|ManArmature"));
                        if (auto pAnimNode = FindNodeWithComponent<Animator>(pInst))
                                pPlayerAnimator = pAnimNode->GetComponent<Animator>();
                        if (!pCharVisualNode)
                                log_w("world_demo: ManArmature not found under '{}'", base);
                } else {
                        log_w("world_demo: failed to instantiate char prefab '{}'", sCharScenePath);
                }
        }

        if (pPlayerCC && pPlayerAnimator)
                GetSystem<CharacterAnimationSystem>().Register(pPlayerNode, pPlayerCC, pPlayerAnimator);

        v_cam_target_smooth = pPlayerNode->GetTransform().GetWorldPos()
                            + glm::vec3{0.f, 1.4f, 0.f};
}

// ---------------------------------------------------------------------------
// SpawnNpcEntity — EntityManager template instantiator for Population members.
// The .setm template carries the visual (Armature + Animator) and the AI
// components (NavAgent, PathFollower, Blackboard, BehaviorTree); app-level
// components that cannot ride the FlatBuffers union (InputState,
// CharacterController) plus the locomotion→animation bridge are attached here.
// ---------------------------------------------------------------------------

TSceneTree::TSceneNodeWeak Scene::SpawnNpcEntity(const SpawnIntent& intent) {

        auto & oETS = GetSystem<TEntityTemplateSystem>();
        auto pNode = oETS.Instantiate(intent.prefab_id, intent.pParent,
                                      intent.vTranslation, intent.vRotation,
                                      intent.vScale, intent.enabled).lock();
        if (!pNode) {
                log_e("world_demo: NPC template '{}' failed to instantiate", intent.prefab_id);
                return {};
        }

        // Input contract for the crowd driver (CrowdSystem writes move_axis).
        pNode->CreateComponent<InputState>();

        // Physics
        CharacterController::Desc ccDesc;
        ccDesc.capsule_radius      = 0.4f;
        ccDesc.capsule_half_height = 0.9f;
        ccDesc.max_speed           = 3.2f;
        ccDesc.sprint_multiplier   = 1.0f;
        ccDesc.acceleration        = 15.0f;
        ccDesc.deceleration        = 25.0f;
        ccDesc.jump_impulse        = 0.0f;
        // Sweep-only: no inner presence body. NPCs stay ghosts to each other
        // (the crowd layer handles avoidance) and never fire player-only
        // trigger sensors — a villager must not ring the bell by walking past.
        ccDesc.inner_body_layer    = -1;
        // Ghost the KINEMATIC follower capsules (the template's "Physics"
        // child exists to block the PLAYER only) — otherwise every villager
        // sweeps its own capsule and depenetration hurls it into the void.
        ccDesc.collides_kinematic  = false;
        // Character LOD: with ~15 characters alive (villagers + alarm guards)
        // the per-step heightfield/broadphase sweeps dominated a core; 30 Hz
        // per-NPC updates match the crowd batch tick and halve that cost.
        ccDesc.update_divider      = 2;
        pNode->CreateComponent<CharacterController>(ccDesc);

        // Locomotion→animation bridge
        if (auto pAnimNode = FindNodeWithComponent<Animator>(pNode)) {
                if (auto * pCC = pNode->GetComponent<CharacterController>()) {
                        GetSystem<CharacterAnimationSystem>().Register(
                                        TSceneTree::TSceneNodeWeak(pNode), pCC,
                                        pAnimNode->GetComponent<Animator>());
                }
        } else {
                log_w("world_demo: NPC '{}' has no Animator — dead visual", intent.prefab_id);
        }

        // facing: the armature child rotates toward the movement direction
        if (auto pArmature = pNode->FindChild(StrID("Armature"), /*recursive=*/true)) {
                NpcVisual vis;
                vis.pNode   = pNode->weak_from_this();
                vis.pVisual = pArmature;
                vis.pCC     = pNode->GetComponent<CharacterController>();
                vNpcVisuals.push_back(std::move(vis));
        } else {
                log_w("world_demo: NPC '{}' has no Armature child — facing disabled",
                      intent.prefab_id);
        }

        return pNode->weak_from_this();
}

// ---------------------------------------------------------------------------

void Scene::Process() {

        const float indent = 10;
        ImVec2 window_pos = ImVec2(ImGui::GetIO().DisplaySize.x - indent, indent);
        ImVec2 window_pos_pivot = ImVec2(1.0f, 0.0f);
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
        ImGui::SetNextWindowBgAlpha(0.7f);
        ImGui::Begin("Info", nullptr,
                        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar |
                        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize |
                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                        ImGuiWindowFlags_NoNav);

        auto & oFpsTracker = GetSystem<FpsTracker>();
        ImGui::Text("Frame time: %.2f, FPS: %.2f", oFpsTracker.FrameMs(), oFpsTracker.Fps());

        ImGui::Separator();
        ImGui::Text("F3 debug window  G LOD overlay   B navmesh");
        ImGui::Text("P stress-spawn    E doors           ESC unlock mouse");

        auto & oEntityMgr = GetSystem<EntityManager>();
        ImGui::Separator();
        ImGui::Text("Entity spawn queue:   %u", oEntityMgr.SpawnQueueDepth());
        ImGui::Text("Entity destroy queue: %u", oEntityMgr.DestroyQueueDepth());
        ImGui::End();

        // --- interact prompt (bottom-center) -----------------------------------
        if (auto pDoor = pInteractDoor.lock()) {
                const std::string sName = PrettyDoorName(pDoor->GetName());
                bool open = false;
                for (const Door & d : vDoors)
                        if (d.pNode.lock().get() == pDoor.get()) { open = d.open; break; }
                ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f,
                                               ImGui::GetIO().DisplaySize.y - 90.f),
                                        ImGuiCond_Always, ImVec2(0.5f, 0.0f));
                ImGui::SetNextWindowBgAlpha(0.6f);
                ImGui::Begin("Interact", nullptr,
                                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar |
                                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize |
                                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                ImGuiWindowFlags_NoNav);
                ImGui::Text("[E] %s %s", open ? "Close" : "Open", sName.c_str());
                ImGui::End();
        }

        // --- event toasts (bottom-left, fade out) -------------------------------
        if (!vToasts.empty()) {
                ImGui::SetNextWindowPos(ImVec2(16.f, ImGui::GetIO().DisplaySize.y - 60.f),
                                        ImGuiCond_Always, ImVec2(0.0f, 1.0f));
                ImGui::SetNextWindowBgAlpha(0.5f);
                ImGui::Begin("Toasts", nullptr,
                                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar |
                                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize |
                                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                ImGuiWindowFlags_NoNav);
                for (const Toast & t : vToasts) {
                        ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, glm::clamp(t.life, 0.f, 1.f)),
                                           "%s", t.sText.c_str());
                }
                ImGui::End();
        }

        // --- F3: single tabbed engine-debug window ------------------------------
        if (debug_window) {
                ImGui::SetNextWindowSize(ImVec2(560.f, 480.f), ImGuiCond_FirstUseEver);
                if (ImGui::Begin("Engine Debug", &debug_window)) {
                        if (ImGui::BeginTabBar("DebugTabs")) {
                                GetSystem<StreamingDebugger>().DrawTab();
                                GetSystem<TerrainDebugger>().DrawTab();
                                GetSystem<SpatialIndexDebugger>().DrawTab();
                                GetSystem<SpawnerDebugger>().DrawTab();
                                GetSystem<NavigationDebugger>().DrawTab();
                                GetSystem<CrowdDebugger>().DrawTab();
                                GetSystem<BehaviorTreeDebugger>().DrawTab();
                                GetSystem<BlackboardInspector>().DrawTab();
                                ImGui::EndTabBar();
                        }
                }
                ImGui::End();
        }
}

// ---------------------------------------------------------------------------

void Scene::OnUpdate(const Event & oEvent) {

        const auto & e = oEvent.Get<EUpdate>();
        const float dt = e.last_frame_time;

        UpdateCharFacing(dt);
        UpdateCamera(dt);
        UpdateGlobalBlackboard(dt);
        UpdateDoors(dt);

        // age HUD toasts
        for (auto & t : vToasts) t.life -= dt;
        vToasts.erase(std::remove_if(vToasts.begin(), vToasts.end(),
                        [](const Toast & t) { return t.life <= 0.f; }),
                vToasts.end());

        // nearest door in interact range drives the HUD prompt
        pInteractDoor.reset();
        if (pPlayerNode) {
                const glm::vec3 vPlayer = pPlayerNode->GetTransform().GetWorldPos();
                float best = 2.6f;
                for (const Door & d : vDoors) {
                        auto pDoor = d.pNode.lock();
                        if (!pDoor) continue;
                        const float dist = glm::length(pDoor->GetTransform().GetWorldPos() - vPlayer);
                        if (dist < best) {
                                best = dist;
                                pInteractDoor = pDoor;
                        }
                }
        }

        // one-shot spawn ground snap: terrain tiles page in right after the
        // world loads; the authored spawn Y (plateau height) is only a fallback
        // for custom hand-authored heightmaps
        if (!spawn_snapped && pPlayerCC && pPlayerNode) {
                const glm::vec3 vPos = pPlayerNode->GetTransform().GetWorldPos();
                float fHeight = 0.f;
                if (GetSystem<TerrainSystem>().TryGetHeight(vPos, fHeight)) {
                        GetSystem<PhysicsSystem>().TeleportCharacter(
                                        pPlayerCC->GetHandle(),
                                        glm::vec3(vPos.x, fHeight + 0.05f, vPos.z),
                                        glm::quat(1.f, 0.f, 0.f, 0.f));
                        spawn_snapped = true;
                        log_i("world_demo: player snapped to terrain ({:.2f} m)",
                              fHeight);
                }
        }

        // terrain quadtree/LOD overlay (G)
        if (lod_overlay && pTerrainNode) {
                if (auto * pTerrain = pTerrainNode->GetComponent<Terrain>()) {
                        pTerrain->DrawDebug();
                }
        }
}

// ---------------------------------------------------------------------------

void Scene::UpdateCharFacing(float dt) {

        auto doFacing = [dt](const TSceneTree::TSceneNode & pVisual,
                             CharacterController * pCC, float & facing_deg) {
                if (!pVisual || !pCC) return;
                const glm::vec3 vel = pCC->GetVelocity();
                const glm::vec2 v2d = {vel.x, vel.z};
                if (glm::length(v2d) > 0.5f) {
                        const float target = glm::degrees(std::atan2(v2d.x, v2d.y));
                        float diff = target - facing_deg;
                        while (diff >  180.f) diff -= 360.f;
                        while (diff < -180.f) diff += 360.f;
                        facing_deg += diff * glm::clamp(10.f * dt, 0.f, 1.f);
                }
                pVisual->SetRotation(glm::vec3{0.f, facing_deg, 0.f});
        };

        doFacing(pCharVisualNode, pPlayerCC, char_facing_deg);

        // prune despawned NPCs, rotate the rest
        vNpcVisuals.erase(std::remove_if(vNpcVisuals.begin(), vNpcVisuals.end(),
                        [](const NpcVisual & v) { return v.pNode.expired(); }),
                vNpcVisuals.end());
        for (NpcVisual & nv : vNpcVisuals) {
                doFacing(nv.pVisual.lock(), nv.pCC, nv.facing_deg);
        }
}

// ---------------------------------------------------------------------------

void Scene::UpdateCamera(float dt) {

        if (!pPlayerNode || !pCamera) return;

        // Track the character tightly (moving_actors pattern: no exponential
        // smoothing — the capsule updates at the physics rate).
        v_cam_target_smooth = pPlayerNode->GetTransform().GetWorldPos()
                            + glm::vec3{0.f, 1.4f, 0.f};

        const float yaw_rad   = glm::radians(cam_yaw);
        const float pitch_rad = glm::radians(cam_pitch);
        const glm::vec3 offset{
                cam_dist * std::cos(pitch_rad) * std::sin(yaw_rad),
                cam_dist * std::sin(pitch_rad),
                cam_dist * std::cos(pitch_rad) * std::cos(yaw_rad)
        };
        const glm::vec3 cam_pos = v_cam_target_smooth + offset;

        pCamera->SetPos(cam_pos.x, cam_pos.y, cam_pos.z);
        pCamera->LookAt(v_cam_target_smooth);

#if SE_DEFERRED_RENDERER
        // keep the shadow frustum centred on the player (the renderer snaps
        // the anchor to the shadow-map texel grid, so no edge shimmer)
        oDirLight.shadow_center = pPlayerNode->GetTransform().GetWorldPos();
        GetSystem<TRenderer>().SetDirLight(oDirLight);
#endif
}

// ---------------------------------------------------------------------------

void Scene::UpdateGlobalBlackboard(float dt) {

        // Guards chase player_pos through the crowd/MoveTo stack; 10 Hz is
        // plenty for a wandering target and keeps the change events quiet.
        bb_pos_timer += dt;
        if (bb_pos_timer < 0.1f) return;
        bb_pos_timer = 0.f;

        auto & oBB = GetSystem<BlackboardSystem>();
        if (!oBB.HasGlobal() || !pPlayerNode) return;
        oBB.GetGlobal().SetVec3(StrID("player_pos"),
                                pPlayerNode->GetTransform().GetWorldPos());
}

// ---------------------------------------------------------------------------

void Scene::ApplyInputFrame(InputState & oInput) {

        static const StrID sStateNormal("Normal");

        // High-level player state gates intent: no movement/jump while not
        // Normal (e.g. ragdolled).
        if (pPlayerSM && pPlayerSM->GetCurrentState() != sStateNormal) {
                oInput.move_axis    = {0.f, 0.f};
                oInput.jump_pressed = false;
                oInput.jump_held    = false;
                return;
        }

        const float yaw_rad = glm::radians(cam_yaw);
        const float c = std::cos(yaw_rad);
        const float s = std::sin(yaw_rad);
        const glm::vec2 raw = oInput.move_axis;
        // raw.x = A/D (strafe), raw.y = W/S (forward). The orbit camera sits at
        // offset (sin yaw, cos yaw) and looks back, so "into the screen" is
        // (-sin, -cos) and screen-right is (cos, -sin).
        oInput.move_axis = { raw.x * c - raw.y * s,
                            -raw.x * s - raw.y * c };
}

// ---------------------------------------------------------------------------

void Scene::OnMouseMove(const Event& e) {
        if (!mouse_locked) return;
        const auto& ev = e.Get<EMouseMove>();
        cam_yaw   += ev.delta.x * 0.25f;
        cam_pitch  = glm::clamp(cam_pitch - ev.delta.y * 0.25f, -10.f, 65.f);
}

void Scene::OnMouseButtonUp(const Event& e) {
        if (e.Get<EMouseButtonUp>().button != MouseB::RIGHT) return;
        // RMB toggles the grab — but never steal the click from ImGui windows
        // when the cursor is free (that's how panels get interacted with).
        if (!mouse_locked && ImGui::GetIO().WantCaptureMouse) return;

        mouse_locked = !mouse_locked;
        auto & oInput = GetSystem<InputManager>();
        oInput.SetMouseGrabbed(mouse_locked);
        oInput.SetMouseVisible(!mouse_locked);
}

void Scene::OnMouseWheel(const Event& e) {
        const float delta = e.Get<EMouseWheel>().delta;
        cam_dist = glm::clamp(cam_dist - delta * 0.6f, 3.0f, 18.0f);
}

// ---------------------------------------------------------------------------

void Scene::OnKeyDown(const Event & oEvent) {

        const auto key = oEvent.Get<EKeyDown>().key;

        if (key == Keys::F3) {
                // one tabbed debug window: the panels render only while their
                // tab is active (previously all 8 windows drew every frame)
                debug_window = !debug_window;
        }
        else if (key == Keys::G) {
                lod_overlay = !lod_overlay;
        }
        else if (key == Keys::B) {
                auto & oCfg = GetSystem<NavigationSystem>().DebugConfig();
                oCfg.enabled = !oCfg.enabled;
                oCfg.tiles   = oCfg.enabled;
                oCfg.polys   = oCfg.enabled;
                oCfg.paths   = oCfg.enabled;
        }
        else if (key == Keys::P) {
                // stress-spawn guards around the camp square (crowd load check;
                // target: ~64 avoiding agents at 30 Hz within < 1 ms)
                auto & oEM = GetSystem<EntityManager>();
                for (int i = 0; i < 8; ++i) {
                        SpawnIntent intent;
                        intent.prefab_id = "guard";
                        const float ang = float(stress_seq % 16) * (3.1415926f / 8.0f);
                        const float rad = 8.0f + float(stress_seq % 8);
                        intent.vTranslation = glm::vec3(-76.0f + std::cos(ang) * rad, 3.0f,
                                                        4.0f + std::sin(ang) * rad);
                        oEM.RequestSpawn(intent);
                        ++stress_seq;
                }
                log_i("world_demo: stress wave spawned (total {})", stress_seq);
        }
        else if (key == Keys::E) {
                ToggleNearestDoor();
        }
        else if (key == Keys::ESCAPE) {
                if (mouse_locked) {
                        mouse_locked = false;
                        auto & oInput = GetSystem<InputManager>();
                        oInput.SetMouseGrabbed(false);
                        oInput.SetMouseVisible(true);
                } else {
                        GetSystem<EventManager>().TriggerEvent(EQuit{});
                }
        }
}

// ---------------------------------------------------------------------------

void Scene::OnTriggerFired(const Event & oEvent) {

        const auto & e = oEvent.Get<ETriggerFired>();

        if (e.event_id == StrID("bell_ring")) {
                SoundEventContext ctx;
                GetSystem<SoundEventSystem>().Post("village.bell", ctx);
                AddToast("The village bell rings — the villagers will investigate.");
                log_i("world_demo: the village bell rings");
        }
        else if (e.event_id == StrID("camp_alarm")) {
                SoundEventContext ctx;
                GetSystem<SoundEventSystem>().Post("camp.alarm", ctx);
                AddToast("Camp alarm! The guards are coming for you.");
                log_i("world_demo: camp alarm — the guards are coming");
        }
}

// ---------------------------------------------------------------------------
// Doors — sub-level nodes named "*_Door" are hinge-origin leaves. Opening one
// can also drive the world: the inn door loads/unloads the Manual interior
// (its navmesh layer follows through WorldNavBridge), the crypt doors run the
// interior-vs-exterior streaming swap.
// ---------------------------------------------------------------------------

Scene::Door::Kind Scene::KindOf(const std::string & sName) {
        if (sName == "Crypt_Door")          return Door::Kind::CryptEnter;
        if (sName == "Crypt_Exit_Door")     return Door::Kind::CryptExit;
        if (sName.find("Inn") != std::string::npos) return Door::Kind::Inn;
        return Door::Kind::Plain;
}

std::string Scene::PrettyDoorName(const std::string & sName) {
        std::string s = sName;
        const std::string suffix = "_Door";
        if (s.size() > suffix.size() && s.compare(s.size() - suffix.size(),
                                                  suffix.size(), suffix) == 0) {
                s.resize(s.size() - suffix.size());
        }
        std::replace(s.begin(), s.end(), '_', ' ');
        return s;
}

void Scene::CollectDoors(const TSceneTree::TSceneNodeWeak & pRootWeak) {

        auto pRoot = pRootWeak.lock();
        if (!pRoot) return;

        pRoot->DepthFirstWalk([this](auto & oNode) -> bool {
                const std::string & sName = oNode.GetName();
                if (sName.size() > 5 && sName.compare(sName.size() - 5, 5, "_Door") == 0) {
                        auto pNode = oNode.shared_from_this();
                        const bool bKnown = std::any_of(vDoors.begin(), vDoors.end(),
                                [&pNode](const Door & d) { return d.pNode.lock().get() == pNode.get(); });
                        if (!bKnown) {
                                Door d;
                                d.pNode = pNode;
                                d.sName = sName;
                                d.kind  = KindOf(sName);
                                d.closed_yaw = oNode.GetTransform().GetRotationDeg().y;
                                // door→interior streaming lives in the engine-side
                                // Portal component authored on the door nodes
                                vDoors.push_back(std::move(d));
                        }
                }
                return true;
        });
}

void Scene::OnSubLevelActivated(const Event & oEvent) {
        CollectDoors(oEvent.Get<ESubLevelActivated>().root);
}

void Scene::OnSubLevelDeactivated(const Event &) {
        // Drop registry entries whose level tore down (weak refs expired).
        vDoors.erase(std::remove_if(vDoors.begin(), vDoors.end(),
                        [](const Door & d) { return d.pNode.expired(); }),
                vDoors.end());
}

void Scene::UpdateDoors(float dt) {

        // prune expired leaves first — their anims die with them
        vDoorAnims.erase(std::remove_if(vDoorAnims.begin(), vDoorAnims.end(),
                        [](const DoorAnim & a) {
                                if (a.pNode.expired()) return true;
                                if (a.t >= 1.f) return true;
                                return false;
                        }),
                vDoorAnims.end());

        constexpr float kDuration = 0.4f;
        for (DoorAnim & a : vDoorAnims) {
                a.t = glm::min(a.t + dt / kDuration, 1.f);
                if (auto pNode = a.pNode.lock()) {
                        const float fYaw = a.from + (a.to - a.from) * a.t;
                        pNode->SetRotation(glm::vec3{0.f, fYaw, 0.f});
                }
        }

        // armed crypt swaps fire once the leaf has finished swinging; the door
        // shuts silently first so a failed swap leaves it re-openable with E
        for (Door & d : vDoors) {
                if (d.swap_timer < 0.f) { continue; }
                d.swap_timer -= dt;
                if (d.swap_timer >= 0.f) { continue; }
                const Door::Kind eKind = d.kind;
                SilentCloseDoor(d);
                if (eKind == Door::Kind::CryptEnter) { EnterCrypt(); }
                else                                 { ExitCrypt(); }
        }
}

// Door↔interior streaming lives in the engine-side Portal component (the
// "portal" pattern): a Manual interior is staged through the budgeted drain
// while any streaming focus is NEAR its door node, and released once the
// focus has left the hysteresis band — no manual focus zones, no
// synchronous activation spike on interact. The demo's Door component only
// owns the leaf-swing + swap gameplay.

void Scene::ToggleNearestDoor() {

        auto pTarget = pInteractDoor.lock();
        if (!pTarget) return;

        for (Door & d : vDoors) {
                auto pNode = d.pNode.lock();
                if (!pNode || pNode.get() != pTarget.get()) continue;
                SetDoorOpen(d, !d.open);
                return;
        }
}

void Scene::SetDoorOpen(Door & d, bool open) {

        if (d.swap_timer >= 0.f) { return; }   // swap already armed — locked

        d.open = open;

        // swing the leaf (the kinematic leaf body follows via MoveKinematic)
        if (auto pNode = d.pNode.lock()) {
                const float from = pNode->GetTransform().GetRotationDeg().y;   // yaw-only leaf
                DoorAnim a;
                a.pNode = pNode;
                a.from = from;
                // relative to the authored closed yaw: doors sit on x-walls
                // (closed 0) and z-walls (closed ±90) alike. The inn swings
                // INWARD: its facade fronts the street, and an outward leaf
                // juts 1.1 m across the approach — the open door blocked the
                // entry line it was opened for.
                const float sign = (d.kind == Door::Kind::Inn) ? -1.f : 1.f;
                a.to   = open ? d.closed_yaw + sign * 90.f : d.closed_yaw;
                a.t     = 0.f;
                vDoorAnims.push_back(std::move(a));
        }

        auto & oWorlds = GetSystem<WorldSystem>();

        switch (d.kind) {
        case Door::Kind::Inn:
                if (open && inn_level_index != UINT32_MAX) {
                        // UpdateDoorStreaming may already be staging (Loading) or
                        // have finished (Active) the interior — ActivateLevel only
                        // takes fully Unloaded levels, so treat "in flight" as in.
                        const auto eState = oWorlds.GetLevelState(inn_level_index);
                        inn_active = (eState != WorldSystem::ELevelState::Unloaded)
                                  || oWorlds.ActivateLevel(inn_level_index);
                        log_i("world_demo: inn interior {}", inn_active ? "ready" : "activation refused");
                } else if (!open && inn_active && inn_level_index != UINT32_MAX) {
                        // unloading the interior with the player inside would
                        // drop them onto the street — refuse until they leave
                        const glm::vec3 vPlayer = pPlayerNode->GetTransform().GetWorldPos();
                        const auto & entry = oWorlds.GetWorldAsset()->GetSubLevels()[inn_level_index];
                        const bool bInside = glm::all(glm::greaterThanEqual(vPlayer, entry.bounds_min))
                                          && glm::all(glm::lessThanEqual(vPlayer, entry.bounds_max));
                        if (bInside) {
                                AddToast("The inn stays furnished while you're inside.");
                        } else if (oWorlds.DeactivateLevel(inn_level_index)) {
                                inn_active = false;
                                log_i("world_demo: inn interior deactivated");
                        }
                }
                break;

        case Door::Kind::CryptEnter:
                if (open) {
                        // the leaf swings shut behind the player: the swap
                        // (teleport + silent close) fires in UpdateDoors once
                        // the 0.4 s tween has settled
                        d.swap_timer = kCryptSwapDelay;
                }
                break;

        case Door::Kind::CryptExit:
                if (open) {
                        d.swap_timer = kCryptSwapDelay;
                }
                break;

        case Door::Kind::Plain:
                break;
        }
}

// Reset a crypt door leaf without a tween (the player is elsewhere by then);
// drops any queued swing so it can't fight the reset.
void Scene::SilentCloseDoor(Door & d) {

        d.open      = false;
        d.swap_timer = -1.f;

        vDoorAnims.erase(std::remove_if(vDoorAnims.begin(), vDoorAnims.end(),
                        [&d](const DoorAnim & a) {
                                return a.pNode.lock().get() == d.pNode.lock().get();
                        }),
                vDoorAnims.end());

        if (auto pNode = d.pNode.lock()) {
                pNode->SetRotation(glm::vec3{0.f, d.closed_yaw, 0.f});
        }
}

void Scene::FlushPump(uint32_t, uint32_t) {
        SDL_PumpEvents();
        SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
}

void Scene::EnterCrypt() {

        if (crypt_level_index == UINT32_MAX || crypt_active) return;

        auto & oWorlds = GetSystem<WorldSystem>();

        // The door portal may already be staging the crypt (UpdateDoorStreaming
        // pre-loads it inside 22 m) — ActivateLevel refuses anything not
        // Unloaded, and FlushStreaming's stats can't see Manual levels at all.
        // FlushLevel drives whatever is in flight to Active; a level that was
        // mid-teardown lands Unloaded on pass 1 and activates on pass 2.
        for (int attempt = 0; attempt < 2; ++attempt) {
                const auto eState = oWorlds.GetLevelState(crypt_level_index);
                if (eState == WorldSystem::ELevelState::Active) { break; }
                if (eState == WorldSystem::ELevelState::Unloaded) {
                        if (!oWorlds.RequestActivation(crypt_level_index)) {
                                log_e("world_demo: crypt activation failed");
                                return;
                        }
                }
                oWorlds.FlushLevel(crypt_level_index, 5000.f, &Scene::FlushPump);
        }
        if (oWorlds.GetLevelState(crypt_level_index) != WorldSystem::ELevelState::Active) {
                log_e("world_demo: crypt activation failed");
                return;
        }

        // move the player onto the crypt's authored spawn anchor
        if (auto pRoot = oWorlds.GetLevelRoot(crypt_level_index).lock()) {
                if (auto pSpawn = pRoot->FindChild(StrID("CryptSpawn"), /*recursive=*/true)) {
                        const glm::vec3 vPos = pSpawn->GetTransform().GetWorldPos();
                        GetSystem<PhysicsSystem>().TeleportCharacter(
                                        pPlayerCC->GetHandle(),
                                        vPos + glm::vec3(0.f, 0.05f, 0.f),
                                        glm::quat(1.f, 0.f, 0.f, 0.f));
                } else {
                        log_e("world_demo: crypt spawn anchor missing");
                }
        }

        // Streaming swap: the player focus moves into the crypt with the
        // teleport. The crypt sits directly under the tower portal, so the
        // focus radius doubles as the transition grace zone — levels and
        // terrain near the exit stay resident while the far exterior sheds
        // through the planner's normal hysteresis. The crypt itself is Manual
        // (the director never touches it). Without a director the exterior is
        // dropped explicitly.
        if (!oWorlds.GetWorldAsset()->GetStreaming().enabled) {
                for (uint32_t i = 0; i < oWorlds.GetSubLevelCount(); ++i) {
                        if (i == crypt_level_index) continue;
                        const auto & entry = oWorlds.GetWorldAsset()->GetSubLevels()[i];
                        if (!entry.IsManual() && oWorlds.IsLevelActive(i)) {
                                oWorlds.DeactivateLevel(i);
                        }
                }
        }

        crypt_active = true;
        AddToast("You descend into the crypt — the world above goes quiet.");
        log_i("world_demo: crypt entered (grace zone keeps the portal area resident)");
}

void Scene::ExitCrypt() {

        if (!crypt_active) return;

        auto & oWorlds = GetSystem<WorldSystem>();

        // Out to the tower portal. The player focus never left, so the area
        // around the exit is already resident — no flush, no stall; far tiles
        // stream back in the background. Deactivate the crypt after the
        // teleport so its teardown never touches the player.
        bool bTeleported = false;
        if (tower_level_index != UINT32_MAX) {
                if (auto pRoot = oWorlds.GetLevelRoot(tower_level_index).lock()) {
                        if (auto pExit = pRoot->FindChild(StrID("CryptExit"), /*recursive=*/true)) {
                                const glm::vec3 vPos = pExit->GetTransform().GetWorldPos();
                                GetSystem<PhysicsSystem>().TeleportCharacter(
                                                pPlayerCC->GetHandle(),
                                                vPos + glm::vec3(0.f, 0.05f, 0.f),
                                                glm::quat(1.f, 0.f, 0.f, 0.f));
                                bTeleported = true;
                        }
                }
        }
        if (!bTeleported) log_e("world_demo: crypt exit anchor missing");

        if (oWorlds.DeactivateLevel(crypt_level_index)) {
                log_i("world_demo: crypt unloaded");
        }

        crypt_active = false;
        AddToast("You return to the surface.");
}

void Scene::AddToast(const std::string & sText) {
        vToasts.push_back({sText, 4.5f});
        if (vToasts.size() > 4) vToasts.erase(vToasts.begin());
}

// ---------------------------------------------------------------------------

} // namespace SE


#pragma pop_macro("Bool")

#endif // SE_IMPL
