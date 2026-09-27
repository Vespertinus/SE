
#ifndef BT_APP_TYPES_SECTION
#define BT_APP_TYPES_SECTION

// ---------------------------------------------------------------------------
// BT trio aliases — derived from ONE task-list alias (plan D3).
//
// Forward declarations only: this section runs during GlobalTypes.h's FIRST
// App.h pass, where TSceneTree is still incomplete — real headers (task types,
// component/system headers) come in the INC_CUSTOM_*_HEADER pass below, where
// the engine types are complete. Incomplete task structs are fine in the
// typelist; TTaskTable's executor table needs them complete only at its own
// instantiation point (the system .tcc, compiled after the header pass).
// ---------------------------------------------------------------------------

namespace MP {
template <class... T> struct TypelistWrapper;
}

namespace SE {

struct BTWait;
struct BTMoveTo;
struct BTSetBlackboard;
struct BTEmitEvent;
struct BTSetSMParam;
struct BTTriggerSM;
struct BTSpawnTemplate;
struct BTDummyTask;
struct BTNextWaypoint;
struct BTSucceedTask;
struct BTFailTask;

using TMyBTTasks = MP::TypelistWrapper<BTWait, BTMoveTo, BTSetBlackboard, BTEmitEvent,
                                       BTSetSMParam, BTTriggerSM, BTSpawnTemplate,
                                       BTDummyTask, BTNextWaypoint, BTSucceedTask, BTFailTask>;

template <class TTasks> class TBehaviorTree;
template <class TTasks> class TBehaviorTreeSystem;
template <class TTasks> class TBehaviorTreeAsset;

using BTComponent = TBehaviorTree<TMyBTTasks>;
using BTSystem    = TBehaviorTreeSystem<TMyBTTasks>;
using BTAsset     = TBehaviorTreeAsset<TMyBTTasks>;

}

#endif

// ---------------------------------------------------------------------------
// Shadow App.h for the behavior_tree_test target ONLY (include-path priority,
// crowd_app precedent). Composes the movement + crowd/nav stack (Step 7/8)
// with the AI layer (Step 9 blackboard + Step 10 behavior trees). Terrain and
// the spawner are deliberately NOT composed — BT must compile and tick without
// them (plan D2); spawner-linked BT tasks live in the opt-in TSpawnerBTTasks.
// ---------------------------------------------------------------------------

#ifdef FORWARD_CUSTOM_COMPONENTS
#ifndef FORWARD_CUSTOM_COMPONENTS_GUARD
#define FORWARD_CUSTOM_COMPONENTS_GUARD

namespace SE {

class InputState;
class CharacterController;
class StateMachine;
class NavAgent;
class PathFollower;
class Blackboard;
class TriggerBinding;

// TriggerBinding rides along: BlackboardSystem's ETriggerFired dispatch
// instantiates GetComponent<TriggerBinding> (the blackboard_app contract).
using TCustomComponents = MP::TypelistWrapper<InputState, CharacterController, StateMachine,
                NavAgent, PathFollower, Blackboard, TriggerBinding, BTComponent>;

}

#endif
#endif

#ifdef INC_CUSTOM_COMPONENTS_HEADER
#ifndef INC_CUSTOM_COMPONENTS_HEADER_GUARD
#define INC_CUSTOM_COMPONENTS_HEADER_GUARD

#include <InputState.h>
#include <CharacterController.h>
#include <hsm/StateMachine.h>
#include <nav/NavAgent.h>
#include <nav/PathFollower.h>
#include <ai/Blackboard.h>
#include <ai/TriggerBinding.h>

#endif
#endif

#if defined(INC_CUSTOM_COMPONENTS_IMPL) && defined(SE_IMPL)
#ifndef INC_CUSTOM_COMPONENTS_IMPL_GUARD
#define INC_CUSTOM_COMPONENTS_IMPL_GUARD

#include <InputState.h>
#include <CharacterController.tcc>
#include <hsm/StateMachine.tcc>
#include <nav/NavAgent.tcc>
#include <nav/PathFollower.tcc>
#include <nav/NavAgentEntityPlugin.tcc>
#include <nav/PathFollowerEntityPlugin.tcc>
#include <ai/Blackboard.tcc>
#include <ai/TriggerBinding.tcc>

#endif
#endif

#ifdef FORWARD_CUSTOM_SYSTEMS
#ifndef FORWARD_CUSTOM_SYSTEMS_GUARD
#define FORWARD_CUSTOM_SYSTEMS_GUARD

