
#ifdef SE_IMPL

// Xlib (via graphics headers in mixed TUs) does `#define Bool int` —
// keep it out of this file; generated union members use Bool as a
// scope-qualified identifier (same dance as the entity plugins).
#pragma push_macro("Bool")
#undef Bool

#ifndef APP_AI_BEHAVIORTREETASKS_TCC
#define APP_AI_BEHAVIORTREETASKS_TCC 1


// Built-in task bodies (see BehaviorTreeTasks.h for the param conventions).
// Included only by AI compositions: SpawnTemplate instantiates
// GetSystem<EntityManager>, a compile-time-checked system lookup.

#include <ai/BehaviorTreeTasks.h>
#include <Logging.h>

#include <nav/PathFollower.h>
#include <hsm/StateMachine.h>
#include <EntityManager.h>

namespace SE {

namespace {

// Load-time param validation helpers (asset Build calls fnValidate per task).

int FindAuthoredParam(const FlatBuffers::BTTask* pTask, const char* szKey) {
        if (!pTask || !pTask->params()) return -1;
        const StrID key(szKey);
        for (const auto* p : *pTask->params()) {
                if (p && p->key() && StrID(p->key()->str()) == key) {
                        return static_cast<int>(p->value_type());
                }
        }
        return -1;
}

bool HasParamOfType(const FlatBuffers::BTTask* pTask, const char* szKey,
                    std::initializer_list<FlatBuffers::BTParamValue> allowed) {
        const int found = FindAuthoredParam(pTask, szKey);
        if (found < 0) return false;
        for (const auto t : allowed) {
                if (static_cast<int>(t) == found) return true;
        }
        return false;
}

bool ParamPresent(const FlatBuffers::BTTask* pTask, const char* szKey) {
        return FindAuthoredParam(pTask, szKey) >= 0;
}

// One-shot task failure warn (SNodeState::kWarnedOnce).
void WarnOnce(STaskCtx& ctx, const char* szTask, const char* szWhat) {
        SNodeState& st = ctx.states[ctx.node_index];
        if (st.flags & SNodeState::kWarnedOnce) return;
        st.flags |= SNodeState::kWarnedOnce;
        log_w("BT task '{}' node {}: {}", szTask, ctx.node_index, szWhat);
}

// MoveTo re-issues its goal once the blackboard target has moved this far
// from the goal captured at Init (chase-style keys like a live player
// position); static keys compare equal and never re-path.
constexpr float kMoveToRetargetDist = 0.25f;

// Per-Enter cache kept in SNodeState::mem: the resolved PathFollower and the
// blackboard slot the target key resolved to (entity core first, global
// fallback — same precedence as a fresh ReadBBVec3). The VALUE stays read
// live every tick (that is the point of a player_pos key); only the lookup
// path — component scan, key re-hash, FindSlot linear scan — is cached.
struct BTMoveToMem {
        PathFollower *   pFollower = nullptr;
        BlackboardCore * pCore     = nullptr;
        SSlotRef         slot      {};
        bool             resolved = false;
};

} // namespace

// --- Wait -------------------------------------------------------------------

void BTWait::Validate(const FlatBuffers::BTTask* pTask) {
        if (!HasParamOfType(pTask, "duration", { FlatBuffers::BTParamValue::Float })) {
                throw std::runtime_error("BT task 'Wait': requires float param 'duration'");
        }
}

EBTStatus BTWait::Tick(STaskCtx& ctx) {
        SNodeState& st = ctx.states[ctx.node_index];
        // resolved-param cache (param blob is baked/read-only)
        const SParam* pDuration;
        std::memcpy(&pDuration, st.mem.data(), sizeof pDuration);
        if (!pDuration) {
                pDuration = ctx.FindParam("duration");
                std::memcpy(st.mem.data(), &pDuration, sizeof pDuration);
        }
        st.timer += ctx.dt;
        return (st.timer >= pDuration->f) ? EBTStatus::Success : EBTStatus::Running;
}

// --- MoveTo -----------------------------------------------------------------

void BTMoveTo::Validate(const FlatBuffers::BTTask* pTask) {
        if (!HasParamOfType(pTask, "target", { FlatBuffers::BTParamValue::BTParamString })) {
                throw std::runtime_error("BT task 'MoveTo': requires string param 'target' (blackboard Vec3 key)");
        }
}

void BTMoveTo::Init(STaskCtx& ctx) {
        SNodeState& st = ctx.states[ctx.node_index];
        BTMoveToMem mem;
        mem.pFollower = ctx.pNode ? ctx.pNode->GetComponent<PathFollower>() : nullptr;
        if (!mem.pFollower) {
                WarnOnce(ctx, "MoveTo", "no PathFollower sibling — task always fails");
                std::memcpy(st.mem.data(), &mem, sizeof mem);
                return;
        }

        const SParam* p = ctx.FindParam("target");
        glm::vec3 vTarget;
        bool bFound = false;
        if (p && ctx.ReadBBVec3(StrID(p->s), vTarget)) {
                bFound = true;
                // cache which core resolved the key (entity first, global fallback)
                if (ctx.pEntityBB && ctx.pEntityBB->TryResolve(StrID(p->s), EBBType::Vec3, mem.slot)) {
                        mem.pCore = ctx.pEntityBB;
                } else if (ctx.pGlobalBB && ctx.pGlobalBB->TryResolve(StrID(p->s), EBBType::Vec3, mem.slot)) {
                        mem.pCore = ctx.pGlobalBB;
                }
        } else {
                WarnOnce(ctx, "MoveTo", "target key unresolvable on entity and global blackboards");
        }
        mem.resolved = true;
        std::memcpy(st.mem.data(), &mem, sizeof mem);

        if (bFound) {
                mem.pFollower->MoveTo(vTarget);
        }
}

EBTStatus BTMoveTo::Tick(STaskCtx& ctx) {
        SNodeState& st = ctx.states[ctx.node_index];
        BTMoveToMem mem;
        std::memcpy(&mem, st.mem.data(), sizeof mem);
        if (!mem.resolved) {
                // Init never ran its cache (warned there) — no follower, fail fast
                return EBTStatus::Failure;
        }

        PathFollower * pFollower = mem.pFollower;
        if (!pFollower) return EBTStatus::Failure;   // warned at Init

        if (pFollower->GetStatus() == PathStatus::Following) {
                // Track live blackboard targets while running (a moving player
                // position); path requests stay crowd-budgeted. The slot is
                // cached — only the vec3 read is per-tick.
                glm::vec3 vTarget;
                bool bHas = false;
                if (mem.pCore) {
                        vTarget = mem.pCore->GetVec3(mem.slot);
                        bHas = true;
                } else {
                        const SParam* p = ctx.FindParam("target");
                        bHas = p && ctx.ReadBBVec3(StrID(p->s), vTarget);
                }
                if (bHas) {
                        const glm::vec3 d = vTarget - pFollower->GetGoal();
                        if (d.x * d.x + d.z * d.z >= kMoveToRetargetDist * kMoveToRetargetDist) {
                                pFollower->MoveTo(vTarget);
                        }
                }
                return EBTStatus::Running;
        }

        switch (pFollower->GetStatus()) {
                case PathStatus::Arrived:   return EBTStatus::Success;
                case PathStatus::Failed:    return EBTStatus::Failure;
                case PathStatus::Idle:      // goal released under us (HSM/other writer)
                        WarnOnce(ctx, "MoveTo", "follower went Idle mid-branch (external Stop?)");
                        return EBTStatus::Failure;
                case PathStatus::Following: break;   // handled above
        }
        return EBTStatus::Failure;
}

void BTMoveTo::Stop(STaskCtx& ctx) {
        SNodeState& st = ctx.states[ctx.node_index];
        BTMoveToMem mem;
        std::memcpy(&mem, st.mem.data(), sizeof mem);
        if (mem.pFollower) mem.pFollower->Stop();
}

// --- SetBlackboard -----------------------------------------------------------

void BTSetBlackboard::Validate(const FlatBuffers::BTTask* pTask) {
        if (!ParamPresent(pTask, "key")) {
                throw std::runtime_error("BT task 'SetBlackboard': requires string param 'key'");
        }
        static constexpr std::initializer_list<FlatBuffers::BTParamValue> kValueTypes = {
                FlatBuffers::BTParamValue::Float,
                FlatBuffers::BTParamValue::Bool,
                FlatBuffers::BTParamValue::Int,
                FlatBuffers::BTParamValue::Vec3,
        };
        if (!HasParamOfType(pTask, "value", kValueTypes)) {
                throw std::runtime_error("BT task 'SetBlackboard': requires param 'value' (float|bool|int|vec3)");
        }
}

EBTStatus BTSetBlackboard::Tick(STaskCtx& ctx) {
        const SParam* pKey   = ctx.FindParam("key");
        const SParam* pValue = ctx.FindParam("value");
        if (!pKey || !pValue) return EBTStatus::Failure;   // validated at load

        const StrID target(pKey->s);   // authored key name is the string value
        bool ok = false;
        switch (pValue->type) {
                case EBTParamType::Float: ok = ctx.WriteBBFloat (target, pValue->f); break;
                case EBTParamType::Bool:  ok = ctx.WriteBBBool  (target, pValue->b); break;
                case EBTParamType::Int:   ok = ctx.WriteBBInt   (target, pValue->i); break;
                case EBTParamType::Vec3:  ok = ctx.WriteBBVec3  (target, pValue->v); break;
                default: break;
        }
        if (!ok) WarnOnce(ctx, "SetBlackboard", "key unresolvable or type mismatch on entity and global blackboards");
        return ok ? EBTStatus::Success : EBTStatus::Failure;
}

// --- EmitEvent ---------------------------------------------------------------

void BTEmitEvent::Validate(const FlatBuffers::BTTask* pTask) {
        if (!HasParamOfType(pTask, "event", { FlatBuffers::BTParamValue::BTParamString })) {
                throw std::runtime_error("BT task 'EmitEvent': requires string param 'event'");
        }
}

EBTStatus BTEmitEvent::Tick(STaskCtx& ctx) {
        const SParam* p = ctx.FindParam("event");
        if (!p) return EBTStatus::Failure;

        EBTEvent ev;
        ev.name   = StrID(p->s);   // authored event name is the string value
        ev.kind   = EBTEventKind::Message;
        ev.entity = ctx.pNode ? ctx.pNode->GetShared() : TSceneTree::TSceneNodeWeak{};
        GetSystem<EventManager>().QueueEvent(ev);
        return EBTStatus::Success;
}

// --- SetSMParam / TriggerSM ----------------------------------------------------

void BTSetSMParam::Validate(const FlatBuffers::BTTask* pTask) {
        if (!ParamPresent(pTask, "param")) {
                throw std::runtime_error("BT task 'SetSMParam': requires string param 'param'");
        }
        static constexpr std::initializer_list<FlatBuffers::BTParamValue> kValueTypes = {
                FlatBuffers::BTParamValue::Float,
                FlatBuffers::BTParamValue::Bool,
                FlatBuffers::BTParamValue::Int,
        };
        if (!HasParamOfType(pTask, "value", kValueTypes)) {
                throw std::runtime_error("BT task 'SetSMParam': requires param 'value' (float|bool|int)");
        }
}

EBTStatus BTSetSMParam::Tick(STaskCtx& ctx) {
        auto* pSM = ctx.pNode ? ctx.pNode->GetComponent<StateMachine>() : nullptr;
        if (!pSM) {
                WarnOnce(ctx, "SetSMParam", "no StateMachine sibling — task always fails");
                return EBTStatus::Failure;
        }

        const SParam* pName  = ctx.FindParam("param");
        const SParam* pValue = ctx.FindParam("value");
        if (!pName || !pValue) return EBTStatus::Failure;

        const StrID sm_param(pName->s);   // authored parameter name is the string value
        ParameterStore& params = pSM->GetParams();
        switch (pValue->type) {
                case EBTParamType::Float: params.SetFloat(sm_param, pValue->f); break;
                case EBTParamType::Bool:  params.SetBool (sm_param, pValue->b); break;
                case EBTParamType::Int:   params.SetInt  (sm_param, pValue->i); break;
                default: return EBTStatus::Failure;
        }
        return EBTStatus::Success;
}

void BTTriggerSM::Validate(const FlatBuffers::BTTask* pTask) {
        if (!HasParamOfType(pTask, "param", { FlatBuffers::BTParamValue::BTParamString })) {
                throw std::runtime_error("BT task 'TriggerSM': requires string param 'param'");
        }
}

EBTStatus BTTriggerSM::Tick(STaskCtx& ctx) {
        auto* pSM = ctx.pNode ? ctx.pNode->GetComponent<StateMachine>() : nullptr;
        if (!pSM) {
                WarnOnce(ctx, "TriggerSM", "no StateMachine sibling — task always fails");
                return EBTStatus::Failure;
        }
        const SParam* p = ctx.FindParam("param");
        if (!p) return EBTStatus::Failure;
        pSM->GetParams().SetTrigger(StrID(p->s));
        return EBTStatus::Success;
}

// --- SpawnTemplate -------------------------------------------------------------

void BTSpawnTemplate::Validate(const FlatBuffers::BTTask* pTask) {
        if (!HasParamOfType(pTask, "template", { FlatBuffers::BTParamValue::BTParamString })) {
                throw std::runtime_error("BT task 'SpawnTemplate': requires string param 'template'");
        }
}

EBTStatus BTSpawnTemplate::Tick(STaskCtx& ctx) {
        const SParam* p = ctx.FindParam("template");
        if (!p || !ctx.pNode) return EBTStatus::Failure;

        SpawnIntent intent;
        intent.prefab_id = p->s;
        intent.vTranslation = ctx.pNode->GetTransform().GetWorldPos();

        const SParam* pOffset = ctx.FindParam("offset");
        if (pOffset && pOffset->type == EBTParamType::Vec3) intent.vTranslation += pOffset->v;

        auto hSpawned = GetSystem<EntityManager>().SpawnTemplate(intent);
        return hSpawned.expired() ? EBTStatus::Failure : EBTStatus::Success;
}

} // namespace SE

#endif // SE_IMPL

#endif // APP_AI_BEHAVIORTREETASKS_TCC

#pragma pop_macro("Bool")
