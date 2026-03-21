
#ifdef FORWARD_CORE_COMPONENTS
#ifndef FORWARD_CORE_COMPONENTS_GUARD
#define FORWARD_CORE_COMPONENTS_GUARD

namespace SE {

class StaticModel;
class AnimatedModel;
class Camera;
class RigidBody;
class AudioEmitter;
class AudioListener;

using TCoreComponents = MP::TypelistWrapper<StaticModel, AnimatedModel, Camera, RigidBody, AudioEmitter, AudioListener>;

}

#endif
#endif


#ifdef INC_CORE_COMPONENTS_HEADER
#ifndef INC_CORE_COMPONENTS_HEADER_GUARD
#define INC_CORE_COMPONENTS_HEADER_GUARD

#include <StaticModel.h>
#include <AnimatedModel.h>
#include <Camera.h>
#include <RigidBody.h>
#include <AudioEmitter.h>
#include <AudioListener.h>

#endif
#endif

#if defined(INC_CORE_COMPONENTS_IMPL) && defined (SE_IMPL)
#ifndef INC_CORE_COMPONENTS_IMPL_GUARD
#define INC_CORE_COMPONENTS_IMPL_GUARD

#include <StaticModel.tcc>
#include <AnimatedModel.tcc>
#include <Camera.tcc>
#include <RigidBody.tcc>
#include <AudioEmitter.tcc>
#include <AudioListener.tcc>

#endif
#endif

