//
#ifndef __GLOBAL_TYPES_H__
#define __GLOBAL_TYPES_H__ 1

// C include
#include <string.h>

// Loki include
#include <loki/Singleton.h>
#include <loki/HierarchyGenerators.h>
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
#include <SimpleFPS.h>
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
#include <Allocator.h>

#include <TextureStock.h>
#include <TGALoader.h>
#include <OpenCVImgLoader.h>
#include <Texture.h>
#include <StoreTexture2D.h>
#include <StoreTextureBufferObject.h>
#include <StoreTexture2DRenderTarget.h>
//#include <Skeleton.h>


//*/
//TODO move to SE.h

namespace SE {

//global helper functions
template <class Resource, class ... TConcreateSettings> H<Resource> CreateResource (const std::string & sName, const TConcreateSettings & ... oSettings);
template <class Resource> Resource * GetResource(H<Resource> h);
template <class TSystem> TSystem & GetSystem();


typedef LOKI_TYPELIST_2(TGALoader, OpenCVImgLoader)                     TextureLoadStrategyList;
typedef LOKI_TYPELIST_3(StoreTexture2D, StoreTextureBufferObject, StoreTexture2DRenderTarget) TextureStoreStrategyList;
typedef Texture<TextureStoreStrategyList, TextureLoadStrategyList>      TTexture;

}

#include <ShaderComponent.h>
#include <ShaderProgram.h>
#include <GraphicsState.h>
#include <UniformBuffer.h>
#include <ShaderProgramState.h>
#include <VisualHelpers.h>
#include <Material.h>

#include <Mesh.h>

namespace SE {

typedef Loki::SingletonHolder< SimpleFPS >                              TSimpleFPS;

typedef Mesh                                                            TMesh;

}

#include <Renderer.h>
#include <DeferredRenderer.h>
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
//using TRenderer = Renderer<TVisibilityManager>;
// To use deferred PBR renderer, replace the line above with:
using TRenderer = DeferredRenderer<TVisibilityManager>;

using TCoreSystems = MP::TypelistWrapper<Config, GraphicsConfig, EventManager, FrameAllocator, GraphicsState, TRenderer, DebugRenderer, InputManager>;

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

#define INC_CUSTOM_SYSTEMS_HEADER
#include <App.h>
#undef INC_CUSTOM_SYSTEMS_HEADER

//TODO INC Resource list
//TSceneTree dependent resources
#include <Skeleton.h>

#define INC_CORE_COMPONENTS_HEADER
#include <CoreComponents.h>
#undef INC_CORE_COMPONENTS_HEADER

#define INC_CUSTOM_COMPONENTS_HEADER
#include <App.h>
#undef INC_CUSTOM_COMPONENTS_HEADER

//TODO forward custom resources

namespace SE {

typedef LOKI_TYPELIST_8(
                TTexture,
                Material,
                TMesh,
                TSceneTree,
                ShaderComponent,
                ShaderProgram,
                Skeleton,
                CharacterShell)                                         TResourseList;
//THINK
#ifndef SE_IMPL
extern template class ResourceManager<TResourseList>;
#endif
typedef Loki::SingletonHolder < ResourceManager<TResourseList> >        TResourceManager;



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
