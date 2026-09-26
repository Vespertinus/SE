
#ifdef SE_IMPL

#include <terrain/TerrainSystem.h>

#include <Logging.h>

#include <GlobalTypes.h>
#include <EventManager.h>
#include <CommonEvents.h>
#include <world/WorldSystem.h>
#include <world/WorldAsset.h>
#include <terrain/TerrainQuery.h>
#include <terrain/Terrain.h>

#include <Terrain_generated.h>
#include <flatbuffers/flatbuffers.h>

#include <algorithm>
#include <chrono>
#include <cmath>

#ifdef SE_PHYSICS_ENABLED
#include <PhysicsSystem.h>
#endif

namespace SE {

namespace {

constexpr uint32_t kRegSlotBits = 16;
constexpr uint32_t kRegSlotMask = 0xFFFFu;

inline uint32_t PackDispatch(uint32_t reg_slot, uint32_t tile_index) {
        return (reg_slot << kRegSlotBits) | (tile_index & kRegSlotMask);
}
inline uint32_t UnpackRegSlot(uint32_t dispatch_index) { return dispatch_index >> kRegSlotBits; }
inline uint32_t UnpackTileIndex(uint32_t dispatch_index) { return dispatch_index & kRegSlotMask; }
inline uint32_t RegSlotOf(uint64_t reg_id) { return uint32_t(reg_id & 0xFFFFu) - 1; }

// Manifest path → directory prefix (tile paths are authored relative to it).
std::string DirOf(const std::string & sPath) {
        const size_t pos = sPath.find_last_of('/');
        return pos == std::string::npos ? std::string() : sPath.substr(0, pos + 1);
}

} // namespace

// ---------------------------------------------------------------------------

TerrainSystem::TerrainSystem()
        : pWorld(&GetSystem<WorldSystem>()) {

        pWorld->SetContentResultHandler(StreamingDirector::EContentKind::TerrainTile,
                        [this](const StreamingDirector::SReadResult & res) { OnTileResult(res); });

        GetSystem<EventManager>().AddListener<EUpdate,        &TerrainSystem::OnUpdate>(this);
        GetSystem<EventManager>().AddListener<EWorldLoaded,   &TerrainSystem::OnWorldLoaded>(this);
        GetSystem<EventManager>().AddListener<EWorldUnloaded, &TerrainSystem::OnWorldUnloaded>(this);
}

TerrainSystem::~TerrainSystem() noexcept {
        bShuttingDown = true;
        GetSystem<EventManager>().RemoveListener<EUpdate,        &TerrainSystem::OnUpdate>(this);
        GetSystem<EventManager>().RemoveListener<EWorldLoaded,   &TerrainSystem::OnWorldLoaded>(this);
        GetSystem<EventManager>().RemoveListener<EWorldUnloaded, &TerrainSystem::OnWorldUnloaded>(this);

        if (pWorld) { pWorld->SetContentResultHandler(StreamingDirector::EContentKind::TerrainTile, {}); }
        vGpuSinks.clear();
        DetachAll();
}

// ---------------------------------------------------------------------------
// registrations
// ---------------------------------------------------------------------------

TerrainSystem::SRegistration * TerrainSystem::FindReg(const uint64_t reg_id) {

        if (reg_id == 0) return nullptr;
        const uint32_t slot = static_cast<uint32_t>(reg_id & 0xFFFFu) - 1;
        if (slot >= vRegistrations.size()) return nullptr;
        SRegistration & reg = vRegistrations[slot];
        return reg.id == reg_id ? &reg : nullptr;
}

const TerrainSystem::SRegistration * TerrainSystem::FindReg(const uint64_t reg_id) const {

        if (reg_id == 0) return nullptr;
        const uint32_t slot = static_cast<uint32_t>(reg_id & 0xFFFFu) - 1;
        if (slot >= vRegistrations.size()) return nullptr;
        const SRegistration & reg = vRegistrations[slot];
        return reg.id == reg_id ? &reg : nullptr;
}

uint64_t TerrainSystem::AttachImpl(H<TerrainAsset> hAsset, const bool bPaged, const bool bFromWorld) {

        if (!hAsset.IsValid()) {
                log_e("TerrainSystem: attach with null asset");
                return 0;
        }

        // dedup by manifest path (a world grid + a render component share one registration)
        for (SRegistration & reg : vRegistrations) {
                if (reg.id != 0 && reg.hAsset == hAsset) {
                        reg.b_paged = reg.b_paged || bPaged;
                        reg.b_from_world = reg.b_from_world || bFromWorld;
                        return reg.id;
                }
        }

        const uint32_t slot = vFreeSlots.empty()
                ? [&] { vRegistrations.emplace_back(); return uint32_t(vRegistrations.size() - 1); } ()
                : [&] { const uint32_t s = vFreeSlots.back(); vFreeSlots.pop_back(); return s; } ();

        SRegistration & reg = vRegistrations[slot];
        const uint16_t gen = static_cast<uint16_t>((reg.id >> 16) + 1);
        reg.id             = (static_cast<uint64_t>(gen) << 16) | static_cast<uint64_t>(slot + 1);
        reg.hAsset         = hAsset;
        reg.sTileDir       = DirOf(GetResource(hAsset)->Name());
        reg.b_paged        = bPaged;
        reg.b_from_world   = bFromWorld;
        reg.linked_components = 0;
        reg.ticket         = 0;

        const auto & vRefs = GetResource(hAsset)->GetTiles();
        reg.vTiles.assign(vRefs.size(), STileSlot{});
        if (bPaged) {
                reg.pPlanner = std::make_unique<StreamingDirector>();
                reg.pPlanner->Reset(uint32_t(vRefs.size()));
                reg.vSnapBounds.clear();
                reg.vSnapBounds.reserve(vRefs.size());
                for (const auto & r : vRefs) {
                        StreamingDirector::SLevelSnap oSnap;
                        oSnap.bounds_min = r.vBoundsMin;
                        oSnap.bounds_max = r.vBoundsMax;
                        reg.vSnapBounds.push_back(oSnap);
                }
        }

        ++serial;   // in-flight results from any previous structure become stale
        log_i("TerrainSystem: attached '{}' ({} tiles, {})", GetResource(hAsset)->Name(), vRefs.size(),
              bPaged ? "paged" : "component");
        return reg.id;
}

uint64_t TerrainSystem::AttachGrid(H<TerrainAsset> hAsset) {
        return AttachImpl(std::move(hAsset), /*bPaged=*/true, /*bFromWorld=*/false);
}

uint64_t TerrainSystem::AttachTerrain(H<TerrainAsset> hAsset, const bool bPaged) {
        return AttachImpl(std::move(hAsset), bPaged, /*bFromWorld=*/false);
}

void TerrainSystem::LinkComponent(const uint64_t reg_id, Terrain * pRenderable) {
        if (SRegistration * reg = FindReg(reg_id)) {
                ++reg->linked_components;
                if (pRenderable && !reg->pRenderable) { reg->pRenderable = pRenderable; }
        }
}

void TerrainSystem::UnlinkComponent(const uint64_t reg_id) {
        if (SRegistration * reg = FindReg(reg_id)) {
                reg->linked_components = reg->linked_components > 0 ? reg->linked_components - 1 : 0;
                if (reg->linked_components == 0) { reg->pRenderable = nullptr; }
        }
}

void TerrainSystem::Detach(const uint64_t reg_id) {

        SRegistration * reg = FindReg(reg_id);
        if (!reg) return;

        // release every tile this registration owns
        for (uint32_t idx = 0; idx < reg->vTiles.size(); ++idx) {
                STileSlot & slot = reg->vTiles[idx];
                if (slot.status == ETileStatus::Active)  { ReleaseTile(*reg, idx); }
                slot.status = ETileStatus::Unloaded;
                slot.pRuntime.reset();
                slot.ticket = 0;
        }

        const uint32_t slot_idx = static_cast<uint32_t>(reg_id & 0xFFFFu) - 1;
        reg->id = 0;   // mark free (keeps the generation counter for the next user)
        reg->pPlanner.reset();
        reg->vTiles.clear();
        reg->vSnapBounds.clear();
        vFreeSlots.push_back(slot_idx);
        for (SGpuSink & sink : vGpuSinks) {
                if (sink.reg_id == reg_id) { sink.reg_id = 0; sink.fnReady = {}; sink.fnRelease = {}; }
        }

        ++serial;
        log_i("TerrainSystem: detached registration #{}", slot_idx);
}

void TerrainSystem::DetachAll() {
        for (SRegistration & reg : vRegistrations) {
                if (reg.id != 0) { Detach(reg.id); }
        }
}

void TerrainSystem::SetGpuSink(const uint64_t reg_id, TGpuReadyFn fnReady, TGpuReleaseFn fnRelease) {

        for (SGpuSink & sink : vGpuSinks) {
                if (sink.reg_id == reg_id) {
                        sink.fnReady   = std::move(fnReady);
                        sink.fnRelease = std::move(fnRelease);
                        return;
                }
        }
        if (reg_id != 0) {
                vGpuSinks.push_back(SGpuSink{ reg_id, std::move(fnReady), std::move(fnRelease) });
        }
}

// ---------------------------------------------------------------------------
// worker results (main thread, routed by WorldSystem's result drain)
// ---------------------------------------------------------------------------

void TerrainSystem::OnTileResult(const StreamingDirector::SReadResult & res) {

        if (res.kind != StreamingDirector::EContentKind::TerrainTile) { return; }
        if (res.world_serial != serial) { return; }   // stale (structure changed meanwhile)

        const uint32_t reg_slot   = UnpackRegSlot(res.index);
        const uint32_t tile_index = UnpackTileIndex(res.index);
        if (reg_slot >= vRegistrations.size()) { return; }
        SRegistration & reg = vRegistrations[reg_slot];
        if (reg.id == 0 || tile_index >= reg.vTiles.size()) { return; }

        STileSlot & slot = reg.vTiles[tile_index];
        if (slot.status != ETileStatus::Requested || slot.ticket != res.ticket) {
                return;   // cancelled (or re-requested) while the read was out
        }

        if (!res.ok) {
                slot.status = ETileStatus::Unloaded;
                slot.ticket = 0;
                if (reg.pPlanner) { reg.pPlanner->NotifyFailure(tile_index); }   // cooldown, no retry storm
                return;
        }

        // decode into the runtime form (quantized stays quantized in RAM)
        const auto * pFb = flatbuffers::GetRoot<FlatBuffers::TerrainTileData>(res.pBytes->data());
        auto pRuntime = std::make_shared<STileRuntime>();
        pRuntime->x = pFb->x();
        pRuntime->y = pFb->y();
        pRuntime->height_offset = pFb->height_offset();
        pRuntime->height_scale  = pFb->height_scale();
        pRuntime->tile_size     = GetResource(reg.hAsset)->GetTileSize();
        if (const auto * pRef = GetResource(reg.hAsset)->FindTile(pFb->x(), pFb->y())) {
                pRuntime->vBoundsMin = pRef->vBoundsMin;
                pRuntime->vBoundsMax = pRef->vBoundsMax;
        }

        if (pFb->lods()) {
                pRuntime->vLods.reserve(pFb->lods()->size());
                for (const auto * pLod : *pFb->lods()) {
                        if (!pLod || !pLod->heights() || !pLod->normals()) {
                                // malformed tile → treat exactly like a failed read:
                                // reset the slot so the planner retries after its
                                // cooldown (an early return here would leave the
                                // slot stuck Requested — a permanent terrain hole)
                                slot.status = ETileStatus::Unloaded;
                                slot.ticket = 0;
                                slot.pRuntime.reset();
                                if (reg.pPlanner) { reg.pPlanner->NotifyFailure(tile_index); }
                                return;
                        }
                        SDecodedLod oLod;
                        oLod.resolution = pLod->resolution();
                        oLod.vHeights.assign(pLod->heights()->begin(), pLod->heights()->end());
                        oLod.vNormals.assign(pLod->normals()->begin(), pLod->normals()->end());
                        pRuntime->vLods.push_back(std::move(oLod));
                }
        }
        if (pRuntime->vLods.empty() || pRuntime->vLods[0].resolution < 2) {
                slot.status = ETileStatus::Unloaded;
                slot.ticket = 0;
                slot.pRuntime.reset();
                if (reg.pPlanner) { reg.pPlanner->NotifyFailure(tile_index); }
                return;
        }

        slot.pRuntime = std::move(pRuntime);
        slot.status   = ETileStatus::Loading;
}

// ---------------------------------------------------------------------------
// per-frame pass
// ---------------------------------------------------------------------------

void TerrainSystem::OnUpdate(const Event &) {

        last_budget_spent_ms = 0.0f;

        for (SRegistration & reg : vRegistrations) {
                if (reg.id == 0) continue;

                if (reg.b_paged) {
                        TickRegistration(reg, oConfig.max_in_flight_reads, true);
                }
                else {
                        TickComponentWanted(reg);
                }
        }

        // budgeted build loop: Loading tiles → Active, nearest first, >= 1 per frame
        vBuildOrder.clear();
        const glm::vec3 vFocusPos = !pWorld->GetFocuses().empty()
                ? pWorld->GetFocuses().front().pos
                : glm::vec3{ 0.0f };

        for (size_t reg_pos = 0; reg_pos < vRegistrations.size(); ++reg_pos) {
                SRegistration & reg = vRegistrations[reg_pos];
                if (reg.id == 0) continue;
                const auto & vRefs = GetResource(reg.hAsset)->GetTiles();
                for (uint32_t idx = 0; idx < reg.vTiles.size(); ++idx) {
                        if (reg.vTiles[idx].status != ETileStatus::Loading) continue;
                        const float d = StreamingDirector::DistanceToBounds(vFocusPos,
                                        vRefs[idx].vBoundsMin, vRefs[idx].vBoundsMax);
                        vBuildOrder.emplace_back(d, PackDispatch(uint32_t(reg_pos), idx));
                }
        }
        std::sort(vBuildOrder.begin(), vBuildOrder.end(),
                  [](const auto & a, const auto & b) {
                          if (a.first != b.first) return a.first < b.first;
                          return a.second < b.second;
                  });

        const auto t_start = std::chrono::steady_clock::now();
        // Honor the declared per-frame activation cap: the wall budget alone
        // let a streaming burst spike to exactly `main_budget_ms` EVERY frame.
        const uint32_t cap = oConfig.max_tile_activations_per_frame;
        uint32_t built = 0;
        for (const auto & [dist, packed] : vBuildOrder) {
                SRegistration & reg = vRegistrations[UnpackRegSlot(packed)];
                if (!FinishTile(reg, UnpackTileIndex(packed))) { continue; }
                ++built;
                if (cap > 0 && built >= cap) { break; }
                const auto t_now = std::chrono::steady_clock::now();
                const float spent_ms = std::chrono::duration<float, std::milli>(t_now - t_start).count();
                if (spent_ms > oConfig.main_budget_ms) { break; }   // >= 1 unit guaranteed
        }
        last_budget_spent_ms = built > 0
                ? std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - t_start).count()
                : 0.0f;
}

