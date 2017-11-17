
#ifndef __MESH_STOCK_H_
#define __MESH_STOCK_H_ 1

namespace SE {

struct MeshSettings {

        uint8_t ext_material    : 1,
                skip_normals    : 1;
};

struct MeshStock {

        std::vector <
                std::tuple<
                        std::vector<float>,
                        std::string>
                          >             vShapes;
        std::vector < SE::TTexture * >  vTextures;
        const MeshSettings & oMeshSettings;

        ~MeshStock() throw() { ;; }
        MeshStock(const MeshSettings & oNewMeshSettings) :
                oMeshSettings(oNewMeshSettings) { ;; }
};

struct MeshData {

        uint32_t        buf_id;
        uint32_t        triangles_cnt;
        TTexture      * pTex;
        std::string     sName;
        glm::vec3       min;
        glm::vec3       max;
};


} //namespace SE

#endif
