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

#include <Logging.h>

#include <Chrono.h>
#include <SimpleFPS.h>
#include <MPUtil.h>
#include <BoundingBox.h>
#include <CommonTypes.h>

#include <ResourceHolder.h>
#include <ResourceManager.h>

#include <Engine.h>
#include <BasicConfig.h>

#include <TextureStock.h>
#include <TGALoader.h>
#include <OpenCVImgLoader.h>
#include <Texture.h>
#include <StoreTexture2D.h>
#include <StoreTextureBufferObject.h>

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
#include <RenderState.h>
#include <VisualHelpers.h>
#include <Material.h>

#include <Mesh.h>

namespace SE {

typedef Loki::SingletonHolder< SimpleFPS >                              TSimpleFPS;

typedef Mesh                                                            TMesh;

}

#include <Renderer.h>
//TEMP
#include <AllVisible.h>

//TODO forward custom components

namespace SE {

//forward components
class StaticModel;


//renderable components list

//engine subsytem types

//FIXME rewrite on custom component handling
using TVisibilityManager = AllVisible<StaticModel>;
using TRenderer = Renderer<TVisibilityManager>;

//singleton
using TEngine = Loki::SingletonHolder<Engine<Config, TRenderer/*, TRenderState, TInputManager, ...*/>>;

}


#include <SceneTree.h>

namespace SE {

using TSceneTree = SceneTree<StaticModel>;

}

// ___Start___ include components headers

#include <StaticModel.h>
//#include <AnimatedModel.h>
// ___End_____ include components headers


//TODO include custom components


//TODO forward custom resources

namespace SE {

typedef LOKI_TYPELIST_6(
                TTexture,
                Material,
                TMesh,
                TSceneTree,
                ShaderComponent,
                ShaderProgram)                                          TResourseList;
typedef Loki::SingletonHolder < ResourceManager<TResourseList> >        TResourceManager;



template <class Resource, class ... TConcreateSettings> Resource * CreateResource (const std::string & sPath, const TConcreateSettings & ... oSettings) {

        return TResourceManager::Instance().Create<Resource>(sPath, oSettings...);
}

template <class TSystem> TSystem & GetSystem() {

        return TEngine::Instance().Get<TSystem>();
}


} //namespace SE

#endif