void TerrainSystem::TickRegistration(SRegistration & reg, const uint32_t in_flight_budget,
                                     const bool bAdvanceTimers) {

        // focus snapshot from the world's focus registry (reused scratch).
        // Positions gate tile wantedness, but the per-focus radius override is
        // tuned for sub-levels (tens of meters) — tiles must page by the
        // terrain's own load radius, so the override is dropped here.
        reg.vSnapFocuses.clear();
        for (const auto & f : pWorld->GetFocuses()) {
                reg.vSnapFocuses.push_back({ f.pos, 0.f, f.priority });
        }

        // status snapshot
        reg.vSnapStatuses.clear();
        uint32_t in_flight = 0;
        for (const STileSlot & slot : reg.vTiles) {
                // Requested (read in flight) and Loading (staged, awaiting build) both
                // map to the planner's "Loading" — the cancellable in-flight state.
                switch (slot.status) {
                case ETileStatus::Unloaded:  reg.vSnapStatuses.push_back(StreamingDirector::ELevelStatus::Unloaded); break;
                case ETileStatus::Requested:
                case ETileStatus::Loading:   reg.vSnapStatuses.push_back(StreamingDirector::ELevelStatus::Loading);  break;
                case ETileStatus::Active:    reg.vSnapStatuses.push_back(StreamingDirector::ELevelStatus::Active);   break;
                }
                in_flight += slot.status == ETileStatus::Requested ? 1 : 0;
        }

        StreamingDirector::SParams oParams;
        oParams.load_radius        = oConfig.load_radius;
        oParams.unload_radius      = oConfig.unload_radius;
        oParams.unload_frame_delay = std::max(1u, oConfig.unload_frame_delay);
        oParams.max_in_flight      = in_flight_budget;

        StreamingDirector::SPlan oPlan;
        reg.pPlanner->Plan(oParams, reg.vSnapBounds.data(), reg.vSnapStatuses.data(),
                           uint32_t(reg.vTiles.size()),
                           reg.vSnapFocuses.data(), uint32_t(reg.vSnapFocuses.size()),
                           in_flight, bAdvanceTimers, oPlan);

        for (const uint32_t idx : oPlan.vCancels) {
                STileSlot & slot = reg.vTiles[idx];
                if (slot.status == ETileStatus::Requested || slot.status == ETileStatus::Loading) {
                        CancelTile(reg, slot);
                }
        }

        const auto & vRefs = GetResource(reg.hAsset)->GetTiles();
        for (const uint32_t idx : oPlan.vRequests) {
                STileSlot & slot = reg.vTiles[idx];
                if (slot.status != ETileStatus::Unloaded) continue;

                ++reg.ticket;
                const std::string sPath = reg.sTileDir + vRefs[idx].sPath;
                if (pWorld->GetDirector().DispatchRead(serial, reg.ticket,
                                PackDispatch(RegSlotOf(reg.id), idx), sPath,
                                StreamingDirector::EContentKind::TerrainTile)) {
                        slot.status = ETileStatus::Requested;
                        slot.ticket = reg.ticket;
                }
                else {
                        log_i("TerrainDBG: dispatch FULL for tile {}", idx);
                }
        }

        // Unload burst cap: the planner emits every tile whose unload timer
        // expired in ONE plan; applying them all in one frame produced a
        // streaming-crossing hitch (GL deletes, body destroys, events). Cap
        // per frame — the rest arrive next tick.
        uint32_t unloads = 0;
        for (const uint32_t idx : oPlan.vUnloads) {
                if (oConfig.max_tile_activations_per_frame > 0
                    && unloads >= oConfig.max_tile_activations_per_frame) { break; }
                ReleaseTile(reg, idx);
                ++unloads;
        }

}

