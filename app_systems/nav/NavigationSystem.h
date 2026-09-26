
#ifndef APP_NAV_NAVIGATION_SYSTEM_H
#define APP_NAV_NAVIGATION_SYSTEM_H 1

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>
#include <DetourPathCorridor.h>

#include <StrID.h>
#include <ResourceHandle.h>
#include <nav/NavMeshAsset.h>

namespace SE {

class NavAgent;
class Event;

// ---------------------------------------------------------------------------
// NavigationSystem — the world-agnostic nav runtime.
//
// Owns one dtNavMesh + dtNavMeshQuery + dtQueryFilter per registered agent
// class (one .senm each), the active tile sets (per content-group id), and the
// per-agent path corridors. Queries are synchronous, allocation-free and
// budget-guarded; missing tiles / unknown classes degrade to clean failures.
//
// World knowledge lives in WorldNavBridge: this system never reads WorldAsset
// and subscribes no world events — nav meshes ENTER ONLY through RegisterClass
// and tiles appear only through the explicit group/terrain activation API.
// Compose it after WorldSystem so same-frame activations are consistent, and
// pair it with WorldNavBridge for world-driven lifecycle.
//
// Ref-addressing contract: Detour 32-bit dtPolyRef needs
// tileBits + polyBits <= 22 (salt >= 10) or dtNavMesh::init fails. Caps come
// from the asset (pow2ceil of tile count / max per-tile polys); violations log
// once and leave the class inert (queries return false). DT_POLYREF64 is the
// escape hatch for very large tile counts — not enabled currently.
//
// Ownership: addTile hands Detour dtAlloc'd copies flagged DT_TILE_FREE_DATA
// (Detour frees on removeTile); corridors are dtAlloc'd per agent. Agent
// records survive UnregisterAll (their class index is re-validated lazily and
// queries fail cleanly until the class returns) — components re-bind through
// Enable()/Disable().
// ---------------------------------------------------------------------------

struct SNavPoint {
        glm::vec3 pos{};
        dtPolyRef ref = 0;
};

// Caller-owned, reusable across calls (no per-call allocation).
struct SNavPath {
        std::vector<glm::vec3> vPoints;       // string-pulled waypoints (start..end)
        std::vector<dtPolyRef> vCornerPolys;  // poly entered after each waypoint
        std::vector<dtPolyRef> vCorridor;     // findPath corridor (dtPathCorridor seed)
        bool                   partial = false;
};

class NavigationSystem {
public:
        static constexpr int kMaxPathPolys  = 256;   // findPath / corridor capacity
        static constexpr int kMaxStraight   = 64;    // findStraightPath capacity
        static constexpr int kMaxQueryNodes = 4096;  // dtNavMeshQuery node pool

        struct SConfig {
                uint32_t max_queries_per_frame = 0;   // 0 = uncapped (pathological-world guard)
                float    query_radius  = 1.0f;        // default findNearestPoly half-extents
                float    query_height  = 2.0f;
        };

        struct SDebug {
                bool enabled = false;
                bool tiles   = false;
                bool polys   = false;
                bool paths   = false;
                bool markers = false;   // off-mesh link records of active groups
        };

        struct SStats {
                // per frame (reset at EPostUpdate)
                uint32_t queries         = 0;
                uint32_t budget_rejected = 0;
                uint32_t tiles_added     = 0;
                uint32_t tiles_removed   = 0;
                // live
                uint32_t tiles_live      = 0;
                uint32_t agents          = 0;
        };

        // Debugger-facing agent snapshot (NavigationDebugger panel)
        struct SAgentDebug {
                std::string sName;
                std::string sClass;
                glm::vec3   pos{};
                bool        has_path    = false;
                size_t      path_points = 0;
        };

        NavigationSystem();
        ~NavigationSystem() noexcept;

        void SetConfig(const SConfig & oCfg) { oConfig = oCfg; }
        void SetDebug(const SDebug & oDbg)   { oDebug = oDbg; }

        // --- registration (the only way nav meshes enter) -------------------

