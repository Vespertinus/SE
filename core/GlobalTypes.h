
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

#include <ResourceHolder.h>
#include <ResourceManager.h>


#include <TextureStock.h>
#include <TGALoader.h>
#include <OpenCVImgLoader.h>
#include <Texture.h>
#include <StoreTexture2D.h>


namespace SE {

template <class Resource, class ... TConcreateSettings> Resource * CreateResource (const std::string & sName, const TConcreateSettings & ... oSettings);

typedef LOKI_TYPELIST_2(TGALoader, OpenCVImgLoader)                     TextureLoadStrategyList;
typedef LOKI_TYPELIST_1(StoreTexture2D)                                 TextureStoreStrategyList;
typedef Texture<TextureStoreStrategyList, TextureLoadStrategyList>      TTexture;

}

#include <ShaderComponent.h>
#include <ShaderProgram.h>
#include <RenderState.h>
#include <VisualHelpers.h>

#include <Mesh.h>

namespace SE {

typedef Loki::SingletonHolder< SimpleFPS >                              TSimpleFPS;

typedef Mesh                                                            TMesh;

}

#include <SceneTree.h>

namespace SE {

typedef SceneTree<TMesh *> TSceneTree;


}


namespace SE {

typedef LOKI_TYPELIST_5(
                TTexture,
                TMesh,
                TSceneTree,
                ShaderComponent,
                ShaderProgram)                                          TResourseList;
typedef Loki::SingletonHolder < ResourceManager<TResourseList> >        TResourceManager;



template <class Resource, class ... TConcreateSettings> Resource * CreateResource (const std::string & sPath, const TConcreateSettings & ... oSettings) {

        return TResourceManager::Instance().Create<Resource>(sPath, oSettings...);
}


} //namespace SE

#endif