void TerrainSystem::TickComponentWanted(SRegistration & reg) {

        if (reg.linked_components == 0) {
                // nobody wants this terrain: release everything deterministically
                for (uint32_t idx = 0; idx < reg.vTiles.size(); ++idx) {
                        if (reg.vTiles[idx].status == ETileStatus::Active)  { ReleaseTile(reg, idx); }
                        if (reg.vTiles[idx].status == ETileStatus::Loading) { CancelTile(reg, reg.vTiles[idx]); }
                }
                return;
        }

        // all tiles wanted; dispatch in manifest order (deterministic), in-flight capped
        uint32_t in_flight = 0;
        for (const STileSlot & slot : reg.vTiles) {
                in_flight += slot.status == ETileStatus::Requested ? 1 : 0;
        }

        const auto & vRefs = GetResource(reg.hAsset)->GetTiles();
        for (uint32_t idx = 0; idx < reg.vTiles.size() && in_flight < oConfig.max_in_flight_reads; ++idx) {
                STileSlot & slot = reg.vTiles[idx];
                if (slot.status != ETileStatus::Unloaded) continue;

                ++reg.ticket;
                const std::string sPath = reg.sTileDir + vRefs[idx].sPath;
                if (pWorld->GetDirector().DispatchRead(serial, reg.ticket,
                                PackDispatch(RegSlotOf(reg.id), idx), sPath,
                                StreamingDirector::EContentKind::TerrainTile)) {
                        slot.status = ETileStatus::Requested;
                        slot.ticket = reg.ticket;
                        ++in_flight;
                }
        }
}

