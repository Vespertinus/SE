
#ifdef FORWARD_CORE_COMPONENTS
#ifndef FORWARD_CORE_COMPONENTS_GUARD
#define FORWARD_CORE_COMPONENTS_GUARD

namespace SE {

class StaticModel;
class AnimatedModel;
class Camera;

using TCoreComponents = MP::TypelistWrapper<StaticModel, AnimatedModel, Camera>;

}

#endif
#endif


#ifdef INC_CORE_COMPONENTS_HEADER
#ifndef INC_CORE_COMPONENTS_HEADER_GUARD
#define INC_CORE_COMPONENTS_HEADER_GUARD

#include <StaticModel.h>
#include <AnimatedModel.h>
#include <Camera.h>

#endif
#endif

#if defined(INC_CORE_COMPONENTS_IMPL) && defined (SE_IMPL)
#ifndef INC_CORE_COMPONENTS_IMPL_GUARD
#define INC_CORE_COMPONENTS_IMPL_GUARD

#include <StaticModel.tcc>
#include <AnimatedModel.tcc>
#include <Camera.tcc>

#endif
#endif

