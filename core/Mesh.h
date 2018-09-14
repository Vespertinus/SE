
#ifndef __MESH_H__
#define __MESH_H__ 1

#include <Transform.h>
#include <Mesh_generated.h>

namespace SE {

struct MeshSettings {

        uint8_t ext_material    : 1;
};

struct ShapeCtx {

        uint32_t        vao_id;
        uint32_t        triangles_cnt;
        uint32_t        gl_index_type;
        //TODO move into material manipulation
        TTexture      * pTex;
        ShaderProgram * pShader;

        std::string     sName;
        BoundingBox     oBBox;
};

struct MeshCtx {
        std::vector<ShapeCtx>   vShapes;
        BoundingBox             oBBox;
        uint32_t                stride;//FIXME
        bool                    skip_normals;//FIXME
};


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

        uint32_t        GetShapesCnt() const;
        uint32_t        GetTrianglesCnt() const;
        void            Draw() const;
        void            Draw(const size_t shape_ind) const;
        TShapesInfo     GetShapesInfo() const;
        glm::vec3       GetCenter() const;
        glm::vec3       GetCenter(const size_t shape_ind) const;
        const           BoundingBox & GetBBox() const;
        std::string     Str() const;
        //std::string     Str(const size_t shape_ind) const; TODO
};

} //namespace SE

#ifdef SE_IMPL
#include <Mesh.tcc>
#endif

#endif
