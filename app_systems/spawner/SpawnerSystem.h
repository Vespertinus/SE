
#ifndef APP_SPAWNER_SPAWNER_SYSTEM_H
#define APP_SPAWNER_SPAWNER_SYSTEM_H 1

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <StrID.h>
#include <Spawner_generated.h>
#include <PhysicsTypes.h>   // PhysicsRay/RaycastHit for the probe signature only

namespace SE {

class Population;
class SpawnPoint;
class WorldSystem;
class SpatialIndexSystem;
class BlackboardCore;
class Event;

// ---------------------------------------------------------------------------
// SpawnerSystem — driver for Population/SpawnPoint components.
//
// Composition: hard-depends on WorldSystem + EntityManager (compose after both).
// Optional providers are setter seams — GetSystem<T> is a hard compile-time
// dependency, so presence must be opt-in, and terrain headers drag resource-pool
// registrations in, so the snap probes are plain functions the app wires from
// its scene setup (where the systems are complete types):
//
//   SetIndex(world::SpatialIndexSystem*)     — enables TagNear conditions
//   SetTerrainProbe(fn(pos, out_height))     — enables snap = TerrainHeight
//   SetPhysicsProbe(fn(ray, out_hit))        — enables snap = PhysicsRay
//
// Apps composing this system must include SpawnPoint/SpawnVolume/Population in
// TCustomComponents (geometry lookup needs the complete component types).
//
// Pass shape (EUpdate, AppClock-scaled): staggered population ticks (phase from
// id hash) → condition gate → despawn sweep → spawn commands → shared per-frame
// budget executed round-robin in population registration order → SpawnTemplate.
// Deterministic: same inputs ⇒ same population, run to run.
// ---------------------------------------------------------------------------

class SpawnerSystem {

        friend class SpawnerDebugger;
public:
        struct SConfig {
                uint32_t max_spawns_per_frame = 8;
        };

        using TTerrainProbe = std::function<bool(const glm::vec3 &, float &)>;
        using TPhysicsProbe = std::function<bool(const PhysicsRay &, RaycastHit &)>;
        /// navmesh snap — project vPos onto the navmesh
        /// (NavigationSystem::ProjectToNavMesh with the world's default class);
        /// vOut receives the snapped point.
        using TNavProbe     = std::function<bool(const glm::vec3 & vPos, glm::vec3 & vOut)>;

        struct SStats {
                uint32_t populations        = 0;
                uint32_t spawn_points       = 0;
                uint32_t spawns_executed    = 0;
                uint32_t spawns_deferred    = 0;   // budget leftovers (re-emitted next tick)
                uint32_t despawns           = 0;
                uint32_t world_slots_total    = 0; // .seworld spawn population slots
                uint32_t world_slots_resolved = 0;
        };

        SpawnerSystem();
        ~SpawnerSystem() noexcept;

        // --- optional providers + config (compose-time wiring) ---
        void SetIndex(SpatialIndexSystem * p)            { pIndex = p; }
        void SetTerrainProbe(TTerrainProbe fn)           { fnTerrainProbe = std::move(fn); }
        void SetPhysicsProbe(TPhysicsProbe fn)           { fnPhysicsProbe = std::move(fn); }
        void SetNavProbe(TNavProbe fn)                   { fnNavProbe = std::move(fn); }
        void SetConfig(const SConfig & oConfigIn)        { oConfig = oConfigIn; }

        SpatialIndexSystem * GetIndex() const { return pIndex; }

        /// Global blackboard via WorldSystem; null without BlackboardSystem.
        const BlackboardCore * GetBlackboard() const;

        /// Seed for deterministic population PRNGs (0 until a world loads).
        uint64_t WorldSeed() const;

        // --- component registries (components call from Enable()/Disable()) ---
        void AddComponent(Population * p);
        void RemoveComponent(Population * p);
        void AddComponent(SpawnPoint * p);
        void RemoveComponent(SpawnPoint * p);

        const SStats & GetStats() const { return oStats; }

private:
        void OnUpdate(const Event & oEvent);
        void OnEntityDestroyed(const Event & oEvent);
        void OnTriggerFired(const Event & oEvent);
        void OnWorldLoaded(const Event & oEvent);
        void OnWorldUnloaded(const Event & oEvent);

        void TickPass(float dt);
        bool TryClaimWorldSlot(Population * pPop);
        bool SnapPosition(FlatBuffers::SpawnSnap eMode, glm::vec3 & vPos);
        bool ExecutePopulationSpawn(Population * pPop, uint32_t member_index);
        bool ExecuteSpawnPoint(SpawnPoint * pSP);

        WorldSystem *        pWorld   = nullptr;
        SpatialIndexSystem * pIndex   = nullptr;
        TTerrainProbe        fnTerrainProbe;
        TPhysicsProbe        fnPhysicsProbe;
        TNavProbe            fnNavProbe;

        SConfig oConfig;
        SStats  oStats;

        std::vector<Population *> vPopulations;    // registration (activation) order — the deterministic serial
        std::vector<SpawnPoint *> vSpawnPoints;

        std::vector<glm::vec3> vFocusScratch;      // per-pass focus positions (reused)
        std::vector<StrID>     vWorldSlots;
        std::vector<bool>      vSlotResolved;

        /** node-id → owner: O(1) dispatch for EEntityDestroyed. The handler
         *  used to walk every population's records + every SpawnPoint's live
         *  list (weak_ptr::lock each) for EVERY destroyed node — an O(n²)
         *  storm during sub-level unloads. Maintained at spawn/destroy sites. */
        struct SOwned {
                Population * pPop = nullptr;
                SpawnPoint * pSP  = nullptr;
        };
        std::unordered_map<uint32_t, SOwned> mSpawnIndex;

        bool bWarnedNoTerrain = false;
        bool bWarnedNoPhysics = false;
        bool bWarnedNavSnap   = false;
        bool bWarnedNavFail   = false;
};

} // namespace SE

#endif
