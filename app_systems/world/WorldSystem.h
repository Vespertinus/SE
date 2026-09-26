
#ifndef APP_WORLD_SYSTEM_H
#define APP_WORLD_SYSTEM_H 1

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_set>
#include <vector>
#include <glm/glm.hpp>

#include <StrID.h>
#include <ResourceHandle.h>
#include <world/StreamingDirector.h>

namespace SE {
namespace FlatBuffers { struct SceneTree; }
class WorldAsset;
class BlackboardCore;   // app_systems/ai/ registered via SetGlobalBlackboard
}

// ---------------------------------------------------------------------------
// Events posted by WorldSystem (trivially copyable, CommonEvents precedent)
// ---------------------------------------------------------------------------

namespace SE {

struct ESubLevelActivating {
        uint32_t                   sub_level = 0;
        StrID                      name;
        TSceneTree::TSceneNodeWeak root;
};

struct ESubLevelActivated {
        uint32_t                   sub_level = 0;
        StrID                      name;
        TSceneTree::TSceneNodeWeak root;
};

struct ESubLevelDeactivating {
        uint32_t                   sub_level = 0;
        StrID                      name;
        TSceneTree::TSceneNodeWeak root;
};

struct ESubLevelDeactivated {
        uint32_t sub_level = 0;
        StrID    name;
};

struct EWorldLoaded {
        StrID world;
};

struct EWorldUnloaded {
        StrID world;
};

struct EFocusMoved {
        TSceneTree::TSceneNodeWeak node;
        glm::vec3                  pos;
};

// ---------------------------------------------------------------------------
// WorldSystem — owns the world/sub-level lifecycle.
//
// synchronous ActivateLevel/DeactivateLevel path; 
// the StreamingDirector pass (OnUpdate, streaming worlds only): requested
// sub-levels load through a worker thread (bytes staged) and a budgeted
// main-thread dependency drain, then finalize atomically. The full lifecycle
// state enum is shared by both paths.
//
// Structure: one SceneTree; a world root holds sub-level wrapper nodes, a
// persistent dynamics root holds long-lived entities no sub-level owns.
// Ownership is bookkeeping (spawn registries), never node reparenting.
// ---------------------------------------------------------------------------

class WorldSystem {
public:
        using TSceneNodeWeak = TSceneTree::TSceneNodeWeak;
        using FocusHandle    = uint64_t;   // 0 = invalid

        enum class ELevelState : uint8_t {
                Unloaded = 0,
                Requested,   // queued for load
                Loading,     // bytes staged / dependencies pinned
                Active,
                Unloading,   // teardown queued, awaiting deferred destruction
        };

        struct FocusPoint {
                TSceneNodeWeak node;           // expired => position frozen at last known
                glm::vec3      pos      {};
                float          radius   = 0.f;
                float          priority = 0.f;
        };

        // Type-erased retained-resource record; fnUnpin un-retains the exact pool entry.
        struct Pin {
                void   (*fnUnpin)(uint32_t index, uint32_t generation) = nullptr;
                uint32_t index      = 0;
                uint32_t generation = 0;
        };

        struct SubLevelInstance {
                uint32_t                    index = 0;
                ELevelState                 state = ELevelState::Unloaded;
                TSceneNodeWeak              pRoot;           // wrapper node under the world root
                std::vector<TSceneNodeWeak> vSpawns;         // entities lifetime-bound to this level
                std::vector<Pin>            vPins;           // resources retained for the level lifetime
                std::vector<TSceneNodeWeak> vTeardownExpect; // nodes awaited during teardown
                /** node-id set over vTeardownExpect: OnEntityDestroyed fires for
                 *  EVERY node of the unloading subtree — the O(1) membership
                 *  check replaces a weak_ptr-locking linear scan per node. */
                std::unordered_set<uint32_t> mTeardownExpectIds;

                // --- staged async loading ---------------------------------
                std::shared_ptr<std::vector<char>> pStagedBytes;  // worker-read .sesc (Requested->Active)
                uint32_t                          dep_total = 0;  // declared dependencies (declared table size)
                uint32_t                          dep_done  = 0;  // dependencies created+pinned so far
                uint32_t                          ticket    = 0;  // dispatch id; stale worker results dropped
        };

        WorldSystem();
        ~WorldSystem() noexcept;

        /// Call once after the active SceneTree is available (or on scene switch;
        /// unloads a still-loaded world first).
        void SetSceneTree(TSceneTree * pTree);

