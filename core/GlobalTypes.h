
#ifndef __GLOBAL_TYPES_H__
#define __GLOBAL_TYPES_H__ 1

// C include
#include <string.h>

//#include <Global.h>

// Loki include
#include <Singleton.h>
#include <HierarchyGenerators.h>
#include <Typelist.h>

#include <ErrCode.h>
#include <Util.h>

#include <SimpleFPS.h>
#include <MPUtil.h>
#include <VisualHelpers.h>

#include <ResourceHolder.h>

#include <TextureStock.h>
#include <TGALoader.h>
#include <OpenCVImgLoader.h>
#include <Texture.h>
#include <StoreTexture2D.h>

namespace SE {

typedef LOKI_TYPELIST_2(TGALoader, OpenCVImgLoader)                     TextureLoadStrategyList;
//typedef LOKI_TYPELIST_1(OpenCVImgLoader)                                TextureLoadStrategyList;
typedef LOKI_TYPELIST_1(StoreTexture2D)                                 TextureStoreStrategyList;
typedef Texture<TextureStoreStrategyList, TextureLoadStrategyList>      TTexture;

}

#include <MeshStock.h>
#include <Mesh.h>
#include <OBJLoader.h>
#include <StoreMesh.h>

#include <ResourceManager.h>


namespace SE {

typedef Loki::SingletonHolder< SimpleFPS >  TSimpleFPS;
///*
typedef LOKI_TYPELIST_1(OBJLoader)                                      MeshLoadStrategyList;
typedef LOKI_TYPELIST_1(StoreMesh)                                      MeshStoreStrategyList;
typedef Mesh<MeshStoreStrategyList, MeshLoadStrategyList>               TMesh;
//*/

typedef LOKI_TYPELIST_2(TTexture, TMesh)                                TResourseList;
//typedef LOKI_TYPELIST_1(TTexture)                                TResourseList;

typedef Loki::SingletonHolder < ResourceManager<TResourseList> >    TResourceManager;

} //namespace SE

//FIXME...
#include <OBJLoader.tcc>

#endif
