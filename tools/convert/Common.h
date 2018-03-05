#ifndef __CONVERT_COMMON_H__
#define __CONVERT_COMMON_H__

#include <vector>
#include <string>
#include <glm/vec3.hpp>

namespace SE {
namespace TOOLS {

static const uint8_t VERTEX_BASE_SIZE  = 3 + 2;
static const uint8_t VERTEX_SIZE       = VERTEX_BASE_SIZE + 3;

struct ShapeData {

        std::vector<float>      vVertices;  // pos(3float), normal(3float), tex(2float)
        std::string             sName;
        std::string             sTextureName;
        uint32_t                triangles_cnt;
        glm::vec3               min;
        glm::vec3               max;
};

struct MeshData {

        std::vector<ShapeData>  vShapes;
        bool                    skip_normals;
        glm::vec3               min;
        glm::vec3               max;

};

struct NodeData {

        std::string             sName;
        glm::vec3               translation;
        glm::vec3               rotation;
        glm::vec3               scale;
        std::vector<NodeData>   vChildren;
        std::vector<MeshData>   vEntity;
};

struct ImportCtx {

        bool                    skip_normals;
        std::string             sCutPath;
        std::string             sReplace;
        /** stats */
        uint32_t                node_cnt;
        uint32_t                mesh_cnt;
        uint32_t                total_triangles_cnt;
        uint32_t                textures_cnt;

        void FixPath(std::string & sPath);
};

void CalcNormal(float normals[3], float v0[3], float v1[3], float v2[3]);

void CalcBasicBBox(
                std::vector<float> & vVertices,
                const uint8_t        elem_size,
                glm::vec3          & min,
                glm::vec3          & max);

template <class T> void CalcCompositeBBox(
                std::vector<T>     & vItems,
                glm::vec3          & min,
                glm::vec3          & max) {

                min = glm::vec3(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
                max = glm::vec3(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());

                for (auto & oItem : vItems) {
                        min.x = std::min(oItem.min.x, min.x);
                        min.y = std::min(oItem.min.y, min.y);
                        min.z = std::min(oItem.min.z, min.z);

                        max.x = std::max(oItem.max.x, max.x);
                        max.y = std::max(oItem.max.y, max.y);
                        max.z = std::max(oItem.max.z, max.z);
                }
}

}
}
#endif
