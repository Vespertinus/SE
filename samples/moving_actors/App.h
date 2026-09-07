
// ---------------------------------------------------------------------------
// App.h — Moving Actors demo
//
// TCustomComponents / TCustomSystems / TCustomResources declarations and includes.
// All Stage 2 Moving Actors subsystems are opt-in here.
// ---------------------------------------------------------------------------

// ---- Forward declarations --------------------------------------------------
#ifdef FORWARD_CUSTOM_COMPONENTS
#ifndef FORWARD_CUSTOM_COMPONENTS_GUARD
#define FORWARD_CUSTOM_COMPONENTS_GUARD

namespace SE {

class InputMappingContext;
class InputState;
class CharacterController;
class SprintStamina;
class AIBrain;

namespace HSM { }  // StateMachine lives in the SE namespace

class StateMachine;

using TCustomComponents = MP::TypelistWrapper<
        InputMappingContext,
        InputState,
        CharacterController,
        StateMachine,
        SprintStamina,
        AIBrain
>;

}

#endif
#endif

// ---- Headers ---------------------------------------------------------------
#ifdef INC_CUSTOM_COMPONENTS_HEADER
#ifndef INC_CUSTOM_COMPONENTS_HEADER_GUARD
#define INC_CUSTOM_COMPONENTS_HEADER_GUARD

#include <InputMappingContext.h>
#include <InputState.h>
#include <CharacterController.h>
#include <hsm/StateMachine.h>
#include <SprintStamina.h>
#include <AIBrain.h>

#endif
#endif

// ---- Implementation --------------------------------------------------------
#if defined(INC_CUSTOM_COMPONENTS_IMPL) && defined(SE_IMPL)
#ifndef INC_CUSTOM_COMPONENTS_IMPL_GUARD
#define INC_CUSTOM_COMPONENTS_IMPL_GUARD

#include <InputMappingContext.tcc>
#include <CharacterController.tcc>
#include <hsm/StateMachine.tcc>
#include <AIBrain.tcc>

#endif
#endif

// ===========================================================================

// ---- System forward declarations -------------------------------------------
#ifdef FORWARD_CUSTOM_SYSTEMS
#ifndef FORWARD_CUSTOM_SYSTEMS_GUARD
#define FORWARD_CUSTOM_SYSTEMS_GUARD

namespace SE {

class CharacterMovementSystem;
class CharacterAnimationSystem;
class CharacterDebugger;
class EntityManager;
class StateMachineSystem;
class StateMachineDebugger;

using TCustomSystems = MP::TypelistWrapper<
        CharacterMovementSystem,
        CharacterAnimationSystem,
        CharacterDebugger,
        StateMachineSystem,
        StateMachineDebugger,
        EntityManager
>;

}

#endif
#endif

// ---- System headers --------------------------------------------------------
#ifdef INC_CUSTOM_SYSTEMS_HEADER
#ifndef INC_CUSTOM_SYSTEMS_HEADER_GUARD
#define INC_CUSTOM_SYSTEMS_HEADER_GUARD

#include <CharacterMovementSystem.h>
#include <CharacterAnimationSystem.h>
#include <CharacterDebugger.h>
#include <hsm/StateMachineSystem.h>
#include <hsm/StateMachineDebugger.h>
#include <EntityManager.h>

#endif
#endif

// ---- System implementations ------------------------------------------------
#if defined(INC_CUSTOM_SYSTEMS_IMPL) && defined(SE_IMPL)
#ifndef INC_CUSTOM_SYSTEMS_IMPL_GUARD
#define INC_CUSTOM_SYSTEMS_IMPL_GUARD

#include <CharacterMovementSystem.tcc>
#include <CharacterAnimationSystem.tcc>
#include <CharacterDebugger.tcc>
#include <hsm/HSMResolver.tcc>
#include <hsm/StateMachineSystem.tcc>
#include <hsm/StateMachineDebugger.tcc>
#include <EntityManager.tcc>

#endif
#endif

// ===========================================================================

// ---- Resource forward declarations -----------------------------------------
#ifdef FORWARD_CUSTOM_RESOURCES
#ifndef FORWARD_CUSTOM_RESOURCES_GUARD
#define FORWARD_CUSTOM_RESOURCES_GUARD

namespace SE {

class StateMachineAsset;

using TCustomResources = MP::TypelistWrapper<StateMachineAsset>;

}

#endif
#endif

// ---- Resource headers ------------------------------------------------------
#ifdef INC_CUSTOM_RESOURCES_HEADER
#ifndef INC_CUSTOM_RESOURCES_HEADER_GUARD
#define INC_CUSTOM_RESOURCES_HEADER_GUARD

#include <hsm/StateMachineAsset.h>

#endif
#endif

// ---- Resource implementations ----------------------------------------------
#if defined(INC_CUSTOM_RESOURCES_IMPL) && defined(SE_IMPL)
#ifndef INC_CUSTOM_RESOURCES_IMPL_GUARD
#define INC_CUSTOM_RESOURCES_IMPL_GUARD

#include <hsm/StateMachineAsset.tcc>

#endif
#endif
