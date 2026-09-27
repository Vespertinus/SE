// ---------------------------------------------------------------------------
// Shadow App.h for the world_demo test targets (include-path priority,
// world_app/bt_app precedent). Full Stage 3 headless composition: world +
// terrain + spatial index + spawner + nav runtime + bridge + crowd + movement
// + blackboard + behavior trees. No renderers, no audio, no debug panels.
// ---------------------------------------------------------------------------

// BT trio aliases (bt_app pattern) — complete before the passes.
#ifndef WORLDDEMO_APP_BT_TYPES
#define WORLDDEMO_APP_BT_TYPES

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

using BTComponent = TBehaviorTree<TMyBTTasks>;
using BTSystem    = TBehaviorTreeSystem<TMyBTTasks>;
using BTAsset     = TBehaviorTreeAsset<TMyBTTasks>;

}

#endif // WORLDDEMO_APP_BT_TYPES

#ifdef FORWARD_CUSTOM_COMPONENTS
#ifndef FORWARD_CUSTOM_COMPONENTS_GUARD
#define FORWARD_CUSTOM_COMPONENTS_GUARD

namespace SE {

class InputState;
class CharacterController;
class StateMachine;
class TriggerVolume;
class SpatialEntry;
class SpawnPoint;
class SpawnVolume;
class Population;
class NavAgent;
class PathFollower;
class TriggerBinding;
class Blackboard;
class Portal;

using TCustomComponents = MP::TypelistWrapper<InputState, CharacterController, StateMachine,
                TriggerVolume, SpatialEntry, SpawnPoint, SpawnVolume, Population,
                NavAgent, PathFollower, TriggerBinding, Blackboard, Portal, BTComponent>;

}

#endif
#endif

#ifdef INC_CUSTOM_COMPONENTS_HEADER
#ifndef INC_CUSTOM_COMPONENTS_HEADER_GUARD
#define INC_CUSTOM_COMPONENTS_HEADER_GUARD

#include <InputState.h>
#include <CharacterController.h>
#include <hsm/StateMachine.h>
#include <TriggerVolume.h>
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
#include <CharacterController.tcc>
#include <hsm/StateMachine.tcc>
#include <TriggerVolume.tcc>
#include <world/SpatialEntry.tcc>
#include <spawner/SpawnVolume.tcc>
#include <spawner/SpawnPoint.tcc>
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
class WorldSystem;
class TerrainSystem;
class BlackboardSystem;
class SpatialIndexSystem;
class NavigationSystem;
class CharacterMovementSystem;
class WorldNavBridge;
class SpawnerSystem;
template <typename TBackend> class CrowdSystemT;
class DetourCrowdAvoidance;
using CrowdSystem = CrowdSystemT<DetourCrowdAvoidance>;
template <class...> class EntityTemplateSystem;

using TEntityTemplateSystem = typename MP::Typelist2TmplPack<
        EntityTemplateSystem,
        decltype(MP::TypelistConcatenate(TCoreComponents{}, TCustomComponents{}))
>::Type;

// Order: EntityManager before WorldSystem; TerrainSystem after WorldSystem;
// BlackboardSystem after WorldSystem; crowd after NavigationSystem; the spawner
// after the index and the template system; the BT system last.
using TCustomSystems = MP::TypelistWrapper<EntityManager, StateMachineSystem, WorldSystem,
                TerrainSystem, BlackboardSystem, SpatialIndexSystem, NavigationSystem,
                CharacterMovementSystem, WorldNavBridge, CrowdSystem,
                TEntityTemplateSystem, SpawnerSystem, BTSystem>;

}

#endif
#endif

#ifdef INC_CUSTOM_SYSTEMS_HEADER
#ifndef INC_CUSTOM_SYSTEMS_HEADER_GUARD
#define INC_CUSTOM_SYSTEMS_HEADER_GUARD

#include <EntityManager.h>
#include <hsm/StateMachineSystem.h>
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
#include <ai/BehaviorTreeTasks.h>
#include <ai/BehaviorTreeAsset.h>

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
#include <world/SpatialIndex.tcc>
#include <terrain/TerrainSystem.tcc>
#include <ai/BlackboardSystem.tcc>
#include <world/SpatialIndexSystem.tcc>
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
#include <ai/BehaviorTreeSystem.tcc>

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
