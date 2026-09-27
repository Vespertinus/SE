// ---------------------------------------------------------------------------
// Shadow App.h for the nav_runtime_test target ONLY (include-path priority,
// same precedent as world_app/terrain_app). Composes the world + terrain +
// spawner stack (terrain_app copy) PLUS the Stage 3 Step 7 navigation runtime:
// NavMeshAsset resource, NavigationSystem, WorldNavBridge, NavAgent.
// ---------------------------------------------------------------------------

#ifdef FORWARD_CUSTOM_COMPONENTS
#ifndef FORWARD_CUSTOM_COMPONENTS_GUARD
#define FORWARD_CUSTOM_COMPONENTS_GUARD

namespace SE {

class StateMachine;
class Terrain;
class SpawnPoint;
class SpawnVolume;
class Population;
class NavAgent;

using TCustomComponents = MP::TypelistWrapper<StateMachine, Terrain,
                SpawnPoint, SpawnVolume, Population, NavAgent>;

}

#endif
#endif

#ifdef INC_CUSTOM_COMPONENTS_HEADER
#ifndef INC_CUSTOM_COMPONENTS_HEADER_GUARD
#define INC_CUSTOM_COMPONENTS_HEADER_GUARD

#include <hsm/StateMachine.h>
#include <terrain/Terrain.h>
#include <spawner/SpawnPoint.h>
#include <spawner/SpawnVolume.h>
#include <spawner/Population.h>
#include <nav/NavAgent.h>

#endif
#endif

#if defined(INC_CUSTOM_COMPONENTS_IMPL) && defined(SE_IMPL)
#ifndef INC_CUSTOM_COMPONENTS_IMPL_GUARD
#define INC_CUSTOM_COMPONENTS_IMPL_GUARD

#include <hsm/StateMachine.tcc>
#include <terrain/Terrain.tcc>
#include <spawner/SpawnVolume.tcc>
#include <spawner/SpawnPoint.tcc>
#include <spawner/Population.tcc>
#include <nav/NavAgent.tcc>

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
class SpawnerSystem;
class NavigationSystem;
class WorldNavBridge;
template <class...> class EntityTemplateSystem;

using TEntityTemplateSystem = typename MP::Typelist2TmplPack<
        EntityTemplateSystem,
        decltype(MP::TypelistConcatenate(TCoreComponents{}, TCustomComponents{}))
>::Type;

// Composition order mirrors the sample wiring: world first, then terrain, then
// the nav runtime, then the bridge (which composes after both).
using TCustomSystems = MP::TypelistWrapper<EntityManager, StateMachineSystem, WorldSystem,
                SpatialIndexSystem, TerrainSystem, NavigationSystem, TEntityTemplateSystem,
                SpawnerSystem, WorldNavBridge>;

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