        /// Load a world: creates the world root + dynamics root, applies the startup
        /// policy (streaming disabled => all non-Manual levels; else AlwaysLoaded only),
        /// fires EWorldLoaded. Refused while a previous world is still tearing down.
        bool LoadWorld(H<WorldAsset> hWorld);
        bool LoadWorld(const std::string & sWorldPath);
        /// Deactivates every active level and destroys both roots (deferred).
        void UnloadWorld();

        /// Synchronous activation: pins the declared dependencies, instantiates the
        /// .sesc under a uniquely-named wrapper, fires Activating/Activated.
        bool ActivateLevel(uint32_t index);
        /// Budgeted Manual activation: stages the .sesc synchronously (local
        /// file read) and hands the dependency drain + instantiation to the
        /// regular TickStreaming budget — no busy-wait frame spike mid-game
        /// (door-anchored interior levels). Returns false when already
        /// loaded/loading or the stage read fails.
        bool RequestActivation(uint32_t index);
        /// Deferred deactivation: fires Deactivating, queues wrapper + registered
        /// spawns for destruction; Deactivated fires once all nodes are gone.
        bool DeactivateLevel(uint32_t index);

        bool           IsLevelActive(uint32_t index) const;
        ELevelState    GetLevelState(uint32_t index) const;
        TSceneNodeWeak GetLevelRoot(uint32_t index) const;
        uint32_t       GetSubLevelCount() const { return uint32_t(vInstances.size()); }

        /// Blocking wait until ONE level reaches Active — Manual levels included,
        /// which FlushStreaming's stats-based exit condition cannot see (the
        /// planner never counts them). Gameplay transitions (crypt swap, Portal)
        /// use this to drive a staged/Loading level to Active before teleporting.
        /// Returns false on timeout or if the level fell back to Unloaded
        /// (teardown won the race; the caller may re-request).
        bool FlushLevel(uint32_t index, float timeout_ms,
                        const std::function<void(uint32_t, uint32_t)> & fnPump = {});

        /// Dependency-drain progress for a staged (Loading) level; false if the
        /// level is not staged (or out of range). done/total = declared resources
        /// created+pinned so far / declared total.
        bool GetLevelDepProgress(uint32_t index, uint32_t & done, uint32_t & total) const;

        TSceneNodeWeak GetWorldRoot()    const { return pWorldRoot; }
        TSceneNodeWeak GetDynamicsRoot() const { return pDynamicsRoot; }
        H<WorldAsset>  GetWorld()        const { return hWorld; }
        TSceneTree *   GetSceneTree()    const { return pSceneTree; }
        const WorldAsset * GetWorldAsset() const;

        /// --- the world-level (global) blackboard ------------------------
        /// Registered by BlackboardSystem at init (inverted self-registration —
        /// keeps WorldSystem free of the AI typelist). Null unless the app composes
        /// BlackboardSystem and a world with a declared global schema is loaded.
        void             SetGlobalBlackboard(BlackboardCore * pCore) { pGlobalBlackboard = pCore; }
        BlackboardCore * GetGlobalBlackboard() const { return pGlobalBlackboard; }

        /// Lifetime-bind a spawned entity to a sub-level: teardown destroys it.
        /// Entities that must survive their level live under the dynamics root and
        /// are simply never registered.
        void RegisterSpawn(uint32_t level, TSceneNodeWeak pNode);

        /// Register a streaming source. The node's world position is tracked; a move
        /// beyond epsilon posts EFocusMoved (consumed by the director).
        FocusHandle RegisterFocus(TSceneNodeWeak pNode, float radius, float priority);
        void UnregisterFocus(FocusHandle handle);
        const std::vector<FocusPoint> & GetFocuses() const { return vFocuses; }

        /// --- streaming director surface ---------------------------------

        /// Shared streaming worker access (terrain tiles dispatch through the
        /// same worker thread — the stage's one-worker-thread rule).
        StreamingDirector & GetDirector() { return oDirector; }
        uint32_t            GetWorldSerial() const { return world_serial; }

        /// Route worker results of a non-scene content kind to a consumer 
        /// TerrainSystem receives its tile reads here. One handler per kind; a null
        /// function clears. Invoked on the main thread from the result drain.
        using TContentResultHandler = std::function<void(const StreamingDirector::SReadResult &)>;
        void SetContentResultHandler(StreamingDirector::EContentKind eKind, TContentResultHandler fn);

