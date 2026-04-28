//
#ifndef __GLOBAL_TYPES_H__
#define __GLOBAL_TYPES_H__ 1

// C include
#include <string.h>

// Loki include
#include <loki/Singleton.h>
#include <loki/Typelist.h>

#include <ErrCode.h>
#include <Util.h>
#include <StrID.h>
#include <Global.h>
///*
#include <Logging.h>
///*
#include <CommonEvents.h>
#include <Chrono.h>
#include <MPUtil.h>
#include <BoundingBox.h>
#include <CommonTypes.h>

#include <ResourceHolder.h>
#include <ResourceHandle.h>
#include <ResourceManager.h>

#include <Engine.h>
#include <BasicConfig.h>
#include <GraphicsConfig.h>
#include <EventManager.h>
#include <InputEvents.h>
#include <InputManager.h>
#include <InputCodes.h>
#include <PhysicsTypes.h>
#include <PhysicsEvents.h>
#include <PhysicsSystem.h>
#include <Allocator.h>
#include <AudioTypes.h>
#include <AudioClip.h>
#include <AudioSystem.h>
#include <SoundEventSystem.h>

#include <TextureStock.h>
#include <TGALoader.h>
#include <OpenCVImgLoader.h>
#include <KTXLoader.h>
#include <Texture.h>
#include <StoreTexture2D.h>
#include <StoreTextureBufferObject.h>
#include <StoreTexture2DRenderTarget.h>
#include <StoreTextureCubeMap.h>
//#include <Skeleton.h>


//*/
//TODO move to SE.h

namespace SE {

//global helper functions
template <class Resource, class ... TConcreateSettings> H<Resource> CreateResource (const std::string & sName, const TConcreateSettings & ... oSettings);
template <class Resource> Resource * GetResource(H<Resource> h);
template <class TSystem> TSystem & GetSystem();


typedef LOKI_TYPELIST_3(TGALoader, OpenCVImgLoader, KTXLoader)           TextureLoadStrategyList;
typedef LOKI_TYPELIST_4(StoreTexture2D, StoreTextureBufferObject, StoreTexture2DRenderTarget, StoreTextureCubeMap) TextureStoreStrategyList;
typedef Texture<TextureStoreStrategyList, TextureLoadStrategyList>      TTexture;

}

#include <ShaderComponent.h>
#include <ShaderProgram.h>
#ifdef SE_UI_ENABLED
#include <ui/UITypes.h>
#include <ui/UISystem.h>
#endif
#include <GraphicsState.h>
#include <AppClock.h>
#include <FpsTracker.h>
#include <FixedClock.h>
#include <UniformBuffer.h>
#include <ShaderProgramState.h>
#include <VisualHelpers.h>
#include <Material.h>

#include <Mesh.h>

namespace SE {

typedef Mesh                                                            TMesh;

}

#include <VertexLayout.h>
#include <MeshBuilder.h>
#include <MeshGen.h>
#include <TextureBuilder.h>

#if SE_DEFERRED_RENDERER
#include <DeferredRenderer.h>
#else
#include <Renderer.h>
#endif
#include <DebugRenderer.h>
//TEMP
#include <AllVisible.h>

//forward components
#define FORWARD_CORE_COMPONENTS
#include <CoreComponents.h>
#undef FORWARD_CORE_COMPONENTS
//concreate application custom types and settings
#define FORWARD_CUSTOM_COMPONENTS
#include <App.h>
#undef FORWARD_CUSTOM_COMPONENTS

#define FORWARD_CUSTOM_SYSTEMS
#include <App.h>
#undef FORWARD_CUSTOM_SYSTEMS

namespace SE {

//renderable components list

//engine subsytem types
using TVisibilityManager = AllVisible<StaticModel, AnimatedModel>;
#if SE_DEFERRED_RENDERER
using TRenderer = DeferredRenderer<TVisibilityManager>;
#else
using TRenderer = Renderer<TVisibilityManager>;
#endif

// Build TCoreSystems by concatenating conditional partial lists
using TCoreSystemsBase = MP::TypelistWrapper<Config, GraphicsConfig, EventManager, FrameAllocator, GraphicsState, AppClock, FpsTracker, TRenderer, DebugRenderer, InputManager>;

#ifdef SE_PHYSICS_ENABLED
using TCoreSystemsPhysics = MP::TypelistWrapper<PhysicsSystem>;
#else
using TCoreSystemsPhysics = MP::TypelistWrapper<>;
#endif
#ifdef SE_AUDIO_ENABLED
using TCoreSystemsAudio = MP::TypelistWrapper<AudioSystem, SoundEventSystem>;
#else
using TCoreSystemsAudio = MP::TypelistWrapper<>;
#endif
#ifdef SE_UI_ENABLED
using TCoreSystemsUI = MP::TypelistWrapper<UISystem>;
#else
using TCoreSystemsUI = MP::TypelistWrapper<>;
#endif
using TCoreSystems = decltype(MP::TypelistConcatenate(
        MP::TypelistConcatenate(
                MP::TypelistConcatenate(TCoreSystemsBase{}, TCoreSystemsPhysics{}),
                TCoreSystemsAudio{}),
        TCoreSystemsUI{}));

using TEngine =
        typename Loki::SingletonHolder<
        typename MP::Typelist2TmplPack<
        Engine,
        decltype(MP::TypelistConcatenate(TCoreSystems{}, TCustomSystems{}))
                >::Type>;


}


