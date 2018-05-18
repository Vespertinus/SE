#ifndef __CONVERT_COMMON_H__
#define __CONVERT_COMMON_H__

#include <vector>
#include <string>
#include <unordered_map>
#include <variant>
#include <regex>

#include <glm/vec3.hpp>

namespace SE {
namespace TOOLS {


struct ShapeData {

        using TIndexVariant = std::variant <
                std::vector<uint8_t>,
                std::vector<uint16_t>,
                std::vector<uint32_t> >;

        using TVertexVariant = std::variant <
                std::vector<float>,
                std::vector<uint8_t>,
                std::vector<uint32_t> >;

        struct VertexAttribute {
                std::string     sName;
                uint16_t        offset;
                uint8_t         elem_size;
                uint8_t         buffer_ind;
        };

        TIndexVariant                   oIndex;
        std::vector<TVertexVariant>     vVertexBuffers;
        std::vector<VertexAttribute>    vAttributes;

        std::string                     sName;
        std::string                     sTextureName;
        uint32_t                        triangles_cnt;
        glm::vec3                       min;
        glm::vec3                       max;
        uint8_t                         stride;
};

using TPackVertexIndex = void (*)(ShapeData::TIndexVariant & oData, const uint32_t value);

struct MeshData {

        std::vector<ShapeData>  vShapes;
        glm::vec3               min;
        glm::vec3               max;
        std::string             sName;
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

        std::optional<std::regex>       sCutPath;
        std::string             sReplace;
        bool                    skip_normals;
        bool                    flip_yz;
        /** stats */
        uint32_t                node_cnt;
        uint32_t                mesh_cnt;
        uint32_t                total_triangles_cnt;
        uint32_t                total_vertices_cnt;
        uint32_t                textures_cnt;

        void FixPath(std::string & sPath);
};

class VertexIndex {

        std::unordered_map<uint64_t, uint32_t> mIndex;
        uint32_t                               last_index;

        public:

        VertexIndex();
        bool Get(std::vector<float> & mData, uint32_t & index);
        void Clear();
};

template <class T> void PackValue(ShapeData::TIndexVariant & oData, const uint32_t value) {

        (std::get<std::vector<T>>(oData)).emplace_back(value);
}


TPackVertexIndex PackVertexIndexInit(const uint32_t index_size, ShapeData::TIndexVariant & oIndex);

}
}
#endif
