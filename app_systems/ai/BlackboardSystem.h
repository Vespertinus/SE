
#ifndef APP_AI_BLACKBOARD_SYSTEM_H
#define APP_AI_BLACKBOARD_SYSTEM_H 1

#include <cstdint>
#include <unordered_map>

#include <StrID.h>
#include <ResourceHandle.h>
#include <EventManager.h>
#include <ai/BlackboardCore.h>
#include <ai/TriggerBinding.h>

namespace SE {

class Blackboard;
class WorldAsset;
struct ETriggerFired;

// ---------------------------------------------------------------------------
// BlackboardSystem — registry of per-entity Blackboard components + the
// world-level (global) blackboard + TriggerBinding dispatch.
//
// Composition contract (compile-time, the SpawnerSystem precedent):
//   * requires WorldSystem — compose AFTER it in TCustomSystems (ctor order);
//   * the dispatch path resolves Blackboard/TriggerBinding siblings through
//     SceneTree::GetComponent, so apps composing BlackboardSystem must have
//     both components in TCustomComponents (blackboard_app is the example).
//
// Global lifecycle: on EWorldLoaded the world's declared global_blackboard
// holder (path or inline — the NavMeshEntry mechanism) initializes the global
// and registers WorldSystem::GetGlobalBlackboard(); on EWorldUnloaded the
// accessor unregisters and the core de-initializes (world-scoped state dies
// with the world; deterministic re-entry). No holder declared ⇒ the accessor
// stays null: writers warn once, readers see no core.
// ---------------------------------------------------------------------------
class BlackboardSystem {

public:
        BlackboardSystem();
        ~BlackboardSystem() noexcept;

        void AddComponent(Blackboard* pComp);
        void RemoveComponent(Blackboard* pComp);

        // Registry iteration (debug HUD / perception query surface).
        template <class TFn> void ForEachComponent(TFn&& fn) {
                for (auto& [id, pComp] : mComponents) fn(pComp);
        }

        BlackboardCore&       GetGlobal()       { return oGlobal; }
        const BlackboardCore& GetGlobal() const { return oGlobal; }
        bool                  HasGlobal() const { return oGlobal.IsInitialized(); }

private:
        void OnWorldLoaded(const Event& e);
        void OnWorldUnloaded(const Event& e);
        void OnTriggerFired(const Event& e);

        void InitGlobalFromWorld();
        void DispatchBindings(TSceneTree::TSceneNodeExact* pTriggerNode, const ETriggerFired& ev);
        void ApplyBinding(TSceneTree::TSceneNodeExact* pTriggerNode,
                          TriggerBinding::SDef& binding, const ETriggerFired& ev);

        WorldSystem* pWorld = nullptr;

        H<BlackboardAsset> hGlobalAsset;
        BlackboardCore     oGlobal;
        bool               bWarnedNoGlobal = false;

        std::unordered_map<uint32_t, Blackboard*> mComponents;
};

} // namespace SE

#endif