// ---------------------------------------------------------------------------
// tile build / release
// ---------------------------------------------------------------------------

bool TerrainSystem::FinishTile(SRegistration & reg, const uint32_t tile_index) {

        STileSlot & slot = reg.vTiles[tile_index];
        if (slot.status != ETileStatus::Loading || !slot.pRuntime) { return false; }

        if (vActiveTiles.size() >= oConfig.max_active_tiles) {
                log_e("TerrainSystem: active-tile ceiling {} reached", oConfig.max_active_tiles);
                return false;   // stays Loading; retried next frame
        }

        const uint32_t reg_slot = RegSlotOf(reg.id);

        SActiveTile oActive;
        oActive.reg_slot   = reg_slot;
        oActive.tile_index = tile_index;
        oActive.pRuntime   = slot.pRuntime;

#ifdef SE_PHYSICS_ENABLED
        {
                // dequantize LOD0 once per build (scratch); Jolt re-quantizes internally
                const SDecodedLod & oLod0 = slot.pRuntime->vLods[0];
                const uint32_t res = oLod0.resolution;
                vScratchSamples.resize(size_t(res) * res);
                for (size_t k = 0; k < vScratchSamples.size(); ++k) {
                        vScratchSamples[k] = slot.pRuntime->Dequantize(oLod0.vHeights[k]);
                }
                const float step = TerrainQuery::SampleStep(res, slot.pRuntime->tile_size);
                const glm::vec3 vOffset(slot.pRuntime->vBoundsMin.x, 0.0f, slot.pRuntime->vBoundsMin.z);
                oActive.hBody = GetSystem<PhysicsSystem>().CreateStaticHeightField(
                                vScratchSamples, res, vOffset,
                                glm::vec3(step, 1.0f, step),
                                GetResource(reg.hAsset)->GetPhysics().friction);
                if (!oActive.hBody.IsValid()) {
                        log_e("TerrainSystem: heightfield body failed for tile ({}, {})",
                              slot.pRuntime->x, slot.pRuntime->y);
                }
        }
#endif

        mActiveLookup[{reg_slot, tile_index}] = uint32_t(vActiveTiles.size());
        vActiveTiles.push_back(std::move(oActive));

        for (const SGpuSink & sink : vGpuSinks) {
                if (sink.reg_id == reg.id && sink.fnReady) { sink.fnReady(tile_index, *slot.pRuntime); }
        }

        slot.status = ETileStatus::Active;
        slot.pRuntime.reset();

        // the tile is fully up (body + sinks) — announce to listeners
        // (the navigation runtime pages terrain-grid nav tiles with these).
        if (!bShuttingDown) {
                const TerrainAsset & oAsset = *GetResource(reg.hAsset);
                const TerrainAsset::STileRef & oRef = oAsset.GetTiles()[tile_index];
                GetSystem<EventManager>().TriggerEvent(ETerrainTileActivated{
                                StrID(oAsset.Name()), tile_index, oRef.x, oRef.y,
                                oRef.vBoundsMin, oRef.vBoundsMax });
        }
        return true;
}

