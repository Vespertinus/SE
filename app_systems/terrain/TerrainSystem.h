
#ifndef APP_TERRAIN_SYSTEM_H
#define APP_TERRAIN_SYSTEM_H 1

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include <StrID.h>
#include <ResourceHandle.h>
#include <world/StreamingDirector.h>
#include <terrain/TileData.h>
#include <terrain/TerrainAsset.h>

#ifdef SE_PHYSICS_ENABLED
#include <PhysicsTypes.h>
#endif

namespace SE {

class WorldSystem;
class Terrain;   // linked renderable (stats only); complete type in Terrain.h

// ---------------------------------------------------------------------------
// Posted per tile activation/deactivation of every
// registration (synchronously, on the main thread, from FinishTile/ReleaseTile
// — so both the paging unload and Detach/world-unload paths announce). The
// navigation runtime pages terrain-grid nav tiles with these; the manifest
// StrID is the .seterrain resource path, which is also the nav bake's terrain
// group id. Listeners observe the tile after its physics body + GPU sinks are
// up (activation) / after teardown completed (deactivation).
// ---------------------------------------------------------------------------
struct ETerrainTileActivated {
        StrID     manifest;
        uint32_t  tile_index = 0;
        uint16_t  x = 0, y = 0;
        glm::vec3 bounds_min{};
        glm::vec3 bounds_max{};
};

struct ETerrainTileDeactivated {
        StrID     manifest;
        uint32_t  tile_index = 0;
        uint16_t  x = 0, y = 0;
        glm::vec3 bounds_min{};
        glm::vec3 bounds_max{};
};

// ---------------------------------------------------------------------------
// TerrainSystem — baked heightfield tiles, paged.
//
// Terrain is NOT scene nodes: this system owns a tile table (one record per
// tile of every registered terrain manifest), the paging state machine, the
// Jolt HeightFieldShape per active tile, and the height/normal queries. The
// render side (Terrain component) registers GPU callbacks and draws selected
// patches through the standard RenderCommand pipeline.
//
// Two consumption modes, one tile lifecycle:
//   - paged ("grid"/flight-sim): tile wantedness comes from focus distances via
//     the StreamingDirector planner (hysteresis, budgets — same policy as
//     sub-levels). File reads dispatch through WorldSystem's shared worker
//     (content-kind tagged — the stage's one-worker-thread rule).
//   - component-mode (RPG): tiles are wanted while the owning sub-level is
//     active (the Terrain component links/unlinks on Enable/Disable).
//
// Requires WorldSystem in the app composition (focus registry + worker).
// Physics is optional at compile time (SE_PHYSICS_ENABLED): queries and paging
// work without bodies.
// ---------------------------------------------------------------------------

class TerrainSystem {
public:
        using TTileRuntimePtr = std::shared_ptr<const STileRuntime>;

        struct SConfig {
                float    load_radius   = 768.0f;   // paged mode, overridden per-focus
                float    unload_radius = 1024.0f;  // > load_radius => hysteresis band
                uint32_t unload_frame_delay = 60;
                uint32_t max_tile_activations_per_frame = 2;
                uint32_t max_in_flight_reads = 8;
                float    main_budget_ms = 2.0f;    // wall-clock budget for tile builds
                uint32_t max_active_tiles = 256;   // debug ceiling (GPU/RAM/body bound)
                float    max_slope_deg_default = 50.0f; // unused here; manifest carries it
        };

        // GPU sink installed by the render-side Terrain component for its registration;
        // unset in headless apps (tests). Called on the main thread during tile
        // build/teardown. Sinks are per-registration (several Terrain components may coexist).
        using TGpuReadyFn   = std::function<void(uint32_t tile_index, const STileRuntime &)>;
        using TGpuReleaseFn = std::function<void(uint32_t tile_index)>;

        struct SStats {
                uint32_t registrations  = 0;
                uint32_t active_tiles   = 0;
                uint32_t requested      = 0;
                uint32_t loading        = 0;
                uint32_t in_flight_reads = 0;
                uint32_t visible_cmds   = 0;   // commands emitted by the last gather (linked renderables)
                uint64_t visible_tris   = 0;   // triangles those commands draw
                float    budget_spent_ms = 0.0f;
        };

        TerrainSystem();
        ~TerrainSystem() noexcept;

        /// Grid registration: world-level terrain paged by focus distance. Dedups by
        /// manifest path. Returns 0 on failure.
        uint64_t AttachGrid(H<TerrainAsset> hAsset);
        /// Component registration: tiles follow the linked-component count. bPaged
        /// = true additionally distance-pages a (large) component terrain.
        uint64_t AttachTerrain(H<TerrainAsset> hAsset, bool bPaged);
        void     LinkComponent(uint64_t reg_id, Terrain * pRenderable = nullptr);
        void     UnlinkComponent(uint64_t reg_id);
        void     Detach(uint64_t reg_id);
        void     DetachAll();

        /// Install (or clear, with null fns) the GPU sink for one registration.
        void SetGpuSink(uint64_t reg_id, TGpuReadyFn fnReady, TGpuReleaseFn fnRelease);

