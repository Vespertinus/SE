
#include <fstream>

#include <Logging.h>
#include <MPUtil.h>
#include <Component_generated.h>
#include "FlatBuffersMeshWriterDetails.h"
#include "FlatBuffersComponentWriterDetails.h"

namespace SE {
namespace TOOLS {

using TComponentOffset = std::tuple<flatbuffers::Offset<SE::FlatBuffers::Component>, SE::ret_code_t>;

TComponentOffset SerializeModel(
                const ModelData                 & oModel,
                flatbuffers::FlatBufferBuilder  & oBuilder);

TComponentOffset SerializeComponent(
                const TComponent                & oComponent,
                flatbuffers::FlatBufferBuilder  & oBuilder) {

         return MP::Visit(oComponent,
                        [&oBuilder](const ModelData & oModel) {
                                return SerializeModel(oModel, oBuilder);
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
                flatbuffers::FlatBufferBuilder  & oBuilder) {

        using namespace SE::FlatBuffers;

        //FIXME types inside Material, flatbuffer scheme and asset importer out of sync
        using FBVariable = std::tuple<float, int32_t, Vec2 *, Vec3 *, Vec4 *, UVec2 *, UVec3 *, UVec4 *>;
        std::tuple<Vec2, Vec3, Vec4, UVec2, UVec3, UVec4> oValueStorage;

        flatbuffers::Offset<SE::FlatBuffers::MaterialHolder> material_holder_fb = 0;

        if (!oModel.oMaterial.sShaderPath.empty()) {

                std::vector<flatbuffers::Offset<TextureHolder>>        vTextures;
                std::vector<flatbuffers::Offset<ShaderVariable>> vShaderVariables;

                for (auto & oItem : oModel.oMaterial.mTextures) {
                        //TODO currently serialize only external texture
                        vTextures.emplace_back(CreateTextureHolder(
                                                oBuilder,
                                                0,
                                                oBuilder.CreateString(oItem.second.sPath),
                                                0,
                                                StoreSettings::NONE,
                                                0,
                                                static_cast<SE::FlatBuffers::TextureUnit>(oItem.first) ));
                }

                for (auto & oItem : oModel.oMaterial.mVariables) {

                        FBVariable oValue{};

                        //FIXME rewrite
                        MP::Visit(oItem.second,
                                        [&oBuilder, &oValue, &oValueStorage](const float var) {

                                        std::get<float>(oValue) = var;
                                        },
                                        [&oBuilder, &oValue, &oValueStorage](const int32_t var) {

                                        std::get<int32_t>(oValue) = var;
                                        },
                                        [&oBuilder, &oValue, &oValueStorage](const glm::vec2 & vData) {
                                        std::get<Vec2>(oValueStorage) = Vec2(vData.x, vData.y);
                                        std::get<Vec2 *>(oValue) = &std::get<Vec2>(oValueStorage);
                                        },
                                        [&oBuilder, &oValue, &oValueStorage](const glm::vec3 & vData) {
                                        std::get<Vec3>(oValueStorage) = Vec3(vData.x, vData.y, vData.z);
                                        std::get<Vec3 *>(oValue) = &std::get<Vec3>(oValueStorage);
                                        },
                                        [&oBuilder, &oValue, &oValueStorage](const glm::vec4 & vData) {
                                        std::get<Vec4>(oValueStorage) = Vec4(vData.x, vData.y, vData.z, vData.w);
                                        std::get<Vec4 *>(oValue) = &std::get<Vec4>(oValueStorage);
                                        },
                                        [&oBuilder, &oValue, &oValueStorage](const glm::uvec2 & vData) {
                                        std::get<UVec2>(oValueStorage) = UVec2(vData.x, vData.y);
                                        std::get<UVec2 *>(oValue) = &std::get<UVec2>(oValueStorage);
                                        },
                                        [&oBuilder, &oValue, &oValueStorage](const glm::uvec3 & vData) {
                                                std::get<UVec3>(oValueStorage) = UVec3(vData.x, vData.y, vData.z);
                                                std::get<UVec3 *>(oValue) = &std::get<UVec3>(oValueStorage);
                                        },
                                        [&oBuilder, &oValue, &oValueStorage](const glm::uvec4 & vData) {
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
                                                std::get<UVec4 *>(oValue)
                                                ));
                }

                material_holder_fb   = CreateMaterialHolder(
                                oBuilder,
                                CreateMaterial(
                                        oBuilder,
                                        CreateShaderProgramHolder(
                                                oBuilder,
                                                0,
                                                oBuilder.CreateString(oModel.oMaterial.sShaderPath),
                                                0
                                                ),
                                        oBuilder.CreateVector(vTextures),
                                        oBuilder.CreateVector(vShaderVariables)
                                        ),
                                0,
                                oBuilder.CreateString(oModel.oMaterial.sName)
                                );
        }

        auto [mesh_fb, res] = SerializeMesh(oModel.oMesh, oBuilder);
        if (res != uSUCCESS) {
                return {0, res};
        }

        auto mesh_holder_fb = CreateMeshHolder(
                        oBuilder,
                        mesh_fb,
                        0,
                        oBuilder.CreateString(oModel.oMesh.sName)
                        );

        if (oModel.oBlendShape.vDefaultWeights.size() == 0 && oModel.vJointIndexes.size() == 0) { //StaticModel


                auto model_fb = CreateStaticModel(
                                oBuilder,
                                mesh_holder_fb,
                                material_holder_fb
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
                flatbuffers::Offset<
                        SkeletonHolder
                        >                               skeleton_holder_fb      = 0;

                flatbuffers::Offset<
                        flatbuffers::Vector<uint8_t>
                        >                               joint_indexes_fb        = 0;
                flatbuffers::Offset<
                        flatbuffers::Vector<
                                flatbuffers::Offset<JointBind>
                                >
                        >                               joint_bind_mat_fb       = 0;
                flatbuffers::Offset<
                        CharacterShellHolder
                        >                               shell_fb                = 0;


                //___Start___ bs
                if (oModel.oBlendShape.vDefaultWeights.size() != 0) {
                        auto tex_stock_fb = CreateTextureStock(
                                        oBuilder,
                                        oBuilder.CreateVector(
                                                reinterpret_cast<const uint8_t *>(&oModel.oBlendShape.vBuffer[0]),
                                                oModel.oBlendShape.vBuffer.size() * sizeof(float)),
                                        0,
                                        SE_GL_R32F,
                                        oModel.oBlendShape.vBuffer.size(),
                                        0);

                        blendshapes_fb = CreateTextureHolder(
                                        oBuilder,
                                        tex_stock_fb,
                                        0,
                                        oBuilder.CreateString(oModel.oBlendShape.sName),
                                        StoreSettings::StoreTextureBuffer,
                                        CreateStoreTextureBuffer(oBuilder).Union(),
                                        SE::FlatBuffers::TextureUnit::UNIT_BUFFER);

                        default_weights_fb = oBuilder.CreateVector(oModel.oBlendShape.vDefaultWeights);
                }
                //___End_____ bs

                //___Start___ skeleton
                if (oModel.vJointIndexes.size() != 0) {


                        std::vector<flatbuffers::Offset<SE::FlatBuffers::Joint>>        vJoints;
                        vJoints.reserve(oModel.oShell.oSkeleton.vJoints.size());

                        for (auto & oItem : oModel.oShell.oSkeleton.vJoints) {
                                vJoints.emplace_back(
                                                CreateJoint(
                                                        oBuilder,
                                                        oBuilder.CreateString(oItem.sName),
                                                        oItem.parent_index)
                                                );
                        }


                        auto skeleton_fb = CreateSkeleton(
                                        oBuilder,
                                        oBuilder.CreateVector(vJoints));

                        skeleton_holder_fb = CreateSkeletonHolder(
                                        oBuilder,
                                        skeleton_fb,
                                        0,
                                        oBuilder.CreateString(oModel.oShell.oSkeleton.sName)
                                        );

                        //--->
                        auto shell_base_fb = CreateCharacterShell(
                                        oBuilder,
                                        oBuilder.CreateString(oModel.oShell.sRootNode),
                                        skeleton_holder_fb);

                        shell_fb = CreateCharacterShellHolder(
                                        oBuilder,
                                        shell_base_fb,
                                        0,
                                        oBuilder.CreateString(oModel.oShell.sName)
                                        );
                        //--->
                        std::vector<flatbuffers::Offset<SE::FlatBuffers::JointBind>> vJointsBind;
                        vJointsBind.reserve(oModel.vJointBindPose.size());

                        for (auto & oItem : oModel.vJointBindPose) {
                                vJointsBind.emplace_back(
                                                CreateJointBind(
                                                        oBuilder,
                                                        reinterpret_cast<const SE::FlatBuffers::Vec4 *>(&oItem.bind_rot[0]),
                                                        reinterpret_cast<const SE::FlatBuffers::Vec3 *>(&oItem.bind_pos[0]),
                                                        reinterpret_cast<const SE::FlatBuffers::Vec3 *>(&oItem.bind_scale[0])
                                                        )
                                                );
                        }

                        joint_indexes_fb        = oBuilder.CreateVector(oModel.vJointIndexes);
                        joint_bind_mat_fb       = oBuilder.CreateVector(vJointsBind);
                }
                //___End_____ skeleton

                auto model_fb = CreateAnimatedModel(
                                oBuilder,
                                mesh_holder_fb,
                                material_holder_fb,
                                blendshapes_fb,
                                default_weights_fb,
                                shell_fb,
                                joint_indexes_fb,
                                joint_bind_mat_fb
                                ).Union();
                return { CreateComponent(oBuilder, ComponentU::AnimatedModel, model_fb), uSUCCESS };
        }


        return {0, uWRONG_INPUT_DATA};
}

}
}
