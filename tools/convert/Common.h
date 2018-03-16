#ifndef __CONVERT_COMMON_H__
#define __CONVERT_COMMON_H__

#include <vector>
#include <string>
#include <glm/vec3.hpp>

namespace SE {
namespace TOOLS {


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
        glm::vec3               min;
        glm::vec3               max;
        std::string             sName;
        bool                    skip_normals;

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

        std::string             sCutPath;
        std::string             sReplace;
        bool                    skip_normals;
        bool                    flip_yz;
        /** stats */
        uint32_t                node_cnt;
        uint32_t                mesh_cnt;
        uint32_t                total_triangles_cnt;
        uint32_t                textures_cnt;

        void FixPath(std::string & sPath);
};


}
}
#endif
