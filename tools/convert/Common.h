#ifndef __CONVERT_COMMON_H__
#define __CONVERT_COMMON_H__

#include <vector>
#include <string>
#include <unordered_map>
#include <variant>
#include <regex>
#include <optional>

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

#include <CommonTypes.h>
#include <BoundingBox.h>
#include <TextureStock.h>
#include <StrID.h>
#include <Logging.h>

namespace SE {
namespace TOOLS {


struct ShapeData {

        uint32_t        start;
        uint32_t        count;
        BoundingBox     oBBox;
        uint16_t        material_index{0};

        ShapeData(uint32_t new_start, uint32_t new_count, const BoundingBox & oNewBBox,
                  uint16_t mat_idx = 0) :
                start(new_start), count(new_count), oBBox(oNewBBox), material_index(mat_idx) { ;; }
};

struct BlendShapeData {
        /*TODO currently only position stored*/

        std::vector<float>      vBuffer;
        //std::vector<std::string> vNames;
        std::vector<float>      vDefaultWeights;
        //uint32_t                 vertices_cnt;
        //std::vector<VertexAttribute>    vAttributes ?? pos, normal, tangent
        std::string             sName;

        uint32_t
                                serialized_fb{};
        uint32_t                serialized_weights_fb{};
};

struct BindPoseData {

        glm::vec4               vBindRot;
        glm::vec3               vBindPos;
        glm::vec3               vBindScale;
};

struct JointData {

        static const uint16_t   ROOT_PARENT_IND = 0xFFFF;

        std::string             sName;
        uint16_t                parent_index{};
        bool                    bind_inited{false};
        BindPoseData            oBindLocal;       ///< local bind SQT (parent-relative)
        BindPoseData            oInvBind;         ///< inv bind SQT (used when inv_bind_is_mat4==false)
        glm::mat4               mInvBindMat4{1.0f}; ///< exact inv bind mat4 from GLTF
        bool                    inv_bind_is_mat4{false}; ///< use inv_bind_mat4 instead of SQT reconstruction
};

struct Skeleton {

        std::string             sName;
        std::vector<JointData>  vJoints;
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
                enum Type : uint8_t {
                        DEST_FLOAT = 1,
                        DEST_INT   = 2
                };

                std::string     sName;
                uint16_t        offset;
                uint8_t         elem_size;
                uint8_t         buffer_ind;
                uint32_t        custom{};
                Type            oDestination{Type::DEST_FLOAT};
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

        uint32_t                        serialized_fb{};
};

using TPackVertexIndex = void (*)(MeshData::TIndexVariant & oData, const uint32_t value);

enum class TextureEncoding : uint8_t {
        RGBA8 = 0,  // raw decoded pixels (legacy / GLTF embedded)
        PNG   = 1,
        JPG   = 2,
        TGA   = 3,
        DDS   = 4,  // future: GPU-compressed BC1..BC7
        KTX2  = 5,  // future: Basis/UASTC
};

struct TextureData {

        std::string                     sPath;
        std::string                     sName;
        std::vector<uint8_t>            vImageData;    // decoded RGBA pixels (GLTF embedded path)
        TextureStock                    oStock;
        std::vector<uint8_t>            vEncodedData;  // original compressed file bytes (inline_all)
        TextureEncoding                 oEncoding{TextureEncoding::RGBA8};
        int32_t                         wrap_mode{10497}; // GL_REPEAT (glTF spec default)
        uint32_t                        serialized_fb{};
};

struct MaterialData {

        using TVariant = std::variant<
                float,
                int32_t,
                glm::vec2,
                glm::vec3,
                glm::vec4,
                glm::uvec2,
                glm::uvec3,
                glm::uvec4>;

        std::unordered_map<std::string, TVariant>       mVariables;
        std::unordered_map<TextureUnit, TextureData>    mTextures;

        /** TODO currently inplace storage for shader program unsupported */
        std::string                                     sShaderPath;
        std::string                                     sName;
        BlendMode                                       oBlendMode{BlendMode::Opaque};

        uint32_t                                        serialized_fb{};
};

struct SkinData {

        Skeleton             * pSkeleton{};
        std::string            sRootNode;
        std::vector<uint16_t>  vJointIndexes;
        std::vector<
                BindPoseData
                >              vJointsInvBindPose;
        BindPoseData           oMeshBindPose;
        std::string            sName;
};

struct ModelData {

        MeshData                    * pMesh{};
        std::vector<MaterialData*>    vMaterials;   ///< one entry per material group; index matches ShapeData::material_index
        BlendShapeData              * pBlendShape{};
        SkinData                    * pSkin{};
};

/** One sampled channel (position/quaternion/scale component) for one bone */
struct AnimCurveChannel {
        enum class Format : uint8_t {
                ConstantF32 = 0,
                StepF32     = 1,
                HermiteF32  = 2,
                LinearF32   = 4  // matches FlatBuffer CurveFormat::LinearF32
        };

