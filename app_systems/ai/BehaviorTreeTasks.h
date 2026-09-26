
#ifndef APP_AI_BEHAVIOR_TREE_TASKS_H
#define APP_AI_BEHAVIOR_TREE_TASKS_H 1

#include <ai/BehaviorTreeCommon.h>

namespace SE {

// ---------------------------------------------------------------------------
// Built-in task library (stage plan §13 property 4): Wait, MoveTo,
// SetBlackboard, EmitEvent, SetSMParam, TriggerSM, SpawnTemplate.
//
// Tasks are structs of static functions (TBTTaskBase contract). The executor
// bodies live in BehaviorTreeTasks.tcc — included only by AI compositions —
// because SpawnTemplate calls GetSystem<EntityManager> (a compile-time-checked
// system lookup): BT apps without the entity side could not compile a core
// task table that references it.
//
// SpawnFromPopulation ships separately in
// TSpawnerBTTasks: SpawnerSystem is an opt-in composition dependency and a
// core-list reference would force it into every BT app.
//
// Param conventions (per-task [BTParam] runs, validated here at load):
//   Wait           duration:Float
//   MoveTo         target:String (blackboard Vec3 key)
//   SetBlackboard  key:String, value:Float|Bool|Int|Vec3 (authored type = write type)
//   EmitEvent      event:String
//   SetSMParam     param:String, value:Float|Bool|Int (StateMachine parameter)
//   TriggerSM      param:String
//   SpawnTemplate  template:String[, offset:Vec3]
// ---------------------------------------------------------------------------

struct BTWait : TBTTaskBase<BTWait> {
        static constexpr const char* kName = "Wait";
        static void       Validate(const FlatBuffers::BTTask* pTask);
        static EBTStatus  Tick(STaskCtx& ctx);
};

struct BTMoveTo : TBTTaskBase<BTMoveTo> {
        static constexpr const char* kName = "MoveTo";
        static void       Validate(const FlatBuffers::BTTask* pTask);
        static void       Init(STaskCtx& ctx);
        static EBTStatus  Tick(STaskCtx& ctx);
        static void       Stop(STaskCtx& ctx);
};

struct BTSetBlackboard : TBTTaskBase<BTSetBlackboard> {
        static constexpr const char* kName = "SetBlackboard";
        static void       Validate(const FlatBuffers::BTTask* pTask);
        static EBTStatus  Tick(STaskCtx& ctx);
};

struct BTEmitEvent : TBTTaskBase<BTEmitEvent> {
        static constexpr const char* kName = "EmitEvent";
        static void       Validate(const FlatBuffers::BTTask* pTask);
        static EBTStatus  Tick(STaskCtx& ctx);
};

struct BTSetSMParam : TBTTaskBase<BTSetSMParam> {
        static constexpr const char* kName = "SetSMParam";
        static void       Validate(const FlatBuffers::BTTask* pTask);
        static EBTStatus  Tick(STaskCtx& ctx);
};

struct BTTriggerSM : TBTTaskBase<BTTriggerSM> {
        static constexpr const char* kName = "TriggerSM";
        static void       Validate(const FlatBuffers::BTTask* pTask);
        static EBTStatus  Tick(STaskCtx& ctx);
};

struct BTSpawnTemplate : TBTTaskBase<BTSpawnTemplate> {
        static constexpr const char* kName = "SpawnTemplate";
        static void       Validate(const FlatBuffers::BTTask* pTask);
        static EBTStatus  Tick(STaskCtx& ctx);
};

// Core list: everything an AI app needs that only depends on EntityManager
// (a documented hard compose dependency of BehaviorTreeSystem).
using TCoreBTTasks = MP::TypelistWrapper<BTWait, BTMoveTo, BTSetBlackboard, BTEmitEvent,
                                         BTSetSMParam, BTTriggerSM, BTSpawnTemplate>;

// Opt-in spawner hook (plan D2): bodies live in BehaviorTreeTasksSpawner.tcc,
// which only compositions that concatenate this group include (from the app's
// shadow App.h impl block) — a core-list reference to SpawnerSystem would force
// the spawner into every BT app, since system lookups are compile-time-checked.
//
//   using TMyBTTasks = std::decay_t<decltype(MP::TypelistConcatenate(
//           TCoreBTTasks{}, TSpawnerBTTasks{}))>;
struct BTSpawnFromPopulation : TBTTaskBase<BTSpawnFromPopulation> {
        static constexpr const char* kName = "SpawnFromPopulation";
        static void       Validate(const FlatBuffers::BTTask* pTask);
        static EBTStatus  Tick(STaskCtx& ctx);
};

using TSpawnerBTTasks = MP::TypelistWrapper<BTSpawnFromPopulation>;

} // namespace SE

#endif
