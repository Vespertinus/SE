
#ifdef FORWARD_CUSTOM_COMPONENTS
#ifndef FORWARD_CUSTOM_COMPONENTS_GUARD
#define FORWARD_CUSTOM_COMPONENTS_GUARD

namespace SE {

class BasicController;
class BallController;
class TriggerVolume;

using TCustomComponents = MP::TypelistWrapper<BasicController, BallController, TriggerVolume>;

}

#endif
#endif

#ifdef INC_CUSTOM_COMPONENTS_HEADER
#ifndef INC_CUSTOM_COMPONENTS_HEADER_GUARD
#define INC_CUSTOM_COMPONENTS_HEADER_GUARD

#include <BasicController.h>
#include "BallController.h"
#include "PhysicsDebugController.h"
#include <TriggerVolume.h>

#endif
#endif

#if defined(INC_CUSTOM_COMPONENTS_IMPL) && defined(SE_IMPL)
#ifndef INC_CUSTOM_COMPONENTS_IMPL_GUARD
#define INC_CUSTOM_COMPONENTS_IMPL_GUARD

#include <EntityTemplateUtility.tcc>
#include <BasicController.tcc>
#include "BallController.tcc"
#include "PhysicsDebugController.tcc"
#include <TriggerVolume.tcc>

#endif
#endif

#ifdef FORWARD_CUSTOM_SYSTEMS
#ifndef FORWARD_CUSTOM_SYSTEMS_GUARD
#define FORWARD_CUSTOM_SYSTEMS_GUARD

namespace SE {

using TCustomSystems = MP::TypelistWrapper<>;

}

#endif
#endif
