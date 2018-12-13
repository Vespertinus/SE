
#ifndef __MESH_H__
#define __MESH_H__ 1

#include <Mesh_generated.h>

namespace SE {

class GeometryEntity;

class Mesh : public ResourceHolder {

        uint32_t                        vao_id;
        std::vector <GeometryEntity>    vSubMeshes;//THINK?
        /** last stored BoundingBox inside vector used as combined Mesh BoundingBox */
        std::vector <BoundingBox>       vBBoxes;

        void Load();
        void Load(const SE::FlatBuffers::Mesh * pMesh);

        public:

        Mesh(const std::string & sName, const rid_t new_rid);
        Mesh(const std::string & sName, const rid_t new_rid, const SE::FlatBuffers::Mesh * pMesh);
        ~Mesh() noexcept;

        uint32_t        GetShapesCnt() const;
        uint32_t        GetIndicesCnt() const;
        const std::vector<GeometryEntity> & GetShapes() const;
        glm::vec3       GetCenter() const;
        glm::vec3       GetCenter(const size_t shape_ind) const;
        const           BoundingBox & GetBBox() const;
        const std::vector<BoundingBox> & GetBBoxes() const;
        std::string     Str() const; //one line only
        //std::string     Str(const size_t shape_ind) const; TODO
        std::string     StrDump() const; //multi line full info dump with child objects state
};

} //namespace SE

#ifdef SE_IMPL
#include <Mesh.tcc>
#endif

#endif
