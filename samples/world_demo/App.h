// ---------------------------------------------------------------------------
// App.h — world integration demo & validation.
//
// Composes subsystems: world/streaming, terrain grid, spatial
// index, spawners, nav runtime + bridge, crowd, blackboards, behavior trees —
// plus the debug panels for all of them (F3). SE_TERRAIN_ENABLED is defined
// for this target (CMakeLists). Systems are opt-in per the house rule.
//
// Composition order: EntityManager before WorldSystem; TerrainSystem after
// WorldSystem; BlackboardSystem after WorldSystem (ctor dependency);
// crowd driver after NavigationSystem; SpawnerSystem after the index and the
// template system; the BT system last (reads the blackboard, spawns via
// EntityManager). Debuggers ride last.
// ---------------------------------------------------------------------------

// Xlib (reachable through graphics headers in mixed TUs) does
// `#define Bool int` — keep it away from every pass below, whose AI .tccs use
// the generated `Bool` union members (same dance as the entity plugins).
#pragma push_macro("Bool")
#undef Bool

// ---------------------------------------------------------------------------
// BT trio aliases — derived from ONE task-list alias (bt_app pattern). This
// section runs during GlobalTypes.h's FIRST App.h pass, where engine types are
// still incomplete; real headers come in the INC_CUSTOM_*_HEADER passes.
// ---------------------------------------------------------------------------

#ifndef WORLD_DEMO_BT_TYPES
#define WORLD_DEMO_BT_TYPES

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

using TMyBTTasks = MP::TypelistWrapper<BTWait, BTMoveTo, BTSetBlackboard, BTEmitEvent,
                                       BTSetSMParam, BTTriggerSM, BTSpawnTemplate>;

template <class TTasks> class TBehaviorTree;
template <class TTasks> class TBehaviorTreeSystem;
template <class TTasks> class TBehaviorTreeAsset;
template <class TTasks> class TBehaviorTreeDebugger;

using BTComponent         = TBehaviorTree<TMyBTTasks>;
using BTSystem            = TBehaviorTreeSystem<TMyBTTasks>;
using BTAsset             = TBehaviorTreeAsset<TMyBTTasks>;
using BehaviorTreeDebugger = TBehaviorTreeDebugger<TMyBTTasks>;

}

#endif // WORLD_DEMO_BT_TYPES

#ifdef FORWARD_CUSTOM_COMPONENTS
#ifndef FORWARD_CUSTOM_COMPONENTS_GUARD
#define FORWARD_CUSTOM_COMPONENTS_GUARD

namespace SE {

class InputState;
class InputMappingContext;
class CharacterController;
class StateMachine;
class TriggerVolume;
class Terrain;
class SpatialEntry;
class SpawnPoint;
class SpawnVolume;
class Population;
class NavAgent;
class PathFollower;
class TriggerBinding;
class Blackboard;
class Portal;

using TCustomComponents = MP::TypelistWrapper<InputState, InputMappingContext,
                CharacterController, StateMachine, TriggerVolume, Terrain, SpatialEntry,
                SpawnPoint, SpawnVolume, Population, NavAgent, PathFollower,
                TriggerBinding, Blackboard, Portal, BTComponent>;

}

#endif
#endif

#ifdef INC_CUSTOM_COMPONENTS_HEADER
#ifndef INC_CUSTOM_COMPONENTS_HEADER_GUARD
#define INC_CUSTOM_COMPONENTS_HEADER_GUARD

#include <InputState.h>
#include <InputMappingContext.h>
#include <CharacterController.h>
#include <hsm/StateMachine.h>
#include <TriggerVolume.h>
#include <terrain/Terrain.h>
#include <world/SpatialEntry.h>
#include <spawner/SpawnPoint.h>
#include <spawner/SpawnVolume.h>
#include <spawner/Population.h>
#include <nav/NavAgent.h>
#include <nav/PathFollower.h>
#include <ai/Blackboard.h>
#include <ai/TriggerBinding.h>
#include <world/Portal.h>

#endif
#endif

#if defined(INC_CUSTOM_COMPONENTS_IMPL) && defined(SE_IMPL)
#ifndef INC_CUSTOM_COMPONENTS_IMPL_GUARD
#define INC_CUSTOM_COMPONENTS_IMPL_GUARD

