
#ifdef SE_IMPL

#include <ai/BlackboardSystem.h>

#include <ai/Blackboard.h>
#include <ai/TriggerBinding.h>
#include <world/WorldSystem.h>
#include <world/WorldAsset.h>
#include <TriggerVolume.h>
#include <PhysicsSystem.h>

#include <CommonEvents.h>
#include <GlobalTypes.h>
#include <Logging.h>
#include <ai/BlackboardPin.tcc>   // the resource pin body (AI compositions only)

namespace SE {

BlackboardSystem::BlackboardSystem() {

        pWorld = &GetSystem<WorldSystem>();

        auto& em = GetSystem<EventManager>();
        em.AddListener<EWorldLoaded,   &BlackboardSystem::OnWorldLoaded>   (this);
        em.AddListener<EWorldUnloaded, &BlackboardSystem::OnWorldUnloaded> (this);
        em.AddListener<ETriggerFired,  &BlackboardSystem::OnTriggerFired>  (this);
}

BlackboardSystem::~BlackboardSystem() noexcept {

        auto& em = GetSystem<EventManager>();
        em.RemoveListener<EWorldLoaded,   &BlackboardSystem::OnWorldLoaded>   (this);
        em.RemoveListener<EWorldUnloaded, &BlackboardSystem::OnWorldUnloaded> (this);
        em.RemoveListener<ETriggerFired,  &BlackboardSystem::OnTriggerFired>  (this);

        if (pWorld) {
                pWorld->SetGlobalBlackboard(nullptr);
        }
}

void BlackboardSystem::AddComponent(Blackboard* pComp) {
        if (!pComp || !pComp->Node()) return;
        mComponents.try_emplace(pComp->Node()->GetID(), pComp);
}

void BlackboardSystem::RemoveComponent(Blackboard* pComp) {
        if (!pComp || !pComp->Node()) return;
        mComponents.erase(pComp->Node()->GetID());
}

// ---------------------------------------------------------------------------
// global blackboard lifecycle
// ---------------------------------------------------------------------------

void BlackboardSystem::OnWorldLoaded(const Event& /*e*/) {
        InitGlobalFromWorld();
}

void BlackboardSystem::InitGlobalFromWorld() {

        const WorldAsset* pAsset = pWorld->GetWorldAsset();
        if (!pAsset) return;

        const WorldAsset::SBlackboardEntry& entry = pAsset->GetGlobalBlackboard();
        if (entry.Empty()) return;   // nothing declared — global stays uninitialized

        if (entry.pInlineData) {
                // Inline embedding: the world asset repacked the definition into a
                // standalone .sebb blob (identifier included — the verifier demands it).
                hGlobalAsset = CreateResource<BlackboardAsset>(entry.cache_name,
                                                               entry.pInlineData->data(),
                                                               entry.pInlineData->size());
        } else {
                hGlobalAsset = CreateResource<BlackboardAsset>(entry.path);
        }

        const BlackboardAsset* pBBAsset = GetResource(hGlobalAsset);
        if (!pBBAsset) {
                throw std::runtime_error(fmt::format(
                                "BlackboardSystem: failed to load world global blackboard '{}' "
                                "(inline: '{}')",
                                entry.path, entry.cache_name));
        }

        // Owner handle stays empty: global change events carry no entity.
        oGlobal.Init(pBBAsset);
        bWarnedNoGlobal = false;

        // The plan's accessor goes live only while a world with a declared
        // global schema is loaded.
        pWorld->SetGlobalBlackboard(&oGlobal);

        log_d("BlackboardSystem: global blackboard initialized from '{}'",
                        entry.pInlineData ? entry.cache_name : entry.path);
}

void BlackboardSystem::OnWorldUnloaded(const Event& /*e*/) {
        // World-scoped state dies with the world: the accessor reads as absent
        // and the core releases its schema + values (a re-loaded world Init's
        // from scratch — deterministic re-entry).
        pWorld->SetGlobalBlackboard(nullptr);
        oGlobal.DeInit();
        hGlobalAsset = H<BlackboardAsset>::Null();
}

// ---------------------------------------------------------------------------
// TriggerBinding dispatch
// ---------------------------------------------------------------------------

void BlackboardSystem::OnTriggerFired(const Event& e) {

        const auto& ev = e.Get<ETriggerFired>();

        void* pRaw = GetSystem<PhysicsSystem>().GetNode(ev.hTrigger);
        if (!pRaw) return;

        DispatchBindings(static_cast<TSceneTree::TSceneNodeExact*>(pRaw), ev);
}

void BlackboardSystem::DispatchBindings(TSceneTree::TSceneNodeExact* pTriggerNode,
                                        const ETriggerFired& ev) {

        auto* pBinding = pTriggerNode->GetComponent<TriggerBinding>();
        if (!pBinding) return;

        for (auto& def : pBinding->GetBindings()) {
                if (def.event_id != ev.event_id) continue;
                ApplyBinding(pTriggerNode, def, ev);
        }
}

void BlackboardSystem::ApplyBinding(TSceneTree::TSceneNodeExact* pTriggerNode,
                                    TriggerBinding::SDef& binding, const ETriggerFired& ev) {

        // Resolve the target core; warn once per binding on unavailability.
        BlackboardCore* pTarget = nullptr;
        if (binding.global_target) {
                if (!oGlobal.IsInitialized()) {
                        if (!bWarnedNoGlobal) {
                                log_w("BlackboardSystem: binding '{}->{}' has no global blackboard "
                                      "(world declares none or not yet loaded)",
                                      binding.s_event, binding.s_key);
                                bWarnedNoGlobal = true;
                        }
                        return;
                }
                pTarget = &oGlobal;
        } else {
                if (Blackboard* pBB = pTriggerNode->GetComponent<Blackboard>()) {
                        pTarget = &pBB->GetCore();
                } else {
                        if (!binding.warned) {
                                log_w("BlackboardSystem: binding '{}->{}' targets Self but the "
                                      "trigger node has no Blackboard component",
                                      binding.s_event, binding.s_key);
                                binding.warned = true;
                        }
                        return;
                }
        }

        SSlotRef slot;

        if (binding.write_position) {
                // Entering actor's world position into a Vec3 key.
                if (!pTarget->TryResolve(binding.key, EBBType::Vec3, slot)) {
                        if (!binding.warned) {
                                log_w("BlackboardSystem: binding '{}->{}' (write_position) does not "
                                      "resolve to a Vec3 key",
                                      binding.s_event, binding.s_key);
                                binding.warned = true;
                        }
                        return;
                }

                void* pRaw = GetSystem<PhysicsSystem>().GetNode(ev.hOther);
                if (!pRaw) {
                        if (!binding.warned) {
                                log_w("BlackboardSystem: binding '{}->{}' cannot resolve the other "
                                      "body to a node",
                                      binding.s_event, binding.s_key);
                                binding.warned = true;
                        }
                        return;
                }

                auto* pOtherNode = static_cast<TSceneTree::TSceneNodeExact*>(pRaw);
                pTarget->SetVec3(slot, pOtherNode->GetTransform().GetWorldPos());
                return;
        }

        // Authored value: type must agree with the target key's schema.
        if (!binding.has_value) return;

        if (!pTarget->TryResolve(binding.key, binding.value_type, slot)) {
                if (!binding.warned) {
                        log_w("BlackboardSystem: binding '{}->{}' does not resolve "
                              "(unknown key or type mismatch)",
                              binding.s_event, binding.s_key);
                        binding.warned = true;
                }
                return;
        }

        switch (binding.value_type) {
                case EBBType::Float: pTarget->SetFloat(slot, binding.f_value); break;
                case EBBType::Bool:  pTarget->SetBool(slot, binding.b_value);  break;
                case EBBType::Int:   pTarget->SetInt(slot, binding.i_value);   break;
                case EBBType::Vec3:  pTarget->SetVec3(slot, binding.v_value);  break;
                default: break;
        }
}

} // namespace SE

#endif // SE_IMPL
