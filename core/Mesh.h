
#ifndef __MESH_H__
#define __MESH_H__ 1

#include <Transform.h>
#include <Mesh_generated.h>

namespace SE {


template <class StoreStrategyList, class LoadStrategyList> class Mesh : public ResourceHolder {

        typedef typename  StoreStrategyList::Head TDefaultStoreStrategy;
        typedef typename  LoadStrategyList::Head  TDefaultLoadStrategy;

        MeshCtx                 oMeshCtx;
        Transform       const * pTransform; //TODO into iface struct

        MeshSettings            oMeshSettings;

        template <class TStoreStrategySettings,  class TLoadStrategySettings> void Import(
                        const TStoreStrategySettings & oStoreStrategySettings,
                        const TLoadStrategySettings & oLoadStrategySettings);

        void Load();
        void Load(const SE::FlatBuffers::Mesh * pMesh);

        void DrawShape(const ShapeCtx & oShapeCtx) const;

        public:

        typedef std::tuple<uint32_t, const std::string &> TShapeInfo;
        typedef std::vector<TShapeInfo> TShapesInfo;
        typedef std::tuple<const glm::vec3 &, const glm::vec3 &> TBBoxDim;

        template <class StoreStrategy, class LoadStrategy> struct TSettings :
                public SettingsType <LOKI_TYPELIST_2(typename StoreStrategy::Settings,
                                                     typename LoadStrategy::Settings) > {  };

        template <class TStoreStrategySettings,  class TLoadStrategySettings>
                Mesh(const std::string & sName,
                     const rid_t new_rid,
                     const TStoreStrategySettings & oStoreStrategySettings,
                     const TLoadStrategySettings & oLoadStrategySettings,
                     const MeshSettings & oNewMeshSettings);

        template <class TConcreateSettings, std::enable_if_t< MP::InnerContain<StoreStrategyList, TConcreateSettings>::value, TConcreateSettings> * = nullptr>
                Mesh(const std::string & sName,
                     const rid_t new_rid,
                     const TConcreateSettings & oSettings,
                     const MeshSettings & oNewMeshSettings);

        template <class TConcreateSettings, std::enable_if_t< MP::InnerContain<LoadStrategyList, TConcreateSettings>::value, TConcreateSettings> * = nullptr>
                Mesh(const std::string & sName,
                     const rid_t new_rid,
                     const TConcreateSettings & oSettings,
                     const MeshSettings & oNewMeshSettings);

        Mesh(const std::string & sName, const rid_t new_rid, const MeshSettings & oNewMeshSettings = MeshSettings());
        Mesh(const std::string & sName, const rid_t new_rid, const SE::FlatBuffers::Mesh * pMesh, const MeshSettings & oNewMeshSettings = MeshSettings());
        ~Mesh() noexcept;

        uint32_t GetShapesCnt() const;
        uint32_t GetTrianglesCnt() const;
        void     Draw() const;
        void     Draw(const size_t shape_ind) const;
        void     SetTransform(Transform const * const pNewTransform);
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
