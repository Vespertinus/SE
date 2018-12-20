#ifndef __CONVERT_COMMON_H__
#define __CONVERT_COMMON_H__

#include <vector>
#include <string>
#include <unordered_map>
#include <variant>
#include <regex>

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

#include <CommonTypes.h>
#include <BoundingBox.h>
#include <TextureStock.h>

namespace SE {
namespace TOOLS {


struct ShapeData {

        uint32_t        start;
        uint32_t        count;
        BoundingBox     oBBox;

        ShapeData(uint32_t new_start, uint32_t new_count, const BoundingBox & oNewBBox) :
                start(new_start), count(new_count), oBBox(oNewBBox) { ;; }
};

struct BlendShapeData {
        /*TODO currently only position stored*/

        std::vector<float>      vBuffer;
        //std::vector<std::string> vNames;
        std::vector<float>      vDefaultWeights;
        //uint32_t                 vertices_cnt;
        //std::vector<VertexAttribute>    vAttributes ?? pos, normal, tangent
        std::string             sName;
};


struct MeshData {

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

        struct VertexBuffer {
                TVertexVariant  oBuffer;
                uint8_t         stride;
        };

        TIndexVariant                   oIndex;
        std::vector<VertexBuffer>       vVertexBuffers;
        std::vector<VertexAttribute>    vAttributes;

        std::vector<ShapeData>          vShapes;
        BoundingBox                     oBBox;
        std::string                     sName;
};

using TPackVertexIndex = void (*)(MeshData::TIndexVariant & oData, const uint32_t value);

struct TextureData {

        std::string             sPath;
        TextureStock            oStock;
};

//TODO cache inside ctx
struct MaterialData {

        using TVariant = std::variant<
                float,
                glm::vec2,
                glm::vec3,
                glm::vec4,
                glm::uvec2,
                glm::uvec3,
                glm::uvec4>;

        std::unordered_map<std::string, TVariant> mVariables;
        std::unordered_map<TextureUnit, TextureData> mTextures;

        /** TODO currently inplace storage for shader program unsupported */
        std::string sShaderPath;
        std::string sName;
};

struct ModelData {

        MeshData        oMesh;
        MaterialData    oMaterial;
        BlendShapeData  oBlendShape{};
};

using TComponent = std::variant<ModelData/*, Camera, Light, CustomComponent etc*/>;

struct NodeData {

        glm::vec3               translation;
        glm::vec3               rotation;
        glm::vec3               scale;
        std::string             sName;
        std::vector<NodeData>   vChildren;
        std::vector<TComponent> vComponents;
        std::string             sInfo;
        bool                    enabled = true;
};


/**
 TODO support cross pack asset reference
 store id only for asset reference
 separate resource list: asset id -> flatbuffers offset
 load resource list from pack files

 global asset id stored inside database (sqlite)
 all tools generate id, using db and input source file

 two different range in runtime;
        4byte id range for serialized assets
        4byte id range for runtime objects

 ResourceManager:
        Create for runtime objects
        Load for serialized objects by id

 */
struct ImportCtx {

        std::optional<std::regex>       sCutPath;
        std::string             sReplace;
        std::string             sPackName;
        bool                    skip_normals;
        bool                    skip_material;
        bool                    flip_yz;
        bool                    import_info_prop;
        bool                    import_blend_shape;
        bool                    disable_nodes;
        /** stats */
        uint32_t                node_cnt;
        uint32_t                mesh_cnt;
        uint32_t                total_triangles_cnt;
        uint32_t                total_vertices_cnt;
        uint32_t                textures_cnt;
        uint32_t                material_cnt;

        void FixPath(std::string & sPath);
};

class VertexIndex {

        std::unordered_map<uint64_t, uint32_t> mIndex;
        uint32_t                               last_index;

        public:

        VertexIndex();
        bool Get(std::vector<float> & mData, uint32_t & index);
        void Clear();
        uint32_t Size() const;
};

template <class T> void PackValue(MeshData::TIndexVariant & oData, const uint32_t value) {

        (std::get<std::vector<T>>(oData)).emplace_back(value);
}


TPackVertexIndex PackVertexIndexInit(const uint32_t index_size, MeshData::TIndexVariant & oIndex);

}
}
#endif