        uint16_t                bone_index;
        uint8_t                 target; // 0-2 TX/TY/TZ, 3-6 QX/QY/QZ/QW, 7-9 SX/SY/SZ
        Format                  oFormat{Format::StepF32};
        std::vector<float>      vTimes;
        std::vector<float>      vValues;
        std::vector<float>      vTangents;  // hermite tangents; empty for non-HermiteF32
};

/** Negates quaternion keys with negative dot vs their predecessor, per bone
 *  (targets 3..6), so per-component interpolation follows the short arc.
 *  Mirrors the load-time pass in AnimClip — call once per clip before export. */
void EnforceQuatContinuity(std::vector<AnimCurveChannel> & vChannels);

/**
 * One animation event authored on a FBX stack.
 *
 * Convention: add a custom string property named "events" to the FbxAnimStack
 * (or to the root joint node) with the format:
 *   "time:name:value;time:name:value;..."
 * e.g. "0.40:footstep.left:0.0;0.95:footstep.right:0.0"
 * The value field is optional (defaults to 0.0 if omitted).
 */
struct AnimEventData {
        float           time{};
        std::string     sName;
        float           value{0.0f};
};

/** One exported animation clip (one FBX animation stack) */
struct AnimClipData {
        std::string                     sName;
        float                           duration{};
        bool                            looping{false};
        std::vector<AnimCurveChannel>   vChannels;
        std::vector<AnimEventData>      vEvents;    // sorted by time
};

using TComponent = std::variant<ModelData/*, Camera, Light, CustomComponent etc*/>;

struct NodeData {

        glm::vec3               vTranslation{0};
        glm::vec3               vRotation{0};
        glm::vec3               vScale{1};
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

        std::optional<std::regex>       oCutPath;
        std::string             sReplace;
        std::string             sPackName;
        bool                    skip_normals;
        bool                    skip_material;
        bool                    flip_yz;
        bool                    import_info_prop;
        bool                    import_blend_shapes;
        bool                    import_skin;
        bool                    import_animations;
        bool                    disable_nodes;
        bool                    skeleton_inline{false};  ///< embed skeleton inline vs separate file
        bool                    inline_all{false};       ///< embed all resources (textures, clips) in .sesc
        float                   anim_sample_rate{30.0f};
        /** per-clip import overrides (populated from .sceneimport sidecar) */
        struct ClipImportSettings {
                bool loop{false};
        };
        std::unordered_map<std::string, ClipImportSettings> mClipSettings;
        /** output: exported animation clips */
        std::vector<AnimClipData> vAnimClips;
        /** output: resource path for each clip, parallel to vAnimClips.
         *  Populated in main.cpp before WriteSceneTree so SerializeAnimatorComponent
         *  can reference them.  Uses the FixPath-adjusted relative path. */
        std::vector<std::string> vAnimClipPaths;
        /** output: Skeleton* -> relative resource path (separate mode only) */
        std::unordered_map<Skeleton*, std::string> mSkeletonPaths;
        /** stats */
        uint32_t                node_cnt;
        uint32_t                mesh_cnt;
        uint32_t                shape_cnt;
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
        explicit VertexIndex(uint32_t base_offset);
        bool Get(std::vector<float> & mData, uint32_t & index);
        void Clear();
        uint32_t Size() const;
};

template <class T> void PackValue(MeshData::TIndexVariant & oData, const uint32_t value) {

        (std::get<std::vector<T>>(oData)).emplace_back(value);
}


TPackVertexIndex PackVertexIndexInit(const uint32_t index_size, MeshData::TIndexVariant & oIndex);


struct ResourceStash {

        using TResourceData = std::variant<MeshData, TextureData, MaterialData, BlendShapeData, Skeleton, SkinData>;
        /** TODO later rewrite on tuple of vectors of all types
         and return handle
         */
        using TResourceMap  = std::unordered_map<StrID, std::unique_ptr<TResourceData>>;
        TResourceMap    mResources;

        template <class TResource> bool GetResourceData(const StrID name_id, TResource ** pResource);
        inline void Clear();
};

template <class TResource> bool ResourceStash::GetResourceData(const StrID name_id, TResource ** pResource) {

        bool created = false;

        auto itResource = mResources.find(name_id);
        if (itResource == mResources.end()) {
                auto * pItem = mResources.emplace(name_id, std::make_unique<TResourceData>(TResource{})).first->second.get();
                *pResource = std::get_if<TResource>(pItem);
                se_assert(*pResource);
                created = true;
                //log_d("item type: '{}', var index: {}, name_id: '{}'", typeid(TResource).name(), oItem.index(), name_id);
        }
        else {
                *pResource = std::get_if<TResource>(itResource->second.get());
                //log_d("item type: '{}', name_id: '{}'", typeid(TResource).name(), name_id);
                se_assert(*pResource);
        }

        return created;
}

void ResourceStash::Clear() {

        mResources.clear();
}

}
}

#endif
