
#ifndef APP_AI_BEHAVIOR_TREE_COMMON_H
#define APP_AI_BEHAVIOR_TREE_COMMON_H 1

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <StrID.h>
#include <MPUtil.h>

#include <ai/BlackboardCore.h>
#include <BehaviorTree_generated.h>

namespace SE {

// NOTE: TSceneTree (the GlobalTypes alias) must already be declared when this
// header is included — always pull it in after GlobalTypes/SceneTree, the
// BlackboardCore.h convention (nested handle types are used below).

template <class TTasks> class TBehaviorTree;

// ---------------------------------------------------------------------------
// Runtime status vocabulary (authored enums come from the generated schema).
// ---------------------------------------------------------------------------

// Task tick result.
enum class EBTStatus : uint8_t { Running = 0, Success = 1, Failure = 2 };

// Per-node-instance state machine (SNodeState.status).
enum class EBTNodeStatus : uint8_t {
        Idle      = 0,   // not on the active path
        Running   = 1,   // on the active path this tick
        Succeeded = 2,   // completed (parallel keeps these while it runs)
        Failed    = 3,
};

// Deferred cue event: named nodes post Enter/Exit, EmitEvent posts Message
// (QueueEvent — the ETriggerFired convention). Unnamed nodes post nothing.
enum class EBTEventKind : uint8_t { Enter = 0, Exit = 1, Message = 2 };

struct EBTEvent {
        StrID                  name;
        EBTEventKind           kind = EBTEventKind::Message;
        TSceneTree::TSceneNodeWeak entity;
};

// ---------------------------------------------------------------------------
// SNodeState — per-instance state slot, one per baked tree node.
//
// Reset rules (plan D7): task memory, timers and counters are entry-semantic —
// they are cleared when the node (re-)enters or is aborted. The one exception
// is a Cooldown decorator's `timer`, which is cross-entry state: it accumulates
// continuously and only restarts when the decorated child completes.
// ---------------------------------------------------------------------------

struct SNodeState {
        // SNodeState::flags bits
        static constexpr uint8_t kWarnedOnce = 1;

        EBTNodeStatus status        = EBTNodeStatus::Idle;
        int16_t       running_child = -1;   // composite memory
        uint8_t       flags         = 0;
        float         timer         = 0.f;  // Cooldown / TimeLimit / task timer
        uint32_t      counter       = 0;    // Loop counter / task scratch
        // Task memory (POD only). 32 bytes so hot tasks can cache per-Enter
        // resolutions (PathFollower*, blackboard core + slot, resolved param
        // pointers) instead of re-hashing keys / re-scanning components per tick.
        std::array<std::byte, 32> mem {};

        void EnterTask() {
                status  = EBTNodeStatus::Running;
                timer   = 0.f;
                counter = 0;
                mem.fill(std::byte{0});
        }
};

// ---------------------------------------------------------------------------
// Baked node / param layouts (filled by TBehaviorTreeAsset, read-only runtime).
// ---------------------------------------------------------------------------

struct SNode {
        FlatBuffers::BTNodeU type = FlatBuffers::BTNodeU::BTTask;

        StrID    name;                                   // 0 = unnamed (no events)
        int32_t  parent           = -1;
        int32_t  child_first      = -1;                  // composite: offset into the child-index list
        uint16_t child_count      = 0;                   // composite
        int32_t  decorated_child  = -1;                  // decorator

        // composite payload
        FlatBuffers::EBTCompositeKind composite_kind = FlatBuffers::EBTCompositeKind::Sequence;
        uint16_t n_required = 0;                         // Parallel; 0 = all

        // decorator payload
        FlatBuffers::EBTDecoratorKind decorator_kind = FlatBuffers::EBTDecoratorKind::BlackboardCondition;
        FlatBuffers::EBTAbort         abort          = FlatBuffers::EBTAbort::None;
        FlatBuffers::SMConditionOp    op             = FlatBuffers::SMConditionOp::IsTrue;
        FlatBuffers::EBTForceTo       force_to       = FlatBuffers::EBTForceTo::Success;
        StrID    cond_key;                               // BlackboardCondition target key
        float    threshold = 0.f;
        float    duration  = 0.f;                         // Cooldown gate / TimeLimit budget
        int32_t  count     = 0;                           // Loop; 0 = infinite

        // task payload
        uint16_t task_index  = 0;                        // index into the app's task table
        int32_t  param_first = -1;
        uint16_t param_count = 0;
};

enum class EBTParamType : uint8_t { Float = 0, Bool = 1, Int = 2, String = 3, Vec3 = 4 };

// Resolved blackboard location for one condition key (component-owned table,
// read by the system's condition evaluation through STaskCtx).
struct SBBBinding {
        BlackboardCore* pCore  = nullptr;
        SSlotRef        slot   {};
        bool            valid  = false;
};

struct SParam {
        StrID        key;
        EBTParamType type = EBTParamType::Float;
        float        f = 0.f;
        bool         b = false;
        int32_t      i = 0;
        std::string  s;
        glm::vec3    v {};
};

// ---------------------------------------------------------------------------
// STaskCtx — what a task executor may touch. No component type: tasks work on
// the baked node arrays and the instance state pool through raw pointers the
// system refreshes per tick, so the executor contract stays composition-free.
// ---------------------------------------------------------------------------

struct STaskCtx {
        const SNode*    nodes  = nullptr;
        const SParam*   params = nullptr;        // full tree param blob
        const int32_t*  child_indices = nullptr; // composite child list (child_first..+count)
        SNodeState*     states = nullptr;        // full instance pool
        const struct SBBBinding* bindings = nullptr;  // per-node condition bindings
        uint16_t        node_index = 0;