void TerrainSystem::ReleaseTile(SRegistration & reg, const uint32_t tile_index) {

        STileSlot & slot = reg.vTiles[tile_index];
        if (slot.status != ETileStatus::Active) { return; }

        const uint32_t reg_slot = RegSlotOf(reg.id);
        const auto it = mActiveLookup.find({ reg_slot, tile_index });
        if (it == mActiveLookup.end()) {
                slot.status = ETileStatus::Unloaded;
                return;
        }

        const size_t active_idx = it->second;
#ifdef SE_PHYSICS_ENABLED
        if (vActiveTiles[active_idx].hBody.IsValid()) {
                GetSystem<PhysicsSystem>().DestroyBody(vActiveTiles[active_idx].hBody);
        }
#endif
        for (const SGpuSink & sink : vGpuSinks) {
                if (sink.reg_id == reg.id && sink.fnRelease) { sink.fnRelease(tile_index); }
        }

        mActiveLookup.erase(it);
        if (active_idx + 1 != vActiveTiles.size()) {
                vActiveTiles[active_idx] = std::move(vActiveTiles.back());
                mActiveLookup[{ vActiveTiles[active_idx].reg_slot, vActiveTiles[active_idx].tile_index }]
                        = uint32_t(active_idx);
        }
        vActiveTiles.pop_back();

        slot.status = ETileStatus::Unloaded;
        slot.ticket = 0;
        slot.pRuntime.reset();

        // the tile is fully down — announce to listeners. Fires on both
        // release paths (paging unload and Detach/world unload); suppressed only
        // during engine shutdown.
        if (!bShuttingDown) {
                const TerrainAsset & oAsset = *GetResource(reg.hAsset);
                const TerrainAsset::STileRef & oRef = oAsset.GetTiles()[tile_index];
                GetSystem<EventManager>().TriggerEvent(ETerrainTileDeactivated{
                                StrID(oAsset.Name()), tile_index, oRef.x, oRef.y,
                                oRef.vBoundsMin, oRef.vBoundsMax });
        }
}

