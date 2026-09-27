
#ifndef APP_WORLD_PORTAL_TCC
#define APP_WORLD_PORTAL_TCC

#include <world/Portal.h>
#include <world/WorldSystem.h>
#include <EventManager.h>
#include <Logging.h>
#include <GlobalTypes.h>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace SE {

Portal::Portal(TSceneTree::TSceneNodeExact * pNewNode, const PortalDesc & oDesc) :
        pNode(pNewNode),
        level_path(oDesc.level_path),
        activate_radius(oDesc.activate_radius),
        release_radius(std::max(oDesc.release_radius, oDesc.activate_radius)) {
}

Portal::Portal(TSceneTree::TSceneNodeExact * pNewNode, const FlatBuffers::Portal * pFB) :
        pNode(pNewNode),
        level_path(pFB && pFB->level_path() ? pFB->level_path()->c_str() : ""),
        activate_radius(pFB ? pFB->activate_radius() : 22.0f),
        release_radius(pFB ? std::max(pFB->release_radius(), pFB->activate_radius()) : 34.0f) {

        if (level_path.empty()) {
                throw(std::runtime_error("Portal: empty level_path"));
        }
}

Portal::~Portal() noexcept {
        Disable();
}

void Portal::Enable() {

        if (subscribed) { return; }
        subscribed = true;
        GetSystem<EventManager>().AddListener<EUpdate, &Portal::OnUpdate>(this);
}

void Portal::Disable() {

        if (!subscribed) { return; }
        subscribed = false;
        GetSystem<EventManager>().RemoveListener<EUpdate, &Portal::OnUpdate>(this);

        // If we staged the paired level and are going away before any focus
        // crossed the release band (our own level is unloading, or the scene is
        // shutting down), hand the level back — otherwise it would stay staged
        // with no owner left to release it. DeactivateLevel is deferred and
        // self-guarding; the WorldSystem instance outlives scene teardown.
        if (requested && level_index != UINT32_MAX) {
                requested = false;
                GetSystem<WorldSystem>().DeactivateLevel(level_index);
        }
}

void Portal::OnUpdate(const Event &) {

        if (!pNode || level_path.empty()) { return; }

        auto & oWorlds = GetSystem<WorldSystem>();
        const auto * pAsset = oWorlds.GetWorldAsset();
        if (!pAsset) { return; }   // world not (yet) loaded

        if (!resolved) {
                resolved = true;
                const auto & vLevels = pAsset->GetSubLevels();
                for (uint32_t i = 0; i < vLevels.size(); ++i) {
                        // exact path, or a distinctive suffix ("wd_crypt") —
                        // authoring the full GEN_DIR path should not be required
                        if (vLevels[i].path == level_path
                            || vLevels[i].path.find(level_path) != std::string::npos) {
                                level_index = i;
                                break;
                        }
                }
                if (level_index == UINT32_MAX) {
                        log_e("Portal: level '{}' not found in the loaded world", level_path);
                        return;
                }
                if (!vLevels[level_index].IsManual()) {
                        // the streaming director owns this one — a portal must
                        // not fight its load/unload decisions
                        log_w("Portal: level '{}' is director-managed — portal idles", level_path);
                        level_index = UINT32_MAX;
                        return;
                }
        }
        if (level_index == UINT32_MAX) { return; }

        // distance from the nearest streaming focus
        const auto & vFocuses = oWorlds.GetFocuses();
        if (vFocuses.empty()) { return; }
        const glm::vec3 vHere = pNode->GetTransform().GetWorldPos();
        float best2 = std::numeric_limits<float>::max();
        for (const auto & f : vFocuses) {
                const glm::vec3 d = f.pos - vHere;
                best2 = std::min(best2, glm::dot(d, d));
        }

        const auto  eState  = oWorlds.GetLevelState(level_index);
        const bool  bLoaded = (eState == WorldSystem::ELevelState::Loading
                            || eState == WorldSystem::ELevelState::Active
                            || eState == WorldSystem::ELevelState::Unloading);
        const float act2 = activate_radius * activate_radius;
        const float rel2 = release_radius * release_radius;

        if (!bLoaded && !requested && best2 <= act2) {
                if (oWorlds.RequestActivation(level_index)) {
                        requested = true;
                        log_i("Portal: '{}' staged (focus inside the band)", level_path);
                }
        }
        else if (requested && best2 > rel2) {
                // last focus left the hysteresis band — release regardless of
                // whether activation finished (it stays usable until teardown)
                oWorlds.DeactivateLevel(level_index);
                requested = false;
                log_i("Portal: '{}' released (focus left the band)", level_path);
        }
        else if (!bLoaded) {
                requested = false;   // staging failed or teardown finished
        }
}

std::string Portal::Str() const {
        return fmt::format("Portal('{}' act {:.1f} rel {:.1f})",
                        level_path, double(activate_radius), double(release_radius));
}

} // namespace SE

#endif // APP_WORLD_PORTAL_TCC
