// ---------------------------------------------------------------------------
// Shadow App.h for the crowd_test target ONLY (include-path priority, same
// precedent as nav_app). Composes the full nav_app stack PLUS the Stage 3
// Step 8 steering layer: CharacterController + InputState + CharacterMovement-
// System (the player movement stack NPCs reuse), PathFollower, CrowdSystem
// (CrowdSystemT<DetourCrowdAvoidance>).
// ---------------------------------------------------------------------------

#ifdef FORWARD_CUSTOM_COMPONENTS
#ifndef FORWARD_CUSTOM_COMPONENTS_GUARD
#define FORWARD_CUSTOM_COMPONENTS_GUARD

namespace SE {

class InputState;
class CharacterController;
class StateMachine;
class Terrain;
class SpawnPoint;
class SpawnVolume;
class Population;
class NavAgent;
class PathFollower;

using TCustomComponents = MP::TypelistWrapper<InputState, CharacterController, StateMachine, Terrain,
                SpawnPoint, SpawnVolume, Population, NavAgent, PathFollower>;

}

#endif
#endif

#ifdef INC_CUSTOM_COMPONENTS_HEADER
#ifndef INC_CUSTOM_COMPONENTS_HEADER_GUARD
#define INC_CUSTOM_COMPONENTS_HEADER_GUARD

#include <InputState.h>
#include <CharacterController.h>
#include <hsm/StateMachine.h>
#include <terrain/Terrain.h>
#include <spawner/SpawnPoint.h>
#include <spawner/SpawnVolume.h>
#include <spawner/Population.h>
#include <nav/NavAgent.h>
#include <nav/PathFollower.h>

#endif
#endif

#if defined(INC_CUSTOM_COMPONENTS_IMPL) && defined(SE_IMPL)
#ifndef INC_CUSTOM_COMPONENTS_IMPL_GUARD
#define INC_CUSTOM_COMPONENTS_IMPL_GUARD

#include <InputState.h>
#include <CharacterController.tcc>
#include <hsm/StateMachine.tcc>
#include <terrain/Terrain.tcc>
#include <spawner/SpawnVolume.tcc>
#include <spawner/SpawnPoint.tcc>
#include <spawner/Population.tcc>
#include <nav/NavAgent.tcc>
#include <nav/PathFollower.tcc>
#include <nav/NavAgentEntityPlugin.tcc>
#include <nav/PathFollowerEntityPlugin.tcc>

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
class SpatialIndexSystem;
class CharacterMovementSystem;
class NavigationSystem;
// CrowdSystem is a template alias (CrowdSystemT<DetourCrowdAvoidance>) — a
// plain `class CrowdSystem;` forward would conflict.
template <typename TBackend> class CrowdSystemT;
class DetourCrowdAvoidance;
using CrowdSystem = CrowdSystemT<DetourCrowdAvoidance>;
class SpawnerSystem;
class WorldNavBridge;
template <class...> class EntityTemplateSystem;

using TEntityTemplateSystem = typename MP::Typelist2TmplPack<
        EntityTemplateSystem,
        decltype(MP::TypelistConcatenate(TCoreComponents{}, TCustomComponents{}))
>::Type;

// Composition order: world, terrain, nav runtime, movement stack, then the
// crowd driver (after NavigationSystem — its query/filter/corridor source),
// then the template/spawner/bridge layer.
using TCustomSystems = MP::TypelistWrapper<EntityManager, StateMachineSystem, WorldSystem,
                SpatialIndexSystem, TerrainSystem, NavigationSystem, CharacterMovementSystem,
                CrowdSystem, TEntityTemplateSystem, SpawnerSystem, WorldNavBridge>;

}

#endif
#endif

#ifdef INC_CUSTOM_SYSTEMS_HEADER
#ifndef INC_CUSTOM_SYSTEMS_HEADER_GUARD
#define INC_CUSTOM_SYSTEMS_HEADER_GUARD

#include <EntityManager.h>
#include <hsm/StateMachineSystem.h>
#include <world/WorldSystem.h>
#include <world/SpatialIndexSystem.h>
#include <terrain/TerrainSystem.h>
#include <nav/NavigationSystem.h>
#include <CharacterMovementSystem.h>
#include <nav/CrowdSystem.h>
#include <EntityTemplateSystem.h>
#include <spawner/SpawnerSystem.h>
#include <nav/WorldNavBridge.h>

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
#include <world/SpatialIndexSystem.tcc>
#include <terrain/TerrainSystem.tcc>
#include <nav/NavigationSystem.tcc>
#include <CharacterMovementSystem.tcc>
#include <nav/DetourCrowdAvoidance.tcc>
#include <nav/CrowdSystem.tcc>
#include <EntityTemplateSystem.tcc>
#include <spawner/PopulationRuntime.tcc>
#include <spawner/SpawnConditions.tcc>
#include <spawner/SpawnPlacement.tcc>
#include <spawner/SpawnerSystem.tcc>
#include <nav/WorldNavBridge.tcc>

#endif
#endif

#ifdef FORWARD_CUSTOM_RESOURCES
#ifndef FORWARD_CUSTOM_RESOURCES_GUARD
#define FORWARD_CUSTOM_RESOURCES_GUARD

namespace SE {

class WorldAsset;
class StateMachineAsset;
class TerrainAsset;
class NavMeshAsset;

using TCustomResources = MP::TypelistWrapper<WorldAsset, StateMachineAsset, TerrainAsset, NavMeshAsset>;

}

#endif
#endif

#ifdef INC_CUSTOM_RESOURCES_HEADER
#ifndef INC_CUSTOM_RESOURCES_HEADER_GUARD
#define INC_CUSTOM_RESOURCES_HEADER_GUARD

#include <world/WorldAsset.h>
#include <hsm/StateMachineAsset.h>
#include <terrain/TerrainAsset.h>
#include <nav/NavMeshAsset.h>

#endif
#endif

#if defined(INC_CUSTOM_RESOURCES_IMPL) && defined(SE_IMPL)
#ifndef INC_CUSTOM_RESOURCES_IMPL_GUARD
#define INC_CUSTOM_RESOURCES_IMPL_GUARD

#include <world/WorldAsset.tcc>
#include <hsm/StateMachineAsset.tcc>
#include <terrain/TerrainAsset.tcc>
#include <nav/NavMeshAsset.tcc>

#endif
#endif
