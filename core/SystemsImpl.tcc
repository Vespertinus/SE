
#ifdef SE_IMPL
#include <AnimGraphRuntime.tcc>
#include <GraphicsState.tcc>
#include <GraphicsConfig.tcc>
#include <DebugRenderer.tcc>
#include <InputManager.tcc>
#if SE_DEFERRED_RENDERER
#include <FrameBuffer.tcc>
#include <GBuffer.tcc>
#include <SSAOBuffer.tcc>
#include <ShadowBuffer.tcc>
#include <BloomBuffer.tcc>
#endif
#ifdef SE_PHYSICS_ENABLED
#include <PhysicsSystem.tcc>
#endif
#include <MeshBuilder.tcc>
#include <MeshGen.tcc>
#include <TextureBuilder.tcc>
#ifdef SE_AUDIO_ENABLED
#include <AudioThread.tcc>
#include <AudioSystem.tcc>
#include <SoundEventSystem.tcc>
#endif
#ifdef SE_UI_ENABLED
#include <ui/UIFileInterface.tcc>
#include <ui/UISystemInterface.tcc>
#include <ui/UIRenderInterface.tcc>
#include <ui/UIInputTranslator.tcc>
#include <ui/UIDocumentManager.tcc>
#include <ui/UIAnimationController.tcc>
#include <ui/UIScreenManager.tcc>
#include <ui/UIEventRouter.tcc>
#include <ui/UIThemeManager.tcc>
#include <ui/UILocalization.tcc>
#include <ui/UISystem.tcc>
#endif
#endif
//template class, so we need realization also
#if SE_DEFERRED_RENDERER
#include <DeferredRenderer.tcc>
#else
#include <Renderer.tcc>
#endif
#include <EventManager.tcc>
#include <Allocator.tcc>