void TerrainSystem::CancelTile(SRegistration & reg, STileSlot & slot) {

        (void)reg;
        slot.status = ETileStatus::Unloaded;
        slot.ticket = 0;
        slot.pRuntime.reset();
}

// ---------------------------------------------------------------------------
// queries
// ---------------------------------------------------------------------------

bool TerrainSystem::TryGetHeight(const glm::vec3 & vPos, float & out_height) const {

        for (const SRegistration & reg : vRegistrations) {
                if (reg.id == 0) continue;
                const TerrainAsset & oAsset = *GetResource(reg.hAsset);

                const glm::vec3 vRel = vPos - oAsset.GetOrigin();
                const float ts = oAsset.GetTileSize();
                const int32_t tx = int32_t(std::floor(vRel.x / ts));
                const int32_t ty = int32_t(std::floor(vRel.z / ts));
                if (tx < 0 || ty < 0 || tx >= oAsset.GetTilesX() || ty >= oAsset.GetTilesY()) { continue; }

                const int32_t tile_index = oAsset.TileIndexAt(uint16_t(tx), uint16_t(ty));
                if (tile_index < 0) { continue; }

                const auto it = mActiveLookup.find({ uint32_t(reg.id & 0xFFFFu) - 1, uint32_t(tile_index) });
                if (it == mActiveLookup.end()) { continue; }   // tile not active

                const STileRuntime & oRuntime = *vActiveTiles[it->second].pRuntime;
                const float local_x = vRel.x - tx * ts;
                const float local_z = vRel.z - ty * ts;
                if (TerrainQuery::SampleHeight(oRuntime, 0, local_x, local_z, out_height)) {
                        return true;
                }
        }
        return false;
}