        /// Streaming snapshot for HUDs, loading screens and tests.
        struct StreamingStats {
                uint32_t wanted          = 0;   // levels that should be active right now
                uint32_t active          = 0;
                uint32_t requested       = 0;   // queued / worker read in flight
                uint32_t loading         = 0;   // bytes staged, dependencies draining
                uint32_t unloading       = 0;
                uint32_t in_flight_reads = 0;   // outstanding worker reads
                uint32_t pinned_total    = 0;   // sum of per-level resource pins
                float    budget_spent_ms = 0.f; // last director pass, main-thread work
                float    budget_limit_ms = 0.f;
        };

        StreamingStats GetStreamingStats() const;

        /// Loading-screen / level-transition helper: run the director pass
        /// budget-free until the wanted set is fully active (or timeout_ms wall
        /// time passes — a repeatedly failing level returns false). Only relevant
        /// with streaming enabled; otherwise an immediate true.
        /** Synchronous loading-screen flush. fnPump (optional) runs between
         *  passes — the app pumps OS events / presents a loading frame so the
         *  window is not frozen for the whole flush (args: wanted, active). */
        bool FlushStreaming(float timeout_ms = 5000.f,
                            const std::function<void(uint32_t, uint32_t)> & fnPump = {});

        /// Un-retains one pool entry; stored in Pin::fnUnpin (one instantiation per
        /// resource type — the type discriminator of the pin record).
        template <class R>
        static void UnpinResource(const uint32_t idx, const uint32_t gen) {
                UnretainResource(H<R>{ ResourceHandle{ idx, gen } });
        }

private:
        void OnUpdate(const Event & oEvent);
        void OnPostUpdate(const Event & oEvent);
        void OnEntityDestroyed(const Event & oEvent);
        // Completes Unloading instances whose expected nodes have all expired:
        // a spawn parented under the sub-level subtree dies with the wrapper's
        // Unlink and never posts its own EEntityDestroyed, so event matching
        // alone cannot detect the end of a teardown.
        void PruneTeardowns();

        bool LoadWorldImpl();
        bool ActivateLevelImpl(SubLevelInstance & inst);
        void FinalizeActivation(SubLevelInstance & inst);   // wrapper+Activating+Instantiate+Active+Activated
        void BeginTeardown(SubLevelInstance & inst);
        void FinishTeardown(SubLevelInstance & inst);
        void UnpinAll(SubLevelInstance & inst) noexcept;

        // --- streaming director pass ------------------------------------

        // One director pass: plan, cancels, worker results, request dispatch,
        // budgeted dependency drain, instantiations, hysteresis unloads.
        // budget_ms = std::numeric_limits<float>::max() means unbudgeted.
        void TickStreaming(float budget_ms, bool bFlushMode);
        // Pop + route worker results without a streaming pass (foreign content
        // like terrain tiles must drain even with no world loaded).
        void DrainWorkerResults();
        // Create + pin the next declared dependency. Returns true when the whole
        // declared table is pinned (or the scene declares none).
        bool StepDependency(SubLevelInstance & inst, const FlatBuffers::SceneTree * pScene);
        // Requested/Loading -> Unloaded: drop staged bytes, unpin partial pins.
        // No events were fired, so cancellation is invisible to gameplay.
        bool CancelStagedLoad(uint32_t index);

        TSceneTree *                  pSceneTree { nullptr };
        H<WorldAsset>                 hWorld;
        bool                          world_loaded { false };

        TSceneNodeWeak                pWorldRoot;
        TSceneNodeWeak                pDynamicsRoot;

        std::vector<SubLevelInstance> vInstances;   // index == world sub_levels slot

        uint32_t                      focus_serial { 1 };
        std::vector<FocusPoint>       vFocuses;
        std::vector<FocusHandle>      vFocusHandles;  // parallel to vFocuses; 0 = dead slot

        uint32_t                      active_count { 0 };

        StreamingDirector                   oDirector;
        uint32_t                            world_serial    { 0 };  // stale worker-result guard
        uint32_t                            dispatch_ticket { 0 };  // stale worker-result guard
        float                               last_budget_spent_ms { 0.f };
        StreamingDirector::SPlan            oLastPlan;
        std::vector<TContentResultHandler>  vContentHandlers;       // per EContentKind (non-scene)
        // planner snapshots (reused scratch — no per-frame allocation)
        std::vector<StreamingDirector::SLevelSnap>   vSnapLevels;
        std::vector<StreamingDirector::ELevelStatus> vSnapStatuses;
        std::vector<StreamingDirector::SFocus>       vSnapFocuses;
        std::vector<std::pair<float, uint32_t>>      vDrainOrder;   // (distance, index)

        BlackboardCore * pGlobalBlackboard = nullptr;   // owned by BlackboardSystem

};

} // namespace SE

#endif
