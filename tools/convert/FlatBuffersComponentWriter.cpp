
#include <fstream>

#include <Logging.h>
#include <MPUtil.h>
#include <Component_generated.h>
#include <AnimationGraph_generated.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include "FlatBuffersMeshWriterDetails.h"
#include "FlatBuffersAnimWriter.h"
#include "FlatBuffersComponentWriterDetails.h"

namespace SE {
namespace TOOLS {

using TComponentOffset = std::tuple<flatbuffers::Offset<SE::FlatBuffers::Component>, SE::ret_code_t>;

TComponentOffset SerializeModel(
                const ModelData                 & oModel,
                flatbuffers::FlatBufferBuilder  & oBuilder,
                const ImportCtx                 & oCtx);

TComponentOffset SerializeComponent(
                const TComponent                & oComponent,
                flatbuffers::FlatBufferBuilder  & oBuilder,
                const ImportCtx                 & oCtx) {

         return MP::Visit(oComponent,
                        [&oBuilder, &oCtx](const ModelData & oModel) {
                                return SerializeModel(oModel, oBuilder, oCtx);
                        },/*
                        [&oBuilder](const Camera & oCamera) {

                        },*/
                        [](auto & arg) {
                                log_e("unsupported component type: '{}'", typeid(arg).name());
                                return {0, uLOGIC_ERROR};
                        }
        );
}


TComponentOffset SerializeModel(
                const ModelData                 & oModel,
                flatbuffers::FlatBufferBuilder  & oBuilder,
                const ImportCtx                 & oCtx) {

        using namespace SE::FlatBuffers;

        //FIXME types inside Material, flatbuffer scheme and asset importer out of sync
        using FBVariable = std::tuple<float, int32_t, Vec2 *, Vec3 *, Vec4 *, UVec2 *, UVec3 *, UVec4 *>;
        std::tuple<Vec2, Vec3, Vec4, UVec2, UVec3, UVec4> oValueStorage;

        // Helper: serialize one MaterialData* → MaterialHolder offset (0 if null/empty shader)
        auto SerializeOneMaterial = [&](MaterialData * pMat) -> flatbuffers::Offset<SE::FlatBuffers::MaterialHolder> {
                if (!pMat || pMat->sShaderPath.empty()) return 0;

                if (pMat->serialized_fb) return pMat->serialized_fb;

                std::vector<flatbuffers::Offset<TextureHolder>>  vTextures;
                std::vector<flatbuffers::Offset<ShaderVariable>> vShaderVariables;

                for (auto & oItem : pMat->mTextures) {

                        log_d("Texture: path: {}, name: {}, vImageData size: {}, vEncodedData size: {}, enc: {}, TextureUnit: {}, format: {}, internal_format: {}, widhth: {}, height: {}",
                                        oItem.second.sPath,
                                        oItem.second.sName,
                                        oItem.second.vImageData.size(),
                                        oItem.second.vEncodedData.size(),
                                        static_cast<uint8_t>(oItem.second.oEncoding),
                                        static_cast<int32_t>(oItem.first),
                                        oItem.second.oStock.format,
                                        oItem.second.oStock.internal_format,
                                        oItem.second.oStock.width,
                                        oItem.second.oStock.height);

                        if (!oItem.second.serialized_fb) {

                                flatbuffers::Offset<SE::FlatBuffers::TextureStock> stock_fb  = 0;
                                flatbuffers::Offset<flatbuffers::String>           path_fb   = 0;
                                flatbuffers::Offset<flatbuffers::String>           name_fb   = 0;
                                flatbuffers::Offset<void>                          store_union_fb = 0;
                                StoreSettings                                      store_type = StoreSettings::NONE;

                                const int32_t wrap = oItem.second.wrap_mode;
                                if (!oItem.second.vEncodedData.empty()) {
                                        stock_fb = CreateTextureStockDirect(
                                                oBuilder,
                                                nullptr, 0, 0, 0, 0,
                                                &oItem.second.vEncodedData,
                                                static_cast<SE::FlatBuffers::TextureEncoding>(oItem.second.oEncoding));
                                        name_fb        = oBuilder.CreateString(oItem.second.sName);
                                        store_union_fb = CreateStoreTexture2D(oBuilder, wrap).Union();
                                        store_type     = StoreSettings::StoreTexture2D;
                                }
                                else if (oItem.second.oStock.raw_image != nullptr) {
                                        stock_fb = CreateTextureStockDirect(
                                                oBuilder,
                                                &oItem.second.vImageData,
                                                oItem.second.oStock.format,
                                                oItem.second.oStock.internal_format,
                                                oItem.second.oStock.width,
                                                oItem.second.oStock.height);
                                        name_fb        = oBuilder.CreateString(oItem.second.sName);
                                        store_union_fb = CreateStoreTexture2D(oBuilder, wrap).Union();
                                        store_type     = StoreSettings::StoreTexture2D;
                                }
                                else if (!oItem.second.sPath.empty()) {
                                        path_fb        = oBuilder.CreateString(oItem.second.sPath);
                                        store_union_fb = CreateStoreTexture2D(oBuilder, wrap).Union();
                                        store_type     = StoreSettings::StoreTexture2D;
                                }

                                auto texture_holder_fb = vTextures.emplace_back(CreateTextureHolder(
                                        oBuilder,
                                        stock_fb,
                                        path_fb,
                                        name_fb,
                                        store_type,
                                        store_union_fb,
                                        static_cast<SE::FlatBuffers::TextureUnit>(oItem.first)));
                                oItem.second.serialized_fb = texture_holder_fb.o;
                        }
                        else {
                                vTextures.emplace_back(oItem.second.serialized_fb);
                        }
                }

                for (auto & oItem : pMat->mVariables) {

                        FBVariable oValue{};

                        MP::Visit(oItem.second,
                                [&oValue](const float var)           { std::get<float>(oValue)    = var; },
                                [&oValue](const int32_t var)         { std::get<int32_t>(oValue)  = var; },
                                [&oValue, &oValueStorage](const glm::vec2 & vData) {
                                        std::get<Vec2>(oValueStorage) = Vec2(vData.x, vData.y);
                                        std::get<Vec2 *>(oValue) = &std::get<Vec2>(oValueStorage);
                                },
                                [&oValue, &oValueStorage](const glm::vec3 & vData) {
                                        std::get<Vec3>(oValueStorage) = Vec3(vData.x, vData.y, vData.z);
                                        std::get<Vec3 *>(oValue) = &std::get<Vec3>(oValueStorage);
                                },
                                [&oValue, &oValueStorage](const glm::vec4 & vData) {
                                        std::get<Vec4>(oValueStorage) = Vec4(vData.x, vData.y, vData.z, vData.w);
                                        std::get<Vec4 *>(oValue) = &std::get<Vec4>(oValueStorage);
                                },
                                [&oValue, &oValueStorage](const glm::uvec2 & vData) {
                                        std::get<UVec2>(oValueStorage) = UVec2(vData.x, vData.y);
                                        std::get<UVec2 *>(oValue) = &std::get<UVec2>(oValueStorage);
                                },
                                [&oValue, &oValueStorage](const glm::uvec3 & vData) {
                                        std::get<UVec3>(oValueStorage) = UVec3(vData.x, vData.y, vData.z);
                                        std::get<UVec3 *>(oValue) = &std::get<UVec3>(oValueStorage);
                                },
                                [&oValue, &oValueStorage](const glm::uvec4 & vData) {
                                        std::get<UVec4>(oValueStorage) = UVec4(vData.x, vData.y, vData.z, vData.w);
                                        std::get<UVec4 *>(oValue) = &std::get<UVec4>(oValueStorage);
                                }
                        );

                        vShaderVariables.emplace_back(CreateShaderVariable(
                                oBuilder,
                                oBuilder.CreateString(oItem.first),
                                std::get<float>(oValue),
                                std::get<int32_t>(oValue),
                                std::get<Vec2 *>(oValue),
                                std::get<Vec3 *>(oValue),
                                std::get<Vec4 *>(oValue),
                                std::get<UVec2 *>(oValue),
                                std::get<UVec3 *>(oValue),
                                std::get<UVec4 *>(oValue)));
                }

                auto holder_fb = CreateMaterialHolder(
                        oBuilder,
                        CreateMaterial(
                                oBuilder,
                                CreateShaderProgramHolder(oBuilder, 0, oBuilder.CreateString(pMat->sShaderPath), 0),
                                oBuilder.CreateVector(vTextures),
                                oBuilder.CreateVector(vShaderVariables),
                                static_cast<SE::FlatBuffers::BlendMode>(pMat->oBlendMode)),
                        0,
                        oBuilder.CreateString(pMat->sName));

                pMat->serialized_fb = holder_fb.o;
                return holder_fb;
        };

        // For any animated model (blendshapes, bones, or both), remap pbr_geometry →
        // pbr_geometry_animated BEFORE SerializeOneMaterial caches pMat->serialized_fb.
        const bool isAnimated = (oModel.pBlendShape && !oModel.pBlendShape->vDefaultWeights.empty())
                             || (oModel.pSkin       && !oModel.pSkin->vJointIndexes.empty());
        if (isAnimated) {
                static const std::string sPbr         = "shader_program/pbr_geometry.sesp";
                static const std::string sPbrAnimated = "shader_program/pbr_geometry_animated.sesp";
                for (auto * pMat : oModel.vMaterials) {
                        if (pMat && pMat->sShaderPath == sPbr)
                                pMat->sShaderPath = sPbrAnimated;
                }
        }


        flatbuffers::Offset<SE::FlatBuffers::MeshHolder> mesh_holder_fb{};

        if (!oModel.pMesh->serialized_fb) { //need to serialize

                auto [mesh_fb, res] = SerializeMesh(*oModel.pMesh, oBuilder);
                if (res != uSUCCESS) {
                        return {0, res};
                }

                mesh_holder_fb = CreateMeshHolder(
                                oBuilder,
                                mesh_fb,
                                0,
                                oBuilder.CreateString(oModel.pMesh->sName)
                                );

                oModel.pMesh->serialized_fb = mesh_holder_fb.o;
        }
        else { //already serialized
                mesh_holder_fb = oModel.pMesh->serialized_fb;
        }


        if ((!oModel.pBlendShape || oModel.pBlendShape->vDefaultWeights.size() == 0) && (!oModel.pSkin || oModel.pSkin->vJointIndexes.size() == 0)) { //StaticModel

                std::vector<flatbuffers::Offset<SE::FlatBuffers::MaterialHolder>> vMatFB;
                vMatFB.reserve(oModel.vMaterials.size());
                for (auto * pMat : oModel.vMaterials) {
                        vMatFB.emplace_back(SerializeOneMaterial(pMat));
                }

                auto model_fb = CreateStaticModel(
                                oBuilder,
                                mesh_holder_fb,
                                oBuilder.CreateVector(vMatFB)
                                ).Union();
                return { CreateComponent(oBuilder, ComponentU::StaticModel, model_fb), uSUCCESS };
        }
        else { //AnimatedModel
//FIXME
#define SE_GL_R32F 0x822E

                flatbuffers::Offset<TextureHolder>      blendshapes_fb          = 0;
                flatbuffers::Offset<
                        flatbuffers::Vector<float>
                        >                               default_weights_fb      = 0;
                flatbuffers::Offset<SkeletonHolder>     skeleton_holder_fb      = 0;
                flatbuffers::Offset<flatbuffers::String>                skeleton_root_node_fb   = 0;

                flatbuffers::Offset<
                        flatbuffers::Vector<uint16_t>
                        >                               joints_indexes_fb       = 0;
                flatbuffers::Offset<
                        flatbuffers::Vector<
                                flatbuffers::Offset<BindSQT>
                                >
                        >                               joints_inv_bind_pose_fb = 0;

                flatbuffers::Offset<
                        BindSQT
                        >                               mesh_bind_fb            = 0;


                //___Start___ bs
                if (oModel.pBlendShape && oModel.pBlendShape->vDefaultWeights.size() != 0) {

                        if (!oModel.pBlendShape->serialized_fb) {

                                auto tex_stock_fb = CreateTextureStock(
                                                oBuilder,
                                                oBuilder.CreateVector(
                                                        reinterpret_cast<const uint8_t *>(&oModel.pBlendShape->vBuffer[0]),
                                                        oModel.pBlendShape->vBuffer.size() * sizeof(float)),
                                                0,
                                                SE_GL_R32F,
                                                oModel.pBlendShape->vBuffer.size(),
                                                0);

                                blendshapes_fb = CreateTextureHolder(
                                                oBuilder,
                                                tex_stock_fb,
                                                0,
                                                oBuilder.CreateString(oModel.pBlendShape->sName),
                                                StoreSettings::StoreTextureBuffer,
                                                CreateStoreTextureBuffer(oBuilder).Union(),
                                                SE::FlatBuffers::TextureUnit::UNIT_BUFFER);

                                default_weights_fb = oBuilder.CreateVector(oModel.pBlendShape->vDefaultWeights);

                                oModel.pBlendShape->serialized_fb              = blendshapes_fb.o;
                                oModel.pBlendShape->serialized_weights_fb      = default_weights_fb.o;
                        }
                        else {
                             blendshapes_fb     = oModel.pBlendShape->serialized_fb;
                             default_weights_fb = oModel.pBlendShape->serialized_weights_fb;
                        }
                }
                //___End_____ bs

                //___Start___ skeleton
                if (oModel.pSkin && oModel.pSkin->vJointIndexes.size() != 0) {

                        se_assert(!oModel.pSkin->sRootNode.empty());

                        skeleton_root_node_fb = oBuilder.CreateString(oModel.pSkin->sRootNode);
                        {

                        mesh_bind_fb            = CreateBindSQT(
                                        oBuilder,
                                        reinterpret_cast<const SE::FlatBuffers::Vec4 *>(&oModel.pSkin->oMeshBindPose.vBindRot[0]),
                                        reinterpret_cast<const SE::FlatBuffers::Vec3 *>(&oModel.pSkin->oMeshBindPose.vBindPos[0]),
                                        reinterpret_cast<const SE::FlatBuffers::Vec3 *>(&oModel.pSkin->oMeshBindPose.vBindScale[0])
                                        );

                        std::vector<flatbuffers::Offset<SE::FlatBuffers::BindSQT>> vJointsBind;
                        vJointsBind.reserve(oModel.pSkin->vJointsInvBindPose.size());

                        for (auto & oItem : oModel.pSkin->vJointsInvBindPose) {
                                vJointsBind.emplace_back(
                                                CreateBindSQT(
                                                        oBuilder,
                                                        reinterpret_cast<const SE::FlatBuffers::Vec4 *>(&oItem.vBindRot[0]),
                                                        reinterpret_cast<const SE::FlatBuffers::Vec3 *>(&oItem.vBindPos[0]),
                                                        reinterpret_cast<const SE::FlatBuffers::Vec3 *>(&oItem.vBindScale[0])
                                                        )
                                                );
                        }

                        joints_indexes_fb       = oBuilder.CreateVector(oModel.pSkin->vJointIndexes);
                        joints_inv_bind_pose_fb = oBuilder.CreateVector(vJointsBind);

                        // Build SkeletonHolder
                        if (oModel.pSkin->pSkeleton) {
                                if (oCtx.skeleton_inline) {
                                        // Embed skeleton data directly into the scene buffer
                                        std::vector<flatbuffers::Offset<SE::FlatBuffers::SkeletonBone>> vBones;
                                        vBones.reserve(oModel.pSkin->pSkeleton->vJoints.size());
                                        for (const auto & jd : oModel.pSkin->pSkeleton->vJoints) {
                                                auto name_fb = oBuilder.CreateString(jd.sName);
                                                SE::FlatBuffers::Vec3 bpos{ jd.oBindLocal.vBindPos.x, jd.oBindLocal.vBindPos.y, jd.oBindLocal.vBindPos.z };
                                                SE::FlatBuffers::Vec4 brot{ jd.oBindLocal.vBindRot.x, jd.oBindLocal.vBindRot.y, jd.oBindLocal.vBindRot.z, jd.oBindLocal.vBindRot.w };
                                                SE::FlatBuffers::Vec3 bscl{ jd.oBindLocal.vBindScale.x, jd.oBindLocal.vBindScale.y, jd.oBindLocal.vBindScale.z };
                                                glm::quat q{ jd.oInvBind.vBindRot.w, jd.oInvBind.vBindRot.x, jd.oInvBind.vBindRot.y, jd.oInvBind.vBindRot.z };
                                                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(jd.oInvBind.vBindPos))
                                                            * glm::mat4_cast(q)
                                                            * glm::scale(glm::mat4(1.0f), glm::vec3(jd.oInvBind.vBindScale));
                                                SE::FlatBuffers::ColMat4 cm{
                                                        {m[0][0],m[0][1],m[0][2],m[0][3]},
                                                        {m[1][0],m[1][1],m[1][2],m[1][3]},
                                                        {m[2][0],m[2][1],m[2][2],m[2][3]},
                                                        {m[3][0],m[3][1],m[3][2],m[3][3]}
                                                };
                                                vBones.push_back(CreateSkeletonBone(oBuilder, name_fb, jd.parent_index, &bpos, &brot, &bscl, &cm));
                                        }
                                        auto bones_fb     = oBuilder.CreateVector(vBones);
                                        auto skel_data_fb = CreateSkeleton(oBuilder, 1, bones_fb);
                                        auto name_fb      = oBuilder.CreateString(oModel.pSkin->pSkeleton->sName);
                                        skeleton_holder_fb = CreateSkeletonHolder(oBuilder, skel_data_fb, 0, name_fb);
                                } else {
                                        // Reference separate .sesk file
                                        auto it = oCtx.mSkeletonPaths.find(oModel.pSkin->pSkeleton);
                                        if (it != oCtx.mSkeletonPaths.end() && !it->second.empty()) {
                                                auto path_fb = oBuilder.CreateString(it->second);
                                                skeleton_holder_fb = CreateSkeletonHolder(oBuilder, 0, path_fb, 0);
                                        }
                                }
                        }
                        } // inner scope
                }
                //___End_____ skeleton

                std::vector<flatbuffers::Offset<SE::FlatBuffers::MaterialHolder>> vAnimMatFB;
                vAnimMatFB.reserve(oModel.vMaterials.size());
                for (auto * pMat : oModel.vMaterials)
                        vAnimMatFB.emplace_back(SerializeOneMaterial(pMat));
                auto anim_materials_fb = oBuilder.CreateVector(vAnimMatFB);

                auto model_fb = CreateAnimatedModel(
                                oBuilder,
                                mesh_holder_fb,
                                anim_materials_fb,
                                blendshapes_fb,
                                default_weights_fb,
                                skeleton_holder_fb,
                                skeleton_root_node_fb,
                                joints_indexes_fb,
                                joints_inv_bind_pose_fb,
                                mesh_bind_fb
                                ).Union();
                return { CreateComponent(oBuilder, ComponentU::AnimatedModel, model_fb), uSUCCESS };
        }