bool TerrainSystem::TryGetNormal(const glm::vec3 & vPos, glm::vec3 & out_normal) const {

        for (const SRegistration & reg : vRegistrations) {
                if (reg.id == 0) continue;
                const TerrainAsset & oAsset = *GetResource(reg.hAsset);

                const glm::vec3 vRel = vPos - oAsset.GetOrigin();
                const float ts = oAsset.GetTileSize();
                const int32_t tx = int32_t(std::floor(vRel.x / ts));
                const int32_t ty = int32_t(std::floor(vRel.z / ts));
                if (tx < 0 || ty < 0 || tx >= oAsset.GetTilesX() || ty >= oAsset.GetTilesY()) { continue; }

                const int32_t tile_index = oAsset.TileIndexAt(uint16_t(tx), uint16_t(ty));
                if (tile_index < 0) { continue; }

                const auto it = mActiveLookup.find({ uint32_t(reg.id & 0xFFFFu) - 1, uint32_t(tile_index) });
                if (it == mActiveLookup.end()) { continue; }

                const STileRuntime & oRuntime = *vActiveTiles[it->second].pRuntime;
                const float local_x = vRel.x - tx * ts;
                const float local_z = vRel.z - ty * ts;
                if (TerrainQuery::SampleNormal(oRuntime, 0, local_x, local_z, out_normal)) {
                        return true;
                }
        }
        return false;
}

