
// ---------------------------------------------------------------------------
// Shadow App.h for the phys_trigger_volume_test target ONLY (include-path
// priority, same precedent as functional/shadow/AnimatedModel.h). It adds the
// application-level TriggerVolume component to the test scene composition —
// tests/App.h (shared by every other target) deliberately keeps custom
// components empty. Mirrors samples/physics_demo/App.h.
// ---------------------------------------------------------------------------

#ifdef FORWARD_CUSTOM_COMPONENTS
#ifndef FORWARD_CUSTOM_COMPONENTS_GUARD
#define FORWARD_CUSTOM_COMPONENTS_GUARD

namespace SE {

class TriggerVolume;

using TCustomComponents = MP::TypelistWrapper<TriggerVolume>;

}

#endif
#endif

#ifdef INC_CUSTOM_COMPONENTS_HEADER
#ifndef INC_CUSTOM_COMPONENTS_HEADER_GUARD
#define INC_CUSTOM_COMPONENTS_HEADER_GUARD

#include <TriggerVolume.h>

#endif
#endif

#if defined(INC_CUSTOM_COMPONENTS_IMPL) && defined(SE_IMPL)
#ifndef INC_CUSTOM_COMPONENTS_IMPL_GUARD
#define INC_CUSTOM_COMPONENTS_IMPL_GUARD

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

#ifdef FORWARD_CUSTOM_RESOURCES
#ifndef FORWARD_CUSTOM_RESOURCES_GUARD
#define FORWARD_CUSTOM_RESOURCES_GUARD

namespace SE {

using TCustomResources = MP::TypelistWrapper<>;

}

#endif
#endif
