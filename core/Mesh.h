
#ifndef __MESH_H__
#define __MESH_H__ 1

#include <Transform.h>
#include <Mesh_generated.h>

namespace SE {


class Mesh : public ResourceHolder {

        MeshCtx                 oMeshCtx;
        MeshSettings            oMeshSettings;

        void Load();
        void Load(const SE::FlatBuffers::Mesh * pMesh);
        void DrawShape(const ShapeCtx & oShapeCtx) const;
        void Clean();

        public:

        typedef std::tuple<uint32_t, const std::string &> TShapeInfo;
        typedef std::vector<TShapeInfo> TShapesInfo;
        typedef std::tuple<const glm::vec3 &, const glm::vec3 &> TBBoxDim;

        Mesh(const std::string & sName, const rid_t new_rid, const MeshSettings & oNewMeshSettings = MeshSettings());
        Mesh(const std::string & sName, const rid_t new_rid, const SE::FlatBuffers::Mesh * pMesh, const MeshSettings & oNewMeshSettings = MeshSettings());
        ~Mesh() noexcept;

        uint32_t GetShapesCnt() const;
        uint32_t GetTrianglesCnt() const;
        void     Draw() const;
        void     Draw(const size_t shape_ind) const;
        TShapesInfo GetShapesInfo() const;
        glm::vec3 GetCenter() const;
        glm::vec3 GetCenter(const size_t shape_ind) const;
        void     DrawBBox() const;
        void     DrawBBox(const size_t shape_ind) const;
        std::tuple<const glm::vec3 &, const glm::vec3 &> GetBBox() const;
};

} //namespace SE

#ifdef SE_IMPL
#include <Mesh.tcc>
#endif

#endif
