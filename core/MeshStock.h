
#ifndef __MESH_STOCK_H_
#define __MESH_STOCK_H_ 1

namespace SE {

struct MeshSettings {

        uint8_t ext_material    : 1;
};

struct ShapeState {

        std::vector<float>      vVertices;
        std::string             sName;
        SE::TTexture          * pTexture;
        uint32_t                triangles_cnt;
        glm::vec3               min;
        glm::vec3               max;
};

struct MeshState {
        std::vector<ShapeState> vShapes;
        glm::vec3               min;
        glm::vec3               max;
        bool                    skip_normals;
};

struct MeshStock {

        MeshState oMeshState;
        const MeshSettings & oMeshSettings;

        ~MeshStock() throw() { ;; }
        MeshStock(const MeshSettings & oNewMeshSettings) :
                oMeshState{},
                oMeshSettings(oNewMeshSettings) { ;; }
};

struct ShapeCtx {

        uint32_t        buf_id;
        uint32_t        triangles_cnt;
        TTexture      * pTex;
        std::string     sName;
        glm::vec3       min;
        glm::vec3       max;
};

struct MeshCtx {
        std::vector<ShapeCtx>   vShapes;
        glm::vec3               min;
        glm::vec3               max;
        uint32_t                stride;
        bool                    skip_normals;
};

} //namespace SE

#endif
