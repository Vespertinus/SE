
#ifdef FORWARD_CUSTOM_COMPONENTS
#ifndef FORWARD_CUSTOM_COMPONENTS_GUARD
#define FORWARD_CUSTOM_COMPONENTS_GUARD

namespace SE {

class BasicController;

using TCustomComponents = MP::TypelistWrapper<BasicController>;

}

#endif
#endif

#ifdef INC_CUSTOM_COMPONENTS_HEADER
#ifndef INC_CUSTOM_COMPONENTS_HEADER_GUARD
#define INC_CUSTOM_COMPONENTS_HEADER_GUARD

#include <BasicController.h>

#endif
#endif

#if defined(INC_CUSTOM_COMPONENTS_IMPL) && defined (SE_IMPL)
#ifndef INC_CUSTOM_COMPONENTS_IMPL_GUARD
#define INC_CUSTOM_COMPONENTS_IMPL_GUARD

#include <BasicController.tcc>

#endif
#endif

#ifdef FORWARD_CUSTOM_SYSTEMS
#ifndef FORWARD_CUSTOM_SYSTEMS_GUARD
#define FORWARD_CUSTOM_SYSTEMS_GUARD

namespace SE {

class WorldProcessManager;
class EntityManager;
template <class...> class EntityTemplateSystem;

using TEntityTemplateSystem = typename MP::Typelist2TmplPack<
        EntityTemplateSystem,
        decltype(MP::TypelistConcatenate(TCoreComponents{}, TCustomComponents{}))
>::Type;

using TCustomSystems = MP::TypelistWrapper<WorldProcessManager, EntityManager, TEntityTemplateSystem>;

}

#endif
#endif

#ifdef  INC_CUSTOM_SYSTEMS_HEADER
#ifndef INC_CUSTOM_SYSTEMS_HEADER_GUARD
#define INC_CUSTOM_SYSTEMS_HEADER_GUARD

#include <WorldProcess.h>
#include <WorldProcessManager.h>
#include <EntityManager.h>
#include <EntityTemplateSystem.h>

#endif
#endif

#if defined(INC_CUSTOM_SYSTEMS_IMPL) && defined (SE_IMPL)
#ifndef INC_CUSTOM_SYSTEMS_IMPL_GUARD
#define INC_CUSTOM_SYSTEMS_IMPL_GUARD

#include <WorldProcess.tcc>
#include <WorldProcessManager.tcc>
#include <EntityManager.tcc>
#include <EntityTemplateSystem.tcc>

#endif
#endif
