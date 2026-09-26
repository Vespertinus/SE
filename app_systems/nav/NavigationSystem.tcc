
#ifdef SE_IMPL

#include <nav/NavigationSystem.h>
#include <nav/NavAgent.h>

#include <Logging.h>
#include <EventManager.h>
#include <CommonEvents.h>
#include <DebugRenderer.h>
#include <GlobalTypes.h>

#include <DetourCommon.h>

#include <algorithm>
#include <cstring>
#include <unordered_set>

namespace SE {

namespace {

// deterministic frand for Detour's random-point queries (nav is main-thread
// only; seeded from SetRandomSeed so tests/replays are reproducible)
uint64_t g_navRngState = 0x9E3779B97F4A7C15ull;

float NavFrand() {
        g_navRngState = g_navRngState * 6364136223846793005ull + 1442695040888963407ull;
        return float((g_navRngState >> 40) & 0xFFFFFFull) / float(0xFFFFFFull);
}

// dtPathCorridor lives by dtAlloc, like every other Detour object
dtPathCorridor * AllocCorridor() {
        void * pMem = dtAlloc(sizeof(dtPathCorridor), DT_ALLOC_PERM);
        if (!pMem) { return nullptr; }
        return new (pMem) dtPathCorridor();
}

void FreeCorridor(dtPathCorridor * pCorridor) {
        if (!pCorridor) { return; }
        pCorridor->~dtPathCorridor();
        dtFree(pCorridor);
}

// Copies tile bytes into Detour's allocator; on success Detour owns the copy
// (DT_TILE_FREE_DATA) and frees it in removeTile. 0 on failure (copy freed).
dtTileRef AddTileBytes(dtNavMesh * pMesh, const uint8_t * pData, const uint32_t nSize) {

        unsigned char * pCopy = reinterpret_cast<unsigned char *>(dtAlloc(nSize, DT_ALLOC_PERM));
        if (!pCopy) { return 0; }
        memcpy(pCopy, pData, nSize);

        dtTileRef ref = 0;
        const dtStatus status = pMesh->addTile(pCopy, static_cast<int>(nSize), DT_TILE_FREE_DATA, 0, &ref);
        if (dtStatusFailed(status)) {
                // addTile does not take ownership on failure
                dtFree(pCopy);
                if (status != (DT_FAILURE | DT_ALREADY_OCCUPIED)) {
                        log_w("NavigationSystem: addTile failed ({:#x})", static_cast<uint32_t>(status));
                }
                return 0;
        }
        return ref;
}

void RemoveTile(dtNavMesh * pMesh, const dtTileRef ref) {
        if (!ref) { return; }
        unsigned char * pData = nullptr;
        int nSize = 0;
        pMesh->removeTile(ref, &pData, &nSize);   // DT_TILE_FREE_DATA: Detour freed already
}

void CopyVec(float * pOut, const glm::vec3 & v) {
        pOut[0] = v.x;
        pOut[1] = v.y;
        pOut[2] = v.z;
}

glm::vec3 VecOf(const float * p) { return {p[0], p[1], p[2]}; }

} // namespace

// ---------------------------------------------------------------------------

NavigationSystem::NavigationSystem() {
        GetSystem<EventManager>().AddListener<EUpdate,     &NavigationSystem::OnUpdate>(this);
        GetSystem<EventManager>().AddListener<EPostUpdate, &NavigationSystem::OnPostUpdate>(this);
}

void NavigationSystem::SetRandomSeed(const uint64_t seed) {
        g_navRngState = seed ? seed : 0x9E3779B97F4A7C15ull;
}

NavigationSystem::~NavigationSystem() noexcept {
        GetSystem<EventManager>().RemoveListener<EUpdate,     &NavigationSystem::OnUpdate>(this);
        GetSystem<EventManager>().RemoveListener<EPostUpdate, &NavigationSystem::OnPostUpdate>(this);

        UnregisterAll();

        for (SAgentRecord & rec : vAgents) { FreeCorridor(rec.pCorridor); }
}

// ---------------------------------------------------------------------------
// registration
// ---------------------------------------------------------------------------

bool NavigationSystem::RegisterClass(const std::string & sClass, H<NavMeshAsset> hAsset) {

        if (!hAsset.IsValid()) {
                log_e("NavigationSystem: RegisterClass('{}') with null asset", sClass);
                return false;
        }

        const StrID key(sClass);

        // replace semantics: drop the old entry first (tiles removed, mesh freed)
        const auto itOld = mClassLookup.find(key);
        if (itOld != mClassLookup.end()) {
                SNavClass & c = vClasses[itOld->second];
                for (auto & [id, st] : c.mGroups) {
                        for (const dtTileRef ref : st.vGroupTileRefs) { RemoveTile(c.pMesh, ref); }
                        for (auto & [tile_idx, ref] : st.mTerrainTileRefs) { RemoveTile(c.pMesh, ref); }
                }
                dtFreeNavMeshQuery(c.pQuery);
                dtFreeNavMesh(c.pMesh);
                vClasses.erase(vClasses.begin() + itOld->second);
                mClassLookup.erase(itOld);
                for (auto & [k, v] : mClassLookup) { if (v > itOld->second) { --v; } }
        }

        NavMeshAsset * pAsset = GetResource(hAsset);
        if (!pAsset) {
                log_e("NavigationSystem: RegisterClass('{}') — asset handle does not resolve", sClass);
                return false;
        }

        SNavClass c;
        c.sName  = sClass;
        c.hAsset = hAsset;
        c.pAsset = pAsset;
        c.oFilter.setIncludeFlags(0xffff);
        c.oFilter.setExcludeFlags(0);
        // Bake area-byte encoding: 0 = null (never on polys), 1 = ground
        // (areas[0]), i + 1 = areas[i]. dtQueryFilter indexes costs by the raw
        // poly area byte, so registry index i lives in cost slot i + 1.
        for (uint32_t i = 0; i < uint32_t(pAsset->GetAreas().size()) && i < 63; ++i) {
                c.oFilter.setAreaCost(int(i) + 1, 1.0f);
        }

        // dtPolyRef budget: 32-bit refs need tileBits + polyBits <= 22 (salt >= 10)
        // or dtNavMesh::init rejects the params outright.
        auto Pow2Ceil = [](uint32_t v) { unsigned p = 1; while (p < v) { p <<= 1; } return p; };

        const unsigned maxTiles = std::max(Pow2Ceil(uint32_t(pAsset->GetTiles().size())), 64u);
        const unsigned maxPolys = std::max(Pow2Ceil(pAsset->MaxTilePolys()), 64u);
        const int tileBits = int(dtIlog2(dtNextPow2(maxTiles)));
        const int polyBits = int(dtIlog2(dtNextPow2(maxPolys)));

        if (tileBits + polyBits > 22) {
                log_e("NavigationSystem: class '{}' ref-bit budget exceeded "
                      "({} tiles -> {} bits + {} polys/tile -> {} bits = {}; max 22). "
                      "Class stays inert — increase tile_dim or enable DT_POLYREF64.",
                      sClass, pAsset->GetTiles().size(), tileBits,
                      pAsset->MaxTilePolys(), polyBits, tileBits + polyBits);
        } else {
                dtNavMeshParams params;
                memset(&params, 0, sizeof(params));
                params.orig[0]      = pAsset->Origin().x;
                params.orig[1]      = pAsset->Origin().y;
                params.orig[2]      = pAsset->Origin().z;
                params.tileWidth    = pAsset->TileDim();
                params.tileHeight   = pAsset->TileDim();
                params.maxTiles     = int(maxTiles);
                params.maxPolys     = int(maxPolys);

                c.pMesh = dtAllocNavMesh();
                const dtStatus stInit = c.pMesh ? c.pMesh->init(&params) : DT_FAILURE | DT_OUT_OF_MEMORY;
                if (!c.pMesh || dtStatusFailed(stInit)) {
                        log_e("NavigationSystem: dtNavMesh init failed for class '{}' ({:#x})",
                              sClass, static_cast<uint32_t>(stInit));
                        dtFreeNavMesh(c.pMesh);
                        c.pMesh = nullptr;
                } else {
                        c.pQuery = dtAllocNavMeshQuery();
                        const dtStatus stQuery = c.pQuery ? c.pQuery->init(c.pMesh, kMaxQueryNodes)
                                                                          : DT_FAILURE | DT_OUT_OF_MEMORY;
                        if (!c.pQuery || dtStatusFailed(stQuery)) {
                                log_e("NavigationSystem: dtNavMeshQuery init failed for class '{}'", sClass);
                                dtFreeNavMeshQuery(c.pQuery);
                                dtFreeNavMesh(c.pMesh);
                                c.pQuery = nullptr;
                                c.pMesh  = nullptr;
                        }
                }
        }

        if (!c.pMesh) {
                log_w("NavigationSystem: class '{}' registered but inert (queries fail cleanly)", sClass);
        }

        const uint32_t idx = uint32_t(vClasses.size());
        vClasses.push_back(std::move(c));
        mClassLookup.emplace(key, idx);
        ++mesh_generation;

        log_i("NavigationSystem: class '{}' registered over '{}' ({} tiles, {} groups, {} areas)",
              sClass, pAsset->Name(), pAsset->GetTiles().size(), pAsset->GetGroups().size(),
              pAsset->GetAreas().size());
        return true;
}

void NavigationSystem::UnregisterAll() {

        for (SNavClass & c : vClasses) {
                if (!c.pMesh) { continue; }
                for (auto & [id, st] : c.mGroups) {
                        for (const dtTileRef ref : st.vGroupTileRefs) { RemoveTile(c.pMesh, ref); }
                        for (auto & [tile_idx, ref] : st.mTerrainTileRefs) { RemoveTile(c.pMesh, ref); }
                }
                dtFreeNavMeshQuery(c.pQuery);
                dtFreeNavMesh(c.pMesh);
        }
        vClasses.clear();
        mClassLookup.clear();
        oStats.tiles_live = 0;
        ++mesh_generation;
}

// ---------------------------------------------------------------------------
// tile lifecycle
// ---------------------------------------------------------------------------

bool NavigationSystem::ActivateGroup(const StrID group) {

        bool bAny = false;

        for (SNavClass & c : vClasses) {
                const int gi = c.pAsset->GroupIndexID(group);
                if (gi < 0) { continue; }   // this class's bake has no such group

                SGroupState & st = c.mGroups[group];
                if (st.group_refs++ > 0) { bAny = true; continue; }   // idempotent

                if (c.pMesh) {
                        for (const uint32_t tile_idx : c.pAsset->GroupTiles(uint32_t(gi))) {
                                const NavMeshAsset::STile & t = c.pAsset->GetTiles()[tile_idx];
                                const dtTileRef ref = AddTileBytes(c.pMesh, t.pData, t.size);
                                if (ref) {
                                        st.vGroupTileRefs.push_back(ref);
                                        ++c.tiles_live;
                                        ++oStats.tiles_live;
                                        ++oStats.tiles_added;
                                }
                        }
                }
                bAny = true;
        }
        return bAny;
}

bool NavigationSystem::DeactivateGroup(const StrID group) {

        bool bAny = false;

        for (SNavClass & c : vClasses) {
                const auto itGroup = c.mGroups.find(group);
                if (itGroup == c.mGroups.end()) { continue; }

                SGroupState & st = itGroup->second;
                if (st.group_refs == 0) { continue; }   // idempotent

                if (--st.group_refs == 0) {
                        if (c.pMesh) {
                                for (const dtTileRef ref : st.vGroupTileRefs) {
                                        RemoveTile(c.pMesh, ref);
                                        --c.tiles_live;
                                        --oStats.tiles_live;
                                        ++oStats.tiles_removed;
                                }
                        }
                        st.vGroupTileRefs.clear();
                        // terrain refs of this group would have been drained by their
                        // own events; nothing else can hold tiles here
                }
                bAny = true;
        }
        return bAny;
}

bool NavigationSystem::ActivateTerrainTile(const StrID manifest,
                                           const glm::vec3 & vBoundsMin, const glm::vec3 & vBoundsMax) {

        bool bAny = false;

        for (SNavClass & c : vClasses) {
                if (!c.pMesh) { continue; }
                const int gi = c.pAsset->GroupIndexID(manifest);
                if (gi < 0) { continue; }   // component-merged terrain: rides its sub-level group

                SGroupState & st = c.mGroups[manifest];

                int32_t tx0 = 0, tx1 = 0, ty0 = 0, ty1 = 0;
                c.pAsset->RectForAABB(vBoundsMin, vBoundsMax, tx0, tx1, ty0, ty1);

                c.pAsset->ForEachTileInRect(uint32_t(gi), tx0, tx1, ty0, ty1, [&](const uint32_t tile_idx) {
                        uint32_t & rc = st.mTerrainRefs[tile_idx];
                        if (++rc > 1) { return; }   // another terrain tile already supports it

                        const NavMeshAsset::STile & t = c.pAsset->GetTiles()[tile_idx];
                        const dtTileRef ref = AddTileBytes(c.pMesh, t.pData, t.size);
                        if (ref) {
                                st.mTerrainTileRefs[tile_idx] = ref;
                                ++c.tiles_live;
                                ++oStats.tiles_live;
                                ++oStats.tiles_added;
                        } else {
                                --rc;   // allow a retry on the next supporter
                        }
                });
                bAny = true;
        }
        return bAny;
}

bool NavigationSystem::DeactivateTerrainTile(const StrID manifest,
                                             const glm::vec3 & vBoundsMin, const glm::vec3 & vBoundsMax) {

        bool bAny = false;

        for (SNavClass & c : vClasses) {
                if (!c.pMesh) { continue; }
                const int gi = c.pAsset->GroupIndexID(manifest);
                if (gi < 0) { continue; }

                const auto itGroup = c.mGroups.find(manifest);
                if (itGroup == c.mGroups.end()) { continue; }
                SGroupState & st = itGroup->second;

                int32_t tx0 = 0, tx1 = 0, ty0 = 0, ty1 = 0;
                c.pAsset->RectForAABB(vBoundsMin, vBoundsMax, tx0, tx1, ty0, ty1);

                c.pAsset->ForEachTileInRect(uint32_t(gi), tx0, tx1, ty0, ty1, [&](const uint32_t tile_idx) {
                        const auto itRef = st.mTerrainRefs.find(tile_idx);
                        if (itRef == st.mTerrainRefs.end() || itRef->second == 0) { return; }

                        if (--itRef->second == 0) {
                                const auto itLive = st.mTerrainTileRefs.find(tile_idx);
                                if (itLive != st.mTerrainTileRefs.end()) {
                                        RemoveTile(c.pMesh, itLive->second);
                                        st.mTerrainTileRefs.erase(itLive);
                                        --c.tiles_live;
                                        --oStats.tiles_live;
                                        ++oStats.tiles_removed;
                                }
                                st.mTerrainRefs.erase(itRef);
                        }
                });
                bAny = true;
        }
        return bAny;
}

bool NavigationSystem::HasGroup(const StrID group) const {
        for (const SNavClass & c : vClasses) {
                if (c.pAsset->GroupIndexID(group) >= 0) { return true; }
        }
        return false;
}

// ---------------------------------------------------------------------------
// queries
// ---------------------------------------------------------------------------

bool NavigationSystem::ConsumeBudget() {
        if (oConfig.max_queries_per_frame > 0 && oStats.queries >= oConfig.max_queries_per_frame) {
                ++oStats.budget_rejected;
                return false;
        }
        ++oStats.queries;
        return true;
}

int32_t NavigationSystem::ClassIndex(const StrID className) const {
        const auto it = mClassLookup.find(className);
        return it != mClassLookup.end() ? int32_t(it->second) : -1;
}

int32_t NavigationSystem::ResolveClass(const StrID className) {

        int32_t idx = ClassIndex(className);
        if (idx < 0 && defaultClassID != StrID{}) {
                idx = ClassIndex(defaultClassID);   // fall back to the world's default class
        }
        if (idx < 0) {
                static std::unordered_set<uint64_t> warned;
                if (warned.insert(uint64_t(className)).second) {
                        log_w("NavigationSystem: unknown agent class {} — queries fail "
                              "(is the class declared in the world's nav table?)",
                              uint64_t(className));
                }
        }
        return idx;
}

glm::vec3 NavigationSystem::ExtentsFor(const SNavClass & oClass, const SAgentRecord * pAgentRec) const {
        (void)oClass;
        const float r = (pAgentRec && pAgentRec->radius > 0.f) ? pAgentRec->radius : oConfig.query_radius;
        const float h = (pAgentRec && pAgentRec->height > 0.f) ? pAgentRec->height : oConfig.query_height;
        return {r, h, r};
}

bool NavigationSystem::FindNearestForClass(const int32_t class_idx, const glm::vec3 & vPos,
                                           const glm::vec3 & vExt, SNavPoint & out) const {
        const SNavClass & c = vClasses[size_t(class_idx)];
        if (!c.pMesh || !c.pQuery) { return false; }

        float center[3], nearest[3];
        CopyVec(center, vPos);
        float ext[3] = {vExt.x, vExt.y, vExt.z};

        dtPolyRef ref = 0;
        const dtStatus st = c.pQuery->findNearestPoly(center, ext, &c.oFilter, &ref, nearest);
        if (dtStatusFailed(st) || ref == 0) { return false; }

        out.pos = VecOf(nearest);
        out.ref = ref;
        return true;
}

bool NavigationSystem::FindNearest(const StrID className, const glm::vec3 & vPos,
                                   const glm::vec3 & vExt, SNavPoint & out) {
        if (!ConsumeBudget()) { return false; }
        const int32_t idx = ResolveClass(className);
        if (idx < 0) { return false; }
        return FindNearestForClass(idx, vPos, vExt, out);
}

bool NavigationSystem::FindPathForClass(const int32_t class_idx, const glm::vec3 & vFrom,
                                        const glm::vec3 & vTo, SNavPath & out, const bool bPartial,
                                        const float fRadius, const float fHeight) {

        out.vPoints.clear();
        out.vCornerPolys.clear();
        out.vCorridor.clear();
        out.partial = false;

        SNavClass & c = vClasses[size_t(class_idx)];
        if (!c.pMesh || !c.pQuery) { return false; }

        const glm::vec3 vExt = (fRadius > 0.f || fHeight > 0.f)
                ? glm::vec3(fRadius > 0.f ? fRadius : oConfig.query_radius,
                            fHeight > 0.f ? fHeight : oConfig.query_height,
                            fRadius > 0.f ? fRadius : oConfig.query_radius)
                : glm::vec3(oConfig.query_radius, oConfig.query_height, oConfig.query_radius);

        float sPt[3], ePt[3];
        dtPolyRef sRef = 0, eRef = 0;
        {
                float s[3], e[3], ext[3] = {vExt.x, vExt.y, vExt.z};
                CopyVec(s, vFrom);
                CopyVec(e, vTo);
                c.pQuery->findNearestPoly(s, ext, &c.oFilter, &sRef, sPt);
                c.pQuery->findNearestPoly(e, ext, &c.oFilter, &eRef, ePt);
        }
        if (sRef == 0 || eRef == 0) { return false; }

        dtPolyRef path[kMaxPathPolys];
        int nPath = 0;
        const dtStatus stPath = c.pQuery->findPath(sRef, eRef, sPt, ePt, &c.oFilter, path, &nPath, kMaxPathPolys);
        if (dtStatusFailed(stPath) || nPath == 0) { return false; }

        out.partial = dtStatusDetail(stPath, DT_PARTIAL_RESULT);
        if (out.partial && !bPartial) { return false; }

        // string-pulled waypoints; DT_BUFFER_TOO_SMALL just truncates the tail
        float straight[kMaxStraight * 3];
        unsigned char straightFlags[kMaxStraight];
        dtPolyRef straightRefs[kMaxStraight];
        int nStraight = 0;
        const dtStatus stStraight = c.pQuery->findStraightPath(sPt, ePt, path, nPath,
                                                               straight, straightFlags, straightRefs,
                                                               &nStraight, kMaxStraight);
        if (dtStatusFailed(stStraight) && !dtStatusDetail(stStraight, DT_BUFFER_TOO_SMALL)) {
                return false;
        }

        out.vCorridor.assign(path, path + nPath);
        out.vPoints.reserve(size_t(nStraight));
        out.vCornerPolys.reserve(size_t(nStraight));
        for (int i = 0; i < nStraight; ++i) {
                out.vPoints.emplace_back(straight[i * 3], straight[i * 3 + 1], straight[i * 3 + 2]);
                out.vCornerPolys.push_back(straightRefs[i]);
        }
        return true;
}

bool NavigationSystem::FindPath(const StrID className, const glm::vec3 & vFrom, const glm::vec3 & vTo,
                                SNavPath & out, const bool bPartial) {
        if (!ConsumeBudget()) { return false; }
        const int32_t idx = ResolveClass(className);
        if (idx < 0) { return false; }
        return FindPathForClass(idx, vFrom, vTo, out, bPartial, 0.f, 0.f);
}

bool NavigationSystem::ProjectToNavMesh(const StrID className, const glm::vec3 & vPos,
                                        const float fMaxDist, glm::vec3 & vOut) {
        if (!ConsumeBudget()) { return false; }
        const int32_t idx = ResolveClass(className);
        if (idx < 0) { return false; }

        SNavPoint oPoint;
        if (!FindNearestForClass(idx, vPos, glm::vec3(fMaxDist, fMaxDist, fMaxDist), oPoint)) {
                return false;
        }
        vOut = oPoint.pos;
        return true;
}

bool NavigationSystem::RandomPointAround(const StrID className, const glm::vec3 & vCenter,
                                         const float fRadius, glm::vec3 & vOut) {
        if (!ConsumeBudget()) { return false; }
        const int32_t idx = ResolveClass(className);
        if (idx < 0) { return false; }

        SNavClass & c = vClasses[size_t(idx)];
        if (!c.pMesh || !c.pQuery) { return false; }

        SNavPoint oCenter;
        if (!FindNearestForClass(idx, vCenter, glm::vec3(oConfig.query_radius, oConfig.query_height, oConfig.query_radius), oCenter)) {
                return false;
        }

        float center[3], out[3];
        CopyVec(center, oCenter.pos);
        dtPolyRef outRef = 0;
        const dtStatus st = c.pQuery->findRandomPointAroundCircle(oCenter.ref, center, fRadius,
                                                                  &c.oFilter, &NavFrand, &outRef, out);
        if (dtStatusFailed(st) || outRef == 0) { return false; }

        vOut = VecOf(out);
        return true;
}

bool NavigationSystem::SetAreaCost(const std::string & sClass, const std::string & sArea, const float fCost) {
        const int32_t idx = ClassIndex(StrID(sClass));
        if (idx < 0) { return false; }
        const int area = vClasses[size_t(idx)].pAsset->AreaIndex(sArea);
        if (area < 0) { return false; }
        // poly area byte = registry index + 1 (0 is the null area) — see RegisterClass
        vClasses[size_t(idx)].oFilter.setAreaCost(area + 1, fCost);
        return true;
}

bool NavigationSystem::SetIncludeFlags(const std::string & sClass, const uint16_t usFlags) {
        const int32_t idx = ClassIndex(StrID(sClass));
        if (idx < 0) { return false; }
        vClasses[size_t(idx)].oFilter.setIncludeFlags(usFlags);
        return true;
}

bool NavigationSystem::SetExcludeFlags(const std::string & sClass, const uint16_t usFlags) {
        const int32_t idx = ClassIndex(StrID(sClass));
        if (idx < 0) { return false; }
        vClasses[size_t(idx)].oFilter.setExcludeFlags(usFlags);
        return true;
}

// ---------------------------------------------------------------------------
// corridors + component registry
// ---------------------------------------------------------------------------

NavigationSystem::SAgentRecord * NavigationSystem::FindAgent(NavAgent * pAgent) {
        const auto it = mAgentLookup.find(pAgent);
        return it != mAgentLookup.end() ? &vAgents[it->second] : nullptr;
}

bool NavigationSystem::SetAgentPath(NavAgent * pAgent, const glm::vec3 & vFrom,
                                    const glm::vec3 & vTo, const bool bPartial) {
        SAgentRecord * pRec = FindAgent(pAgent);
        if (!pRec || !pRec->pCorridor) { return false; }
        if (!ConsumeBudget()) { return false; }

        int32_t idx = pRec->class_idx;
        if (idx < 0 || size_t(idx) >= vClasses.size()) {
                idx = ResolveClass(pAgent->AgentClassID());   // lazy re-bind after UnregisterAll
                pRec->class_idx = idx;
        }
        if (idx < 0) { return false; }

        SNavPath oPath;
        // findNearestPoly extents: the bare agent radius is too tight to reseed
        // a path once steering/collision has pushed the agent off the active
        // mesh (crowd squeezing, tile-group churn) — pad it; the query still
        // picks the closest poly, a wider box just tolerates being off-mesh.
        const float fExtXZ = glm::max(pRec->radius * 3.f, oConfig.query_radius);
        const float fExtY  = glm::max(pRec->height * 2.f, oConfig.query_height);
        if (!FindPathForClass(idx, vFrom, vTo, oPath, bPartial, fExtXZ, fExtY)) {
                return false;
        }

        float start[3], target[3];
        CopyVec(start, vFrom);
        CopyVec(target, vTo);

        pRec->pCorridor->reset(oPath.vCorridor.front(), start);
        pRec->pCorridor->setCorridor(target, oPath.vCorridor.data(), int(oPath.vCorridor.size()));
        pRec->vPathPoints = oPath.vPoints;
        return true;
}

bool NavigationSystem::MoveAgentAlong(NavAgent * pAgent, const glm::vec3 & vPos) {
        SAgentRecord * pRec = FindAgent(pAgent);
        if (!pRec || !pRec->pCorridor) { return false; }

        int32_t idx = pRec->class_idx;
        if (idx < 0 || size_t(idx) >= vClasses.size() || !vClasses[size_t(idx)].pQuery) { return false; }
        if (!ConsumeBudget()) { return false; }

        float pos[3];
        CopyVec(pos, vPos);
        return pRec->pCorridor->movePosition(pos, vClasses[size_t(idx)].pQuery, &vClasses[size_t(idx)].oFilter);
}

dtPathCorridor * NavigationSystem::GetAgentCorridor(NavAgent * pAgent) {
        SAgentRecord * pRec = FindAgent(pAgent);
        return pRec ? pRec->pCorridor : nullptr;
}

int NavigationSystem::GetAgentCorners(NavAgent * pAgent, glm::vec3 * pVerts, dtPolyRef * pPolys) {

        SAgentRecord * pRec = FindAgent(pAgent);
        if (!pRec || !pRec->pCorridor || !pVerts) { return 0; }
        if (pRec->pCorridor->getPathCount() == 0) { return 0; }   // empty corridor asserts in findCorners

        int32_t idx = pRec->class_idx;
        if (idx < 0 || size_t(idx) >= vClasses.size() || !vClasses[size_t(idx)].pQuery) {
                idx = ResolveClass(pAgent->AgentClassID());   // lazy re-bind after UnregisterAll
                pRec->class_idx = idx;
        }
        if (idx < 0) { return 0; }
        SNavClass & c = vClasses[size_t(idx)];
        if (!c.pQuery) { return 0; }

        float         verts[kMaxAgentCorners * 3];
        unsigned char flags[kMaxAgentCorners];
        dtPolyRef     polys[kMaxAgentCorners];
        const int n = pRec->pCorridor->findCorners(verts, flags, polys, kMaxAgentCorners, c.pQuery, &c.oFilter);

        for (int i = 0; i < n; ++i) {
                pVerts[i] = {verts[i * 3 + 0], verts[i * 3 + 1], verts[i * 3 + 2]};
        }
        if (pPolys) {
                for (int i = 0; i < n; ++i) { pPolys[i] = polys[i]; }
        }
        return n;
}

bool NavigationSystem::IsAgentPathValid(NavAgent * pAgent, const int32_t nLookAhead) {

        SAgentRecord * pRec = FindAgent(pAgent);
        if (!pRec || !pRec->pCorridor) { return false; }
        if (pRec->pCorridor->getPathCount() == 0) { return false; }   // no path yet => repath

        int32_t idx = pRec->class_idx;
        if (idx < 0 || size_t(idx) >= vClasses.size() || !vClasses[size_t(idx)].pQuery) {
                idx = ResolveClass(pAgent->AgentClassID());
                pRec->class_idx = idx;
        }
        if (idx < 0) { return false; }
        SNavClass & c = vClasses[size_t(idx)];
        if (!c.pQuery) { return false; }

        return pRec->pCorridor->isValid(nLookAhead, c.pQuery, &c.oFilter);
}

dtNavMesh * NavigationSystem::GetClassMesh(const StrID className) {
        const int32_t idx = ClassIndex(className);
        return idx >= 0 ? vClasses[size_t(idx)].pMesh : nullptr;
}

const dtQueryFilter * NavigationSystem::GetClassFilter(const StrID className) {
        const int32_t idx = ClassIndex(className);
        return idx >= 0 ? &vClasses[size_t(idx)].oFilter : nullptr;
}

void NavigationSystem::AddComponent(NavAgent * pAgent) {

        if (!pAgent || mAgentLookup.count(pAgent) > 0) { return; }

        SAgentRecord rec;
        rec.pAgent    = pAgent;
        rec.class_idx = ClassIndex(pAgent->AgentClassID());   // may be -1 (class arrives later)
        rec.radius    = pAgent->Radius();
        rec.height    = pAgent->Height();
        rec.pCorridor = AllocCorridor();
        if (rec.pCorridor) { rec.pCorridor->init(kMaxPathPolys); }
        else { log_e("NavigationSystem: corridor alloc failed"); }

        mAgentLookup.emplace(pAgent, uint32_t(vAgents.size()));
        vAgents.push_back(rec);
        oStats.agents = uint32_t(vAgents.size());
}

void NavigationSystem::RemoveComponent(NavAgent * pAgent) {

        const auto it = mAgentLookup.find(pAgent);
        if (it == mAgentLookup.end()) { return; }

        const uint32_t idx = it->second;
        FreeCorridor(vAgents[idx].pCorridor);

        vAgents[idx] = std::move(vAgents.back());
        mAgentLookup[vAgents[idx].pAgent] = idx;
        vAgents.pop_back();
        mAgentLookup.erase(it);
        oStats.agents = uint32_t(vAgents.size());
}

// ---------------------------------------------------------------------------
// per-frame
// ---------------------------------------------------------------------------

void NavigationSystem::OnUpdate(const Event &) {
        if (oDebug.enabled) { DrawDebug(); }
}

void NavigationSystem::OnPostUpdate(const Event &) {
        oStats.queries         = 0;
        oStats.budget_rejected = 0;
        oStats.tiles_added     = 0;
        oStats.tiles_removed   = 0;
}

void NavigationSystem::GetDebugAgents(std::vector<SAgentDebug> & vOut) const {

        vOut.clear();
        vOut.reserve(vAgents.size());

        for (const SAgentRecord & rec : vAgents) {
                SAgentDebug dbg;
                if (rec.pAgent) {
                        dbg.sName = rec.pAgent->Name();
                        dbg.sClass = rec.pAgent->AgentClass();
                }
                if (rec.class_idx >= 0 && size_t(rec.class_idx) < vClasses.size()) {
                        dbg.sClass = vClasses[size_t(rec.class_idx)].sName;
                }
                if (rec.pCorridor) {
                        const float * pPos = rec.pCorridor->getPos();
                        dbg.pos = {pPos[0], pPos[1], pPos[2]};
                        dbg.has_path = rec.pCorridor->getPathCount() > 0;
                }
                dbg.path_points = rec.vPathPoints.size();
                vOut.push_back(std::move(dbg));
        }
}

// ---------------------------------------------------------------------------
// debug draw (per frame, immediate mode — DebugRenderer flushes at EPostUpdate)
// ---------------------------------------------------------------------------

namespace {

glm::vec4 AreaColor(const unsigned char area) {
        static const glm::vec4 palette[] = {
                        {0.25f, 0.85f, 0.35f, 1.f},   // 0/1 ground: green
                        {0.25f, 0.55f, 0.95f, 1.f},   // 2: blue
                        {0.95f, 0.75f, 0.25f, 1.f},   // 3: yellow
                        {0.90f, 0.35f, 0.85f, 1.f},   // 4: magenta
                        {0.35f, 0.90f, 0.90f, 1.f},   // 5: cyan
        };
        return palette[size_t(area) % (sizeof(palette) / sizeof(palette[0]))];
}

} // namespace

void NavigationSystem::DrawDebug() {

        DebugRenderer & dr = GetSystem<DebugRenderer>();
        const glm::vec4 oTileColor(0.15f, 0.45f, 0.75f, 1.f);
        const glm::vec4 oLinkColor(1.f, 0.6f, 0.1f, 1.f);
        const glm::vec4 oPathColor(1.f, 0.25f, 0.25f, 1.f);

        for (const SNavClass & c : vClasses) {
                if (!c.pMesh) { continue; }

                // active tiles: boxes + (optionally) poly outlines, color by area
                for (const auto & [id, st] : c.mGroups) {
                        if (oDebug.tiles) {
                                for (const dtTileRef ref : st.vGroupTileRefs) {
                                        const dtMeshTile * pTile = c.pMesh->getTileByRef(ref);
                                        if (!pTile) { continue; }
                                        const dtMeshHeader & h = *pTile->header;
                                        dr.DrawBBox(BoundingBox(glm::vec3(h.bmin[0], h.bmin[1], h.bmin[2]),
                                                                glm::vec3(h.bmax[0], h.bmax[1], h.bmax[2])),
                                                    Transform(), oTileColor);
                                }
                                for (const auto & [tile_idx, ref] : st.mTerrainTileRefs) {
                                        const dtMeshTile * pTile = c.pMesh->getTileByRef(ref);
                                        if (!pTile) { continue; }
                                        const dtMeshHeader & h = *pTile->header;
                                        dr.DrawBBox(BoundingBox(glm::vec3(h.bmin[0], h.bmin[1], h.bmin[2]),
                                                                glm::vec3(h.bmax[0], h.bmax[1], h.bmax[2])),
                                                    Transform(), oTileColor);
                                }
                        }
                        if (oDebug.polys) {
                                // poly outlines for every live tile of the group —
                                // terrain-merged worlds keep their tiles in vGroupTileRefs
                                auto DrawTilePolys = [&dr](const dtMeshTile * pTile) {
                                        if (!pTile) { return; }
                                        for (int i = 0; i < pTile->header->polyCount; ++i) {
                                                const dtPoly & poly = pTile->polys[i];
                                                if (poly.getType() == DT_POLYTYPE_OFFMESH_CONNECTION) { continue; }
                                                const glm::vec4 col = AreaColor(poly.getArea());
                                                for (int j = 0; j < poly.vertCount; ++j) {
                                                        const float * a = &pTile->verts[poly.verts[j] * 3];
                                                        const float * b = &pTile->verts[poly.verts[(j + 1) % poly.vertCount] * 3];
                                                        dr.DrawLine(glm::vec3(a[0], a[1], a[2]),
                                                                    glm::vec3(b[0], b[1], b[2]), col);
                                                }
                                        }
                                };
                                for (const dtTileRef ref : st.vGroupTileRefs) {
                                        DrawTilePolys(c.pMesh->getTileByRef(ref));
                                }
                                for (const auto & [tile_idx, ref] : st.mTerrainTileRefs) {
                                        DrawTilePolys(c.pMesh->getTileByRef(ref));
                                }
                        }
                        if (oDebug.markers) {
                                const int gi = c.pAsset->GroupIndexID(id);
                                if (gi >= 0) {
                                        for (const NavMeshAsset::SLink & l : c.pAsset->GetLinks()) {
                                                if (l.group_idx != uint32_t(gi)) { continue; }
                                                dr.DrawSphere(l.start, l.radius, oLinkColor);
                                                dr.DrawSphere(l.end, l.radius, oLinkColor);
                                                dr.DrawLine(l.start, l.end, oLinkColor);
                                        }
                                }
                        }
                }
        }

        if (oDebug.paths) {
                for (const SAgentRecord & rec : vAgents) {
                        for (size_t i = 0; i + 1 < rec.vPathPoints.size(); ++i) {
                                dr.DrawLine(rec.vPathPoints[i], rec.vPathPoints[i + 1], oPathColor);
                        }
                }
        }
}

} // namespace SE

#endif // SE_IMPL
