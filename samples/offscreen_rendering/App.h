
#ifdef FORWARD_CUSTOM_COMPONENTS
#ifndef FORWARD_CUSTOM_COMPONENTS_GUARD
#define FORWARD_CUSTOM_COMPONENTS_GUARD

namespace SE {

#ifdef SE_INTERACTIVE        
class BasicController;

using TCustomComponents = MP::TypelistWrapper<BasicController>;
#else

using TCustomComponents = MP::TypelistWrapper<>;
#endif

}

#endif
#endif

#if defined(INC_CUSTOM_COMPONENTS_HEADER) && defined (SE_INTERACTIVE)
#ifndef INC_CUSTOM_COMPONENTS_HEADER_GUARD
#define INC_CUSTOM_COMPONENTS_HEADER_GUARD

#include <BasicController.h>

#endif
#endif

#if defined(INC_CUSTOM_COMPONENTS_IMPL) && defined (SE_IMPL) && defined (SE_INTERACTIVE)
#ifndef INC_CUSTOM_COMPONENTS_IMPL_GUARD
#define INC_CUSTOM_COMPONENTS_IMPL_GUARD

#include <BasicController.tcc>

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
