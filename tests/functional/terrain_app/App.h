// ---------------------------------------------------------------------------
// Shadow App.h for the terrain_test target ONLY (include-path priority, same
// precedent as world_app/App.h). Composes the world systems + TerrainSystem
// and the components the fixtures carry. Headless: the Terrain component is
// created with b_renderable=false (or without SE_TERRAIN_ENABLED, which keeps
// the renderable registration compiled out entirely).
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

using TCustomComponents = MP::TypelistWrapper<StateMachine, Terrain, SpawnPoint, SpawnVolume, Population>;

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
template <class...> class EntityTemplateSystem;

using TEntityTemplateSystem = typename MP::Typelist2TmplPack<
        EntityTemplateSystem,
        decltype(MP::TypelistConcatenate(TCoreComponents{}, TCustomComponents{}))
>::Type;

using TCustomSystems = MP::TypelistWrapper<EntityManager, StateMachineSystem, WorldSystem,
                TerrainSystem, TEntityTemplateSystem, SpawnerSystem>;

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
#include <EntityTemplateSystem.h>
#include <spawner/SpawnerSystem.h>

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
#include <terrain/TerrainSystem.tcc>
#include <EntityTemplateSystem.tcc>
#include <world/SpatialIndex.tcc>
#include <spawner/PopulationRuntime.tcc>
#include <spawner/SpawnConditions.tcc>
#include <spawner/SpawnPlacement.tcc>
#include <spawner/SpawnerSystem.tcc>

#endif
#endif

#ifdef FORWARD_CUSTOM_RESOURCES
#ifndef FORWARD_CUSTOM_RESOURCES_GUARD
#define FORWARD_CUSTOM_RESOURCES_GUARD

namespace SE {

class WorldAsset;
class StateMachineAsset;
class TerrainAsset;

using TCustomResources = MP::TypelistWrapper<WorldAsset, StateMachineAsset, TerrainAsset>;

}

#endif
#endif

#ifdef INC_CUSTOM_RESOURCES_HEADER
#ifndef INC_CUSTOM_RESOURCES_HEADER_GUARD
#define INC_CUSTOM_RESOURCES_HEADER_GUARD

#include <world/WorldAsset.h>
#include <hsm/StateMachineAsset.h>
#include <terrain/TerrainAsset.h>

#endif
#endif

#if defined(INC_CUSTOM_RESOURCES_IMPL) && defined(SE_IMPL)
#ifndef INC_CUSTOM_RESOURCES_IMPL_GUARD
#define INC_CUSTOM_RESOURCES_IMPL_GUARD

#include <world/WorldAsset.tcc>
#include <hsm/StateMachineAsset.tcc>
#include <terrain/TerrainAsset.tcc>

#endif
#endif
