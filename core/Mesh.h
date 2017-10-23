
#ifndef __MESH_H__
#define __MESH_H__ 1

namespace SE {


template <class StoreStrategyList, class LoadStrategyList> class Mesh : public ResourceHolder {

        typedef typename  StoreStrategyList::Head TDefaultStoreStrategy;
        typedef typename  LoadStrategyList::Head  TDefaultLoadStrategy;

        std::vector<MeshData> vMeshData;

        //TODO move pos and rot to separate class
        float pos[3];
        float rot[3];

        glm::vec3 min;
        glm::vec3 max;

        bool ext_material;

        template <class TStoreStrategySettings,  class TLoadStrategySettings> void Create(
                        const std::string oName, 
                        const TStoreStrategySettings & oStoreStrategySettings,
                        const TLoadStrategySettings & oLoadStrategySettings);

        public:
        typedef std::tuple<uint32_t, const std::string &> TShapeInfo;
        typedef std::vector<TShapeInfo> TShapesInfo;
        typedef std::tuple<const glm::vec3 &, const glm::vec3 &> TBBoxDim;

        template <class StoreStrategy, class LoadStrategy> struct TSettings : 
                public SettingsType <LOKI_TYPELIST_2(typename StoreStrategy::Settings, 
                                                     typename LoadStrategy::Settings) > {  };

        template <class TStoreStrategySettings,  class TLoadStrategySettings> 
                Mesh(const std::string oName, 
                     const rid_t new_rid,
                     const TStoreStrategySettings & oStoreStrategySettings, 
                     const TLoadStrategySettings & oLoadStrategySettings,
                     const bool ext_mat = false);
  
        Mesh(const std::string oName, const rid_t new_rid, const bool ext_mat = false);
        ~Mesh() throw();

        uint32_t GetShapesCnt() const;
        uint32_t GetTrianglesCnt() const;
        void     Draw() const;
        void     Draw(const size_t shape_ind) const;
        void     SetPos(const float x, const float y, const float z);
        void     SetRotation(const float x, const float y, const float z);
        TShapesInfo GetShapesInfo() const;
        glm::vec3 GetCenter() const;
        glm::vec3 GetCenter(const size_t shape_ind) const;
        void     DrawBBox() const;
        void     DrawBBox(const size_t shape_ind) const;
        std::tuple<const glm::vec3 &, const glm::vec3 &> GetBBox() const;
        
};

} //namespace SE

#include <Mesh.tcc>

#endif
