
#ifdef SE_IMPL

#include <spawner/SpawnerSystem.h>

#include <spawner/Population.h>
#include <spawner/SpawnPoint.h>
#include <spawner/SpawnVolume.h>
#include <spawner/SpawnPlacement.h>

#include <world/WorldSystem.h>
#include <world/WorldAsset.h>
#include <world/SpatialIndexSystem.h>
#include <ai/BlackboardCore.h>
#include <TriggerVolume.h>

#include <EntityManager.h>
#include <CommonEvents.h>
#include <Logging.h>
#include <GlobalTypes.h>

#include <algorithm>

namespace SE {

// ---------------------------------------------------------------------------

SpawnerSystem::SpawnerSystem()
        : pWorld(&GetSystem<WorldSystem>()) {

        GetSystem<EventManager>().AddListener<EUpdate,          &SpawnerSystem::OnUpdate>(this);
        GetSystem<EventManager>().AddListener<EEntityDestroyed, &SpawnerSystem::OnEntityDestroyed>(this);
        GetSystem<EventManager>().AddListener<ETriggerFired,    &SpawnerSystem::OnTriggerFired>(this);
        GetSystem<EventManager>().AddListener<EWorldLoaded,     &SpawnerSystem::OnWorldLoaded>(this);
        GetSystem<EventManager>().AddListener<EWorldUnloaded,   &SpawnerSystem::OnWorldUnloaded>(this);
}

SpawnerSystem::~SpawnerSystem() noexcept {

        GetSystem<EventManager>().RemoveListener<EUpdate,          &SpawnerSystem::OnUpdate>(this);
        GetSystem<EventManager>().RemoveListener<EEntityDestroyed, &SpawnerSystem::OnEntityDestroyed>(this);
        GetSystem<EventManager>().RemoveListener<ETriggerFired,    &SpawnerSystem::OnTriggerFired>(this);
        GetSystem<EventManager>().RemoveListener<EWorldLoaded,     &SpawnerSystem::OnWorldLoaded>(this);
        GetSystem<EventManager>().RemoveListener<EWorldUnloaded,   &SpawnerSystem::OnWorldUnloaded>(this);
}

// ---------------------------------------------------------------------------
// registries
// ---------------------------------------------------------------------------

void SpawnerSystem::AddComponent(Population * p) {

        if (std::find(vPopulations.begin(), vPopulations.end(), p) != vPopulations.end()) {
                return;
        }
        vPopulations.push_back(p);
        ++oStats.populations;
        TryClaimWorldSlot(p);
}

void SpawnerSystem::RemoveComponent(Population * p) {

        auto it = std::find(vPopulations.begin(), vPopulations.end(), p);
        if (it != vPopulations.end()) {
                vPopulations.erase(it);
                --oStats.populations;
        }
        // owned spawns die deferred — drop the index entries now so the flush
        // never dispatches through a dangling component pointer
        for (auto idx = mSpawnIndex.begin(); idx != mSpawnIndex.end();) {
                if (idx->second.pPop == p) { idx = mSpawnIndex.erase(idx); }
                else { ++idx; }
        }
}

void SpawnerSystem::AddComponent(SpawnPoint * p) {

        if (std::find(vSpawnPoints.begin(), vSpawnPoints.end(), p) != vSpawnPoints.end()) {
                return;
        }
        vSpawnPoints.push_back(p);
        ++oStats.spawn_points;
}

void SpawnerSystem::RemoveComponent(SpawnPoint * p) {

        auto it = std::find(vSpawnPoints.begin(), vSpawnPoints.end(), p);
        if (it != vSpawnPoints.end()) {
                vSpawnPoints.erase(it);
                --oStats.spawn_points;
        }
        for (auto idx = mSpawnIndex.begin(); idx != mSpawnIndex.end();) {
                if (idx->second.pSP == p) { idx = mSpawnIndex.erase(idx); }
                else { ++idx; }
        }
}

const BlackboardCore * SpawnerSystem::GetBlackboard() const {
        return pWorld ? pWorld->GetGlobalBlackboard() : nullptr;
}

uint64_t SpawnerSystem::WorldSeed() const {

        if (auto * pAsset = GetResource<WorldAsset>(pWorld->GetWorld())) {
                return pAsset->GetSeed();
        }
        return 0;
}

// ---------------------------------------------------------------------------
// world population slots
// ---------------------------------------------------------------------------

void SpawnerSystem::OnWorldLoaded(const Event & /*oEvent*/) {

        vWorldSlots.clear();
        vSlotResolved.clear();

        if (auto * pAsset = GetResource<WorldAsset>(pWorld->GetWorld())) {
                for (const auto & slot : pAsset->GetSpawnPopulations()) {
                        vWorldSlots.push_back(slot);
                        vSlotResolved.push_back(false);
                }
        }
        oStats.world_slots_total    = static_cast<uint32_t>(vWorldSlots.size());
        oStats.world_slots_resolved = 0;
}

void SpawnerSystem::OnWorldUnloaded(const Event & /*oEvent*/) {
        vWorldSlots.clear();
        vSlotResolved.clear();
        oStats.world_slots_total    = 0;
        oStats.world_slots_resolved = 0;
}

bool SpawnerSystem::TryClaimWorldSlot(Population * pPop) {

        for (size_t i = 0; i < vWorldSlots.size(); ++i) {
                if (!vSlotResolved[i] && pPop->ClaimWorldSlot(vWorldSlots[i])) {
                        vSlotResolved[i] = true;
                        ++oStats.world_slots_resolved;
                        log_i("SpawnerSystem: population '{}' claimed world spawn slot", pPop->id_string);
                        return true;
                }
        }
        return false;
}

// ---------------------------------------------------------------------------
// snap
// ---------------------------------------------------------------------------

bool SpawnerSystem::SnapPosition(FlatBuffers::SpawnSnap eMode, glm::vec3 & vPos) {

        switch (eMode) {
        case FlatBuffers::SpawnSnap::TerrainHeight: {
                if (!fnTerrainProbe) {
                        if (!bWarnedNoTerrain) {
                                log_w("SpawnerSystem: snap=TerrainHeight but no terrain probe wired "
                                      "(SetTerrainProbe) — falling back to sampled position");
                                bWarnedNoTerrain = true;
                        }
                        return false;
                }
                float height = 0.f;
                if (fnTerrainProbe(vPos, height)) {
                        vPos.y = height;
                        return true;
                }
                return false;
        }
        case FlatBuffers::SpawnSnap::PhysicsRay: {
                if (!fnPhysicsProbe) {
                        if (!bWarnedNoPhysics) {
                                log_w("SpawnerSystem: snap=PhysicsRay but no physics probe wired "
                                      "(SetPhysicsProbe) — falling back to sampled position");
                                bWarnedNoPhysics = true;
                        }
                        return false;
                }
                PhysicsRay oRay;
                oRay.vOrigin    = vPos + glm::vec3(0.f, 4.f, 0.f);
                oRay.vDirection = glm::vec3(0.f, -1.f, 0.f);
                RaycastHit oHit;
                if (fnPhysicsProbe(oRay, oHit)) {
                        vPos = oHit.vPoint;
                        return true;
                }
                return false;
        }
        case FlatBuffers::SpawnSnap::NavMesh: {
                if (!fnNavProbe) {
                        if (!bWarnedNavSnap) {
                                log_w("SpawnerSystem: snap=NavMesh but no nav probe wired "
                                      "(SetNavProbe) — falling back to sampled position");
                                bWarnedNavSnap = true;
                        }
                        return false;
                }
                glm::vec3 vSnapped = vPos;
                if (fnNavProbe(vPos, vSnapped)) {
                        vPos = vSnapped;
                        return true;
                }
                if (!bWarnedNavFail) {
                        log_w("SpawnerSystem: nav probe failed to project a sampled "
                              "position — falling back to it");
                        bWarnedNavFail = true;
                }
                return false;
        }
        case FlatBuffers::SpawnSnap::None:
        default:
                return false;
        }
}

// ---------------------------------------------------------------------------
// spawn execution
// ---------------------------------------------------------------------------

bool SpawnerSystem::ExecutePopulationSpawn(Population * pPop, uint32_t member_index) {

        const auto & member = pPop->oRuntime.Member(member_index);

        // parent: population node (sub-level lifetime) or the world dynamics root
        TSceneTree::TSceneNode pParent = pPop->pNode->shared_from_this();
        if (pPop->eLifetime == FlatBuffers::SpawnLifetime::Persistent) {
                if (auto pDyn = pWorld->GetDynamicsRoot().lock()) {
                        pParent = pDyn;
                } else {
                        log_e("SpawnerSystem: persistent spawn with no dynamics root "
                              "(no world loaded) — spawning under the population node");
                }
        }

        // placement: geometry node's volume/point, else the population origin
        glm::vec3 vPos = pParent->GetTransform().GetWorldPos();
        if (pPop->pNode) {
                vPos = pPop->pNode->GetTransform().GetWorldPos();
        }

        auto pGeo = pPop->vGeometryNodes[member_index].lock();
        // Sub-level children are created after the Population component, so the
        // load-time resolve misses for named geometry — re-resolve now that the
        // subtree exists (self-fallback entries for nameless members skip this).
        if (pGeo && pGeo.get() == pPop->pNode
            && !pPop->oRuntime.Member(member_index).geometry.empty()) {
                pPop->SyncGeometryNodes(true);
                pGeo = pPop->vGeometryNodes[member_index].lock();
        }
        if (pGeo && pGeo.get() != pPop->pNode) {
                vPos = pGeo->GetTransform().GetWorldPos();
                if (auto * pVolume = pGeo->template GetComponent<SpawnVolume>()) {
                        // local rng draw honors the volume's seed offset while keeping
                        // one deterministic stream per population
                        DeterministicRng oVolumeRng(pPop->oRng.NextU64()
                                        ^ (0x9E3779B97F4A7C15ull * pVolume->SeedOffset()));

                        SpawnPlacement::SVolumeDesc oDesc;
                        oDesc.size            = pVolume->Size();
                        oDesc.candidate_count = pVolume->Candidates();
                        oDesc.distribution =
                                pVolume->Distribution() == FlatBuffers::SpawnDistribution::BestCandidate
                                        ? SpawnPlacement::EDistribution::BestCandidate
                                        : SpawnPlacement::EDistribution::Uniform;

                        vPos = SpawnPlacement::SampleVolume(oDesc, oVolumeRng, vPos,
                                        pVolume->Picks().data(),
                                        static_cast<uint32_t>(pVolume->Picks().size()));
                        pVolume->RecordPick(vPos);
                }
        }

        SnapPosition(pPop->eSnap, vPos);

        // Placement (and snapping) happens in WORLD space; the intent transform is
        // parent-LOCAL — convert (translation-only parents assumed, the population
        // case; a rotated parent would need a full inverse transform).
        vPos -= pParent->GetTransform().GetWorldPos();

        SpawnIntent oIntent;
        oIntent.prefab_id   = member.template_id;
        oIntent.pParent     = pParent;
        oIntent.vTranslation = vPos;
        oIntent.name        = member.template_id;   // uniquified by the instantiation path

        auto pHandle = GetSystem<EntityManager>().SpawnTemplate(oIntent);
        if (pHandle.expired()) {
                log_e("SpawnerSystem: failed to spawn template '{}' for population '{}'",
                                member.template_id, pPop->id_string);
                return false;
        }

        pPop->oRuntime.NotifySpawned(member_index);
        pPop->vRecords.push_back({ pHandle, member_index });
        if (auto pSpawned = pHandle.lock()) {
                mSpawnIndex[pSpawned->GetID()] = SOwned{ pPop, nullptr };
        }
        return true;
}

bool SpawnerSystem::ExecuteSpawnPoint(SpawnPoint * pSP) {

        const glm::vec3 vParentWorld = pSP->pNode->GetTransform().GetWorldPos();
        glm::vec3 vPos = vParentWorld;
        SnapPosition(pSP->eSnap, vPos);
        vPos -= vParentWorld;   // world placement → parent-local transform

        SpawnIntent oIntent;
        oIntent.prefab_id    = pSP->sTemplateId;
        oIntent.pParent      = pSP->pNode->shared_from_this();
        oIntent.vTranslation = vPos;
        oIntent.name         = pSP->sTemplateId;

        auto pHandle = GetSystem<EntityManager>().SpawnTemplate(oIntent);
        if (pHandle.expired()) {
                log_e("SpawnerSystem: SpawnPoint failed to spawn template '{}'", pSP->sTemplateId);
                return false;
        }

        ++pSP->spawned_total;
        pSP->vLive.push_back(pHandle);
        if (auto pSpawned = pHandle.lock()) {
                mSpawnIndex[pSpawned->GetID()] = SOwned{ nullptr, pSP };
        }
        return true;
}

// ---------------------------------------------------------------------------
// the pass
// ---------------------------------------------------------------------------

void SpawnerSystem::OnUpdate(const Event & oEvent) {
        TickPass(oEvent.Get<EUpdate>().last_frame_time);
}

void SpawnerSystem::TickPass(float dt) {

        // focus positions once per pass (the FocusNear oracle)
        vFocusScratch.clear();
        for (const auto & f : pWorld->GetFocuses()) {
                vFocusScratch.push_back(f.pos);
        }

        auto make_ctx = [&](const glm::vec3 & vOrigin, float t_since_arm) {
                SpawnConditions::SEvalContext oCtx;
                oCtx.origin      = vOrigin;
                oCtx.t_since_arm = t_since_arm;
                oCtx.vFocuses    = vFocusScratch.data();
                oCtx.focus_count = static_cast<uint32_t>(vFocusScratch.size());
                oCtx.pIndex      = pIndex ? &pIndex->Get() : nullptr;
                oCtx.pBlackboard = pWorld ? pWorld->GetGlobalBlackboard() : nullptr;
                return oCtx;
        };

        // --- populations: staggered ticks, conditions, despawn, commands ---
        for (auto * pPop : vPopulations) {

                pPop->age += dt;
                if (pPop->age < pPop->next_tick_t) {
                        continue;
                }
                pPop->next_tick_t = pPop->age + pPop->tick_interval;

                const auto oCtx = make_ctx(pPop->pNode->GetTransform().GetWorldPos(), pPop->age);

                bool conditions_ok = true;
                for (const auto & c : pPop->vConditions) {
                        if (!SpawnConditions::Evaluate(c, oCtx)) {
                                conditions_ok = false;
                                break;
                        }
                }

                // despawn sweep (density control; default off) — runs INDEPENDENTLY
                // of the arming conditions (it exists precisely for when the focus
                // leaves): kills owned spawns farther than despawn_radius from EVERY
                // focus point; they become respawn-eligible, so density recovers
                // when a focus returns.
                if (pPop->despawn_radius > 0.f) {
                        const float r2 = pPop->despawn_radius * pPop->despawn_radius;
                        for (auto it = pPop->vRecords.begin(); it != pPop->vRecords.end();) {
                                auto pNode = it->node.lock();
                                bool bTooFar = true;
                                if (pNode) {
                                        const glm::vec3 vSpawnPos = pNode->GetTransform().GetWorldPos();
                                        for (const auto & vFocus : vFocusScratch) {
                                                const glm::vec3 d = vSpawnPos - vFocus;
                                                if (d.x * d.x + d.y * d.y + d.z * d.z <= r2) {
                                                        bTooFar = false;
                                                        break;
                                                }
                                        }
                                }
                                if (bTooFar) {
                                        ++oStats.despawns;
                                        pPop->oRuntime.NotifyDied(it->member, pPop->age);
                                        if (auto pNode = it->node.lock()) { mSpawnIndex.erase(pNode->GetID()); }
                                        GetSystem<EntityManager>().Destroy(it->node);
                                        it = pPop->vRecords.erase(it);
                                } else {
                                        ++it;
                                }
                        }
                }

                // collect spawn commands (allocation-free: reused scratch)
                pPop->vCommands.clear();
                pPop->oRuntime.Tick(pPop->age, conditions_ok,
                                [pPop](const PopulationRuntime::SSpawnCommand & c) {
                                        pPop->vCommands.push_back(c);
                                });
        }

        // --- shared budget, round-robin in registration (serial) order ---
        uint32_t budget = oConfig.max_spawns_per_frame;
        bool bAny = true;
        while (budget > 0 && bAny) {
                bAny = false;
                for (auto * pPop : vPopulations) {
                        if (budget == 0) { break; }
                        if (pPop->vCommands.empty()) { continue; }

                        const uint32_t member = pPop->vCommands.front().member;
                        pPop->vCommands.erase(pPop->vCommands.begin());
                        bAny = true;

                        if (ExecutePopulationSpawn(pPop, member)) {
                                --budget;
                                ++oStats.spawns_executed;
                        }
                }
        }

        // leftovers: dropped here; the runtime re-emits them on the next due tick
        for (auto * pPop : vPopulations) {
                oStats.spawns_deferred += static_cast<uint32_t>(pPop->vCommands.size());
                pPop->vCommands.clear();
        }

        // --- spawn points (share the remaining budget) ---
        for (auto * pSP : vSpawnPoints) {

                pSP->age += dt;
                if (pSP->age < pSP->next_ready_t)                 { continue; }
                if (pSP->spawned_total >= pSP->max_spawns)        { continue; }
                if (pSP->HasLiveSpawn())                          { continue; }
                if (budget == 0)                                  { break; }

                if (pSP->has_gate) {
                        const auto oCtx = make_ctx(pSP->pNode->GetTransform().GetWorldPos(), pSP->age);
                        if (!SpawnConditions::Evaluate(pSP->oCondition, oCtx)) {
                                continue;
                        }
                }

                if (ExecuteSpawnPoint(pSP)) {
                        --budget;
                        ++oStats.spawns_executed;
                }
        }
}

// ---------------------------------------------------------------------------
// event fan-out
// ---------------------------------------------------------------------------

void SpawnerSystem::OnEntityDestroyed(const Event & oEvent) {

        const auto hNode = oEvent.Get<EEntityDestroyed>().pNode;
        const auto pLocked = hNode.lock();
        if (!pLocked) { return; }

        // O(1) ownership dispatch — a destroyed node that this system never
        // spawned (i.e. almost every node of an unloading sub-level) costs one
        // hash lookup instead of a full registry sweep.
        const auto owned = mSpawnIndex.find(pLocked->GetID());
        if (owned == mSpawnIndex.end()) { return; }

        if (owned->second.pPop) {
                Population * pPop = owned->second.pPop;
                for (auto it = pPop->vRecords.begin(); it != pPop->vRecords.end(); ++it) {
                        auto pRec = it->node.lock();
                        if (pRec.get() == pLocked.get()) {
                                pPop->oRuntime.NotifyDied(it->member, pPop->age);
                                pPop->vRecords.erase(it);
                                break;
                        }
                }
        }
        else if (owned->second.pSP) {
                SpawnPoint * pSP = owned->second.pSP;
                for (auto it = pSP->vLive.begin(); it != pSP->vLive.end(); ++it) {
                        auto pRec = it->lock();
                        if (pRec.get() == pLocked.get()) {
                                pSP->vLive.erase(it);
                                if (pSP->respawn_delay_s >= 0.f) {
                                        pSP->next_ready_t = pSP->age + pSP->respawn_delay_s;
                                }
                                break;
                        }
                }
        }
        mSpawnIndex.erase(owned);
}

void SpawnerSystem::OnTriggerFired(const Event & oEvent) {

        const auto ev = oEvent.Get<ETriggerFired>();

        for (auto * pPop : vPopulations) {
                for (uint32_t i = 0; i < pPop->oRuntime.WaveCount(); ++i) {
                        const auto & w = pPop->oRuntime.Wave(i);
                        if (w.on_event_hash == 0) { continue; }
                        // pre-hashed at Arm (PopulationRuntime) — no per-event re-hash
                        if (w.on_event_hash != static_cast<uint64_t>(ev.event_id)) { continue; }
                        if (pPop->oRuntime.FireWave(i, pPop->age)) {
                                log_i("SpawnerSystem: population '{}' wave {} armed by event",
                                                pPop->id_string, i);
                        }
                }
        }
}

} // namespace SE

#endif
