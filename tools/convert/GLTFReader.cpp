
#include <nlohmann/json.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_INCLUDE_JSON
#include <tiny_gltf.h>
#pragma GCC diagnostic pop

#include <numeric>
#include <algorithm>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "GLTFReader.h"

#include <Logging.h>
#include <GeometryUtil.h>
#include <StrID.h>
#include <BoundingBox.h>


namespace SE {
namespace TOOLS {

// ---------------------------------------------------------------------------
// Skin cache — built once per skin before node traversal
// ---------------------------------------------------------------------------
struct GLTFSkinCache {
        Skeleton *                       pSkeleton{};
        std::unordered_map<int,uint16_t> mNodeToJoint; // gltf node_idx → joint index
        std::string                      sRootNodeName;
};

static std::string GetBaseDir(const std::string & sPath) {
        if (sPath.find_last_of("/\\") != std::string::npos)
                return sPath.substr(0, sPath.find_last_of("/\\")) + '/';
        return "./";
}

/** Return pointer to the start of accessor data, and fill out stride and count. */
static const uint8_t * AccessorData(
                const tinygltf::Model & model,
                int acc_idx,
                size_t & stride,
                size_t & count) {

        const tinygltf::Accessor   & acc = model.accessors[acc_idx];
        const tinygltf::BufferView & bv  = model.bufferViews[acc.bufferView];
        stride = acc.ByteStride(bv);
        count  = acc.count;
        return model.buffers[bv.buffer].data.data() + bv.byteOffset + acc.byteOffset;
}

static glm::vec3 GetVec3(const uint8_t * data, size_t stride, size_t i) {
        const float * p = reinterpret_cast<const float *>(data + i * stride);
        return { p[0], p[1], p[2] };
}

static glm::vec4 GetVec4(const uint8_t * data, size_t stride, size_t i) {
        const float * p = reinterpret_cast<const float *>(data + i * stride);
        return { p[0], p[1], p[2], p[3] };
}

/** Read a UV pair with automatic type conversion for normalised u8/u16 formats. */
static glm::vec2 GetUV(const uint8_t * data, size_t stride, size_t i, int componentType) {
        const uint8_t * p = data + i * stride;
        switch (componentType) {
                case TINYGLTF_COMPONENT_TYPE_FLOAT:
                        return { *reinterpret_cast<const float *>(p),
                                 *reinterpret_cast<const float *>(p + 4) };
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                        return { p[0] / 255.0f, p[1] / 255.0f };
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                        return { *reinterpret_cast<const uint16_t *>(p)       / 65535.0f,
                                 *reinterpret_cast<const uint16_t *>(p + 2)   / 65535.0f };
                default:
                        return { 0.0f, 0.0f };
        }
}

static uint32_t GetIndex(const uint8_t * data, size_t stride, size_t i, int componentType) {
        const uint8_t * p = data + i * stride;
        switch (componentType) {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                        return p[0];
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                        return *reinterpret_cast<const uint16_t *>(p);
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                        return *reinterpret_cast<const uint32_t *>(p);
                default:
                        return 0;
        }
}

static glm::mat4 GetMat4(const uint8_t * data, size_t stride, size_t i) {
        const float * p = reinterpret_cast<const float *>(data + i * stride);
        // column-major: glm::mat4(col0, col1, col2, col3)
        return glm::mat4(
                p[0], p[1], p[2],  p[3],
                p[4], p[5], p[6],  p[7],
                p[8], p[9], p[10], p[11],
                p[12],p[13],p[14], p[15]);
}

static BindPoseData Mat4ToSQT(const glm::mat4 & m) {
        BindPoseData d;
        d.vBindPos   = glm::vec3(m[3]);
        d.vBindScale = glm::vec3(
                glm::length(glm::vec3(m[0])),
                glm::length(glm::vec3(m[1])),
                glm::length(glm::vec3(m[2])));
        glm::mat3 rot(
                glm::vec3(m[0]) / d.vBindScale.x,
                glm::vec3(m[1]) / d.vBindScale.y,
                glm::vec3(m[2]) / d.vBindScale.z);
        glm::quat q = glm::quat_cast(rot);
        d.vBindRot  = glm::vec4(q.x, q.y, q.z, q.w); // xyzw
        return d;
}

static uint32_t GetJointIndex(const uint8_t * data, size_t stride, size_t vtx, int comp, int componentType) {
        const uint8_t * p = data + vtx * stride + comp * (componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT ? 2 : 1);
        if (componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                return *reinterpret_cast<const uint16_t *>(p);
        return p[0]; // UNSIGNED_BYTE
}

static float GetJointWeight(const uint8_t * data, size_t stride, size_t vtx, int comp, int componentType) {
        const uint8_t * p = data + vtx * stride;
        switch (componentType) {
                case TINYGLTF_COMPONENT_TYPE_FLOAT:
                        return reinterpret_cast<const float *>(p)[comp];
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                        return p[comp] / 255.0f;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                        return reinterpret_cast<const uint16_t *>(p)[comp] / 65535.0f;
                default:
                        return 0.0f;
        }
}

/* ------------------------------------------------------------------ */

static bool ImportGLTFSkin(
                const tinygltf::Model & model,
                int skin_idx,
                ImportCtx & oCtx,
                ResourceStash & oResStash,
                GLTFSkinCache & out) {

        const tinygltf::Skin & skin = model.skins[skin_idx];
        const std::vector<int> & joints = skin.joints;
        if (joints.empty()) return false;

        // Build parent-of map (among joint nodes only)
        std::unordered_map<int,int> parentOf; // node_idx → parent node_idx
        for (int ji : joints) {
                for (int child : model.nodes[ji].children) {
                        parentOf[child] = ji;
                }
        }

        // Build nodeToJoint map
        for (uint16_t i = 0; i < static_cast<uint16_t>(joints.size()); ++i)
                out.mNodeToJoint[joints[i]] = i;

        // Inverse bind matrices accessor (optional)
        const uint8_t * invBindData = nullptr;
        size_t ibStride = 0, ibCount = 0;
        if (skin.inverseBindMatrices >= 0)
                invBindData = AccessorData(model, skin.inverseBindMatrices, ibStride, ibCount);

        // Create Skeleton in ResourceStash
        std::string sSkinName = oCtx.sPackName + "skin|" +
                (skin.name.empty() ? std::to_string(skin_idx) : skin.name);
        Skeleton * pSkel = nullptr;
        oResStash.GetResourceData(StrID(sSkinName), &pSkel);
        pSkel->sName = sSkinName;
        pSkel->vJoints.resize(joints.size());

        for (uint16_t i = 0; i < static_cast<uint16_t>(joints.size()); ++i) {

                int node_idx = joints[i];
                const tinygltf::Node & jNode = model.nodes[node_idx];
                JointData & jd = pSkel->vJoints[i];

                jd.sName = jNode.name.empty()
                        ? ("joint_" + std::to_string(node_idx))
                        : jNode.name;

                // Parent index
                auto itP = parentOf.find(node_idx);
                if (itP == parentOf.end()) {
                        jd.parent_index = JointData::ROOT_PARENT_IND;
                } else {
                        auto itJI = out.mNodeToJoint.find(itP->second);
                        jd.parent_index = (itJI != out.mNodeToJoint.end())
                                ? itJI->second
                                : JointData::ROOT_PARENT_IND;
                }

                // Bind-local pose (node TRS in parent space)
                if (jNode.translation.size() == 3)
                        jd.oBindLocal.vBindPos = glm::vec3(jNode.translation[0], jNode.translation[1], jNode.translation[2]);
                if (jNode.scale.size() == 3)
                        jd.oBindLocal.vBindScale = glm::vec3(jNode.scale[0], jNode.scale[1], jNode.scale[2]);
                else
                        jd.oBindLocal.vBindScale = glm::vec3(1.0f);
                if (jNode.rotation.size() == 4)
                        jd.oBindLocal.vBindRot = glm::vec4(jNode.rotation[0], jNode.rotation[1], jNode.rotation[2], jNode.rotation[3]);
                else
                        jd.oBindLocal.vBindRot = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

                // Inverse bind matrix — store exact mat4 to avoid SQT decomposition loss
                if (invBindData && i < ibCount) {
                        jd.mInvBindMat4     = GetMat4(invBindData, ibStride, i);
                        jd.inv_bind_is_mat4  = true;
                        jd.oInvBind          = Mat4ToSQT(jd.mInvBindMat4); // kept for legacy fallback
                } else {
                        jd.oInvBind.vBindPos   = glm::vec3(0.0f);
                        jd.oInvBind.vBindScale = glm::vec3(1.0f);
                        jd.oInvBind.vBindRot   = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
                }

                jd.bind_inited = true;
        }

        // Root node name
        int rootIdx = (skin.skeleton >= 0) ? skin.skeleton : joints[0];
        out.sRootNodeName = model.nodes[rootIdx].name.empty()
                ? ("node_" + std::to_string(rootIdx))
                : model.nodes[rootIdx].name;

        out.pSkeleton = pSkel;

        log_d("ImportGLTFSkin: skin '{}', {} joints, root '{}'",
              sSkinName, joints.size(), out.sRootNodeName);
        return true;
}

/* ------------------------------------------------------------------ */

static ret_code_t ImportGLTFMaterial(
                const tinygltf::Model & model,
                int mat_idx,
                ModelData & oModel,
                ImportCtx & oCtx,
                ResourceStash & oResStash,
                const std::string & sBaseDir) {

        const tinygltf::Material & mat = model.materials[mat_idx];

        std::string sKey = oCtx.sPackName + "mat|" + std::to_string(mat_idx);
        MaterialData * pMaterial = nullptr;
        oResStash.GetResourceData(StrID(sKey), &pMaterial);
        oModel.vMaterials.push_back(pMaterial);

        pMaterial->sName       = mat.name.empty() ? sKey : mat.name;
        pMaterial->sShaderPath = "shader_program/pbr_geometry.sesp";
        //pMaterial->sShaderPath = "shader_program/simple_tex.sesp";

        const auto & pbr = mat.pbrMetallicRoughness;

        pMaterial->mVariables["BaseColor"] = glm::vec4(
                        static_cast<float>(pbr.baseColorFactor[0]),
                        static_cast<float>(pbr.baseColorFactor[1]),
                        static_cast<float>(pbr.baseColorFactor[2]),
                        static_cast<float>(pbr.baseColorFactor[3]));
        pMaterial->mVariables["Roughness"] = static_cast<float>(pbr.roughnessFactor);
        pMaterial->mVariables["Metallic"]  = static_cast<float>(pbr.metallicFactor);
        pMaterial->mVariables["Emissive"]  = glm::vec3(
                        static_cast<float>(mat.emissiveFactor[0]),
                        static_cast<float>(mat.emissiveFactor[1]),
                        static_cast<float>(mat.emissiveFactor[2]));
        pMaterial->mVariables["AO"]        = 1.0f;

        if      (mat.alphaMode == "BLEND") pMaterial->oBlendMode = BlendMode::Translucent;
        else if (mat.alphaMode == "MASK")  pMaterial->oBlendMode = BlendMode::Masked;
        else                               pMaterial->oBlendMode = BlendMode::Opaque;

        // KHR_materials_transmission: transmissionFactor > 0 means the surface lets light
        // through. Override blend mode and derive BaseColor.a for the OIT pass.
        //
        // True glass (transmissionFactor = 1) has zero surface opacity but still produces
        // specular reflections (Fresnel). Mapping 1 - factor directly gives alpha = 0 and
        // makes the material invisible. Use a floor equal to the dielectric F0 for glass
        // (IOR ≈ 1.5 → F0 ≈ 0.04) so the OIT shader renders at least the surface specular.
        //
        // KHR_materials_volume: if attenuationDistance is finite, Beer-Lambert absorption
        // adds both a colour tint and extra opacity to the base alpha.
        auto it_trans = mat.extensions.find("KHR_materials_transmission");
        if (it_trans != mat.extensions.end() && it_trans->second.IsObject() &&
            it_trans->second.Has("transmissionFactor")) {

                float transmission = static_cast<float>(
                        it_trans->second.Get("transmissionFactor").GetNumberAsDouble());

                if (transmission > 0.0f) {
                        pMaterial->oBlendMode = BlendMode::Translucent;

                        auto & base_color = std::get<glm::vec4>(pMaterial->mVariables["BaseColor"]);

                        // F0 for glass (IOR 1.5): ((1.5-1)/(1.5+1))^2 ≈ 0.04
                        constexpr float kTransmissiveAlphaFloor = 0.04f;
                        base_color.a = glm::max(kTransmissiveAlphaFloor, 1.0f - transmission);

                        // KHR_materials_volume: Beer-Lambert tint + absorption opacity.
                        // Only active when attenuationDistance is finite (> 0 and explicitly set).
                        auto it_vol = mat.extensions.find("KHR_materials_volume");
                        if (it_vol != mat.extensions.end() && it_vol->second.IsObject()) {
                                const auto & vol = it_vol->second;

                                // Only apply Beer-Lambert when attenuationDistance is explicitly
                                // provided. The spec default is +∞ (no absorption), so an absent
                                // field means we have nothing to compute. Using Has() instead of
                                // std::isfinite() avoids -ffast-math optimising isfinite away.
                                float thickness = vol.Has("thicknessFactor")
                                        ? static_cast<float>(vol.Get("thicknessFactor").GetNumberAsDouble())
                                        : 0.0f;

                                if (vol.Has("attenuationDistance") && thickness > 0.0f) {
                                        float atten_dist = static_cast<float>(
                                                vol.Get("attenuationDistance").GetNumberAsDouble());

                                        if (atten_dist > 0.0f) {
                                                glm::vec3 atten_color(1.0f);
                                                if (vol.Has("attenuationColor")) {
                                                        const auto & c = vol.Get("attenuationColor");
                                                        atten_color = glm::vec3(
                                                                static_cast<float>(c.Get(0).GetNumberAsDouble()),
                                                                static_cast<float>(c.Get(1).GetNumberAsDouble()),
                                                                static_cast<float>(c.Get(2).GetNumberAsDouble()));
                                                }

                                                // T(channel) = attenuationColor ^ (thickness / attenuationDistance)
                                                float ratio = thickness / atten_dist;
                                                glm::vec3 T = glm::pow(atten_color, glm::vec3(ratio));
                                                base_color = glm::vec4(glm::vec3(base_color) * T, base_color.a);

                                                float absorption = 1.0f - (T.r + T.g + T.b) / 3.0f;
                                                base_color.a = glm::max(kTransmissiveAlphaFloor,
                                                                        base_color.a + absorption);

                                                log_d("ImportGLTFMaterial: '{}' KHR_materials_volume thickness={:.3f} atten_dist={:.3f} absorption={:.3f}",
                                                      pMaterial->sName, thickness, atten_dist, absorption);
                                        }
                                }
                        }

                        log_d("ImportGLTFMaterial: '{}' KHR_materials_transmission factor={:.3f} -> Translucent, BaseColor.a={:.3f}",
                              pMaterial->sName, transmission, base_color.a);
                }
        }

        log_d("ImportGLTFMaterial: '{}' alphaMode='{}' -> BlendMode={}",
              pMaterial->sName, mat.alphaMode, static_cast<uint32_t>(pMaterial->oBlendMode));

        // glTF spec default wrap mode is GL_REPEAT (10497).
        // We read it from the sampler object so models that explicitly request
        // CLAMP_TO_EDGE or MIRRORED_REPEAT are also handled correctly.
        auto GetWrapMode = [&](int tex_index) -> int32_t {
                constexpr int32_t kGLRepeat = 10497; // GL_REPEAT
                if (tex_index < 0) return kGLRepeat;
                const tinygltf::Texture & tex = model.textures[tex_index];
                if (tex.sampler < 0) return kGLRepeat; // spec default
                const tinygltf::Sampler & samp = model.samplers[tex.sampler];
                // Use wrapT (V axis) as the representative wrap mode; it is the one
                // that matters for the V-coordinate shift present in many glTF models.
                return samp.wrapT != 0 ? samp.wrapT : kGLRepeat;
        };

        auto AddTexture = [&](int tex_index, TextureUnit unit) {
                if (tex_index < 0) return;
                const tinygltf::Texture & tex = model.textures[tex_index];
                if (tex.source < 0) return;
                const tinygltf::Image & img = model.images[tex.source];
                TextureData & oTex = pMaterial->mTextures[unit];
                oTex.wrap_mode = GetWrapMode(tex_index);
                if (!img.uri.empty()) {
                        std::string sPath = sBaseDir + img.uri;
                        oCtx.FixPath(sPath);
                        oTex.sPath = sPath;
                        ++oCtx.textures_cnt;
                }
                else if (img.bufferView >= 0) {
                        // Embedded image in GLB — store original compressed bytes
                        const tinygltf::BufferView & bv = model.bufferViews[img.bufferView];
                        const uint8_t * raw =
                                model.buffers[bv.buffer].data.data() + bv.byteOffset;
                        oTex.vEncodedData.assign(raw, raw + bv.byteLength);
                        if      (img.mimeType == "image/png")  oTex.oEncoding = TextureEncoding::PNG;
                        else if (img.mimeType == "image/jpeg") oTex.oEncoding = TextureEncoding::JPG;
                        else                                   oTex.oEncoding = TextureEncoding::PNG;
                        oTex.sName = img.name.empty()
                                ? (oCtx.sPackName + "tex_" + std::to_string(tex.source))
                                : img.name;
                        ++oCtx.textures_cnt;
                }
        };

        AddTexture(pbr.baseColorTexture.index,         TextureUnit::DIFFUSE);
        AddTexture(mat.normalTexture.index,            TextureUnit::NORMAL);
        AddTexture(pbr.metallicRoughnessTexture.index, TextureUnit::SPECULAR);
        AddTexture(mat.emissiveTexture.index,          TextureUnit::EMISSIVE);

        ++oCtx.material_cnt;
        return uSUCCESS;
}

/* ------------------------------------------------------------------ */

static ret_code_t ImportGLTFPrimitive(
                const tinygltf::Model & model,
                const tinygltf::Primitive & prim,
                const tinygltf::Mesh & gltfMesh,
                const std::string & sMeshName,
                ModelData & oModel,
                ImportCtx & oCtx,
                ResourceStash & oResStash,
                const std::string & sBaseDir,
                const GLTFSkinCache * pSkin = nullptr,
                uint32_t vertex_base_offset = 0,
                uint16_t material_index = 0) {

        // --- Position (required) ---
        auto itPos = prim.attributes.find("POSITION");
        if (itPos == prim.attributes.end()) {
                log_e("GLTFReader: mesh '{}' primitive has no POSITION accessor", sMeshName);
                return uWRONG_INPUT_DATA;
        }
        size_t posStride, posCnt;
        const uint8_t * posData = AccessorData(model, itPos->second, posStride, posCnt);

        // --- Normal ---
        const uint8_t * normData  = nullptr;
        size_t normStride = 0;
        bool hasNormals = false;
        if (!oCtx.skip_normals) {
                auto itNorm = prim.attributes.find("NORMAL");
                if (itNorm != prim.attributes.end()) {
                        size_t normCnt;
                        normData  = AccessorData(model, itNorm->second, normStride, normCnt);
                        hasNormals = true;
                }
        }

        // --- TexCoord0 ---
        const uint8_t * uvData        = nullptr;
        size_t uvStride               = 0;
        int    uvComponentType        = TINYGLTF_COMPONENT_TYPE_FLOAT;
        auto itUV = prim.attributes.find("TEXCOORD_0");
        if (itUV != prim.attributes.end()) {
                size_t uvCnt;
                uvData         = AccessorData(model, itUV->second, uvStride, uvCnt);
                uvComponentType = model.accessors[itUV->second].componentType;
        }

        // --- Tangent (vec4: xyz + handedness w) ---
        const uint8_t * tanData  = nullptr;
        size_t tanStride         = 0;
        bool hasTangents         = false;
        auto itTan = prim.attributes.find("TANGENT");
        if (itTan != prim.attributes.end()) {
                size_t tanCnt;
                tanData    = AccessorData(model, itTan->second, tanStride, tanCnt);
                hasTangents = true;
        }
        else {
                log_w("GLTFReader: mesh '{}' has no TANGENT accessor, computing tangents", sMeshName);
        }

        // --- Indices ---
        std::vector<uint32_t> vIndices;
        if (prim.indices >= 0) {
                const tinygltf::Accessor & idxAcc = model.accessors[prim.indices];
                size_t idxStride, idxCnt;
                const uint8_t * idxData = AccessorData(model, prim.indices, idxStride, idxCnt);
                vIndices.resize(idxCnt);
                for (size_t i = 0; i < idxCnt; ++i) {
                        vIndices[i] = GetIndex(idxData, idxStride, i, idxAcc.componentType);
                }
        }
        else {
                vIndices.resize(posCnt);
                std::iota(vIndices.begin(), vIndices.end(), 0u);
        }

        // --- Compute flat normals if absent ---
        std::vector<glm::vec3> vComputedNormals;
        if (!oCtx.skip_normals && !hasNormals) {
                vComputedNormals.assign(posCnt, glm::vec3(0.0f));
                for (size_t i = 0; i + 2 < vIndices.size(); i += 3) {
                        uint32_t i0 = vIndices[i], i1 = vIndices[i+1], i2 = vIndices[i+2];
                        glm::vec3 p0 = GetVec3(posData, posStride, i0);
                        glm::vec3 p1 = GetVec3(posData, posStride, i1);
                        glm::vec3 p2 = GetVec3(posData, posStride, i2);
                        glm::vec3 N  = glm::cross(p1 - p0, p2 - p0);
                        float len = glm::length(N);
                        if (len > 1e-8f) N /= len;
                        vComputedNormals[i0] += N;
                        vComputedNormals[i1] += N;
                        vComputedNormals[i2] += N;
                }
                for (auto & n : vComputedNormals) {
                        float len = glm::length(n);
                        if (len > 1e-8f) n /= len;
                        else n = glm::vec3(0.0f, 1.0f, 0.0f);
                }
        }

        // --- Compute per-vertex tangents (always when absent; also for debug comparison) ---
        auto ComputeTangents = [&]() -> std::vector<glm::vec4> {
                std::vector<glm::vec4> vTangents(posCnt, glm::vec4(0.0f));
                for (size_t i = 0; i + 2 < vIndices.size(); i += 3) {
                        uint32_t i0 = vIndices[i], i1 = vIndices[i+1], i2 = vIndices[i+2];
                        glm::vec3 p0 = GetVec3(posData, posStride, i0);
                        glm::vec3 p1 = GetVec3(posData, posStride, i1);
                        glm::vec3 p2 = GetVec3(posData, posStride, i2);
                        glm::vec2 uv0 = uvData ? GetUV(uvData, uvStride, i0, uvComponentType) : glm::vec2(0.0f);
                        glm::vec2 uv1 = uvData ? GetUV(uvData, uvStride, i1, uvComponentType) : glm::vec2(0.0f);
                        glm::vec2 uv2 = uvData ? GetUV(uvData, uvStride, i2, uvComponentType) : glm::vec2(0.0f);
                        glm::vec3 edge1 = p1 - p0;
                        glm::vec3 edge2 = p2 - p0;
                        glm::vec2 duv1  = uv1 - uv0;
                        glm::vec2 duv2  = uv2 - uv0;
                        float r = duv1.x * duv2.y - duv2.x * duv1.y;
                        if (std::abs(r) < 1e-8f) r = 1.0f;
                        glm::vec3 T = (edge1 * duv2.y - edge2 * duv1.y) / r;
                        for (uint32_t idx : { i0, i1, i2 })
                                vTangents[idx] += glm::vec4(T, 0.0f);
                }
                size_t fallback_cnt = 0;
                for (auto & t : vTangents) {
                        float len = glm::length(glm::vec3(t));
                        if (len > 1e-8f) t = glm::vec4(glm::vec3(t) / len, 1.0f);
                        else           { t = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f); ++fallback_cnt; }
                }
                if (fallback_cnt > 0)
                        log_w("ImportGLTFPrimitive: mesh '{}' {} / {} vertices got fallback tangent (1,0,0) — UV seams or degenerate triangles",
                              sMeshName, fallback_cnt, posCnt);
                return vTangents;
        };

        std::vector<glm::vec4> vComputedTangents;
        if (!hasTangents)
                vComputedTangents = ComputeTangents();


        // --- Blend shape prep ---
        const bool bDoBlendShapes = oCtx.import_blend_shapes && !prim.targets.empty();
        std::vector<int32_t> vGltfToOut;
        if (bDoBlendShapes)
                vGltfToOut.assign(posCnt, -1);

        // --- Vertex layout ---
        // Position(3) + [Normal(3)] + TexCoord0(2) + Tangent(4)
        const uint8_t floats_per_vert = oCtx.skip_normals ? 9u : 12u;
        const uint8_t stride_bytes    = floats_per_vert * sizeof(float);

        std::vector<float> vVertices;
        std::vector<float> vVertexData;
        vVertexData.reserve(floats_per_vert);

        // --- Skinning accessors (read before dedup loop so gltf_idx maps correctly) ---
        const uint8_t * jData     = nullptr;
        const uint8_t * wData     = nullptr;
        size_t          jStride   = 0, wStride = 0;
        int             jCompType = 0, wCompType = 0;
        bool            have_joints = false;
        constexpr int   joints_per_vertex = 4;

        if (pSkin) {
                auto itJ = prim.attributes.find("JOINTS_0");
                auto itW = prim.attributes.find("WEIGHTS_0");
                if (itJ != prim.attributes.end() && itW != prim.attributes.end()) {
                        size_t jCnt, wCnt;
                        jData     = AccessorData(model, itJ->second, jStride, jCnt);
                        jCompType = model.accessors[itJ->second].componentType;
                        wData     = AccessorData(model, itW->second, wStride, wCnt);
                        wCompType = model.accessors[itW->second].componentType;
                        have_joints = true;
                }
        }

        std::vector<float>    vNewWeights;
        std::vector<uint32_t> vNewJIndices;

        MeshData * pMesh = oModel.pMesh;

        const bool is_appending = !pMesh->vVertexBuffers.empty();
        const uint32_t index_start = std::visit([](auto & v) { return static_cast<uint32_t>(v.size()); }, pMesh->oIndex);

        VertexIndex oVertexIndex(vertex_base_offset);

        TPackVertexIndex Pack;
        if (!is_appending && std::holds_alternative<std::vector<uint8_t>>(pMesh->oIndex)
                          && std::get<std::vector<uint8_t>>(pMesh->oIndex).empty()) {
                Pack = PackVertexIndexInit(static_cast<uint32_t>(vIndices.size()), pMesh->oIndex);
        }
        else {
                Pack = std::visit([](auto & v) -> TPackVertexIndex {
                        using TElem = typename std::decay_t<decltype(v)>::value_type;
                        return &PackValue<TElem>;
                }, pMesh->oIndex);
        }

        uint32_t cur_index = 0;
        BoundingBox oBBox;

        for (uint32_t gltf_idx : vIndices) {

                vVertexData.clear();

                glm::vec3 pos = GetVec3(posData, posStride, gltf_idx);
                vVertexData.push_back(pos.x);
                vVertexData.push_back(pos.y);
                vVertexData.push_back(pos.z);
                oBBox.Concat(pos);

                if (!oCtx.skip_normals) {
                        glm::vec3 norm;
                        if (hasNormals) {
                                norm = GetVec3(normData, normStride, gltf_idx);
                        }
                        else {
                                norm = vComputedNormals[gltf_idx];
                        }
                        vVertexData.push_back(norm.x);
                        vVertexData.push_back(norm.y);
                        vVertexData.push_back(norm.z);
                }

                glm::vec2 uv(0.0f);
                if (uvData) {
                        uv = GetUV(uvData, uvStride, gltf_idx, uvComponentType);
                }
                vVertexData.push_back(uv.x);
                vVertexData.push_back(1.0f - uv.y);  // flip V: GLTF V=0=top → GL V=0=bottom

                glm::vec4 tan = hasTangents
                        ? GetVec4(tanData, tanStride, gltf_idx)
                        : vComputedTangents[gltf_idx];
                vVertexData.push_back(tan.x);
                vVertexData.push_back(tan.y);
                vVertexData.push_back(tan.z);
                vVertexData.push_back(tan.w);

                if (oVertexIndex.Get(vVertexData, cur_index)) {
                        vVertices.insert(vVertices.end(), vVertexData.begin(), vVertexData.end());
                        if (have_joints) {
                                for (int j = 0; j < joints_per_vertex; ++j) {
                                        vNewWeights.push_back(GetJointWeight(wData, wStride, gltf_idx, j, wCompType));
                                        vNewJIndices.push_back(GetJointIndex(jData, jStride, gltf_idx, j, jCompType));
                                }
                        }
                        ++oCtx.total_vertices_cnt;
                }
                Pack(pMesh->oIndex, cur_index);

                if (bDoBlendShapes && vGltfToOut[gltf_idx] < 0)
                        vGltfToOut[gltf_idx] = static_cast<int32_t>(cur_index);
        }

        uint32_t index_count = std::visit([](auto & v) -> uint32_t { return v.size(); }, pMesh->oIndex);

        pMesh->vShapes.emplace_back(index_start, index_count - index_start, oBBox, material_index);
        pMesh->oBBox.Concat(oBBox);

        if (!is_appending) {
                pMesh->vVertexBuffers.emplace_back(
                        MeshData::VertexBuffer{ std::move(vVertices), stride_bytes });
        }
        else {
                auto & existing = std::get<std::vector<float>>(pMesh->vVertexBuffers[0].oBuffer);
                existing.insert(existing.end(), vVertices.begin(), vVertices.end());
        }

        // Vertex attribute descriptors — only on first primitive
        if (!is_appending) {
                uint16_t next_offset = 3;
                pMesh->vAttributes.emplace_back(MeshData::VertexAttribute{ "Position", 0, 3, 0 });
                if (!oCtx.skip_normals) {
                        pMesh->vAttributes.emplace_back(MeshData::VertexAttribute{
                                        "Normal",
                                        static_cast<uint16_t>(3 * sizeof(float)),
                                        3,
                                        0 });
                        next_offset = 6;
                }
                pMesh->vAttributes.emplace_back(MeshData::VertexAttribute{
                                "TexCoord0",
                                static_cast<uint16_t>(next_offset * sizeof(float)),
                                2,
                                0 });
                pMesh->vAttributes.emplace_back(MeshData::VertexAttribute{
                                "Tangent",
                                static_cast<uint16_t>((next_offset + 2) * sizeof(float)),
                                4,
                                0 });
        }

        // --- Skinning vertex buffers (JOINTS_0 + WEIGHTS_0) ---
        // vNewWeights / vNewJIndices were filled in encounter order inside the dedup loop above,
        // so their indices align exactly with the main geometry vertex buffer.
        if (have_joints) {
                if (!is_appending) {
                        uint8_t wBufIdx = static_cast<uint8_t>(pMesh->vVertexBuffers.size());
                        pMesh->vVertexBuffers.emplace_back(MeshData::VertexBuffer{
                                std::move(vNewWeights),
                                static_cast<uint8_t>(joints_per_vertex * sizeof(float))});

                        uint8_t iBufIdx = static_cast<uint8_t>(pMesh->vVertexBuffers.size());
                        pMesh->vVertexBuffers.emplace_back(MeshData::VertexBuffer{
                                std::move(vNewJIndices),
                                static_cast<uint8_t>(joints_per_vertex * sizeof(uint32_t))});

                        pMesh->vAttributes.emplace_back(MeshData::VertexAttribute{
                                "JointWeights", 0,
                                static_cast<uint8_t>(joints_per_vertex),
                                wBufIdx});
                        pMesh->vAttributes.emplace_back(MeshData::VertexAttribute{
                                "JointIndices", 0,
                                static_cast<uint8_t>(joints_per_vertex),
                                iBufIdx,
                                static_cast<uint32_t>(joints_per_vertex),
                                MeshData::VertexAttribute::Type::DEST_INT});
                }
                else {
                        // Append in encounter order — same offset invariant as the geometry buffer
                        auto & wBuf = std::get<std::vector<float>>(pMesh->vVertexBuffers[1].oBuffer);
                        wBuf.insert(wBuf.end(), vNewWeights.begin(), vNewWeights.end());
                        auto & iBuf = std::get<std::vector<uint32_t>>(pMesh->vVertexBuffers[2].oBuffer);
                        iBuf.insert(iBuf.end(), vNewJIndices.begin(), vNewJIndices.end());
                }
        }

        // --- Blend shapes ---
        if (bDoBlendShapes) {
                const uint32_t bs_total_cnt    = static_cast<uint32_t>(prim.targets.size());
                const uint32_t output_vert_cnt = oVertexIndex.Size();

                const size_t buf_bytes =
                        (size_t)output_vert_cnt * bs_total_cnt * 3 * sizeof(float);
                if (buf_bytes >= 100u * 1024 * 1024) {
                        log_e("GLTFReader: blend shape buffer {} MB >= 100 MB limit, skipping",
                              buf_bytes / (1024 * 1024));
                        return uWRONG_INPUT_DATA;
                }

                BlendShapeData * pBS = nullptr;
                const std::string sBsKey = oCtx.sPackName + sMeshName;
                bool bNew = oResStash.GetResourceData(StrID(sBsKey), &pBS);

                if (!bNew) {
                        // Already populated (mesh referenced by multiple nodes) — reuse.
                        oModel.pBlendShape = pBS;
                }
                else {
                        // Build composite name: key + "|" + targetName0 + ...
                        pBS->sName = sBsKey;
                        if (gltfMesh.extras.IsObject() && gltfMesh.extras.Has("targetNames")) {
                                const tinygltf::Value & tnames =
                                        gltfMesh.extras.Get("targetNames");
                                if (tnames.IsArray()) {
                                        for (size_t i = 0; i < tnames.ArrayLen(); ++i)
                                                pBS->sName += "|" +
                                                        tnames.Get(static_cast<int>(i)).Get<std::string>();
                                }
                        }
                        else {
                                for (uint32_t t = 0; t < bs_total_cnt; ++t)
                                        pBS->sName += "|target_" + std::to_string(t);
                        }

                        // Default weights from mesh.weights ([0..1]), pad with 0.0 if absent
                        pBS->vDefaultWeights.reserve(bs_total_cnt);
                        for (uint32_t t = 0; t < bs_total_cnt; ++t) {
                                float w = (t < gltfMesh.weights.size())
                                        ? static_cast<float>(gltfMesh.weights[t])
                                        : 0.0f;
                                pBS->vDefaultWeights.push_back(w);
                        }

                        // Allocate delta buffer (same interleaved layout as FBX):
                        //   vBuffer[vi * bs_total_cnt * 3 + t * 3 + xyz]
                        pBS->vBuffer.assign(output_vert_cnt * bs_total_cnt * 3, 0.0f);

                        // Fill position deltas for each morph target
                        for (uint32_t t = 0; t < bs_total_cnt; ++t) {
                                const auto & target = prim.targets[t];
                                auto itTP = target.find("POSITION");
                                if (itTP == target.end()) continue;

                                size_t tpStride, tpCnt;
                                const uint8_t * tpData =
                                        AccessorData(model, itTP->second, tpStride, tpCnt);

                                for (size_t vi = 0; vi < posCnt; ++vi) {
                                        const int32_t out_idx = vGltfToOut[vi];
                                        if (out_idx < 0) continue;

                                        glm::vec3 base_pos = GetVec3(posData, posStride, vi);
                                        glm::vec3 tgt_pos  = GetVec3(tpData,  tpStride,  vi);
                                        glm::vec3 delta    = tgt_pos - base_pos;

                                        const size_t buf_off =
                                                static_cast<size_t>(out_idx) * bs_total_cnt * 3
                                                + t * 3;
                                        pBS->vBuffer[buf_off + 0] = delta.x;
                                        pBS->vBuffer[buf_off + 1] = delta.y;
                                        pBS->vBuffer[buf_off + 2] = delta.z;
                                }
                        }

                        log_d("GLTFReader: blend shapes '{}' {} targets {} output verts",
                              sBsKey, bs_total_cnt, output_vert_cnt);

                        oModel.pBlendShape = pBS;
                }
        }

        oCtx.total_triangles_cnt += static_cast<uint32_t>(vIndices.size()) / 3;
        ++oCtx.shape_cnt;

        return uSUCCESS;
}

/* ------------------------------------------------------------------ */

static ret_code_t ImportGLTFMesh(
                const tinygltf::Model & model,
                int mesh_idx,
                NodeData & oNodeData,
                ImportCtx & oCtx,
                ResourceStash & oResStash,
                const std::string & sBaseDir,
                const GLTFSkinCache * pSkin = nullptr) {

        const tinygltf::Mesh & mesh = model.meshes[mesh_idx];
        log_d("GLTFReader: mesh '{}', {} primitive(s)", mesh.name, mesh.primitives.size());

        // One MeshData for the whole GLTF mesh; each primitive becomes a Shape inside it.
        std::string sMeshKey = oCtx.sPackName + mesh.name;
        MeshData * pMesh = nullptr;
        oResStash.GetResourceData(StrID(sMeshKey), &pMesh);
        pMesh->sName = sMeshKey;

        ModelData oModel;
        oModel.pMesh = pMesh;

        // Pre-initialize the index buffer as uint32 for multi-primitive meshes so that
        // appended indices from later primitives never overflow uint8/uint16.
        if (mesh.primitives.size() > 1)
                pMesh->oIndex = std::vector<uint32_t>{};

        for (size_t p = 0; p < mesh.primitives.size(); ++p) {
                const tinygltf::Primitive & prim = mesh.primitives[p];

                if (prim.mode != TINYGLTF_MODE_TRIANGLES) {
                        log_w("GLTFReader: mesh '{}' primitive {} is not TRIANGLES, skipping", mesh.name, p);
                        continue;
                }

                // Vertex base offset: number of unique vertices already in geometry buffer[0].
                uint32_t vertex_base_offset = 0;
                if (!pMesh->vVertexBuffers.empty()) {
                        const auto & buf   = pMesh->vVertexBuffers[0];
                        const auto & fvec  = std::get<std::vector<float>>(buf.oBuffer);
                        vertex_base_offset = static_cast<uint32_t>(fvec.size()) / (buf.stride / sizeof(float));
                }

                const uint16_t material_index = static_cast<uint16_t>(oModel.vMaterials.size());

                ret_code_t res = ImportGLTFPrimitive(
                                model, prim, mesh, mesh.name, oModel, oCtx, oResStash, sBaseDir,
                                pSkin, vertex_base_offset, material_index);
                if (res != uSUCCESS) return res;

                if (!oCtx.skip_material && prim.material >= 0) {
                        ret_code_t res2 = ImportGLTFMaterial(
                                        model, prim.material, oModel, oCtx, oResStash, sBaseDir);
                        if (res2 != uSUCCESS) return res2;
                }
                else {
                        oModel.vMaterials.push_back(nullptr); // keep slot aligned with material_index
                }

                // Attach SkinData once (all primitives of one GLTF mesh share the same skin).
                if (!oModel.pSkin && pSkin && pSkin->pSkeleton) {
                        std::string sSkinKey = pSkin->pSkeleton->sName + "|skin";
                        SkinData * pSkinData = nullptr;
                        oResStash.GetResourceData(StrID(sSkinKey), &pSkinData);
                        if (pSkinData->pSkeleton == nullptr) {
                                // First time: populate
                                uint16_t N = static_cast<uint16_t>(pSkin->pSkeleton->vJoints.size());
                                pSkinData->pSkeleton = pSkin->pSkeleton;
                                pSkinData->sRootNode = pSkin->sRootNodeName;
                                pSkinData->sName     = pSkin->pSkeleton->sName;
                                pSkinData->vJointIndexes.resize(N);
                                std::iota(pSkinData->vJointIndexes.begin(), pSkinData->vJointIndexes.end(), uint16_t(0));
                                pSkinData->vJointsInvBindPose.resize(N);
                                for (uint16_t i = 0; i < N; ++i)
                                        pSkinData->vJointsInvBindPose[i] = pSkin->pSkeleton->vJoints[i].oInvBind;

                                // GLTF does not supply a separate mesh bind-pose transform.
                                // Use identity so BuildTransform returns mat4(1) and the
                                // skinning formula inv(MeshBind)*WorldJoint*InvBind*MeshBind
                                // simplifies to WorldJoint*InvBind (the standard GLTF formula).
                                pSkinData->oMeshBindPose.vBindPos   = glm::vec3(0.0f);
                                pSkinData->oMeshBindPose.vBindScale = glm::vec3(1.0f);
                                pSkinData->oMeshBindPose.vBindRot   = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
                        }
                        oModel.pSkin = pSkinData;
                }
        }

        // All primitives merged into one ModelData — attach directly to the node.
        oNodeData.vComponents.emplace_back(std::move(oModel));
        ++oCtx.mesh_cnt;

        return uSUCCESS;
}

/* ------------------------------------------------------------------ */

static ret_code_t ImportGLTFNode(
                const tinygltf::Model & model,
                int node_idx,
                NodeData & oParent,
                ImportCtx & oCtx,
                ResourceStash & oResStash,
                const std::string & sBaseDir,
                const std::vector<GLTFSkinCache> & vSkins) {

        const tinygltf::Node & node = model.nodes[node_idx];

        NodeData oNodeData;
        oNodeData.sName   = node.name.empty() ? ("node_" + std::to_string(node_idx)) : node.name;
        oNodeData.vScale   = glm::vec3(1.0f);
        oNodeData.enabled = !oCtx.disable_nodes;

        // Transform: matrix takes priority, else TRS
        if (node.matrix.size() == 16) {
                glm::mat4 mat(
                        node.matrix[0], node.matrix[1], node.matrix[2],  node.matrix[3],
                        node.matrix[4], node.matrix[5], node.matrix[6],  node.matrix[7],
                        node.matrix[8], node.matrix[9], node.matrix[10], node.matrix[11],
                        node.matrix[12],node.matrix[13],node.matrix[14], node.matrix[15]);

                // Decompose translation
                oNodeData.vTranslation = glm::vec3(mat[3]);
                // Decompose scale
                oNodeData.vScale = glm::vec3(
                        glm::length(glm::vec3(mat[0])),
                        glm::length(glm::vec3(mat[1])),
                        glm::length(glm::vec3(mat[2])));
                // Decompose rotation
                glm::mat3 rotMat(
                        glm::vec3(mat[0]) / oNodeData.vScale.x,
                        glm::vec3(mat[1]) / oNodeData.vScale.y,
                        glm::vec3(mat[2]) / oNodeData.vScale.z);
                glm::quat q         = glm::quat_cast(rotMat);
                oNodeData.vRotation  = glm::degrees(glm::eulerAngles(q));
        }
        else {
                if (node.translation.size() == 3) {
                        oNodeData.vTranslation = glm::vec3(
                                node.translation[0], node.translation[1], node.translation[2]);
                }
                if (node.scale.size() == 3) {
                        oNodeData.vScale = glm::vec3(
                                node.scale[0], node.scale[1], node.scale[2]);
                }
                if (node.rotation.size() == 4) {
                        // glTF quaternion layout: [x, y, z, w]
                        glm::quat q(
                                static_cast<float>(node.rotation[3]),   // w
                                static_cast<float>(node.rotation[0]),   // x
                                static_cast<float>(node.rotation[1]),   // y
                                static_cast<float>(node.rotation[2]));  // z
                        oNodeData.vRotation = glm::degrees(glm::eulerAngles(q));
                }
        }

        ++oCtx.node_cnt;

        if (node.mesh >= 0) {
                const GLTFSkinCache * pSkin = nullptr;
                if (oCtx.import_skin && node.skin >= 0 && node.skin < (int)vSkins.size())
                        pSkin = &vSkins[node.skin];
                ret_code_t res = ImportGLTFMesh(
                                model, node.mesh, oNodeData, oCtx, oResStash, sBaseDir, pSkin);
                if (res != uSUCCESS) return res;
        }

        for (int child_idx : node.children) {
                ret_code_t res = ImportGLTFNode(
                                model, child_idx, oNodeData, oCtx, oResStash, sBaseDir, vSkins);
                if (res != uSUCCESS) return res;
        }

        oParent.vChildren.emplace_back(std::move(oNodeData));
        return uSUCCESS;
}

/* ------------------------------------------------------------------ */

static ret_code_t ImportGLTFAnimations(
                const tinygltf::Model & model,
                const std::vector<GLTFSkinCache> & vSkins,
                ImportCtx & oCtx) {

        // Build node→joint map from first available skin
        const std::unordered_map<int,uint16_t> * pNodeToJoint = nullptr;
        for (const auto & s : vSkins) {
                if (!s.mNodeToJoint.empty()) { pNodeToJoint = &s.mNodeToJoint; break; }
        }

        for (size_t a = 0; a < model.animations.size(); ++a) {
                const tinygltf::Animation & anim = model.animations[a];

                AnimClipData oClip;
                oClip.sName = anim.name.empty()
                        ? ("anim_" + std::to_string(a))
                        : anim.name;

                // Per-clip looping from sidecar settings
                {
                        auto it = oCtx.mClipSettings.find(oClip.sName);
                        oClip.looping = (it != oCtx.mClipSettings.end()) ? it->second.loop : false;
                }

                // Duration = max of all sampler input accessors' max value
                float duration = 0.0f;
                for (const auto & samp : anim.samplers) {
                        if (samp.input >= 0 && !model.accessors[samp.input].maxValues.empty())
                                duration = std::max(duration, static_cast<float>(model.accessors[samp.input].maxValues[0]));
                }
                oClip.duration = duration;

                for (const auto & ch : anim.channels) {
                        if (ch.target_path == "weights") continue; // blend shapes not yet supported

                        // Map GLTF node to joint index
                        uint16_t jointIdx = 0;
                        if (pNodeToJoint) {
                                auto it = pNodeToJoint->find(ch.target_node);
                                if (it == pNodeToJoint->end()) continue; // not a joint
                                jointIdx = it->second;
                        } else {
                                continue; // no skin → can't map
                        }

                        const tinygltf::AnimationSampler & samp = anim.samplers[ch.sampler];
                        if (samp.input < 0 || samp.output < 0) continue;

                        // Read keyframe times
                        size_t tStride, tCnt;
                        const uint8_t * tData = AccessorData(model, samp.input, tStride, tCnt);
                        std::vector<float> vTimes(tCnt);
                        for (size_t i = 0; i < tCnt; ++i)
                                vTimes[i] = *reinterpret_cast<const float *>(tData + i * tStride);

                        // Read output values
                        size_t vStride, vCnt;
                        const uint8_t * vData = AccessorData(model, samp.output, vStride, vCnt);

                        bool isCubicSpline = (samp.interpolation == "CUBICSPLINE");
                        // CUBICSPLINE: 3 values per keyframe (in-tangent, value, out-tangent)
                        size_t valuesPerKey = isCubicSpline ? 3 : 1;
                        // value offset within each group
                        size_t valueOffset  = isCubicSpline ? 1 : 0;

                        // Determine target channels and component count
                        int    baseTarget   = 0;
                        int    numComp      = 3;
                        if (ch.target_path == "translation") { baseTarget = 0; numComp = 3; }
                        else if (ch.target_path == "rotation")    { baseTarget = 3; numComp = 4; }
                        else if (ch.target_path == "scale")       { baseTarget = 7; numComp = 3; }
                        else continue;

                        // Determine channel format from sampler interpolation
                        // CUBICSPLINE tangents are discarded (values already extracted above);
                        // using LinearF32 gives correct per-frame interpolation for sparse keyframes.
                        AnimCurveChannel::Format chanFormat = AnimCurveChannel::Format::LinearF32;
                        if (samp.interpolation == "STEP")
                                chanFormat = AnimCurveChannel::Format::StepF32;

                        // Emit one AnimCurveChannel per component
                        for (int comp = 0; comp < numComp; ++comp) {
                                AnimCurveChannel c;
                                c.bone_index = jointIdx;
                                c.target    = static_cast<uint8_t>(baseTarget + comp);
                                c.oFormat    = chanFormat;
                                c.vTimes     = vTimes;
                                c.vValues.resize(tCnt);
                                for (size_t i = 0; i < tCnt; ++i) {
                                        size_t srcIdx = i * valuesPerKey + valueOffset;
                                        const float * p = reinterpret_cast<const float *>(vData + srcIdx * vStride);
                                        c.vValues[i] = p[comp];
                                }
                                oClip.vChannels.push_back(std::move(c));
                        }
                }

                if (!oClip.vChannels.empty()) {
                        // glTF samplers may carry sign-discontinuous quaternions (euler
                        // wrap in the DCC); per-component runtime interpolation would
                        // sweep limbs the long way round. Fix at the source.
                        EnforceQuatContinuity(oClip.vChannels);

                        log_d("GLTFReader: animation '{}' {:.3f}s, format: {}, {} channels",
                              oClip.sName,
                              oClip.duration,
                              static_cast<uint8_t>(oClip.vChannels[0].oFormat),
                              oClip.vChannels.size());
                        oCtx.vAnimClips.push_back(std::move(oClip));
                }
        }

        return uSUCCESS;
}

/* ------------------------------------------------------------------ */

ret_code_t GLTFReader::ReadScene(
                const std::string & sPath,
                NodeData & oRoot,
                ImportCtx & oCtx) {

        tinygltf::TinyGLTF loader;
        tinygltf::Model    model;
        std::string err, warn;
        bool ok = false;

        loader.SetImageLoader(
            [](tinygltf::Image *, const int, std::string *,
               std::string *, int, int,
               const unsigned char *, int, void *) -> bool {
                // Skip decoding — raw bytes are read later via bufferView
                return true;
            }, nullptr);

        std::string sExt = sPath.substr(sPath.find_last_of('.'));
        std::transform(sExt.begin(), sExt.end(), sExt.begin(), ::tolower);

        if (sExt == ".glb") {
                ok = loader.LoadBinaryFromFile(&model, &err, &warn, sPath);
        }
        else {
                ok = loader.LoadASCIIFromFile(&model, &err, &warn, sPath);
        }

        if (!warn.empty()) log_w("GLTFReader: {}", warn);
        if (!err.empty())  log_e("GLTFReader: {}", err);
        if (!ok) return uREAD_FILE_ERROR;

        if (model.scenes.empty()) {
                log_e("GLTFReader: no scenes in '{}'", sPath);
                return uWRONG_INPUT_DATA;
        }

        std::string sBaseDir = GetBaseDir(sPath);

        oRoot.sName = "RootNode";
        oRoot.vScale = glm::vec3(1.0f);

        // Skin pre-pass: build GLTFSkinCache for each skin
        std::vector<GLTFSkinCache> vSkins;
        if (oCtx.import_skin && !model.skins.empty()) {
                vSkins.resize(model.skins.size());
                for (int i = 0; i < (int)model.skins.size(); ++i) {
                        if (!ImportGLTFSkin(model, i, oCtx, oResStash, vSkins[i])) {
                                log_w("GLTFReader: failed to import skin {}", i);
                        }
                }
        }

        int scene_idx = model.defaultScene >= 0 ? model.defaultScene : 0;
        const tinygltf::Scene & scene = model.scenes[scene_idx];

        for (int node_idx : scene.nodes) {
                ret_code_t res = ImportGLTFNode(
                                model, node_idx, oRoot, oCtx, oResStash, sBaseDir, vSkins);
                if (res != uSUCCESS) return res;
        }

        // Animation pass
        if (oCtx.import_animations && !model.animations.empty()) {
                ret_code_t res = ImportGLTFAnimations(model, vSkins, oCtx);
                if (res != uSUCCESS) return res;
        }

        return uSUCCESS;
}

}
}
