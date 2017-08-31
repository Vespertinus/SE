
#ifndef __MESH_STOCK_H_
#define __MESH_STOCK_H_ 1

namespace SE {

struct MeshStock {

        std::vector < std::vector<float> > vShapes;
        std::vector < SE::TTexture * >     vTextures;

        ~MeshStock() throw() { ;; }
};

struct MeshData {

        uint32_t   buf_id;
        uint32_t   triangles_cnt;
        TTexture * pTex;
};

} //namespace SE

#endif