        /// Registers one agent class over a loaded NavMeshAsset and builds its
        /// dtNavMesh/query/filter. Re-registering a class replaces it. Returns
        /// false (class stays absent) only on a null handle.
        bool RegisterClass(const std::string & sClass, H<NavMeshAsset> hAsset);
        /// Monotonic counter — bumped on RegisterClass/UnregisterAll (see mesh_generation).
        uint64_t            MeshGeneration() const { return mesh_generation; }
        void SetDefaultClass(const std::string & sClass) { sDefaultClass = sClass; defaultClassID = StrID(sClass); }
        const std::string & DefaultClass() const { return sDefaultClass; }
        StrID               DefaultClassID() const { return defaultClassID; }
        /// Removes every class: removes all its tiles (Detour frees the tile
        /// buffers), frees the meshes/queries. Idempotent.
        void UnregisterAll();

        // --- tile lifecycle (group ids are the bake's content-group ids) ----

        /// Adds the group's tiles in every class whose asset carries the group
        /// (one .senm per class; sub-level paths are shared across classes).
        /// Idempotent per group (nesting count). No-op when no asset knows it.
        bool ActivateGroup(StrID group);
        /// Removes the group's tiles when the last nesting releases. Idempotent.
        bool DeactivateGroup(StrID group);
        /// Terrain paging: refcounts every nav tile of the manifest group whose
        /// (x, y) is covered by the terrain tile's world XZ AABB; a nav tile
        /// straddling terrain tiles stays until its last supporter unloads.
        bool ActivateTerrainTile(StrID manifest, const glm::vec3 & vBoundsMin, const glm::vec3 & vBoundsMax);
        bool DeactivateTerrainTile(StrID manifest, const glm::vec3 & vBoundsMin, const glm::vec3 & vBoundsMax);
        /// True when any registered class's asset carries the group (bake/sync
        /// diagnostics; WorldNavBridge::WarnIfUnnavigable).
        bool HasGroup(StrID group) const;

        // --- queries (agent class resolved per call; false = no nav/unreachable/
        //     over-budget/unknown class — never throws) ------------------------

        int32_t ClassIndex(StrID className) const;   // -1 when unknown
        bool    FindNearest(StrID className, const glm::vec3 & vPos, const glm::vec3 & vExt, SNavPoint & out);
        bool    FindPath(StrID className, const glm::vec3 & vFrom, const glm::vec3 & vTo,
                         SNavPath & out, bool bPartial = false);
        bool    ProjectToNavMesh(StrID className, const glm::vec3 & vPos, float fMaxDist, glm::vec3 & vOut);
        bool    RandomPointAround(StrID className, const glm::vec3 & vCenter, float fRadius, glm::vec3 & vOut);
        bool    SetAreaCost(const std::string & sClass, const std::string & sArea, float fCost);
        bool    SetIncludeFlags(const std::string & sClass, uint16_t usFlags);
        bool    SetExcludeFlags(const std::string & sClass, uint16_t usFlags);
        /// Deterministic LCG seed for RandomPointAround (replays/tests).
        void SetRandomSeed(uint64_t seed);

        // --- corridor primitives (PathFollower consumes) -------------

        /// findNearest + findPath for the agent's class, seeds the agent's
        /// corridor and caches the straight-path corners.
        bool SetAgentPath(NavAgent * pAgent, const glm::vec3 & vFrom, const glm::vec3 & vTo, bool bPartial = false);
        /// Moves the corridor position along the surface (movePosition).
        bool MoveAgentAlong(NavAgent * pAgent, const glm::vec3 & vPos);
        dtPathCorridor * GetAgentCorridor(NavAgent * pAgent);

        // --- corridor upkeep primitives (CrowdSystem consumes) -------

        static constexpr int kMaxAgentCorners = 4;   // dtCrowd's DT_CROWDAGENT_MAX_CORNERS

