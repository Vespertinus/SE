#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <Global.h>
#include <GlobalTypes.h>
#include <Camera.h>
#include <InputCodes.h>
#include <CommonEvents.h>
#include <InputEvents.h>
#include <ui/UIDataModel.h>
#include <ui/UIWidget.h>
#include <ui/UIEventRouter.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Context.h>
#include "Scene.h"

namespace SE {

Scene::Scene(const Settings & oSettings) :
        hSceneTree(CreateResource<TSceneTree>("UIScene", true)) {

        pSceneTree = GetResource(hSceneTree);
        se_assert(pSceneTree);

        pCameraNode = pSceneTree->Create("Camera");
        auto res = pCameraNode->CreateComponent<Camera>(oSettings.oCamSettings);
        if (res != uSUCCESS)
                throw std::runtime_error("failed to create Camera component");
        pCamera = pCameraNode->GetComponent<Camera>();
        se_assert(pCamera);

        pCamera->SetPos(0.0f, 0.0f, cam_radius);
        pCamera->LookAt(0.0f, 0.0f, 0.0f);
        GetSystem<TRenderer>().SetCamera(pCamera);

        // Helper: build the fnSetup lambda for a character stats widget
        auto makeStatsSetup = [](TSceneTree::TSceneNode pNode, CharacterStats * pStats) {
                return std::function<void(Rml::DataModelConstructor &)>(
                        [pNode, pStats](Rml::DataModelConstructor & ctor) {
                                ctor.BindFunc("health_w", [pStats](Rml::Variant & v) {
                                        char buf[8];
                                        std::snprintf(buf, sizeof(buf), "%d%%",
                                                      pStats->health * 100 / pStats->max_health);
                                        v = Rml::String(buf);
                                });
                                ctor.BindFunc("health_label", [pStats](Rml::Variant & v) {
                                        char buf[16];
                                        std::snprintf(buf, sizeof(buf), "%d/%d",
                                                      pStats->health, pStats->max_health);
                                        v = Rml::String(buf);
                                });
                                ctor.BindFunc("armor", [pStats](Rml::Variant & v) {
                                        v = Rml::String(std::to_string(pStats->armor));
                                });
                                ctor.BindFunc("level", [pStats](Rml::Variant & v) {
                                        v = Rml::String(std::to_string(pStats->level));
                                });
                                ctor.BindFunc("hovered", [pNode](Rml::Variant & v) {
                                        Camera * pCam = GetSystem<TRenderer>().GetCamera();
                                        if (!pCam) { v = false; return; }
                                        const glm::vec3 centre =
                                                pNode->GetTransform().GetWorldPos() + glm::vec3(0, 1.0f, 0);
                                        const glm::vec4 clip =
                                                pCam->GetWorldMVP() * glm::vec4(centre, 1.0f);
                                        if (clip.w <= 0.0f) { v = false; return; }
                                        const glm::vec2 sz(GetSystem<GraphicsState>().GetScreenSize());
                                        const float sx = ( clip.x / clip.w * 0.5f + 0.5f) * sz.x;
                                        const float sy = (-clip.y / clip.w * 0.5f + 0.5f) * sz.y;
                                        const glm::ivec2 mouse = GetSystem<InputManager>().GetMousePos();
                                        const float dx = mouse.x - sx, dy = mouse.y - sy;
                                        v = (dx * dx + dy * dy < 100.0f * 100.0f);
                                });
                        });
        };

        pChar1Node = pSceneTree->Create("Char1");
        res = pChar1Node->CreateComponent<UIWidget>(
                "ui/widget_stats.rml", UILayer::HUD,
                makeStatsSetup(pChar1Node, &oStats1));
        if (res != uSUCCESS)
                throw std::runtime_error("failed to create UIWidget for Char1");

        pChar2Node = pSceneTree->Create("Char2");
        pChar2Node->SetPos(glm::vec3(3.0f, 0.0f, 0.0f));
        oStats2.health = 65;
        oStats2.armor  = 30;
        oStats2.level  = 7;
        res = pChar2Node->CreateComponent<UIWidget>(
                "ui/widget_stats.rml", UILayer::HUD,
                makeStatsSetup(pChar2Node, &oStats2));
        if (res != uSUCCESS)
                throw std::runtime_error("failed to create UIWidget for Char2");

        // Beacon: second UIWidget type with a different data model (BeaconState)
        pBeaconNode = pSceneTree->Create("Beacon");
        pBeaconNode->SetPos(glm::vec3(1.5f, 0.0f, 2.0f));
        res = pBeaconNode->CreateComponent<UIWidget>(
                "ui/widget_beacon.rml", UILayer::HUD,
                std::function<void(Rml::DataModelConstructor &)>(
                        [this](Rml::DataModelConstructor & ctor) {
                                ctor.BindFunc("active", [this](Rml::Variant & v) {
                                        v = oBeacon.active;
                                });
                                ctor.BindFunc("label", [this](Rml::Variant & v) {
                                        v = Rml::String(oBeacon.label);
                                });
                                ctor.BindFunc("ping", [this](Rml::Variant & v) {
                                        v = Rml::String(std::to_string(oBeacon.ping));
                                });
                        }));
        if (res != uSUCCESS)
                throw std::runtime_error("failed to create UIWidget for Beacon");

        // Populate credits feature list (static data)
        oCredits.vFeatures = {
                "UIThemeManager — theme switching (SCI-FI / WARM)",
                "UILocalization  — EN / FR locale switching",
                "UIScreenManager::PushModal (POPUP layer)",
                "ScreenTransition::SCALE + AnimEasing::EASE_IN_OUT",
                "UIScreenManager::ShowPersistent (SYSTEM layer toast)",
                "UIScreenManager::HidePersistent (auto-dismiss timer)",
                "UIEventRouter::RegisterValidator (callsign input)",
                "ScreenTransition::SLIDE_LEFT / SLIDE_RIGHT",
                "AnimEasing::EASE_OUT (credits slide)",
                "UIWidget::Enable / Disable (hide when not PLAYING)",
                "Multiple UIWidget types (stats + beacon)",
                "data-style-width (dynamic HUD bars)",
                "data-for (this scrollable list)",
                "border-radius (rounded corners)",
                "<img> texture element",
                "Scrolling container (overflow: auto)",
                "Multi-line text block",
                "Grid of elements (HUD status grid)",
                "<input type=text> with validator",
        };

        GetSystem<EventManager>().AddListener<EUIAction, &Scene::OnUIEvent>(this);
        GetSystem<EventManager>().AddListener<EUpdate,   &Scene::OnUpdate >(this);
        GetSystem<EventManager>().AddListener<EKeyDown,  &Scene::OnKeyDown>(this);
        GetSystem<EventManager>().AddListener<EKeyUp,    &Scene::OnKeyUp  >(this);

        // Register callsign validator: trim whitespace, enforce 1-16 chars
        GetSystem<UISystem>().GetEventRouter().RegisterValidator(
                StrID("input-callsign"),
                [](std::string & val) -> bool {
                        const auto s = val.find_first_not_of(" \t");
                        if (s == std::string::npos) { val.clear(); return false; }
                        val = val.substr(s, val.find_last_not_of(" \t") - s + 1);
                        return val.length() >= 1 && val.length() <= 16;
                });

        // Start with all world-space widgets disabled (MENU state)
        DisableWidgets();

        GetSystem<UISystem>().GetScreenManager().Push("ui/main_menu.rml");

        // Debug panel — persistent overlay on the DEBUG layer
        {
                auto * pCtx = GetSystem<UISystem>().GetContext(UILayer::DEBUG);
                Rml::DataModelConstructor ctor = pCtx->CreateDataModel("debug");
                ctor.BindFunc("grid_label", [this](Rml::Variant & v) {
                        v = Rml::String(draw_grid  ? "Grid:  ON" : "Grid:  OFF");
                });
                ctor.BindFunc("debug_label", [this](Rml::Variant & v) {
                        v = Rml::String(draw_debug ? "Debug: ON" : "Debug: OFF");
                });
                hDebugModel = ctor.GetModelHandle();
        }
        debug_panel_doc = GetSystem<UISystem>().GetDocumentManager(UILayer::DEBUG)
                                .Load("ui/debug_panel.rml");
        auto * pPanelDoc = GetSystem<UISystem>().GetDocumentManager(UILayer::DEBUG)
                                .Get(debug_panel_doc);
        if (pPanelDoc) pPanelDoc->Show();
}

Scene::~Scene() noexcept {

        GetSystem<EventManager>().RemoveListener<EUIAction, &Scene::OnUIEvent>(this);
        GetSystem<EventManager>().RemoveListener<EUpdate,   &Scene::OnUpdate >(this);
        GetSystem<EventManager>().RemoveListener<EKeyDown,  &Scene::OnKeyDown>(this);
        GetSystem<EventManager>().RemoveListener<EKeyUp,    &Scene::OnKeyUp  >(this);

        GetSystem<UISystem>().GetEventRouter().UnregisterValidator(StrID("input-callsign"));

        if (hHUD)
                GetSystem<UISystem>().GetContext()->RemoveDataModel("hud");

        if (hCreditsModel)
                GetSystem<UISystem>().GetContext()->RemoveDataModel("credits");

        if (toast_doc != INVALID_UI_DOCUMENT)
                GetSystem<UISystem>().GetScreenManager(UILayer::SYSTEM)
                        .HidePersistent(toast_doc);

        if (debug_panel_doc != INVALID_UI_DOCUMENT) {
                GetSystem<UISystem>().GetDocumentManager(UILayer::DEBUG).Release(debug_panel_doc);
                debug_panel_doc = INVALID_UI_DOCUMENT;
        }
        if (auto * pCtx = GetSystem<UISystem>().GetContext(UILayer::DEBUG))
                pCtx->RemoveDataModel("debug");
}

void Scene::Process() {

        if (draw_grid)
                GetSystem<DebugRenderer>().DrawGrid(pSceneTree->GetRoot()->GetTransform());

        if (draw_debug) {
                const BoundingBox charBox(glm::vec3(-0.5f, 0.0f, -0.5f),
                                           glm::vec3( 0.5f, 2.0f,  0.5f));
                if (pChar1Node)
                        GetSystem<DebugRenderer>().DrawBBox(charBox, pChar1Node->GetTransform());
                if (pChar2Node)
                        GetSystem<DebugRenderer>().DrawBBox(charBox, pChar2Node->GetTransform());
                const BoundingBox beaconBox(glm::vec3(-0.2f, 0.0f, -0.2f),
                                            glm::vec3( 0.2f, 0.8f,  0.2f));
                if (pBeaconNode)
                        GetSystem<DebugRenderer>().DrawBBox(beaconBox, pBeaconNode->GetTransform());
        }
}

// ---------------------------------------------------------------------------

void Scene::OnUpdate(const Event & oEvent) {

        const float dt = oEvent.Get<EUpdate>().last_frame_time;

        if (eState == GameState::PLAYING) {
                UpdateCameraOrbit(dt);
                oHUD.elapsed += dt;
                UpdateHUDModel();
                UpdateHealthDrain(dt);
        }

        // Toast auto-dismiss timer
        if (toast_timer > 0.0f) {
                toast_timer -= dt;
                if (toast_timer <= 0.0f) {
                        toast_timer = 0.0f;
                        if (toast_doc != INVALID_UI_DOCUMENT) {
                                GetSystem<UISystem>().GetScreenManager(UILayer::SYSTEM)
                                        .HidePersistent(toast_doc);
                                toast_doc = INVALID_UI_DOCUMENT;
                        }
                }
        }
}

void Scene::UpdateCameraOrbit(float dt) {

        if (cam_yaw_speed == 0.0f && cam_pitch_speed == 0.0f) return;

        cam_yaw   += cam_yaw_speed   * dt;
        cam_pitch += cam_pitch_speed * dt;

        if (cam_pitch >  80.0f) cam_pitch =  80.0f;
        if (cam_pitch < -80.0f) cam_pitch = -80.0f;

        const float yr = cam_yaw   * (3.14159265f / 180.0f);
        const float pr = cam_pitch * (3.14159265f / 180.0f);
        const float cx = cam_radius * cosf(pr) * sinf(yr);
        const float cy = cam_radius * sinf(pr);
        const float cz = cam_radius * cosf(pr) * cosf(yr);

        pCamera->SetPos(cx, cy, cz);
        pCamera->LookAt(0.0f, 0.0f, 0.0f);
}

void Scene::UpdateHUDModel() {

        if (!hHUD) return;
        const glm::vec3 wp = pCamera->GetWorldPos();
        oHUD.cam_x = wp.x;
        oHUD.cam_y = wp.y;
        oHUD.cam_z = wp.z;
        oHUD.score = oHUD.kills * 100;
        hHUD.DirtyAllVariables();
}

void Scene::UpdateHealthDrain(float dt) {

        health_drain_accum += dt;
        beacon_ping_accum  += dt;

        if (health_drain_accum >= 1.0f) {
                health_drain_accum -= 1.0f;

                if (hHUD && oHUD.health > 0) {
                        oHUD.health--;
                        oHUD.kills++;
                        hHUD.DirtyAllVariables();
                }
                if (oStats1.health > 0) {
                        oStats1.health--;
                        if (auto * w = pChar1Node->GetComponent<UIWidget>()) w->Dirty();
                }
                if (oStats2.health > 0) {
                        oStats2.health--;
                        if (auto * w = pChar2Node->GetComponent<UIWidget>()) w->Dirty();
                }
        }

        if (beacon_ping_accum >= 1.5f) {
                beacon_ping_accum -= 1.5f;
                oBeacon.ping = 20 + (rand() % 80);
                if (auto * w = pBeaconNode->GetComponent<UIWidget>()) w->Dirty();
        }
}

void Scene::OnKeyDown(const Event & oEvent) {

        const auto key = oEvent.Get<EKeyDown>().key;

        if (key == Keys::ESCAPE) {
                if (eState == GameState::PLAYING) {
                        eState = GameState::PAUSED;
                        DisableWidgets();
                        GetSystem<UISystem>().GetScreenManager().Push(
                                "ui/pause_menu.rml", ScreenTransition::FADE, 0.25f);
                } else if (eState == GameState::MENU) {
                        GetSystem<EventManager>().TriggerEvent(EQuit{});
                }
                return;
        }

        if (eState != GameState::PLAYING) return;
        if (GetSystem<UISystem>().GetInputTranslator().IsCapturingKeyboard()) return;

        constexpr float kSpeed = 60.0f;
        switch (key) {
                case Keys::A: cam_yaw_speed   = -kSpeed; break;
                case Keys::D: cam_yaw_speed   = +kSpeed; break;
                case Keys::W: cam_pitch_speed = +kSpeed; break;
                case Keys::S: cam_pitch_speed = -kSpeed; break;
                default: break;
        }
}

void Scene::OnKeyUp(const Event & oEvent) {

        switch (oEvent.Get<EKeyUp>().key) {
                case Keys::A: case Keys::D: cam_yaw_speed   = 0.0f; break;
                case Keys::W: case Keys::S: cam_pitch_speed = 0.0f; break;
                default: break;
        }
}

// ---------------------------------------------------------------------------

void Scene::EnableWidgets() {

        if (auto * w = pChar1Node->GetComponent<UIWidget>())  w->Enable();
        if (auto * w = pChar2Node->GetComponent<UIWidget>())  w->Enable();
        if (auto * w = pBeaconNode->GetComponent<UIWidget>()) w->Enable();
}

void Scene::DisableWidgets() {

        if (auto * w = pChar1Node->GetComponent<UIWidget>())  w->Disable();
        if (auto * w = pChar2Node->GetComponent<UIWidget>())  w->Disable();
        if (auto * w = pBeaconNode->GetComponent<UIWidget>()) w->Disable();
}

void Scene::SetThemeAll(const std::string & theme) {

        for (auto layer : { UILayer::HUD, UILayer::MENU, UILayer::POPUP, UILayer::SYSTEM })
                GetSystem<UISystem>().GetThemeManager(layer).ActivateTheme(theme);
        current_theme = theme;
}

void Scene::StartMission() {

        hHUD = SE::RegisterDataModel(
                GetSystem<UISystem>().GetContext(),
                "hud", oHUD,
                [](Rml::DataModelConstructor & c, HUDModel & m) {
                        c.Bind("health",   &m.health);
                        c.Bind("shield",   &m.shield);
                        c.Bind("energy",   &m.energy);
                        c.Bind("cam_x",    &m.cam_x);
                        c.Bind("cam_y",    &m.cam_y);
                        c.Bind("cam_z",    &m.cam_z);
                        c.Bind("cam_fov",  &m.cam_fov);
                        c.Bind("cam_proj", &m.cam_proj);
                        c.Bind("score",    &m.score);
                        c.Bind("kills",    &m.kills);
                        // Dynamic bar widths (data-style-width)
                        c.BindFunc("health_bar_w", [&m](Rml::Variant & v) {
                                char buf[8];
                                std::snprintf(buf, sizeof(buf), "%d%%", m.health);
                                v = Rml::String(buf);
                        });
                        c.BindFunc("shield_bar_w", [&m](Rml::Variant & v) {
                                char buf[8];
                                std::snprintf(buf, sizeof(buf), "%d%%", m.shield);
                                v = Rml::String(buf);
                        });
                        c.BindFunc("energy_bar_w", [&m](Rml::Variant & v) {
                                char buf[8];
                                std::snprintf(buf, sizeof(buf), "%d%%", m.energy);
                                v = Rml::String(buf);
                        });
                        // Elapsed time formatted as MM:SS
                        c.BindFunc("elapsed", [&m](Rml::Variant & v) {
                                char buf[16];
                                const int mins = static_cast<int>(m.elapsed) / 60;
                                const int secs = static_cast<int>(m.elapsed) % 60;
                                std::snprintf(buf, sizeof(buf), "%02d:%02d", mins, secs);
                                v = Rml::String(buf);
                        });
                });

        eState = GameState::PLAYING;
        EnableWidgets();
        GetSystem<UISystem>().GetScreenManager().Replace(
                "ui/hud.rml", ScreenTransition::FADE, 0.3f);
}

void Scene::ReturnToMenu() {

        GetSystem<UISystem>().GetScreenManager().PopTo(0);

        if (hHUD) {
                GetSystem<UISystem>().GetContext()->RemoveDataModel("hud");
                hHUD = {};
        }

        DisableWidgets();

        oHUD               = HUDModel{};
        oBeacon            = BeaconState{};
        health_drain_accum = 0.0f;
        beacon_ping_accum  = 0.0f;
        settings_open      = false;
        quit_confirm_open  = false;
        credits_open       = false;
        cam_yaw            = 0.0f;
        cam_pitch          = 0.0f;
        cam_yaw_speed      = 0.0f;
        cam_pitch_speed    = 0.0f;
        eState             = GameState::MENU;

        pCamera->SetPos(0.0f, 0.0f, cam_radius);
        pCamera->LookAt(0.0f, 0.0f, 0.0f);

        GetSystem<UISystem>().GetScreenManager().Push("ui/main_menu.rml");
}

void Scene::OpenSettings() {

        if (settings_open) return;
        settings_open = true;
        GetSystem<UISystem>().GetScreenManager().Push(
                "ui/settings.rml", ScreenTransition::FADE, 0.25f);
}

void Scene::CloseSettings() {

        if (!settings_open) return;
        settings_open = false;
        GetSystem<UISystem>().GetScreenManager().Pop(ScreenTransition::FADE, 0.25f);
        ShowToast();
}

void Scene::OpenQuitConfirm() {

        if (quit_confirm_open) return;
        quit_confirm_open = true;
        GetSystem<UISystem>().GetScreenManager(UILayer::POPUP)
                .PushModal("ui/quit_confirm.rml", ScreenTransition::SCALE, 0.2f,
                           AnimEasing::EASE_IN_OUT);
}

void Scene::CloseQuitConfirm() {

        if (!quit_confirm_open) return;
        quit_confirm_open = false;
        GetSystem<UISystem>().GetScreenManager(UILayer::POPUP)
                .Pop(ScreenTransition::SCALE, 0.2f, AnimEasing::EASE_IN_OUT);
}

void Scene::OpenCredits() {

        if (credits_open) return;
        credits_open = true;

        // Lazy-register credits data model (data-for list with RegisterArray)
        if (!hCreditsModel) {
                hCreditsModel = SE::RegisterDataModel(
                        GetSystem<UISystem>().GetContext(),
                        "credits", oCredits,
                        [](Rml::DataModelConstructor & c, CreditsModel & m) {
                                c.RegisterArray<std::vector<Rml::String>>();
                                c.Bind("features", &m.vFeatures);
                        });
        }

        GetSystem<UISystem>().GetScreenManager()
                .Push("ui/credits.rml", ScreenTransition::SLIDE_LEFT, 0.3f,
                      AnimEasing::EASE_OUT);
}

void Scene::CloseCredits() {

        if (!credits_open) return;
        credits_open = false;
        GetSystem<UISystem>().GetScreenManager()
                .Pop(ScreenTransition::SLIDE_RIGHT, 0.3f, AnimEasing::EASE_OUT);
}

void Scene::ShowToast() {

        if (toast_doc != INVALID_UI_DOCUMENT) return;
        toast_doc   = GetSystem<UISystem>().GetScreenManager(UILayer::SYSTEM)
                          .ShowPersistent("ui/toast.rml");
        toast_timer = 2.0f;
}

// ---------------------------------------------------------------------------

void Scene::OnUIEvent(const Event & oEvent) {

        auto & ev = oEvent.Get<EUIAction>();

        // --- Main Menu ---
        if (ev.event_id == StrID("ui.menu.new_mission")) {
                StartMission();
        }
        else if (ev.event_id == StrID("ui.menu.credits")) {
                OpenCredits();
        }
        else if (ev.event_id == StrID("ui.menu.settings")) {
                OpenSettings();
        }
        else if (ev.event_id == StrID("ui.menu.quit")) {
                OpenQuitConfirm();
        }

        // --- Quit Confirmation (POPUP layer) ---
        else if (ev.event_id == StrID("ui.quit.confirm")) {
                GetSystem<EventManager>().TriggerEvent(EQuit{});
        }
        else if (ev.event_id == StrID("ui.quit.cancel")) {
                CloseQuitConfirm();
        }

        // --- Pause Menu ---
        else if (ev.event_id == StrID("ui.pause.resume")) {
                eState = GameState::PLAYING;
                EnableWidgets();
                GetSystem<UISystem>().GetScreenManager().Pop(ScreenTransition::FADE, 0.25f);
        }
        else if (ev.event_id == StrID("ui.pause.settings")) {
                OpenSettings();
        }
        else if (ev.event_id == StrID("ui.pause.main_menu")) {
                ReturnToMenu();
        }

        // --- Settings ---
        else if (ev.event_id == StrID("ui.settings.fov_45")) {
                current_fov = 45.0f;
                pCamera->SetFOV(current_fov);
                oHUD.cam_fov = current_fov;
                if (hHUD) hHUD.DirtyAllVariables();
        }
        else if (ev.event_id == StrID("ui.settings.fov_60")) {
                current_fov = 60.0f;
                pCamera->SetFOV(current_fov);
                oHUD.cam_fov = current_fov;
                if (hHUD) hHUD.DirtyAllVariables();
        }
        else if (ev.event_id == StrID("ui.settings.fov_75")) {
                current_fov = 75.0f;
                pCamera->SetFOV(current_fov);
                oHUD.cam_fov = current_fov;
                if (hHUD) hHUD.DirtyAllVariables();
        }
        else if (ev.event_id == StrID("ui.settings.fov_90")) {
                current_fov = 90.0f;
                pCamera->SetFOV(current_fov);
                oHUD.cam_fov = current_fov;
                if (hHUD) hHUD.DirtyAllVariables();
        }
        else if (ev.event_id == StrID("ui.settings.proj_toggle")) {
                pCamera->ToggleProjection();
                oHUD.cam_proj = (pCamera->GetProjection() == Camera::Projection::PERSPECTIVE)
                        ? "PERSPECTIVE" : "ORTHO";
                if (hHUD) hHUD.DirtyAllVariables();
        }
        else if (ev.event_id == StrID("ui.settings.theme_scifi")) {
                SetThemeAll("");
        }
        else if (ev.event_id == StrID("ui.settings.theme_warm")) {
                SetThemeAll("warm");
        }
        else if (ev.event_id == StrID("ui.settings.lang_en")) {
                GetSystem<UISystem>().SetLocale("en");
        }
        else if (ev.event_id == StrID("ui.settings.lang_fr")) {
                GetSystem<UISystem>().SetLocale("fr");
        }
        else if (ev.event_id == StrID("ui.settings.callsign")) {
                callsign = ev.string_val;
        }
        else if (ev.event_id == StrID("ui.settings.back")) {
                CloseSettings();
        }

        // --- Credits ---
        else if (ev.event_id == StrID("ui.credits.back")) {
                CloseCredits();
        }

        // --- Debug panel ---
        else if (ev.event_id == StrID("debug.toggle_grid")) {
                draw_grid = !draw_grid;
                hDebugModel.DirtyAllVariables();
        }
        else if (ev.event_id == StrID("debug.toggle_debug")) {
                draw_debug = !draw_debug;
                hDebugModel.DirtyAllVariables();
        }
}

} // namespace SE