void TerrainSystem::GetDebugTiles(std::vector<STileDebug> & vOut) const {

        vOut.clear();
        for (const SRegistration & reg : vRegistrations) {
                if (reg.id == 0) continue;
                for (uint32_t idx = 0; idx < reg.vTiles.size(); ++idx) {
                        STileDebug oDbg;
                        const auto & oRef = GetResource(reg.hAsset)->GetTiles()[idx];
                        oDbg.x = oRef.x;
                        oDbg.y = oRef.y;
                        oDbg.status = static_cast<uint8_t>(reg.vTiles[idx].status);
                        oDbg.b_paged = reg.b_paged;
                        vOut.push_back(oDbg);
                }
        }
}

TerrainSystem::SStats TerrainSystem::GetStats() const {

        SStats oStats;
        oStats.budget_spent_ms = last_budget_spent_ms;
        oStats.active_tiles    = uint32_t(vActiveTiles.size());
        oStats.in_flight_reads = pWorld ? pWorld->GetDirector().GetInFlightCount() : 0;
        for (const SRegistration & reg : vRegistrations) {
                if (reg.id == 0) continue;
                ++oStats.registrations;
                if (reg.pRenderable) {
                        oStats.visible_cmds += reg.pRenderable->GetLastVisibleCount();
                        oStats.visible_tris += reg.pRenderable->GetLastTriCount();
                }
                for (const STileSlot & slot : reg.vTiles) {
                        oStats.requested += slot.status == ETileStatus::Requested ? 1 : 0;
                        oStats.loading   += slot.status == ETileStatus::Loading   ? 1 : 0;
                }
        }
        return oStats;
}

Terrain * TerrainSystem::GetRenderable() const {

        for (const SRegistration & reg : vRegistrations) {
                if (reg.id != 0 && reg.pRenderable) { return reg.pRenderable; }
        }
        return nullptr;
}

// ---------------------------------------------------------------------------
// world coupling
// ---------------------------------------------------------------------------

void TerrainSystem::OnWorldLoaded(const Event &) {

        H<WorldAsset> hWorld = pWorld->GetWorld();
        if (!hWorld.IsValid()) { return; }

        const std::string & sManifest = GetResource(hWorld)->GetTerrainGridManifest();
        if (sManifest.empty()) { return; }

        H<TerrainAsset> hAsset = CreateResource<TerrainAsset>(sManifest);
        if (!hAsset.IsValid()) {
                log_e("TerrainSystem: failed to load terrain grid manifest '{}'", sManifest);
                return;
        }
        AttachImpl(hAsset, /*bPaged=*/true, /*bFromWorld=*/true);
}

void TerrainSystem::OnWorldUnloaded(const Event &) {

        // drop world-owned grid registrations; component-owned ones survive their components
        for (SRegistration & reg : vRegistrations) {
                if (reg.id != 0 && reg.b_from_world) { Detach(reg.id); }
        }
}

} // namespace SE

#endif // SE_IMPL
