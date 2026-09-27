// ---------------------------------------------------------------------------
// Shadow App.h for the blackboard_test target ONLY (include-path priority,
// world_app/App.h precedent). Composes the world + AI (Step 9) systems with
// the Blackboard/TriggerBinding components — the minimal wiring example for
// the blackboard subsystem. tests/App.h (every other target) keeps the custom
// lists empty. Mirrors samples/moving_actors/App.h.
// ---------------------------------------------------------------------------

#ifdef FORWARD_CUSTOM_COMPONENTS
#ifndef FORWARD_CUSTOM_COMPONENTS_GUARD
#define FORWARD_CUSTOM_COMPONENTS_GUARD

namespace SE {

class Blackboard;
class TriggerBinding;

using TCustomComponents = MP::TypelistWrapper<Blackboard, TriggerBinding>;

}

#endif
#endif

#ifdef INC_CUSTOM_COMPONENTS_HEADER
#ifndef INC_CUSTOM_COMPONENTS_HEADER_GUARD
#define INC_CUSTOM_COMPONENTS_HEADER_GUARD

#include <ai/Blackboard.h>
#include <ai/TriggerBinding.h>

#endif
#endif

#if defined(INC_CUSTOM_COMPONENTS_IMPL) && defined(SE_IMPL)
#ifndef INC_CUSTOM_COMPONENTS_IMPL_GUARD
#define INC_CUSTOM_COMPONENTS_IMPL_GUARD

#include <ai/Blackboard.tcc>
#include <ai/TriggerBinding.tcc>

#endif
#endif

#ifdef FORWARD_CUSTOM_SYSTEMS
#ifndef FORWARD_CUSTOM_SYSTEMS_GUARD
#define FORWARD_CUSTOM_SYSTEMS_GUARD

namespace SE {

class EntityManager;
class WorldSystem;
class BlackboardSystem;

// BlackboardSystem AFTER WorldSystem (ctor dependency — SpawnerSystem precedent);
// EntityManager before WorldSystem (SpawnerTest ordering convention).
using TCustomSystems = MP::TypelistWrapper<EntityManager, WorldSystem, BlackboardSystem>;

}

#endif
#endif

#ifdef INC_CUSTOM_SYSTEMS_HEADER
#ifndef INC_CUSTOM_SYSTEMS_HEADER_GUARD
#define INC_CUSTOM_SYSTEMS_HEADER_GUARD

#include <EntityManager.h>
#include <world/WorldSystem.h>
#include <ai/BlackboardSystem.h>

#endif
#endif

#if defined(INC_CUSTOM_SYSTEMS_IMPL) && defined(SE_IMPL)
#ifndef INC_CUSTOM_SYSTEMS_IMPL_GUARD
#define INC_CUSTOM_SYSTEMS_IMPL_GUARD

#include <EntityManager.tcc>
#include <world/StreamingDirector.tcc>
#include <world/WorldSystem.tcc>
#include <ai/BlackboardCore.tcc>
#include <ai/BlackboardSystem.tcc>
// BlackboardAsset.tcc rides in the resources impl block below (its natural
// home; the systems block only needs its declarations for CreateResource)

#endif
#endif

#ifdef FORWARD_CUSTOM_RESOURCES
#ifndef FORWARD_CUSTOM_RESOURCES_GUARD
#define FORWARD_CUSTOM_RESOURCES_GUARD

namespace SE {

class WorldAsset;
class BlackboardAsset;

using TCustomResources = MP::TypelistWrapper<WorldAsset, BlackboardAsset>;

}

#endif
#endif

#ifdef INC_CUSTOM_RESOURCES_HEADER
#ifndef INC_CUSTOM_RESOURCES_HEADER_GUARD
#define INC_CUSTOM_RESOURCES_HEADER_GUARD

#include <world/WorldAsset.h>
#include <ai/BlackboardAsset.h>

#endif
#endif

#if defined(INC_CUSTOM_RESOURCES_IMPL) && defined(SE_IMPL)
#ifndef INC_CUSTOM_RESOURCES_IMPL_GUARD
#define INC_CUSTOM_RESOURCES_IMPL_GUARD

#include <world/WorldAsset.tcc>
#include <ai/BlackboardAsset.tcc>

#endif
#endif