namespace SE {

class EntityManager;
class StateMachineSystem;
class WorldSystem;
class BlackboardSystem;
class NavigationSystem;
class CharacterMovementSystem;
// CrowdSystem is a template alias (CrowdSystemT<DetourCrowdAvoidance>) — a
// plain `class CrowdSystem;` forward would conflict.
template <typename TBackend> class CrowdSystemT;
class DetourCrowdAvoidance;
using CrowdSystem = CrowdSystemT<DetourCrowdAvoidance>;
template <class...> class EntityTemplateSystem;

using TEntityTemplateSystem = typename MP::Typelist2TmplPack<
        EntityTemplateSystem,
        decltype(MP::TypelistConcatenate(TCoreComponents{}, TCustomComponents{}))
>::Type;

// Composition order: EntityManager before WorldSystem; BlackboardSystem after
// WorldSystem (ctor dependency); crowd driver after NavigationSystem; the BT
// system last (reads the blackboard, spawns via EntityManager).
using TCustomSystems = MP::TypelistWrapper<EntityManager, StateMachineSystem, WorldSystem,
                BlackboardSystem, NavigationSystem, CharacterMovementSystem,
                CrowdSystem, TEntityTemplateSystem, BTSystem>;

}

#endif
#endif

#ifdef INC_CUSTOM_SYSTEMS_HEADER
#ifndef INC_CUSTOM_SYSTEMS_HEADER_GUARD
#define INC_CUSTOM_SYSTEMS_HEADER_GUARD

#include <EntityManager.h>
#include <hsm/StateMachineSystem.h>
#include <world/WorldSystem.h>
#include <ai/BlackboardSystem.h>
#include <nav/NavigationSystem.h>
#include <CharacterMovementSystem.h>
#include <nav/CrowdSystem.h>
#include <EntityTemplateSystem.h>

// Task types: completes the forward-declared task structs from the types
// section so TTaskTable<TMyBTTasks> can instantiate (system .tcc, next pass).
#include <ai/BehaviorTreeTasks.h>
#include "BTDummyTasks.h"

#endif
#endif

#if defined(INC_CUSTOM_SYSTEMS_IMPL) && defined(SE_IMPL)
#ifndef INC_CUSTOM_SYSTEMS_IMPL_GUARD
#define INC_CUSTOM_SYSTEMS_IMPL_GUARD

#include <hsm/HSMResolver.tcc>
#include <hsm/StateMachineSystem.tcc>
#include <EntityManager.tcc>
#include <world/StreamingDirector.tcc>
#include <world/WorldSystem.tcc>
#include <ai/BlackboardCore.tcc>
#include <ai/BlackboardSystem.tcc>
#include <nav/NavigationSystem.tcc>
#include <CharacterMovementSystem.tcc>
#include <nav/DetourCrowdAvoidance.tcc>
#include <nav/CrowdSystem.tcc>
#include <EntityTemplateSystem.tcc>
// BT: the system .tcc pulls the task, asset and component .tccs (guarded).
#include <ai/BehaviorTreeSystem.tcc>

#endif
#endif

#ifdef FORWARD_CUSTOM_RESOURCES
#ifndef FORWARD_CUSTOM_RESOURCES_GUARD
#define FORWARD_CUSTOM_RESOURCES_GUARD

namespace SE {

class WorldAsset;
class StateMachineAsset;
class NavMeshAsset;
class BlackboardAsset;

using TCustomResources = MP::TypelistWrapper<WorldAsset, StateMachineAsset, NavMeshAsset,
                BlackboardAsset, BTAsset>;

}

#endif
#endif

#ifdef INC_CUSTOM_RESOURCES_HEADER
#ifndef INC_CUSTOM_RESOURCES_HEADER_GUARD
#define INC_CUSTOM_RESOURCES_HEADER_GUARD

#include <world/WorldAsset.h>
#include <hsm/StateMachineAsset.h>
#include <nav/NavMeshAsset.h>
#include <ai/BlackboardAsset.h>

#endif
#endif

#if defined(INC_CUSTOM_RESOURCES_IMPL) && defined(SE_IMPL)
#ifndef INC_CUSTOM_RESOURCES_IMPL_GUARD
#define INC_CUSTOM_RESOURCES_IMPL_GUARD

#include <world/WorldAsset.tcc>
#include <hsm/StateMachineAsset.tcc>
#include <nav/NavMeshAsset.tcc>
#include <ai/BlackboardAsset.tcc>
// BTAsset.tcc rides in via ai/BehaviorTreeSystem.tcc (guarded include).

#endif
#endif
