
#ifndef APP_SPAWNER_POPULATION_H
#define APP_SPAWNER_POPULATION_H 1

#include <string>
#include <vector>

#include <StrID.h>
#include <Component_generated.h>
#include <Spawner_generated.h>

#include <spawner/DeterministicRng.h>
#include <spawner/PopulationRuntime.h>
#include <spawner/SpawnConditions.h>

namespace SE {

class SpawnerSystem;

// ---------------------------------------------------------------------------
// PopulationDesc — programmatic construction (tests, game code).
// ---------------------------------------------------------------------------

struct PopulationMemberDesc {
        std::string template_id;
        std::string geometry;               // SpawnPoint/SpawnVolume node name; "" = origin
        float       weight          = 1.f;
        uint32_t    count           = 1;
        float       initial_delay_s = 0.f;
        uint32_t    respawn_limit   = 0xFFFFFFFFu;
};

struct PopulationWaveDesc {
        float        at_time_s = -1.f;
        std::string  on_event;
        std::vector<PopulationMemberDesc> members;
};

struct PopulationConditionDesc {
        SpawnConditions::EKind kind = SpawnConditions::EKind::FocusNear;
        float      radius = 0.f;
        float      at_s   = 0.f;
        std::string tag;
        std::string key;   // WorldFlag
};

struct PopulationDesc {
        std::string id;
        std::vector<PopulationMemberDesc>    members;
        std::vector<PopulationWaveDesc>      waves;
        uint32_t max_active      = 0;
        float    respawn_delay_s = 0.f;
        float    tick_interval_s = 0.25f;
        float    despawn_radius  = 0.f;
        std::vector<PopulationConditionDesc> conditions;
        FlatBuffers::SpawnLifetime lifetime = FlatBuffers::SpawnLifetime::SubLevel;
        FlatBuffers::SpawnSnap     snap     = FlatBuffers::SpawnSnap::None;
        uint32_t seed_offset = 0;
};

// ---------------------------------------------------------------------------
// Population — the spawner manager component. Quota-holder + lifecycle policy;
// spawn geometry lives in SpawnPoint/SpawnVolume components it references by
// node name. Arms on Enable() (inside the sub-level activation window — a
// population only exists while its level is active) and destroys its owned
// spawns on Disable.
//
// Lifetime policy (schema): SubLevel — spawns parent under the population node,
// teardown cascades kill them; Persistent — spawns parent under the world
// dynamics root so NO sub-level deactivation kills them, their own level's
// included (round 12: pursuers must survive the camp's streaming-edge
// teardown — Population::Disable drops their records but leaves the entities
// alive). The world unload's dynamics-root cascade is what reaps them.
// ---------------------------------------------------------------------------

class Population {
public:
        using TSerialized = FlatBuffers::Population;

        Population(TSceneTree::TSceneNodeExact * pNode, const PopulationDesc & oDesc);
        Population(TSceneTree::TSceneNodeExact * pNode, const FlatBuffers::Population * pFB);
        ~Population() noexcept;

        void Enable();
        void Disable();

        const StrID & Id() const { return id; }
        uint32_t ActiveTotal() const { return oRuntime.ActiveTotal(); }
        bool ClaimWorldSlot(StrID slot);            // returns true if id matches a world spawn slot
        std::string Str() const;

private:
        friend class SpawnerSystem;
        friend class SpawnerDebugger;

        /** Resolve member geometry names under this subtree. Returns false when
            some names were missing. NOTE: a sub-level's node children are
            created after its components, so the load-time resolve legitimately
            misses — SpawnerSystem re-resolves on the first spawn. */
        bool SyncGeometryNodes(bool bVerboseOnMiss);

        TSceneTree::TSceneNodeExact * pNode = nullptr;
        StrID      id;
        std::string id_string;      // authoring form (seeding via stable FNV, logging)

        float tick_interval = 0.25f;
        float tick_phase    = 0.f;
        float age           = 0.f;      // seconds since arm (AppClock-scaled by the system)
        float next_tick_t   = 0.f;
        float despawn_radius = 0.f;
        FlatBuffers::SpawnLifetime eLifetime = FlatBuffers::SpawnLifetime::SubLevel;
        FlatBuffers::SpawnSnap     eSnap     = FlatBuffers::SpawnSnap::None;

        std::vector<SpawnConditions::SConditionDesc> vConditions;
        PopulationRuntime::Runtime oRuntime;
        std::vector<TSceneTree::TSceneNodeWeak>     vGeometryNodes;  // parallel to runtime members (sorted view)
        std::vector<PopulationRuntime::SSpawnCommand> vCommands;     // per-pass scratch

        struct SRecord {
                TSceneTree::TSceneNodeWeak node;
                uint32_t member;
        };
        std::vector<SRecord> vRecords;

        DeterministicRng oRng { 0 };     // reseeded at Enable
        bool enabled = false;
};

} // namespace SE

#endif