        /// String-pulled corners ahead on the agent's corridor (up to
        /// kMaxAgentCorners). 0 when the agent has no corridor or its class is
        /// unknown. Not budget-guarded — follower upkeep, not a user query.
        int  GetAgentCorners(NavAgent * pAgent, glm::vec3 * pVerts, dtPolyRef * pPolys);
        /// True while the agent's corridor is still walkable on the active
        /// tiles for `nLookAhead` polys ahead; false => repath.
        bool IsAgentPathValid(NavAgent * pAgent, int32_t nLookAhead = 8);
        /// Raw class accessors for the local-avoidance backend (dtCrowd binds
        /// one mesh + copies one filter; the filter already carries the bake's
        /// +1 area-cost slot mapping). nullptr when the class is unknown.
        dtNavMesh *           GetClassMesh(StrID className);
        const dtQueryFilter * GetClassFilter(StrID className);

        // --- component registry (NavAgent::Enable/Disable) ------------------

        void AddComponent(NavAgent * pAgent);
        void RemoveComponent(NavAgent * pAgent);

        const SStats & GetStats() const { return oStats; }
        SDebug &       DebugConfig() { return oDebug; }
        void           GetDebugAgents(std::vector<SAgentDebug> & vOut) const;

private:
        struct SGroupState {
                uint32_t group_refs = 0;                     // ActivateGroup nesting
                std::vector<dtTileRef> vGroupTileRefs;       // refs added by group activation
                std::unordered_map<uint32_t, uint32_t> mTerrainRefs;      // tile_idx -> supporters
                std::unordered_map<uint32_t, dtTileRef> mTerrainTileRefs; // tile_idx -> live ref
        };

        struct SNavClass {
                std::string     sName;
                H<NavMeshAsset> hAsset;
                NavMeshAsset *  pAsset = nullptr;
                dtNavMesh *     pMesh  = nullptr;    // null => inert class (build failed / caps)
                dtNavMeshQuery * pQuery = nullptr;
                dtQueryFilter   oFilter;
                std::unordered_map<StrID, SGroupState> mGroups;
                uint32_t tiles_live = 0;
        };

        struct SAgentRecord {
                NavAgent *       pAgent    = nullptr;
                int32_t          class_idx = -1;     // re-validated lazily
                float            radius    = 0.f;   // 0 = config default
                float            height    = 0.f;
                dtPathCorridor * pCorridor = nullptr;
                std::vector<glm::vec3> vPathPoints;  // last straight path (debug draw)
        };

        NavigationSystem(const NavigationSystem &) = delete;
        NavigationSystem & operator = (const NavigationSystem &) = delete;

        void OnUpdate(const Event & oEvent);      // debug draw
        void OnPostUpdate(const Event & oEvent);  // per-frame stat reset

        bool  ConsumeBudget();
        int32_t ResolveClass(StrID className);    // logs once on unknown
        glm::vec3 ExtentsFor(const SNavClass & oClass, const SAgentRecord * pAgentRec) const;
        bool  FindPathForClass(int32_t class_idx, const glm::vec3 & vFrom, const glm::vec3 & vTo,
                               SNavPath & out, bool bPartial, float fRadius, float fHeight);
        bool  FindNearestForClass(int32_t class_idx, const glm::vec3 & vPos, const glm::vec3 & vExt,
                                  SNavPoint & out) const;
        SAgentRecord * FindAgent(NavAgent * pAgent);
        void DrawDebug();

        SConfig oConfig;
        SDebug  oDebug;
        SStats  oStats;

        // Bumped whenever a class mesh is (re)created or torn down — consumers
        // that cache dtNavMesh pointers (CrowdSystem roster) revalidate on it
        // instead of re-hashing the class registry per agent per tick.
        uint64_t mesh_generation = 0;

        std::vector<SNavClass> vClasses;
        std::unordered_map<StrID, uint32_t> mClassLookup;
        std::string sDefaultClass;
        StrID       defaultClassID;

        std::vector<SAgentRecord> vAgents;
        std::unordered_map<NavAgent *, uint32_t> mAgentLookup;
};

} // namespace SE

#endif
