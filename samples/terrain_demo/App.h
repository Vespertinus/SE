// ---------------------------------------------------------------------------
// App.h — Terrain demo
//
// SE_TERRAIN_ENABLED is defined for this target (CMakeLists) — the Terrain
// component joins the visibility list and renders through GeometryPass +
// ShadowPass. Systems are opt-in per the house rule.
// ---------------------------------------------------------------------------

#ifdef FORWARD_CUSTOM_COMPONENTS
#ifndef FORWARD_CUSTOM_COMPONENTS_GUARD
#define FORWARD_CUSTOM_COMPONENTS_GUARD

namespace SE {

// CrowdSystem's pipeline resolves NavAgent / PathFollower /
// InputState / CharacterController siblings per tick — SceneTree GetComponent
// is typelist-indexed, so a crowd-composing app must carry them here (inert
// until a scene instantiates them).
class InputState;
class CharacterController;
class NavAgent;
class PathFollower;
class Terrain;

using TCustomComponents = MP::TypelistWrapper<InputState, CharacterController, Terrain, NavAgent, PathFollower>;

}

#endif
#endif

#ifdef INC_CUSTOM_COMPONENTS_HEADER
#ifndef INC_CUSTOM_COMPONENTS_HEADER_GUARD
#define INC_CUSTOM_COMPONENTS_HEADER_GUARD

#include <InputState.h>
#include <CharacterController.h>
#include <terrain/Terrain.h>
#include <nav/NavAgent.h>
#include <nav/PathFollower.h>

#endif
#endif

#if defined(INC_CUSTOM_COMPONENTS_IMPL) && defined(SE_IMPL)
#ifndef INC_CUSTOM_COMPONENTS_IMPL_GUARD
#define INC_CUSTOM_COMPONENTS_IMPL_GUARD

#include <InputState.h>
#include <CharacterController.tcc>
#include <terrain/Terrain.tcc>
#include <nav/NavAgent.tcc>
#include <nav/PathFollower.tcc>
#include <nav/PathFollowerEntityPlugin.tcc>

#endif
#endif

#ifdef FORWARD_CUSTOM_SYSTEMS
#ifndef FORWARD_CUSTOM_SYSTEMS_GUARD
#define FORWARD_CUSTOM_SYSTEMS_GUARD

namespace SE {

class EntityManager;
class WorldSystem;
class TerrainSystem;
class CharacterMovementSystem;
class NavigationSystem;
class WorldNavBridge;
// CrowdSystem is a template alias (CrowdSystemT<DetourCrowdAvoidance>) — a
// plain `class CrowdSystem;` forward would conflict.
template <typename TBackend> class CrowdSystemT;
class DetourCrowdAvoidance;
using CrowdSystem = CrowdSystemT<DetourCrowdAvoidance>;
class StreamingDebugger;
class TerrainDebugger;
class NavigationDebugger;

// the nav runtime + bridge ride along inert (this demo's world declares no nav table) 
// the composition contract check for a nav-aware app.
// CrowdSystem rides along the same way (no NavAgent ever enables here).
using TCustomSystems = MP::TypelistWrapper<
        EntityManager,
        WorldSystem,
        TerrainSystem,
        CharacterMovementSystem,
        NavigationSystem,
        WorldNavBridge,
        CrowdSystem,
        StreamingDebugger,
        TerrainDebugger,
        NavigationDebugger>;

}

#endif
#endif

#ifdef INC_CUSTOM_SYSTEMS_HEADER
#ifndef INC_CUSTOM_SYSTEMS_HEADER_GUARD
#define INC_CUSTOM_SYSTEMS_HEADER_GUARD

#include <EntityManager.h>
#include <world/WorldSystem.h>
#include <terrain/TerrainSystem.h>
#include <CharacterMovementSystem.h>
#include <nav/NavigationSystem.h>
#include <nav/WorldNavBridge.h>
#include <nav/CrowdSystem.h>
#include <world/StreamingDebugger.h>
#include <terrain/TerrainDebugger.h>
#include <nav/NavigationDebugger.h>

#endif
#endif

#if defined(INC_CUSTOM_SYSTEMS_IMPL) && defined(SE_IMPL)
#ifndef INC_CUSTOM_SYSTEMS_IMPL_GUARD
#define INC_CUSTOM_SYSTEMS_IMPL_GUARD

#include <EntityManager.tcc>
#include <world/StreamingDirector.tcc>
#include <world/WorldSystem.tcc>
#include <terrain/TerrainSystem.tcc>
#include <CharacterMovementSystem.tcc>
#include <nav/NavigationSystem.tcc>
#include <nav/WorldNavBridge.tcc>
#include <nav/DetourCrowdAvoidance.tcc>
#include <nav/CrowdSystem.tcc>
#include <world/StreamingDebugger.tcc>
#include <terrain/TerrainDebugger.tcc>
#include <nav/NavigationDebugger.tcc>

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

using TCustomResources = MP::TypelistWrapper<WorldAsset, TerrainAsset, StateMachineAsset, NavMeshAsset>;

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

#endif
#endif

#if defined(INC_CUSTOM_RESOURCES_IMPL) && defined(SE_IMPL)
#ifndef INC_CUSTOM_RESOURCES_IMPL_GUARD
#define INC_CUSTOM_RESOURCES_IMPL_GUARD

#include <world/WorldAsset.tcc>
#include <terrain/TerrainAsset.tcc>
#include <hsm/StateMachineAsset.tcc>
#include <nav/NavMeshAsset.tcc>

#endif
#endif