        return {0, uWRONG_INPUT_DATA};
}


flatbuffers::Offset<SE::FlatBuffers::Component> SerializeAnimatorComponent(
                flatbuffers::FlatBufferBuilder  & oBuilder,
                const ImportCtx                 & oCtx) {

        using namespace SE::FlatBuffers;

        if (oCtx.vAnimClips.empty()) return 0;
        if (!oCtx.inline_all && oCtx.vAnimClipPaths.size() != oCtx.vAnimClips.size()) return 0;

        // Build one AnimState per clip — each state has a single ClipNode as its root.
        std::vector<flatbuffers::Offset<AnimState>> vStates;
        vStates.reserve(oCtx.vAnimClips.size());

        for (size_t i = 0; i < oCtx.vAnimClips.size(); ++i) {
                const auto & clip      = oCtx.vAnimClips[i];

                flatbuffers::Offset<AnimClipHolder> holder_fb;
                if (oCtx.inline_all) {
                        auto [clip_fb, rc] = SerializeAnimClip(clip, oBuilder);
                        if (rc != SE::uSUCCESS) continue;
                        holder_fb = CreateAnimClipHolder(
                                oBuilder,
                                oBuilder.CreateString(clip.sName),  // name
                                0,                                  // path
                                clip_fb);                           // clip
                } else {
                        const auto & clip_path = oCtx.vAnimClipPaths[i];
                        if (clip_path.empty()) continue;
                        holder_fb = CreateAnimClipHolder(
                                oBuilder,
                                0,                                  // name
                                oBuilder.CreateString(clip_path),   // path
                                0);                                 // clip
                }

                auto clip_data_fb = CreateClipNodeData(
                        oBuilder,
                        holder_fb,
                        1.0f,   // playback_rate
                        false   // mirror
                );

                auto node_fb = CreateBlendTreeNode(
                        oBuilder,
                        BlendNodeDataU::ClipNodeData,
                        clip_data_fb.Union()
                );

                std::vector<flatbuffers::Offset<BlendTreeNode>> vNodes = { node_fb };

                vStates.push_back(CreateAnimState(
                        oBuilder,
                        oBuilder.CreateString(clip.sName),
                        oBuilder.CreateVector(vNodes),
                        1.0f,   // speed
                        false   // mirror
                ));
        }

        if (vStates.empty()) return 0;

        // entry_state = first clip name
        auto entry_state_fb = oBuilder.CreateString(oCtx.vAnimClips[0].sName);

        auto graph_fb = CreateAnimationGraph(
                oBuilder,
                1,                              // schema_version
                0,                              // params (none)
                oBuilder.CreateVector(vStates),
                0,                              // transitions (none)
                entry_state_fb
        );

        auto holder_fb   = CreateAnimationGraphHolder(oBuilder, graph_fb, 0, 0);
        auto animator_fb = CreateAnimator(oBuilder, holder_fb).Union();
        return CreateComponent(oBuilder, ComponentU::Animator, animator_fb);
}

}
}
