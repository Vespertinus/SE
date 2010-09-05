
#ifndef __GLOBAL_TYPES_H__
#define __GLOBAL_TYPES_H__ 1

// C include
#include <string.h>

#include <Global.h>

// Loki include
#include <Singleton.h>
#include <HierarchyGenerators.h>
#include <Typelist.h>

#include <Util.h>
#include <ErrCode.h>

#include <SimpleFPS.h>

#include <MPUtil.h>

#include <ResourceHolder.h>

#include <TextureStock.h>
#include <Texture.h>
#include <TGALoader.h>
#include <StoreTexture2D.h>

#include <ResourceManager.h>


namespace SE {

typedef Loki::SingletonHolder< SimpleFPS >  TSimpleFPS;

typedef LOKI_TYPELIST_1(TGALoader)          TextureLoadStrategyList;
typedef LOKI_TYPELIST_1(StoreTexture2D)     TextureStoreStrategyList;

typedef Texture<TextureStoreStrategyList, TextureLoadStrategyList>  TTexture;


typedef LOKI_TYPELIST_1(TTexture)           TResourseList;

typedef Loki::SingletonHolder < ResourceManager<TResourseList> >    TResourceManager;

} //namespace SE

#endif