#include <InputState.h>
#include <InputMappingContext.tcc>
#include <CharacterController.tcc>
#include <hsm/StateMachine.tcc>
#include <TriggerVolume.tcc>
#include <terrain/Terrain.tcc>
#include <world/SpatialEntry.tcc>
#include <spawner/SpawnPoint.tcc>
#include <spawner/SpawnVolume.tcc>
#include <spawner/Population.tcc>
#include <nav/NavAgent.tcc>
#include <nav/PathFollower.tcc>
#include <nav/NavAgentEntityPlugin.tcc>
#include <nav/PathFollowerEntityPlugin.tcc>
#include <ai/Blackboard.tcc>
#include <ai/TriggerBinding.tcc>
#include <world/Portal.tcc>

#endif
#endif

#ifdef FORWARD_CUSTOM_SYSTEMS
#ifndef FORWARD_CUSTOM_SYSTEMS_GUARD
#define FORWARD_CUSTOM_SYSTEMS_GUARD

namespace SE {

class EntityManager;
class StateMachineSystem;
class CharacterAnimationSystem;
class WorldSystem;
class TerrainSystem;
class BlackboardSystem;
class SpatialIndexSystem;
class NavigationSystem;
class CharacterMovementSystem;
class WorldNavBridge;
class SpawnerSystem;
// CrowdSystem is a template alias (CrowdSystemT<DetourCrowdAvoidance>) — a
// plain `class CrowdSystem;` forward would conflict.
template <typename TBackend> class CrowdSystemT;
class DetourCrowdAvoidance;
using CrowdSystem = CrowdSystemT<DetourCrowdAvoidance>;
template <class...> class EntityTemplateSystem;

class StreamingDebugger;
class TerrainDebugger;
class SpatialIndexDebugger;
class SpawnerDebugger;
class NavigationDebugger;
class CrowdDebugger;
class BlackboardInspector;

using TEntityTemplateSystem = typename MP::Typelist2TmplPack<
        EntityTemplateSystem,
        decltype(MP::TypelistConcatenate(TCoreComponents{}, TCustomComponents{}))
>::Type;

using TCustomSystems = MP::TypelistWrapper<EntityManager, StateMachineSystem,
                CharacterAnimationSystem, WorldSystem, TerrainSystem, BlackboardSystem,
                SpatialIndexSystem, NavigationSystem, CharacterMovementSystem, WorldNavBridge,
                CrowdSystem, TEntityTemplateSystem, SpawnerSystem, BTSystem,
                StreamingDebugger, TerrainDebugger, SpatialIndexDebugger, SpawnerDebugger,
                NavigationDebugger, CrowdDebugger, BehaviorTreeDebugger, BlackboardInspector>;

}

#endif
#endif

#ifdef INC_CUSTOM_SYSTEMS_HEADER
#ifndef INC_CUSTOM_SYSTEMS_HEADER_GUARD
#define INC_CUSTOM_SYSTEMS_HEADER_GUARD

#include <EntityManager.h>
#include <hsm/StateMachineSystem.h>
#include <CharacterAnimationSystem.h>
#include <world/WorldSystem.h>
#include <terrain/TerrainSystem.h>
#include <ai/BlackboardSystem.h>
#include <world/SpatialIndexSystem.h>
#include <nav/NavigationSystem.h>
#include <CharacterMovementSystem.h>
#include <nav/WorldNavBridge.h>
#include <nav/CrowdSystem.h>
#include <EntityTemplateSystem.h>
#include <spawner/PopulationRuntime.h>
#include <spawner/SpawnerSystem.h>

// Task types: completes the forward-declared task structs from the types
// section so TTaskTable<TMyBTTasks> can instantiate (system .tcc, next pass).
// The system and debugger classes themselves must ALSO be complete in this
// pass: every TU that includes GlobalTypes.h instantiates ~Engine (Engine.h
// pulls Engine.tcc), and a TU without SE_IMPL (ImGuiWrapper.cpp) never
// reaches the IMPL block below — an incomplete tuple member there makes
// 'delete pSystem' in ~Engine skip the destructor silently.
#include <ai/BehaviorTreeTasks.h>
#include <ai/BehaviorTreeAsset.h>
#include <ai/BehaviorTreeSystem.h>
#include <ai/BehaviorTreeDebugger.h>

