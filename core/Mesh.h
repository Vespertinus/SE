
#ifndef __MESH_H__
#define __MESH_H__ 1

namespace SE {


template <class StoreStrategyList, class LoadStrategyList> class Mesh : public ResourceHolder {

        typedef typename  StoreStrategyList::Head TDefaultStoreStrategy;
        typedef typename  LoadStrategyList::Head  TDefaultLoadStrategy;

        std::vector<MeshData> vMeshData;

        template <class TStoreStrategySettings,  class TLoadStrategySettings> void Create(
                        const std::string oName, 
                        const TStoreStrategySettings & oStoreStrategySettings, 
                        const TLoadStrategySettings & oLoadStrategySettings);

        public:

        template <class StoreStrategy, class LoadStrategy> struct TSettings : 
                public SettingsType <LOKI_TYPELIST_2(typename StoreStrategy::Settings, 
                                                     typename LoadStrategy::Settings) > {  };

        template <class TStoreStrategySettings,  class TLoadStrategySettings> 
                Mesh(const std::string oName, 
                     const rid_t new_rid,
                     const TStoreStrategySettings & oStoreStrategySettings, 
                     const TLoadStrategySettings & oLoadStrategySettings);
  
        Mesh(const std::string oName, const rid_t new_rid);
        ~Mesh() throw();

        uint32_t GetShapesCnt() const;
        uint32_t GetTrianglesCnt() const;
};

} //namespace SE

#include <Mesh.tcc>

#endif
