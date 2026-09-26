
#ifndef APP_SPAWNER_SPAWN_POINT_H
#define APP_SPAWNER_SPAWN_POINT_H 1

#include <string>
#include <vector>

#include <StrID.h>
#include <Component_generated.h>
#include <Spawner_generated.h>

#include <spawner/SpawnConditions.h>

namespace SE {

class SpawnerSystem;

// ---------------------------------------------------------------------------
// SpawnPoint — geometry + a simple standalone policy: spawns template_id at its
// own transform, up to max_spawns lifetime, optional respawn after death, one
// optional condition gate. One live instance at a time (the "lever spawns one"
// case that needs no population).
// ---------------------------------------------------------------------------

class SpawnPoint {
public:
        using TSerialized = FlatBuffers::SpawnPoint;

        SpawnPoint(TSceneTree::TSceneNodeExact * pNode, const FlatBuffers::SpawnPoint * pFB);
        ~SpawnPoint() noexcept;

        void Enable();
        void Disable();

        const std::string & TemplateId() const { return sTemplateId; }
        FlatBuffers::SpawnSnap Snap() const { return eSnap; }
        uint32_t SpawnedTotal() const { return spawned_total; }
        bool HasLiveSpawn() const { return !vLive.empty(); }

        std::string Str() const;

private:
        friend class SpawnerSystem;

        TSceneTree::TSceneNodeExact * pNode = nullptr;
        std::string  sTemplateId;
        float        initial_delay_s = 0.f;
        uint32_t     max_spawns      = 1;
        float        respawn_delay_s = -1.f;   // < 0: no respawn
        SpawnConditions::SConditionDesc oCondition;   // kind None? — see .tcc (has_gate)
        bool         has_gate        = false;
        FlatBuffers::SpawnSnap eSnap  = FlatBuffers::SpawnSnap::None;

        float     age          = 0.f;
        float     next_ready_t = 0.f;
        uint32_t  spawned_total = 0;
        std::vector<TSceneTree::TSceneNodeWeak> vLive;   // ≤ 1 live instance
        bool      enabled = false;
};

} // namespace SE

#endif
