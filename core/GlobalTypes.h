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
#include <ResourceManager.h>

#include <Engine.h>
#include <BasicConfig.h>
#include <GraphicsConfig.h>
#include <EventManager.h>

#include <TextureStock.h>
#include <TGALoader.h>
#include <OpenCVImgLoader.h>
#include <Texture.h>
#include <StoreTexture2D.h>
#include <StoreTextureBufferObject.h>
//#include <Skeleton.h>


//*/
//TODO move to SE.h

namespace SE {

//global helper functions
template <class Resource, class ... TConcreateSettings> Resource * CreateResource (const std::string & sName, const TConcreateSettings & ... oSettings);
template <class TSystem> TSystem & GetSystem();


typedef LOKI_TYPELIST_2(TGALoader, OpenCVImgLoader)                     TextureLoadStrategyList;
typedef LOKI_TYPELIST_2(StoreTexture2D, StoreTextureBufferObject)       TextureStoreStrategyList;
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
using TRenderer = Renderer<TVisibilityManager>;

using TCoreSystems = MP::TypelistWrapper<Config, GraphicsConfig, EventManager, GraphicsState, TRenderer, DebugRenderer /*, TInputManager, ...*/>;

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



template <class Resource, class ... TConcreateSettings> Resource * CreateResource (const std::string & sPath, const TConcreateSettings & ... oSettings) {

        return TResourceManager::Instance().Create<Resource>(sPath, oSettings...);
}

template <class TSystem> TSystem & GetSystem() {

        return TEngine::Instance().Get<TSystem>();
}

} //namespace SE

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