#include <world/StreamingDebugger.h>
#include <terrain/TerrainDebugger.h>
#include <world/SpatialIndexDebugger.h>
#include <spawner/SpawnerDebugger.h>
#include <nav/NavigationDebugger.h>
#include <nav/CrowdDebugger.h>
#include <ai/BlackboardInspector.h>

#endif
#endif

#if defined(INC_CUSTOM_SYSTEMS_IMPL) && defined(SE_IMPL)
#ifndef INC_CUSTOM_SYSTEMS_IMPL_GUARD
#define INC_CUSTOM_SYSTEMS_IMPL_GUARD

#include <hsm/HSMResolver.tcc>
#include <hsm/StateMachineSystem.tcc>
#include <CharacterAnimationSystem.tcc>
#include <EntityManager.tcc>
#include <world/StreamingDirector.tcc>
#include <world/WorldSystem.tcc>
#include <world/SpatialIndex.tcc>
#include <world/SpatialIndexSystem.tcc>
#include <terrain/TerrainSystem.tcc>
#include <ai/BlackboardSystem.tcc>
#include <nav/NavigationSystem.tcc>
#include <CharacterMovementSystem.tcc>
#include <nav/WorldNavBridge.tcc>
#include <nav/DetourCrowdAvoidance.tcc>
#include <nav/CrowdSystem.tcc>
#include <EntityTemplateSystem.tcc>
#include <spawner/PopulationRuntime.tcc>
#include <spawner/SpawnConditions.tcc>
#include <spawner/SpawnPlacement.tcc>
#include <spawner/SpawnerSystem.tcc>
// BT: the system .tcc pulls the task, asset and component .tccs (guarded).
#include <ai/BehaviorTreeSystem.tcc>
#include <ai/BehaviorTreeDebugger.tcc>

#include <world/StreamingDebugger.tcc>
#include <terrain/TerrainDebugger.tcc>
#include <world/SpatialIndexDebugger.tcc>
#include <spawner/SpawnerDebugger.tcc>
#include <nav/NavigationDebugger.tcc>
#include <nav/CrowdDebugger.tcc>
#include <ai/BlackboardInspector.tcc>

#endif
#endif

#ifdef FORWARD_CUSTOM_RESOURCES
#ifndef FORWARD_CUSTOM_RESOURCES_GUARD
#define FORWARD_CUSTOM_RESOURCES_GUARD

namespace SE {

class WorldAsset;
class TerrainAsset;
class StateMachineAsset;
class NavMeshAsset;
class BlackboardAsset;

using TCustomResources = MP::TypelistWrapper<WorldAsset, TerrainAsset, StateMachineAsset,
                NavMeshAsset, BlackboardAsset, BTAsset>;

}

#endif
#endif

#ifdef INC_CUSTOM_RESOURCES_HEADER
#ifndef INC_CUSTOM_RESOURCES_HEADER_GUARD
#define INC_CUSTOM_RESOURCES_HEADER_GUARD

#include <world/WorldAsset.h>
#include <terrain/TerrainAsset.h>
#include <hsm/StateMachineAsset.h>
#include <nav/NavMeshAsset.h>
#include <ai/BlackboardAsset.h>

#endif
#endif

#if defined(INC_CUSTOM_RESOURCES_IMPL) && defined(SE_IMPL)
#ifndef INC_CUSTOM_RESOURCES_IMPL_GUARD
#define INC_CUSTOM_RESOURCES_IMPL_GUARD

#include <world/WorldAsset.tcc>
#include <terrain/TerrainAsset.tcc>
#include <hsm/StateMachineAsset.tcc>
#include <nav/NavMeshAsset.tcc>
// BlackboardAsset.tcc + BTAsset.tcc ride in via ai/BehaviorTreeSystem.tcc.

#endif
#endif

// restore the caller's `Bool` macro (see the dance at the top of this file)
#pragma pop_macro("Bool")
