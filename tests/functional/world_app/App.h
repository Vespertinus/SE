// ---------------------------------------------------------------------------
// Shadow App.h for the world_asset_test / world_system_test targets ONLY
// (include-path priority, same precedent as trigger_app/App.h). It composes
// the world systems with the components the sub-level fixtures carry —
// tests/App.h (shared by every other target) deliberately keeps custom
// components/systems/resources empty. Mirrors samples/moving_actors/App.h.
// ---------------------------------------------------------------------------

#ifdef FORWARD_CUSTOM_COMPONENTS
#ifndef FORWARD_CUSTOM_COMPONENTS_GUARD
#define FORWARD_CUSTOM_COMPONENTS_GUARD

// RigidBody is a core component already; the fixture sub-levels carry
// StateMachine (app component), the Step 4 SpatialEntry marker and the
// Step 5 spawner components so the .sesc loader picks them up.

namespace SE {

class StateMachine;
class SpatialEntry;
class SpawnPoint;
class SpawnVolume;
class Population;
class Blackboard;       // Stage 3 Step 9 (WorldFlag population conditions,
class TriggerBinding;   // trigger→blackboard writes on spawner test worlds)

using TCustomComponents = MP::TypelistWrapper<StateMachine, SpatialEntry, SpawnPoint, SpawnVolume,
                Population, Blackboard, TriggerBinding>;

}

#endif
#endif

#ifdef INC_CUSTOM_COMPONENTS_HEADER
#ifndef INC_CUSTOM_COMPONENTS_HEADER_GUARD
#define INC_CUSTOM_COMPONENTS_HEADER_GUARD

// StateMachine is a core component, but its full definition (and therefore its
// THasSerialized loader trait) only exists where this header is included.
#include <hsm/StateMachine.h>
#include <world/SpatialEntry.h>
#include <spawner/SpawnPoint.h>
#include <spawner/SpawnVolume.h>
#include <spawner/Population.h>
#include <ai/Blackboard.h>
#include <ai/TriggerBinding.h>

#endif
#endif

#if defined(INC_CUSTOM_COMPONENTS_IMPL) && defined(SE_IMPL)
#ifndef INC_CUSTOM_COMPONENTS_IMPL_GUARD
#define INC_CUSTOM_COMPONENTS_IMPL_GUARD

// StateMachine is a core component, but its impl is pulled per-app
// (StateMachineSystem references StateMachine::GetAsset).
#include <hsm/StateMachine.tcc>
#include <world/SpatialEntry.tcc>
#include <spawner/SpawnVolume.tcc>
#include <spawner/SpawnPoint.tcc>
#include <spawner/Population.tcc>
// component impls may reference the AI registry (systems impl runs first)
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
class SpatialIndexSystem;
class SpawnerSystem;
class BlackboardSystem;
template <class...> class EntityTemplateSystem;

// SpatialIndexSystem composes AFTER WorldSystem (ctor dependency — the
// TerrainSystem precedent); SpawnerSystem after WorldSystem + EntityManager;
// EntityTemplateSystem over the full component pack (simple_engine pattern).
using TEntityTemplateSystem = typename MP::Typelist2TmplPack<
        EntityTemplateSystem,
        decltype(MP::TypelistConcatenate(TCoreComponents{}, TCustomComponents{}))
>::Type;

using TCustomSystems = MP::TypelistWrapper<EntityManager, StateMachineSystem, WorldSystem,
                BlackboardSystem, SpatialIndexSystem, TEntityTemplateSystem, SpawnerSystem>;

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
#include <world/SpatialIndexSystem.h>
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
#include <world/SpatialIndex.tcc>
#include <world/SpatialIndexSystem.tcc>
#include <EntityTemplateSystem.tcc>
#include <spawner/PopulationRuntime.tcc>
#include <spawner/SpawnConditions.tcc>
#include <spawner/SpawnPlacement.tcc>
#include <spawner/SpawnerSystem.tcc>
// Step 9: WorldFlag conditions evaluate through BlackboardCore (single
// inclusion — SpawnConditions.tcc pulls the core's method declarations and
// this provides the definitions)
// Step 9: the BlackboardSystem driver; the asset + core definitions arrive
// transitively via SpawnConditions.tcc above (the asset .tcc must not be
// included twice)
#include <ai/BlackboardSystem.tcc>

#endif
#endif

#ifdef FORWARD_CUSTOM_RESOURCES
#ifndef FORWARD_CUSTOM_RESOURCES_GUARD
#define FORWARD_CUSTOM_RESOURCES_GUARD

namespace SE {

class WorldAsset;
class StateMachineAsset;
class BlackboardAsset;

using TCustomResources = MP::TypelistWrapper<WorldAsset, StateMachineAsset, BlackboardAsset>;

}

#endif
#endif

#ifdef INC_CUSTOM_RESOURCES_HEADER
#ifndef INC_CUSTOM_RESOURCES_HEADER_GUARD
#define INC_CUSTOM_RESOURCES_HEADER_GUARD

#include <world/WorldAsset.h>
#include <hsm/StateMachineAsset.h>
#include <ai/BlackboardAsset.h>

#endif
#endif

#if defined(INC_CUSTOM_RESOURCES_IMPL) && defined(SE_IMPL)
#ifndef INC_CUSTOM_RESOURCES_IMPL_GUARD
#define INC_CUSTOM_RESOURCES_IMPL_GUARD

#include <world/WorldAsset.tcc>
#include <hsm/StateMachineAsset.tcc>
// BlackboardAsset.tcc arrives transitively via SpawnConditions.tcc (systems
// impl); a second inclusion here would redefine its symbols.

#endif
#endif