#include <SceneTree.h>

namespace SE {

using TSceneTree = typename MP::Typelist2TmplPack<
        SceneTree,
        decltype(MP::TypelistConcatenate(TCoreComponents{}, TCustomComponents{}))
                >::Type;

}

#ifdef SE_PHYSICS_ENABLED
#include <PhysicsSystemEx.h>
#endif

#define INC_CUSTOM_SYSTEMS_HEADER
#include <App.h>
#undef INC_CUSTOM_SYSTEMS_HEADER

//TSceneTree dependent resources
#include <AnimClip.h>
#include <Skeleton.h>
#include <AnimGraph.h>

#define INC_CORE_COMPONENTS_HEADER
#include <CoreComponents.h>
#undef INC_CORE_COMPONENTS_HEADER

#define INC_CUSTOM_COMPONENTS_HEADER
#include <App.h>
#undef INC_CUSTOM_COMPONENTS_HEADER

#define FORWARD_CUSTOM_RESOURCES
#include <App.h>
#undef FORWARD_CUSTOM_RESOURCES

#define INC_CUSTOM_RESOURCES_HEADER
#include <App.h>
#undef INC_CUSTOM_RESOURCES_HEADER

namespace SE {

using TCoreResourcesBase  = MP::TypelistWrapper<
        TTexture,
        Material,
        TMesh,
        TSceneTree,
        ShaderComponent,
        ShaderProgram,
        AnimClip,
        Skeleton,
        AnimGraph
                >;

#ifdef SE_AUDIO_ENABLED
using TCoreResourcesAudio = MP::TypelistWrapper<AudioClip>;
#else
using TCoreResourcesAudio = MP::TypelistWrapper<>;
#endif

using TCoreResources = decltype(MP::TypelistConcatenate(TCoreResourcesBase{}, TCoreResourcesAudio{}));

using TResourseList  = decltype(MP::TypelistConcatenate(TCoreResources{}, TCustomResources{}));

using TResourceManagerImpl = typename MP::Typelist2TmplPack<ResourceManager, TResourseList>::Type;
typedef Loki::SingletonHolder<TResourceManagerImpl> TResourceManager;



template <class Resource, class ... TConcreateSettings> H<Resource> CreateResource (const std::string & sPath, const TConcreateSettings & ... oSettings) {

        return TResourceManager::Instance().Create<Resource>(sPath, oSettings...);
}

template <class Resource> Resource * GetResource(H<Resource> h) {

        return TResourceManager::Instance().Get<Resource>(h);
}

template <class Resource, class ... TConcreateSettings>
Resource * CreateRawResource(const std::string & sPath, const TConcreateSettings & ... oSettings) {
        return GetResource(CreateResource<Resource>(sPath, oSettings...));
}

template <class Resource> void DestroyResource(H<Resource> h) {

        TResourceManager::Instance().Destroy<Resource>(h);
}

template <class Resource> bool LockResource(H<Resource> h) {
        return TResourceManager::Instance().Lock<Resource>(h);
}

template <class Resource> void UnlockResource(H<Resource> h) {
        TResourceManager::Instance().Unlock<Resource>(h);
}

template <class Resource> void RetainResource(H<Resource> h) {
        TResourceManager::Instance().Retain<Resource>(h);
}
template <class Resource> void UnretainResource(H<Resource> h) {
        TResourceManager::Instance().Unretain<Resource>(h);
}
inline void DestroyUnretainedResources() {
        TResourceManager::Instance().DestroyUnretained();
}
inline void ClearAllRetainedResources() {
        TResourceManager::Instance().ClearAllRetains();
}

template <class TSystem> TSystem & GetSystem() {

        return TEngine::Instance().Get<TSystem>();
}

} //namespace SE

namespace detail_h_fmt {
    template <class T, class = void> struct has_str : std::false_type {};
    template <class T> struct has_str<T, std::void_t<decltype(std::declval<const T&>().Str())>> : std::true_type {};

    template <class T> std::enable_if_t<has_str<T>::value, std::string> maybe_str(const T * p) {
        return p ? (' ' + p->Str()) : std::string{};
    }
    template <class T> std::enable_if_t<!has_str<T>::value, std::string> maybe_str(const T *) {
        return {};
    }
}

template <class T>
struct fmt::formatter<SE::H<T>> : fmt::formatter<std::string> {
    auto format(const SE::H<T> & h, fmt::format_context & ctx) const {
        if (!h.IsValid()) {
            //TODO abi::__cxa_demangle
            auto s = fmt::format("H[null], type: {}", typeid(T).name());
            return fmt::formatter<std::string>::format(s, ctx);
        }
        std::string s = fmt::format("H[idx={}, gen={}]", h.raw.index, h.raw.generation);
        s += detail_h_fmt::maybe_str(SE::GetResource(h));
        return fmt::formatter<std::string>::format(s, ctx);
    }
};

#define INC_CUSTOM_SYSTEMS_IMPL
#include <App.h>
#undef INC_CUSTOM_SYSTEMS_IMPL

#include <AllImpl.tcc>
#define INC_CORE_COMPONENTS_IMPL
#include <CoreComponents.h>
#undef INC_CORE_COMPONENTS_IMPL
#define INC_CUSTOM_COMPONENTS_IMPL
#include <App.h>
#undef INC_CUSTOM_COMPONENTS_IMPL

#endif