        /// Bilinear height over LOD0 at vPos (world XZ). Active tiles only —
        /// the tile under any live focus point is always active (paging radius).
        bool TryGetHeight(const glm::vec3 & vPos, float & out_height) const;
        /// Nearest baked LOD0 normal at vPos.
        bool TryGetNormal(const glm::vec3 & vPos, glm::vec3 & out_normal) const;

        SStats  GetStats() const;
        SConfig Config() const { return oConfig; }
        void    SetConfig(const SConfig & oCfg) { oConfig = oCfg; }

        /// True when reg_id names a live registration whose manifest is hAsset.
        bool IsRegistrationActive(uint64_t reg_id) const { return FindReg(reg_id) != nullptr; }
        uint32_t GetActiveTileCount() const { return uint32_t(vActiveTiles.size()); }
        /// First linked render-side Terrain component, if any (debugger seam).
        Terrain * GetRenderable() const;

        /// Debugger surface: one record per tile of every live registration.
        struct STileDebug {
                uint16_t x = 0, y = 0;
                uint8_t  status = 0;   // ETileStatus
                bool     b_paged = false;
        };
        void GetDebugTiles(std::vector<STileDebug> & vOut) const;

private:
        // Tile-side status: Requested (read in flight) and Loading (bytes staged,
        // awaiting the budgeted build) both map to the planner's "Loading".
        enum class ETileStatus : uint8_t { Unloaded = 0, Requested, Loading, Active };

        struct STileSlot {
                ETileStatus status = ETileStatus::Unloaded;
                uint32_t    ticket = 0;
                TTileRuntimePtr pRuntime;   // set while Loading (awaiting build)
        };

        struct SRegistration {
                uint64_t        id = 0;               // (generation << 16) | slot+1; 0 = free
                H<TerrainAsset> hAsset;
                std::string     sTileDir;             // manifest dir prefix for tile paths
                bool            b_paged = false;      // focus-distance paging
                bool            b_from_world = false; // auto-attached from .seworld terrain_grid
                uint32_t        linked_components = 0;
                Terrain *       pRenderable = nullptr;   // first linked component (stats)
                uint32_t        ticket = 0;
                std::vector<STileSlot> vTiles;        // parallel to asset->GetTiles()
                std::unique_ptr<StreamingDirector> pPlanner;   // paged only
                std::vector<StreamingDirector::SLevelSnap>   vSnapBounds;   // per-tile (reused)
                std::vector<StreamingDirector::ELevelStatus> vSnapStatuses; // per-tile (reused)
                std::vector<StreamingDirector::SFocus>       vSnapFocuses;  // (reused)
        };

        struct SGpuSink {
                uint64_t       reg_id = 0;
                TGpuReadyFn    fnReady;
                TGpuReleaseFn  fnRelease;
        };

        struct SActiveTile {
                uint32_t        reg_slot = 0;
                uint32_t        tile_index = 0;
                TTileRuntimePtr pRuntime;
#ifdef SE_PHYSICS_ENABLED
                BodyHandle      hBody;
#endif
        };

        TerrainSystem(const TerrainSystem &) = delete;
        TerrainSystem & operator = (const TerrainSystem &) = delete;

        void OnUpdate(const Event & oEvent);
        void OnWorldLoaded(const Event & oEvent);
        void OnWorldUnloaded(const Event & oEvent);
        void OnTileResult(const StreamingDirector::SReadResult & res);

        SRegistration *     FindReg(uint64_t reg_id);
        const SRegistration * FindReg(uint64_t reg_id) const;
        uint64_t AttachImpl(H<TerrainAsset> hAsset, bool bPaged, bool bFromWorld);
        void     TickRegistration(SRegistration & reg, uint32_t in_flight_budget, bool bAdvanceTimers);
        void     TickComponentWanted(SRegistration & reg);
        bool     FinishTile(SRegistration & reg, uint32_t tile_index);
        void     ReleaseTile(SRegistration & reg, uint32_t tile_index);
        void     CancelTile(SRegistration & reg, STileSlot & slot);

        SConfig                    oConfig;
        std::vector<SRegistration> vRegistrations;   // slots; id==0 marks free
        std::vector<uint16_t>      vFreeSlots;

        struct SActiveKey {
                uint32_t reg_slot;
                uint32_t tile_index;
                bool operator == (const SActiveKey & o) const { return reg_slot == o.reg_slot && tile_index == o.tile_index; }
        };
        struct SActiveKeyHash {
                size_t operator () (const SActiveKey & k) const noexcept {
                        return (static_cast<size_t>(k.reg_slot) << 32) ^ k.tile_index;
                }
        };
        std::vector<SActiveTile>    vActiveTiles;
        std::unordered_map<SActiveKey, uint32_t, SActiveKeyHash> mActiveLookup;

        std::vector<SGpuSink>       vGpuSinks;

        WorldSystem *               pWorld = nullptr;   // composition requirement (compile-time)
        std::vector<float>          vScratchSamples;    // LOD0 dequantization scratch (one tile)
        uint32_t                    serial = 1;         // stale-result guard, bumped on structure change
        float                       last_budget_spent_ms = 0.0f;
        std::vector<std::pair<float, uint32_t>> vBuildOrder;   // (distance, packed slot/tile)
        bool                        bShuttingDown = false;   // suppress tile events during dtor teardown
};

} // namespace SE

#endif