        TSceneTree::TSceneNodeExact* pNode = nullptr;   // owner entity
        float dt = 0.f;                                 // accumulated tick dt

        BlackboardCore* pEntityBB = nullptr;            // resolved lazily; either may be null
        BlackboardCore* pGlobalBB = nullptr;

        // --- param access (node's own [BTParam] run) ------------------------

        const SParam* FindParam(std::string_view sKey) const {
                const SNode& n = nodes[node_index];
                for (uint16_t i = 0; i < n.param_count; ++i) {
                        const SParam& p = params[n.param_first + i];
                        if (p.key == StrID(sKey)) return &p;
                }
                return nullptr;
        }

        float GetParamFloat(std::string_view sKey) const {
                const SParam* p = FindParam(sKey);
                if (!p || p->type != EBTParamType::Float) {
                        throw std::runtime_error(fmt::format(
                                        "BT task node {}: missing or ill-typed float param '{}'",
                                        node_index, sKey));
                }
                return p->f;
        }

        // No-throw variants with authored defaults — game-task ergonomics.
        float FindParamFloat(std::string_view sKey, float fDefault) const {
                const SParam* p = FindParam(sKey);
                return (p && p->type == EBTParamType::Float) ? p->f : fDefault;
        }
        int FindParamInt(std::string_view sKey, int iDefault) const {
                const SParam* p = FindParam(sKey);
                return (p && p->type == EBTParamType::Int) ? p->i : iDefault;
        }
        std::string FindParamString(std::string_view sKey, std::string_view sDefault) const {
                const SParam* p = FindParam(sKey);
                return (p && p->type == EBTParamType::String) ? p->s : std::string(sDefault);
        }

        // --- blackboard access: entity core first, global fallback ----------

        BlackboardCore* BB() const { return pEntityBB ? pEntityBB : pGlobalBB; }

        // Entity core first, global fallback; false when neither schema has the key.
        bool ReadBBVec3(StrID key, glm::vec3& vOut) const {
                SSlotRef slot;
                if (pEntityBB && pEntityBB->TryResolve(key, EBBType::Vec3, slot)) {
                        vOut = pEntityBB->GetVec3(slot);
                        return true;
                }
                if (pGlobalBB && pGlobalBB->TryResolve(key, EBBType::Vec3, slot)) {
                        vOut = pGlobalBB->GetVec3(slot);
                        return true;
                }
                return false;
        }

        // Typed write for SetBlackboard: returns false on unknown key / type
        // mismatch / no core (caller warns once and fails the task). The core
        // that resolves the key is the core that gets written.
        bool WriteBBFloat(StrID key, float value) const {
                SSlotRef slot;
                BlackboardCore* pCore = BBResolve(key, EBBType::Float, slot);
                return pCore && pCore->SetFloat(slot, value);
        }
        bool WriteBBBool(StrID key, bool value) const {
                SSlotRef slot;
                BlackboardCore* pCore = BBResolve(key, EBBType::Bool, slot);
                return pCore && pCore->SetBool(slot, value);
        }
        bool WriteBBInt(StrID key, int32_t value) const {
                SSlotRef slot;
                BlackboardCore* pCore = BBResolve(key, EBBType::Int, slot);
                return pCore && pCore->SetInt(slot, value);
        }
        bool WriteBBVec3(StrID key, const glm::vec3& value) const {
                SSlotRef slot;
                BlackboardCore* pCore = BBResolve(key, EBBType::Vec3, slot);
                return pCore && pCore->SetVec3(slot, value);
        }

private:
        BlackboardCore* BBResolve(StrID key, EBBType expected, SSlotRef& out) const {
                if (pEntityBB && pEntityBB->TryResolve(key, expected, out)) return pEntityBB;
                if (pGlobalBB && pGlobalBB->TryResolve(key, expected, out)) return pGlobalBB;
                return nullptr;
        }
};

// ---------------------------------------------------------------------------
// Task executor table — compile-time StrID → executor mapping.
//
// A task is a struct of static functions; derive from TBTTaskBase and define
// what it needs (Tick is mandatory):
//
//   struct BTMyTask : TBTTaskBase<BTMyTask> {
//           static constexpr const char* kName = "MyTask";
//           static EBTStatus Tick(STaskCtx& ctx);
//   };
//
// and append it to the app's task typelist (TMyBTTasks in the app's App.h).
// ---------------------------------------------------------------------------

template <class T> struct TBTTaskBase {
        static constexpr const char* kName = "<unnamed>";
        static void       Validate(const FlatBuffers::BTTask*) {}
        static void       Init(STaskCtx&) {}
        static void       Stop(STaskCtx&) {}
};

struct STaskExecutor {
        const char* szName;
        void       (*fnValidate)(const FlatBuffers::BTTask*);
        void       (*fnInit)(STaskCtx&);
        EBTStatus (*fnTick)(STaskCtx&);
        void       (*fnStop)(STaskCtx&);
};

template <class TTasksList> struct TTaskTable;   // only TypelistWrapper instantiations exist

template <class... TTasks>
struct TTaskTable<MP::TypelistWrapper<TTasks...>> {
        static constexpr std::array<STaskExecutor, sizeof...(TTasks)> kExecutors = {{
                { TTasks::kName, &TTasks::Validate, &TTasks::Init, &TTasks::Tick, &TTasks::Stop }...
        }};

        // -1 when unknown — asset Build turns this into a load error naming the id.
        static int IndexOf(StrID id) {
                for (size_t i = 0; i < kExecutors.size(); ++i) {
                        if (StrID(std::string_view(kExecutors[i].szName)) == id) {
                                return static_cast<int>(i);
                        }
                }
                return -1;
        }

        static const STaskExecutor& At(uint16_t idx) { return kExecutors[idx]; }
};

} // namespace SE

#endif
