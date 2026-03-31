
#ifdef FORWARD_CORE_COMPONENTS
#ifndef FORWARD_CORE_COMPONENTS_GUARD
#define FORWARD_CORE_COMPONENTS_GUARD

namespace SE {

class StaticModel;
class AnimatedModel;
class Animator;
class Camera;
#ifdef SE_PHYSICS_ENABLED
class RigidBody;
#endif
#ifdef SE_AUDIO_ENABLED
class AudioEmitter;
class AudioListener;
class SoundEmitter;
#endif
#ifdef SE_UI_ENABLED
class UIWidget;
#endif

using TCoreComponentsBase    = MP::TypelistWrapper<StaticModel, AnimatedModel, Animator, Camera>;
#ifdef SE_PHYSICS_ENABLED
using TCoreComponentsPhysics = MP::TypelistWrapper<RigidBody>;
#else
using TCoreComponentsPhysics = MP::TypelistWrapper<>;
#endif
#ifdef SE_AUDIO_ENABLED
using TCoreComponentsAudio   = MP::TypelistWrapper<AudioEmitter, AudioListener, SoundEmitter>;
#else
using TCoreComponentsAudio   = MP::TypelistWrapper<>;
#endif
#ifdef SE_UI_ENABLED
using TCoreComponentsUI      = MP::TypelistWrapper<UIWidget>;
#else
using TCoreComponentsUI      = MP::TypelistWrapper<>;
#endif
using TCoreComponents = decltype(MP::TypelistConcatenate(
        MP::TypelistConcatenate(
                MP::TypelistConcatenate(TCoreComponentsBase{}, TCoreComponentsPhysics{}),
                TCoreComponentsAudio{}),
        TCoreComponentsUI{}));

}

#endif
#endif


#ifdef INC_CORE_COMPONENTS_HEADER
#ifndef INC_CORE_COMPONENTS_HEADER_GUARD
#define INC_CORE_COMPONENTS_HEADER_GUARD

#include <StaticModel.h>
#include <AnimatedModel.h>
#include <Animator.h>
#include <Camera.h>
#include <RigidBody.h>
#include <AudioEmitter.h>
#include <AudioListener.h>
#include <SoundEmitter.h>
#ifdef SE_UI_ENABLED
#include <ui/UIWidget.h>
#endif

#endif
#endif

#if defined(INC_CORE_COMPONENTS_IMPL) && defined (SE_IMPL)
#ifndef INC_CORE_COMPONENTS_IMPL_GUARD
#define INC_CORE_COMPONENTS_IMPL_GUARD

#include <StaticModel.tcc>
#include <AnimatedModel.tcc>
#include <Animator.tcc>
#include <Camera.tcc>
#ifdef SE_PHYSICS_ENABLED
#include <RigidBody.tcc>
#endif
#ifdef SE_AUDIO_ENABLED
#include <AudioEmitter.tcc>
#include <AudioListener.tcc>
#include <SoundEmitter.tcc>
#endif
#ifdef SE_UI_ENABLED
#include <ui/UIWidget.tcc>
#endif

#endif
#endif

