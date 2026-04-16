
#include <algorithm>
#include <cmath>
#include <cstring>

#include <Global.h>
#include <GlobalTypes.h>
#include <Camera.h>
#include <AudioSystem.h>
#include <AudioListener.h>
#include <GraphicsState.h>
#include <SoundContextTypes.h>
#include <SoundContextTypes.tcc>
#include "Scene.h"

namespace SE {

// ---------------------------------------------------------------------------
// Surface color helpers
// ---------------------------------------------------------------------------

static ImU32 SurfaceColor(SurfaceType s) {
        switch (s) {
        case SurfaceType::CONCRETE: return IM_COL32(130, 130, 130, 255);
        case SurfaceType::GRASS:    return IM_COL32( 60, 140,  60, 255);
        case SurfaceType::STONE:    return IM_COL32( 80,  80,  90, 255);
        case SurfaceType::METAL:    return IM_COL32(160, 175, 190, 255);
        case SurfaceType::WOOD:     return IM_COL32(140,  95,  50, 255);
        case SurfaceType::WATER:    return IM_COL32( 50, 100, 210, 255);
        case SurfaceType::DIRT:     return IM_COL32(140, 110,  70, 255);
        case SurfaceType::SAND:     return IM_COL32(210, 190, 130, 255);
        default:                    return IM_COL32(100, 100, 100, 255);
        }
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

Scene::Scene(const Settings& oSettings) :
        hSceneTree(CreateResource<TSceneTree>("SoundDemoScene", true)) {

        pSceneTree = GetResource(hSceneTree);
        se_assert(pSceneTree);

        // Camera
        pCameraNode = pSceneTree->Create("Camera");
        auto res = pCameraNode->CreateComponent<Camera>(oSettings.oCamSettings);
        if (res != uSUCCESS) throw std::runtime_error("failed to create Camera");
        pCamera = pCameraNode->GetComponent<Camera>();
        se_assert(pCamera);
        pCamera->SetPos(0, 5, 20);
        pCamera->LookAt(0, 0, 0);
        GetSystem<TRenderer>().SetCamera(pCamera);

        // Listener — positioned at soldier's world position each frame
        pListenerNode = pSceneTree->Create("Listener");
        pListenerNode->CreateComponent<AudioListener>(1.0f);
        pListenerNode->SetPos(glm::vec3(mSoldier.vPos.x, 0.f, mSoldier.vPos.y));

        // Load sound cue libraries
        GetSystem<SoundEventSystem>().LoadCues("resource/sound_cue/character.secl");
        GetSystem<SoundEventSystem>().LoadCues("resource/sound_cue/vehicle.secl");

        // Build map
        // Default: CONCRETE
        for (int y = 0; y < kMapH; ++y)
                for (int x = 0; x < kMapW; ++x)
                        tile_map[y][x] = SurfaceType::CONCRETE;

        // x 0-6, y 0-30: STONE (plaza)
        for (int y = 0; y < kMapH; ++y)
                for (int x = 0; x < 7; ++x)
                        tile_map[y][x] = SurfaceType::STONE;

        // x 34-40, y 0-30: METAL (grating)
        for (int y = 0; y < kMapH; ++y)
                for (int x = 34; x < kMapW; ++x)
                        tile_map[y][x] = SurfaceType::METAL;

        // x 0-40, y 0-4: WOOD (dock) — overwrites other zones near y=0
        for (int y = 0; y < 4; ++y)
                for (int x = 0; x < kMapW; ++x)
                        tile_map[y][x] = SurfaceType::WOOD;

        // x 15-25, y 10-20: GRASS (park)
        for (int y = 10; y < 20; ++y)
                for (int x = 15; x < 26; ++x)
                        tile_map[y][x] = SurfaceType::GRASS;

        // Scattered 3x3 WATER patches (puddles)
        static const int kPuddles[][2] = {
                {8,6}, {28,8}, {12,22}, {32,18}, {22,5}, {5,14}, {36,25}
        };
        for (auto& p : kPuddles) {
                for (int dy = 0; dy < 3 && p[1]+dy < kMapH; ++dy)
                        for (int dx = 0; dx < 3 && p[0]+dx < kMapW; ++dx)
                                tile_map[p[1]+dy][p[0]+dx] = SurfaceType::WATER;
        }

        // Soldier node + SoundEmitter (used for per-emitter state and position)
        mSoldier.pNode = pSceneTree->Create("Soldier");
        mSoldier.pNode->SetPos(glm::vec3(mSoldier.vPos.x, 0.f, mSoldier.vPos.y));
        {
                SoundEmitterDesc d;
                d.sCueName  = "character.footstep";
                d.auto_play = false;
                mSoldier.pNode->CreateComponent<SoundEmitter>(d);
                mSoldier.pEmitter = mSoldier.pNode->GetComponent<SoundEmitter>();
        }

        // Vehicle node + SoundEmitter
        mVehicle.pNode = pSceneTree->Create("Vehicle");
        mVehicle.pNode->SetPos(glm::vec3(mVehicle.vPos.x, 0.f, mVehicle.vPos.y));
        {
                SoundEmitterDesc d;
                d.sCueName  = "vehicle.engine";
                d.auto_play = false;
                mVehicle.pNode->CreateComponent<SoundEmitter>(d);
                mVehicle.pEmitter = mVehicle.pNode->GetComponent<SoundEmitter>();
        }

        // Cooldown tracker entries (durations from JSON)
        vCooldownTracker = {
                {"character.footstep.*",          0.08f, 0.0f},
                {"character.weapon.rifle.fire",   0.05f, 0.0f},
                {"character.weapon.rifle.empty",  0.30f, 0.0f},
                {"character.jump",                0.20f, 0.0f},
                {"vehicle.gear_shift",            0.10f, 0.0f},
                {"vehicle.collision",             0.50f, 0.0f},
        };
}

// ---------------------------------------------------------------------------

Scene::~Scene() noexcept {
        if (mVehicle.hEngineVoice.IsValid())
                GetSystem<AudioSystem>().Stop(mVehicle.hEngineVoice);
        if (mVehicle.hSquealVoice.IsValid())
                GetSystem<AudioSystem>().Stop(mVehicle.hSquealVoice);
}

// ---------------------------------------------------------------------------

void Scene::Process() {

        const float dt = GetSystem<AppClock>().RawDelta();;
        time += dt;

        // Tab: switch active entity (press detection)
        bool tab_down = GetSystem<InputManager>().GetScancodeDown(Scancodes::TAB);
        if (tab_down && !tab_was_down)
                active_entity = 1 - active_entity;
        tab_was_down = tab_down;

        // Decay cooldown tracker visuals
        for (auto& cd : vCooldownTracker)
                cd.remaining = std::max(0.0f, cd.remaining - dt);

        // Music duck auto-restore
        if (music_duck_timer > 0.0f) {
                music_duck_timer -= dt;
                if (music_duck_timer <= 0.0f) {
                        GetSystem<AudioSystem>().SetBusGain(MixBusId::Music, 1.0f, 0.3f);
                        bus_gains[static_cast<int>(MixBusId::Music)] = 1.0f;
                        music_duck_timer = 0.0f;
                }
        }

        // Update active entity
        if (active_entity == 0)
                UpdateSoldier(dt);
        else
                UpdateVehicle(dt);

        // Tick SoundEventSystem cooldowns / voice tracking
        GetSystem<SoundEventSystem>().Update(dt);

        // Keep listener at soldier position for spatial feel
        pListenerNode->SetPos(glm::vec3(mSoldier.vPos.x, 0.f, mSoldier.vPos.y));

        ShowGUI();
}

// ---------------------------------------------------------------------------
// Map helpers
// ---------------------------------------------------------------------------

SurfaceType Scene::GetSurface(glm::vec2 vP) const {
        int x = static_cast<int>(vP.x);
        int y = static_cast<int>(vP.y);
        x = std::max(0, std::min(x, kMapW - 1));
        y = std::max(0, std::min(y, kMapH - 1));
        return tile_map[y][x];
}

// ---------------------------------------------------------------------------
// Event log
// ---------------------------------------------------------------------------

void Scene::LogEvent(const std::string& sCueId,
                     const std::vector<std::pair<std::string,float>>& vParams,
                     const SoundEventResult& oResult) {
        EventLogEntry oEntry;
        oEntry.time    = time;
        oEntry.sCueId  = sCueId;
        oEntry.vParams = vParams;
        oEntry.oResult = oResult;
        vEventLog.push_front(std::move(oEntry));
        while (static_cast<int>(vEventLog.size()) > kMaxLog)
                vEventLog.pop_back();
}

void Scene::UpdateCooldownTracker(const std::string& sCueId, float duration) {
        for (auto& cd : vCooldownTracker) {
                if (cd.sName.find(sCueId) != std::string::npos ||
                    sCueId.find(cd.sName.substr(0, cd.sName.find('*'))) != std::string::npos) {
                        cd.remaining = duration;
                        return;
                }
        }
}

// ---------------------------------------------------------------------------
// Soldier update
// ---------------------------------------------------------------------------

void Scene::FireFootstep() {
        const std::string sCueId =
                std::string("character.footstep.") +
                kSurfaceNames[static_cast<int>(mSoldier.surface)];

        CharacterSoundContext oCtx;
        oCtx.pEmitter  = mSoldier.pEmitter;
        oCtx.vPosition = mSoldier.pEmitter->GetPosition();
        oCtx.vVelocity = mSoldier.pEmitter->GetVelocity();
        oCtx.speed     = mSoldier.speed;
        oCtx.weight    = mSoldier.weight;

        SoundEventResult oResult;
        GetSystem<SoundEventSystem>().PostEx(sCueId, oCtx, oResult);
        LogEvent(sCueId, {{"speed", mSoldier.speed}, {"weight", mSoldier.weight}}, oResult);
        if (oResult.fired)
                UpdateCooldownTracker("character.footstep", 0.08f);
}

void Scene::UpdateSoldier(float dt) {
        auto& im = GetSystem<InputManager>();

        // Movement (WASD)
        glm::vec2 dir = {0.f, 0.f};
        if (im.GetKeyDown(Keys::W)) dir.y -= 1.f;
        if (im.GetKeyDown(Keys::S)) dir.y += 1.f;
        if (im.GetKeyDown(Keys::A)) dir.x -= 1.f;
        if (im.GetKeyDown(Keys::D)) dir.x += 1.f;

        const float dlen = glm::length(dir);
        if (dlen > 0.0f) dir /= dlen;

        static constexpr float kMaxSpeed = 5.0f;
        static constexpr float kAccel    = 10.0f;

        if (dlen > 0.0f)
                mSoldier.speed = std::min(mSoldier.speed + kAccel * dt, kMaxSpeed);
        else
                mSoldier.speed = std::max(mSoldier.speed - kAccel * 2.f * dt, 0.0f);

        mSoldier.vPos  += dir * mSoldier.speed * dt;
        mSoldier.vPos.x = std::max(0.0f, std::min(mSoldier.vPos.x, (float)(kMapW - 1)));
        mSoldier.vPos.y = std::max(0.0f, std::min(mSoldier.vPos.y, (float)(kMapH - 1)));
        mSoldier.surface = GetSurface(mSoldier.vPos);

        // Update node so SoundEmitter velocity tracking works
        mSoldier.pNode->SetPos(glm::vec3(mSoldier.vPos.x, 0.f, mSoldier.vPos.y));

        // Footstep timer
        if (mSoldier.speed > 0.3f && !mSoldier.is_jumping) {
                mSoldier.step_timer += dt;
                const float interval = 0.5f / mSoldier.speed;
                if (mSoldier.step_timer >= interval) {
                        mSoldier.step_timer = 0.f;
                        FireFootstep();
                }
        } else {
                mSoldier.step_timer = 0.f;
        }

        // Space: fire weapon + start jump when grounded
        const bool space_down    = im.GetScancodeDown(Scancodes::SPACE);
        const bool space_pressed = space_down && !mSoldier.space_was_down;
        mSoldier.space_was_down  = space_down;

        if (space_pressed) {
                // Weapon fire
                CharacterSoundContext oCtx;
                oCtx.pEmitter  = mSoldier.pEmitter;
                oCtx.vPosition = mSoldier.pEmitter->GetPosition();
                oCtx.vVelocity = mSoldier.pEmitter->GetVelocity();
                oCtx.speed     = mSoldier.speed;
                oCtx.weight    = mSoldier.weight;

                const std::string weapon_cue = (mSoldier.ammo > 0)
                        ? "character.weapon.rifle.fire"
                        : "character.weapon.rifle.empty";

                SoundEventResult oWResult;
                GetSystem<SoundEventSystem>().PostEx(weapon_cue, oCtx, oWResult);
                LogEvent(weapon_cue, {{"speed", mSoldier.speed}}, oWResult);

                if (mSoldier.ammo > 0) {
                        --mSoldier.ammo;
                        if (oWResult.fired)
                                UpdateCooldownTracker("character.weapon.rifle.fire", 0.05f);
                } else {
                        if (oWResult.fired)
                                UpdateCooldownTracker("character.weapon.rifle.empty", 0.30f);
                }

                // Jump (when grounded)
                if (!mSoldier.is_jumping) {
                        mSoldier.is_jumping      = true;
                        mSoldier.jump_vel_y      = 7.0f;
                        mSoldier.jump_height     = 0.0f;
                        mSoldier.jump_height_cur = 0.0f;

                        SoundEventResult oJResult;
                        GetSystem<SoundEventSystem>().PostEx("character.jump", oCtx, oJResult);
                        LogEvent("character.jump", {}, oJResult);
                        if (oJResult.fired)
                                UpdateCooldownTracker("character.jump", 0.20f);
                }
        }

        // R: reload
        const bool r_down    = im.GetKeyDown(Keys::R);
        const bool r_pressed = r_down && !mSoldier.r_was_down;
        mSoldier.r_was_down  = r_down;

        if (r_pressed && mSoldier.ammo < 30) {
                mSoldier.ammo = 30;
                CharacterSoundContext oCtx;
                oCtx.pEmitter  = mSoldier.pEmitter;
                oCtx.vPosition = mSoldier.pEmitter->GetPosition();
                SoundEventResult oResult;
                GetSystem<SoundEventSystem>().PostEx("character.weapon.rifle.reload", oCtx, oResult);
                LogEvent("character.weapon.rifle.reload", {}, oResult);
        }

        // Jump physics
        if (mSoldier.is_jumping) {
                static constexpr float kGravity = 15.0f;
                mSoldier.jump_vel_y      -= kGravity * dt;
                mSoldier.jump_height_cur += mSoldier.jump_vel_y * dt;

                if (mSoldier.jump_height_cur > mSoldier.jump_height)
                        mSoldier.jump_height = mSoldier.jump_height_cur;

                if (mSoldier.jump_height_cur <= 0.0f && mSoldier.jump_vel_y < 0.0f) {
                        // Landed
                        static constexpr float kMaxJumpH = 49.0f / 30.0f; // v0^2/(2g)
                        const float impact = std::min(1.0f, mSoldier.jump_height / kMaxJumpH);

                        CharacterSoundContext oCtx;
                        oCtx.pEmitter  = mSoldier.pEmitter;
                        oCtx.vPosition = mSoldier.pEmitter->GetPosition();
                        oCtx.impact    = impact;

                        SoundEventResult oResult;
                        GetSystem<SoundEventSystem>().PostEx("character.land", oCtx, oResult);
                        LogEvent("character.land", {{"impact", impact}}, oResult);

                        mSoldier.is_jumping      = false;
                        mSoldier.jump_height_cur = 0.0f;
                        mSoldier.jump_height     = 0.0f;
                        mSoldier.jump_vel_y      = 0.0f;
                }
        }
}

// ---------------------------------------------------------------------------
// Vehicle update
// ---------------------------------------------------------------------------

void Scene::UpdateVehicle(float dt) {
        auto& im = GetSystem<InputManager>();

        static constexpr float kMaxSpeed     = 20.0f;
        static constexpr float kAcceleration = 8.0f;
        static constexpr float kTurnSpeed    = 90.0f; // deg/s

        // Throttle / reverse
        float throttle = 0.0f;
        if (im.GetKeyDown(Keys::W)) throttle =  1.0f;
        if (im.GetKeyDown(Keys::S)) throttle = -0.3f;
        mVehicle.throttle = throttle;

        // Turning (only when moving)
        float turn = 0.0f;
        if (im.GetKeyDown(Keys::A)) turn = -1.0f;
        if (im.GetKeyDown(Keys::D)) turn =  1.0f;

        const float speed = glm::length(mVehicle.vVelocity);
        if (speed > 0.5f)
                mVehicle.heading += turn * kTurnSpeed * dt;

        // Heading direction vector
        const float heading_rad = glm::radians(mVehicle.heading);
        const glm::vec2 forward = { std::sin(heading_rad), -std::cos(heading_rad) };
        const glm::vec2 right   = { std::cos(heading_rad),  std::sin(heading_rad) };

        // Accelerate
        mVehicle.vVelocity += forward * throttle * kAcceleration * dt;

        // Clamp speed
        float new_speed = glm::length(mVehicle.vVelocity);
        if (new_speed > kMaxSpeed) {
                mVehicle.vVelocity *= kMaxSpeed / new_speed;
                new_speed = kMaxSpeed;
        }

        // Friction / deceleration when no throttle
        if (throttle == 0.0f && new_speed > 0.0f) {
                const float friction = 4.0f * dt;
                if (new_speed > friction)
                        mVehicle.vVelocity *= (new_speed - friction) / new_speed;
                else
                        mVehicle.vVelocity = {0.f, 0.f};
                new_speed = glm::length(mVehicle.vVelocity);
        }

        mVehicle.rpm_normalized = std::min(new_speed / kMaxSpeed, 1.0f);
        mVehicle.engine_load    = (throttle > 0.0f) ? throttle : 0.1f;

        // Lateral slip: |dot(velocity_dir, right)|
        if (new_speed > 0.01f)
                mVehicle.lateral_slip = std::abs(glm::dot(mVehicle.vVelocity / new_speed, right));
        else
                mVehicle.lateral_slip = 0.0f;

        // Move and clamp to map
        const float prev_speed = new_speed;
        mVehicle.vPos += mVehicle.vVelocity * dt;

        bool on_boundary = false;
        if (mVehicle.vPos.x < 0.0f || mVehicle.vPos.x >= (float)kMapW ||
            mVehicle.vPos.y < 0.0f || mVehicle.vPos.y >= (float)kMapH) {

                mVehicle.vPos.x    = std::max(0.0f, std::min(mVehicle.vPos.x, (float)(kMapW - 1)));
                mVehicle.vPos.y    = std::max(0.0f, std::min(mVehicle.vPos.y, (float)(kMapH - 1)));
                mVehicle.vVelocity = {0.f, 0.f};
                on_boundary = true;

                if (!mVehicle.prev_on_boundary && prev_speed > 1.0f) {
                        const float impact = std::min(1.0f, prev_speed / kMaxSpeed);

                        VehicleSoundContext oCtx;
                        oCtx.pEmitter       = mVehicle.pEmitter;
                        oCtx.vPosition      = mVehicle.pEmitter->GetPosition();
                        oCtx.impact         = impact;
                        oCtx.rpm_normalized = mVehicle.rpm_normalized;

                        SoundEventResult oResult;
                        GetSystem<SoundEventSystem>().PostEx("vehicle.collision", oCtx, oResult);
                        LogEvent("vehicle.collision", {{"impact", impact}}, oResult);
                        if (oResult.fired)
                                UpdateCooldownTracker("vehicle.collision", 0.50f);
                }
        }
        mVehicle.prev_on_boundary = on_boundary;

        mVehicle.pNode->SetPos(glm::vec3(mVehicle.vPos.x, 0.f, mVehicle.vPos.y));

        // Auto gear shift (4 gears: thresholds at 0.25, 0.5, 0.75)
        static constexpr float kGearThresh[] = {0.25f, 0.50f, 0.75f};
        int new_gear = 1;
        for (int i = 0; i < 3; ++i)
                if (mVehicle.rpm_normalized > kGearThresh[i]) new_gear = i + 2;

        if (new_gear != mVehicle.gear) {
                mVehicle.gear = new_gear;

                VehicleSoundContext oCtx;
                oCtx.pEmitter  = mVehicle.pEmitter;
                oCtx.vPosition = mVehicle.pEmitter->GetPosition();

                SoundEventResult oResult;
                GetSystem<SoundEventSystem>().PostEx("vehicle.gear_shift", oCtx, oResult);
                LogEvent("vehicle.gear_shift", {{"gear", (float)mVehicle.gear}}, oResult);
                if (oResult.fired)
                        UpdateCooldownTracker("vehicle.gear_shift", 0.10f);
        }

        // Engine tier switching
        const int tier = std::min((int)(mVehicle.rpm_normalized * 5.0f), 4);
        if (tier != mVehicle.engine_tier) {
                if (mVehicle.hEngineVoice.IsValid())
                        GetSystem<AudioSystem>().Stop(mVehicle.hEngineVoice);

                VehicleSoundContext oCtx;
                oCtx.pEmitter       = mVehicle.pEmitter;
                oCtx.vPosition      = mVehicle.pEmitter->GetPosition();
                oCtx.rpm_normalized = mVehicle.rpm_normalized;
                oCtx.engine_load    = mVehicle.engine_load;

                SoundEventResult oResult;
                mVehicle.hEngineVoice = GetSystem<SoundEventSystem>().PostEx(
                        "vehicle.engine", oCtx, oResult);
                mVehicle.engine_tier = tier;
                LogEvent("vehicle.engine",
                         {{"rpm_normalized", mVehicle.rpm_normalized},
                          {"engine_load",    mVehicle.engine_load}},
                         oResult);
        }

        // Tire squeal
        if (mVehicle.lateral_slip > 0.3f && !mVehicle.squeal_active) {
                VehicleSoundContext oCtx;
                oCtx.pEmitter     = mVehicle.pEmitter;
                oCtx.vPosition    = mVehicle.pEmitter->GetPosition();
                oCtx.lateral_slip = mVehicle.lateral_slip;

                SoundEventResult oResult;
                mVehicle.hSquealVoice = GetSystem<SoundEventSystem>().PostEx(
                        "vehicle.tire.squeal", oCtx, oResult);
                mVehicle.squeal_active = true;
                LogEvent("vehicle.tire.squeal",
                         {{"lateral_slip", mVehicle.lateral_slip}}, oResult);

        } else if (mVehicle.lateral_slip <= 0.3f && mVehicle.squeal_active) {
                if (mVehicle.hSquealVoice.IsValid())
                        GetSystem<AudioSystem>().Stop(mVehicle.hSquealVoice);
                mVehicle.hSquealVoice  = VoiceHandle{};
                mVehicle.squeal_active = false;
        }
}

// ---------------------------------------------------------------------------
// GUI
// ---------------------------------------------------------------------------

void Scene::ShowGUI() {

        // ---------------------------------------------------------------
        // Window 1: Map
        // ---------------------------------------------------------------
        ImGui::Begin("Map");

        ImGui::Text("Active: %s  [Tab to switch]",
                    active_entity == 0 ? "Soldier" : "Vehicle");
        ImGui::SameLine();
        if (ImGui::Button("Switch")) active_entity = 1 - active_entity;

        static constexpr float kCell = 11.0f;
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImDrawList*  dl     = ImGui::GetWindowDrawList();

        // Tiles
        for (int y = 0; y < kMapH; ++y)
                for (int x = 0; x < kMapW; ++x) {
                        ImVec2 p0 = {origin.x + x * kCell, origin.y + y * kCell};
                        ImVec2 p1 = {p0.x + kCell, p0.y + kCell};
                        dl->AddRectFilled(p0, p1, SurfaceColor(tile_map[y][x]));
                }

        // Soldier (white circle)
        {
                const ImVec2 c = {origin.x + mSoldier.vPos.x * kCell + kCell * 0.5f,
                                  origin.y + mSoldier.vPos.y * kCell + kCell * 0.5f};
                dl->AddCircleFilled(c, 5.0f, IM_COL32(255, 255, 255, 220));
                dl->AddCircle      (c, 5.0f, IM_COL32(  0,   0,   0, 180));
        }

        // Vehicle (orange filled rect with heading arrow)
        {
                const ImVec2 c = {origin.x + mVehicle.vPos.x * kCell + kCell * 0.5f,
                                  origin.y + mVehicle.vPos.y * kCell + kCell * 0.5f};
                dl->AddRectFilled(
                        {c.x - 5.0f, c.y - 4.0f},
                        {c.x + 5.0f, c.y + 4.0f},
                        IM_COL32(255, 165, 0, 220));

                const float hr    = glm::radians(mVehicle.heading);
                const ImVec2 tip  = {c.x + std::sin(hr) * 8.0f, c.y - std::cos(hr) * 8.0f};
                dl->AddLine(c, tip, IM_COL32(255, 255, 255, 200), 1.5f);
        }

        // Reserve space for the map
        ImGui::Dummy(ImVec2(kMapW * kCell, kMapH * kCell));

        // Legend
        static const char* kSurfLabels[] = {
                "default","stone","wood","metal","dirt","water","sand","grass","concrete"
        };
        for (int i = 1; i < (int)SurfaceType::COUNT; ++i) {
                ImGui::SameLine(0, 4);
                ImVec2 p = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddRectFilled(
                        p, {p.x+10.f, p.y+10.f},
                        SurfaceColor(static_cast<SurfaceType>(i)));
                ImGui::Dummy({10.f, 10.f});
                ImGui::SameLine(0, 2);
                ImGui::TextUnformatted(kSurfLabels[i]);
        }

        ImGui::End();

        // ---------------------------------------------------------------
        // Window 2: Entity Inspector
        // ---------------------------------------------------------------
        ImGui::Begin("Entity Inspector");

        if (active_entity == 0) {
                ImGui::TextColored(ImVec4(0.6f,1.0f,0.6f,1.0f), "[ SOLDIER ]");
                ImGui::Separator();

                ImGui::Text("Pos: (%.1f, %.1f)", mSoldier.vPos.x, mSoldier.vPos.y);
                ImGui::Text("Surface: %s",
                            kSurfaceNames[static_cast<int>(mSoldier.surface)]);
                ImGui::Text("Ammo: %d / 30", mSoldier.ammo);

                // Speed bar
                ImGui::Text("Speed: %.2f m/s", mSoldier.speed);
                ImGui::ProgressBar(mSoldier.speed / 5.0f, ImVec2(-1,0), "");

                // Jump state
                if (mSoldier.is_jumping) {
                        ImGui::Text("JUMPING  h=%.2f / peak=%.2f",
                                    mSoldier.jump_height_cur, mSoldier.jump_height);
                } else {
                        ImGui::Text("Grounded");
                }

                // Step timer
                float step_interval = (mSoldier.speed > 0.3f) ? 0.5f / mSoldier.speed : 0.5f;
                ImGui::Text("Step timer: %.3f / %.3f", mSoldier.step_timer, step_interval);
                ImGui::ProgressBar(
                        (step_interval > 0.0f) ? mSoldier.step_timer / step_interval : 0.0f,
                        ImVec2(-1, 0), "");

                ImGui::Separator();
                ImGui::SliderFloat("Weight (kg)", &mSoldier.weight, 40.0f, 150.0f);

                ImGui::Separator();
                ImGui::Text("WASD: move   Space: shoot+jump   R: reload");

        } else {
                ImGui::TextColored(ImVec4(1.0f,0.7f,0.3f,1.0f), "[ VEHICLE ]");
                ImGui::Separator();

                ImGui::Text("Pos: (%.1f, %.1f)", mVehicle.vPos.x, mVehicle.vPos.y);
                ImGui::Text("Gear: %d   Heading: %.0f°", mVehicle.gear, mVehicle.heading);

                // RPM gauge (green→yellow→red)
                ImGui::Text("RPM: %.2f", mVehicle.rpm_normalized);
                ImVec4 rpm_col = {0.2f + mVehicle.rpm_normalized * 0.8f,
                                  1.0f - mVehicle.rpm_normalized,
                                  0.1f, 1.0f};
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, rpm_col);
                ImGui::ProgressBar(mVehicle.rpm_normalized, ImVec2(-1, 0), "");
                ImGui::PopStyleColor();

                ImGui::Text("Engine load: %.2f", mVehicle.engine_load);
                ImGui::ProgressBar(mVehicle.engine_load, ImVec2(-1, 0), "");

                ImGui::Text("Lateral slip: %.2f%s",
                            mVehicle.lateral_slip,
                            mVehicle.squeal_active ? "  [SQUEAL]" : "");
                ImGui::ProgressBar(mVehicle.lateral_slip, ImVec2(-1, 0), "");

                ImGui::Text("Throttle: %.2f", mVehicle.throttle);
                ImGui::ProgressBar(std::abs(mVehicle.throttle), ImVec2(-1, 0), "");

                ImGui::Separator();
                ImGui::Text("WASD: drive   A/D: turn   (Tab: switch)");
        }

        ImGui::End();

        // ---------------------------------------------------------------
        // Window 3: Cue Evaluator
        // ---------------------------------------------------------------
        ImGui::Begin("Cue Evaluator");
        ImGui::Text("%d events (max %d)", (int)vEventLog.size(), kMaxLog);
        ImGui::Separator();
        ImGui::BeginChild("##log", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

        for (int i = 0; i < (int)vEventLog.size(); ++i) {
                const auto& e = vEventLog[i];
                ImGui::PushID(i);

                // Age-based alpha
                const float age   = time - e.time;
                const float alpha = std::max(0.3f, 1.0f - age / 10.0f);

                // Cue ID color
                ImVec4 cue_col;
                if      (e.oResult.fired && !e.oResult.fallback) cue_col = {0.4f, 1.0f, 0.4f, alpha};
                else if (e.oResult.fired &&  e.oResult.fallback) cue_col = {1.0f, 1.0f, 0.3f, alpha};
                else                                             cue_col = {1.0f, 0.3f, 0.3f, alpha};

                ImGui::TextColored(cue_col, "[%5.2f] %s", e.time, e.sCueId.c_str());

                if (e.oResult.cooldown_block) {
                        ImGui::SameLine();
                        ImGui::TextColored({1.0f,0.6f,0.1f,alpha}, " [COOLDOWN]");
                }
                if (e.oResult.polyphony_block) {
                        ImGui::SameLine();
                        ImGui::TextColored({0.8f,0.3f,1.0f,alpha}, " [POLYPHONY]");
                }
                if (e.oResult.fallback) {
                        ImGui::SameLine();
                        ImGui::TextColored({1.0f,1.0f,0.3f,alpha}, " [FALLBACK]");
                }

                if (e.oResult.fired) {
                        // Context params
                        for (const auto& [name, val] : e.vParams)
                                ImGui::Text("  %s: %.3f", name.c_str(), val);

                        ImGui::Text("  var[%d]  vol: %.3f  pitch: %.3f",
                                    e.oResult.variation_idx,
                                    e.oResult.final_volume,
                                    e.oResult.final_pitch);
                }

                ImGui::PopID();
        }

        ImGui::EndChild();
        ImGui::End();

        // ---------------------------------------------------------------
        // Window 4: Cooldown Tracker
        // ---------------------------------------------------------------
        ImGui::Begin("Cooldown Tracker");
        ImGui::Text("Approximate cooldown state");
        ImGui::Separator();

        for (const auto& cd : vCooldownTracker) {
                float frac = (cd.duration > 0.0f) ? cd.remaining / cd.duration : 0.0f;
                frac = std::max(0.0f, std::min(1.0f, frac));

                if (frac > 0.0f) {
                        ImGui::PushStyleColor(ImGuiCol_PlotHistogram,
                                              ImVec4(1.0f, 0.4f, 0.2f, 1.0f));
                } else {
                        ImGui::PushStyleColor(ImGuiCol_PlotHistogram,
                                              ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
                }
                char buf[64];
                snprintf(buf, sizeof(buf), "%.3f / %.3f", cd.remaining, cd.duration);
                ImGui::ProgressBar(frac, ImVec2(-1, 0), buf);
                ImGui::PopStyleColor();
                ImGui::SameLine(0, 4);
                ImGui::TextUnformatted(cd.sName.c_str());
        }

        ImGui::End();

        // ---------------------------------------------------------------
        // Window 5: Bus Mixer
        // ---------------------------------------------------------------
        ImGui::Begin("Bus Mixer");

        static const char* kBusNames[] = {"Master", "Music", "SFX", "Voice", "UI"};
        ImGui::SliderFloat("Fade Time (s)", &fade_time, 0.f, 5.f);

        for (int i = 0; i < 5; ++i) {
                if (ImGui::SliderFloat(kBusNames[i], &bus_gains[i], 0.f, 2.f))
                        GetSystem<AudioSystem>().SetBusGain(
                                static_cast<MixBusId>(i), bus_gains[i], fade_time);
        }

        ImGui::Separator();
        if (music_duck_timer <= 0.0f) {
                if (ImGui::Button("Duck Music (2s)")) {
                        GetSystem<AudioSystem>().SetBusGain(MixBusId::Music, 0.2f, 0.3f);
                        bus_gains[static_cast<int>(MixBusId::Music)] = 0.2f;
                        music_duck_timer = 2.0f;
                }
        } else {
                ImGui::Text("Ducking music... %.1fs remaining", music_duck_timer);
        }

        ImGui::End();
}

} // namespace SE
