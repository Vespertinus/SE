
#ifndef APP_AI_BLACKBOARD_CORE_H
#define APP_AI_BLACKBOARD_CORE_H 1

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include <StrID.h>
#include <ResourceHandle.h>
#include <ai/BlackboardAsset.h>

namespace SE {

class BlackboardAsset;

// ---------------------------------------------------------------------------
// EBlackboardChanged — posted by BlackboardCore on an actual value change only
// (declared here, the layer that posts it; the component and system headers
// include this one). Opt-in: BT decorators poll on their staggered ticks; this
// event exists for explicitly subscribed watchers (audio, HUD, game glue).
// Deferred QueueEvent delivery — the ETriggerFired convention. Trivially
// copyable payload: StrID is uint64_t; an empty handle means the global
// blackboard. Listeners filter by key themselves.
// ---------------------------------------------------------------------------
struct EBlackboardChanged {
        StrID                      key;
        TSceneTree::TSceneNodeWeak entity;
};

// ---------------------------------------------------------------------------
// BlackboardCore — the typed SoA value storage behind both the per-entity
// Blackboard component and the world-level global instance.
//
// Schema (keys + types + defaults) lives in the BlackboardAsset; the core owns
// five contiguous value arrays sized once at Init. Slot writes/reads are array
// ops; key hashing happens only in the StrID conveniences (resolve-per-call,
// for tools/debug/game glue — hot paths cache SSlotRefs). Allocation-free
// after Init.
//
// Change notification posts EBlackboardChanged on actual value change only.
// Snapshot()/Restore() are the save seam: proper weak_ptr copies
// (never memcpy — a byte-copied weak_ptr corrupts the control block), events
// suppressed during restore.
// ---------------------------------------------------------------------------
class BlackboardCore {

public:
        using TNodeWeak = TSceneTree::TSceneNodeWeak;

        // Value arrays for one blackboard instance — proper copies (refcount-safe).
        struct SSnapshot {
                std::vector<float>     vFloats;
                std::vector<uint8_t>   vBools;
                std::vector<int32_t>   vInts;
                std::vector<glm::vec3> vVecs;
                std::vector<TNodeWeak> vEntities;
        };

        BlackboardCore() = default;

        BlackboardCore(const BlackboardCore&) = delete;
        BlackboardCore& operator=(const BlackboardCore&) = delete;

        // Sizes the arrays from the schema and applies defaults. Re-Init on an
        // initialized core is allowed (global reload) and resets everything.
        // Raw pointer by design: the OWNER (Blackboard component / BlackboardSystem)
        // holds the H<BlackboardAsset> handle and does the resource-manager calls —
        // this .tcc must stay free of CreateResource/GetResource instantiations so
        // compositions without the BlackboardAsset resource can still include it
        // (spawner WorldFlag conditions evaluate through BlackboardCore).
        void Init(const BlackboardAsset* pAsset, TNodeWeak hOwner = TNodeWeak{});

        bool                   IsInitialized() const { return pAsset != nullptr; }
        const BlackboardAsset* GetAsset()      const { return pAsset; }

        // Resolution delegates to the schema.
        SSlotRef Resolve(StrID key, EBBType expected) const;
        bool     TryResolve(StrID key, EBBType expected, SSlotRef& out) const;

        // --- hot path: SSlotRef accessors (debug-assert the slot type) -------
        bool      SetFloat (SSlotRef slot, float value);
        bool      SetBool  (SSlotRef slot, bool value);
        bool      SetInt   (SSlotRef slot, int32_t value);
        bool      SetVec3  (SSlotRef slot, const glm::vec3& vValue);
        bool      SetEntity(SSlotRef slot, TNodeWeak hNode);

        float     GetFloat (SSlotRef slot) const;
        bool      GetBool  (SSlotRef slot) const;
        int32_t   GetInt   (SSlotRef slot) const;
        glm::vec3 GetVec3  (SSlotRef slot) const;
        TNodeWeak GetEntity(SSlotRef slot) const;

        // --- convenience: StrID accessors (throw on unknown key/mismatch) ----
        bool      SetFloat (StrID key, float value);
        bool      SetBool  (StrID key, bool value);
        bool      SetInt   (StrID key, int32_t value);
        bool      SetVec3  (StrID key, const glm::vec3& vValue);
        bool      SetEntity(StrID key, TNodeWeak hNode);

        float     GetFloat (StrID key) const;
        bool      GetBool  (StrID key) const;
        int32_t   GetInt   (StrID key) const;
        glm::vec3 GetVec3  (StrID key) const;
        TNodeWeak GetEntity(StrID key) const;

        // Vec3 direct; Entity → lock + node world position; false otherwise.
        bool TryGetLocation(SSlotRef slot, glm::vec3& vOut) const;

        void ResetToDefaults();

        // Releases the schema + arrays (world unload): the core reads as absent.
        void DeInit();

        SSnapshot Snapshot() const;

        // Type-checks array sizes against the current schema (throws on
        // mismatch); writes back; posts no events.
        void Restore(const SSnapshot& oSnapshot);

        // Load-time bulk writes (component `initial` overrides) must not fire
        // change events; wrap the writes with this guard.
        void SetEventsSuppressed(bool suppressed) { bSuppressEvents = suppressed; }

        std::string Str() const;

private:
        void PostChanged(StrID key) const;

        const BlackboardAsset* pAsset = nullptr;
        TNodeWeak              hOwner;   // event payload identity; empty for the global

        bool bSuppressEvents = false;

        std::vector<float>     vFloats;
        std::vector<uint8_t>   vBools;
        std::vector<int32_t>   vInts;
        std::vector<glm::vec3> vVecs;
        std::vector<TNodeWeak> vEntities;

        // slot index → key StrID, per type (change-event payloads)
        std::vector<StrID> vFloatKeys;
        std::vector<StrID> vBoolKeys;
        std::vector<StrID> vIntKeys;
        std::vector<StrID> vVec3Keys;
        std::vector<StrID> vEntityKeys;
};

} // namespace SE

#endif
