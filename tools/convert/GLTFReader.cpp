
#include <nlohmann/json.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

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

#include "GLTFReader.h"

#include <Logging.h>
#include <GeometryUtil.h>
#include <StrID.h>
#include <BoundingBox.h>


namespace SE {
namespace TOOLS {

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
        oModel.pMaterial = pMaterial;

        pMaterial->sName       = mat.name.empty() ? sKey : mat.name;
        pMaterial->sShaderPath = "shader_program/pbr_geometry.sesp";

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

        // GL constants (avoid GL header dependency in tool code)
        constexpr int SE_GLTF_GL_RGBA  = 0x1908;
        constexpr int SE_GLTF_GL_RGBA8 = 0x8058;

        auto AddTexture = [&](int tex_index, TextureUnit unit) {
                if (tex_index < 0) return;
                const tinygltf::Texture & tex = model.textures[tex_index];
                if (tex.source < 0) return;
                const tinygltf::Image & img = model.images[tex.source];
                if (!img.uri.empty()) {
                        std::string sPath = sBaseDir + img.uri;
                        oCtx.FixPath(sPath);
                        pMaterial->mTextures[unit].sPath = sPath;
                        ++oCtx.textures_cnt;
                }
                else if (!img.image.empty()) {
                        TextureData & oTex             = pMaterial->mTextures[unit];
                        oTex.vImageData                = img.image;
                        oTex.oStock.raw_image          = oTex.vImageData.data();
                        oTex.oStock.raw_image_size     = static_cast<uint32_t>(oTex.vImageData.size());
                        oTex.oStock.format             = SE_GLTF_GL_RGBA;
                        oTex.oStock.internal_format    = SE_GLTF_GL_RGBA8;
                        oTex.oStock.width              = static_cast<uint32_t>(img.width);
                        oTex.oStock.height             = static_cast<uint32_t>(img.height);
                        oTex.sName                     = img.name;
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
                const std::string & sMeshName,
                ModelData & oModel,
                ImportCtx & oCtx,
                ResourceStash & oResStash,
                const std::string & sBaseDir) {

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

        // --- Compute per-vertex tangents if absent ---
        std::vector<glm::vec4> vComputedTangents;
        if (!hasTangents) {
                vComputedTangents.assign(posCnt, glm::vec4(0.0f));
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
                        for (uint32_t idx : { i0, i1, i2 }) {
                                vComputedTangents[idx] += glm::vec4(T, 0.0f);
                        }
                }
                for (auto & t : vComputedTangents) {
                        float len = glm::length(glm::vec3(t));
                        if (len > 1e-8f) t = glm::vec4(glm::vec3(t) / len, 1.0f);
                        else             t = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
                }
        }

        // --- Vertex layout ---
        // Position(3) + [Normal(3)] + TexCoord0(2) + Tangent(4)
        const uint8_t floats_per_vert = oCtx.skip_normals ? 9u : 12u;
        const uint8_t stride_bytes    = floats_per_vert * sizeof(float);

        std::vector<float> vVertices;
        std::vector<float> vVertexData;
        vVertexData.reserve(floats_per_vert);

        VertexIndex oVertexIndex;
        MeshData * pMesh = oModel.pMesh;

        TPackVertexIndex Pack = PackVertexIndexInit(
                static_cast<uint32_t>(vIndices.size()), pMesh->oIndex);

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
                vVertexData.push_back(uv.y);

                glm::vec4 tan = hasTangents
                        ? GetVec4(tanData, tanStride, gltf_idx)
                        : vComputedTangents[gltf_idx];
                vVertexData.push_back(tan.x);
                vVertexData.push_back(tan.y);
                vVertexData.push_back(tan.z);
                vVertexData.push_back(tan.w);

                if (oVertexIndex.Get(vVertexData, cur_index)) {
                        vVertices.insert(vVertices.end(), vVertexData.begin(), vVertexData.end());
                        ++oCtx.total_vertices_cnt;
                }
                Pack(pMesh->oIndex, cur_index);
        }

        uint32_t index_count = std::visit([](auto & v) -> uint32_t { return v.size(); }, pMesh->oIndex);

        pMesh->vShapes.emplace_back(0u, index_count, oBBox);
        pMesh->oBBox.Concat(oBBox);

        pMesh->vVertexBuffers.emplace_back(
                MeshData::VertexBuffer{ std::move(vVertices), stride_bytes });

        // Vertex attribute descriptors
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

        oCtx.total_triangles_cnt += static_cast<uint32_t>(vIndices.size()) / 3;
        ++oCtx.mesh_cnt;

        return uSUCCESS;
}

/* ------------------------------------------------------------------ */

static ret_code_t ImportGLTFMesh(
                const tinygltf::Model & model,
                int mesh_idx,
                NodeData & oNodeData,
                ImportCtx & oCtx,
                ResourceStash & oResStash,
                const std::string & sBaseDir) {

        const tinygltf::Mesh & mesh = model.meshes[mesh_idx];
        log_d("GLTFReader: mesh '{}', {} primitive(s)", mesh.name, mesh.primitives.size());

        for (size_t p = 0; p < mesh.primitives.size(); ++p) {
                const tinygltf::Primitive & prim = mesh.primitives[p];

                if (prim.mode != TINYGLTF_MODE_TRIANGLES) {
                        log_w("GLTFReader: mesh '{}' primitive {} is not TRIANGLES, skipping", mesh.name, p);
                        continue;
                }

                std::string sMeshKey = oCtx.sPackName + mesh.name + "|prim" + std::to_string(p);

                MeshData * pMesh = nullptr;
                oResStash.GetResourceData(StrID(sMeshKey), &pMesh);
                pMesh->sName = sMeshKey;

                ModelData oModel;
                oModel.pMesh = pMesh;

                ret_code_t res = ImportGLTFPrimitive(
                                model, prim, mesh.name, oModel, oCtx, oResStash, sBaseDir);
                if (res != uSUCCESS) return res;

                if (!oCtx.skip_material && prim.material >= 0) {
                        ret_code_t res2 = ImportGLTFMaterial(
                                        model, prim.material, oModel, oCtx, oResStash, sBaseDir);
                        if (res2 != uSUCCESS) return res2;
                }

                oNodeData.vComponents.emplace_back(std::move(oModel));
        }

        return uSUCCESS;
}

/* ------------------------------------------------------------------ */

static ret_code_t ImportGLTFNode(
                const tinygltf::Model & model,
                int node_idx,
                NodeData & oParent,
                ImportCtx & oCtx,
                ResourceStash & oResStash,
                const std::string & sBaseDir) {

        const tinygltf::Node & node = model.nodes[node_idx];

        NodeData oNodeData;
        oNodeData.sName   = node.name.empty() ? ("node_" + std::to_string(node_idx)) : node.name;
        oNodeData.scale   = glm::vec3(1.0f);
        oNodeData.enabled = !oCtx.disable_nodes;

        // Transform: matrix takes priority, else TRS
        if (node.matrix.size() == 16) {
                glm::mat4 mat(
                        node.matrix[0], node.matrix[1], node.matrix[2],  node.matrix[3],
                        node.matrix[4], node.matrix[5], node.matrix[6],  node.matrix[7],
                        node.matrix[8], node.matrix[9], node.matrix[10], node.matrix[11],
                        node.matrix[12],node.matrix[13],node.matrix[14], node.matrix[15]);

                // Decompose translation
                oNodeData.translation = glm::vec3(mat[3]);
                // Decompose scale
                oNodeData.scale = glm::vec3(
                        glm::length(glm::vec3(mat[0])),
                        glm::length(glm::vec3(mat[1])),
                        glm::length(glm::vec3(mat[2])));
                // Decompose rotation
                glm::mat3 rotMat(
                        glm::vec3(mat[0]) / oNodeData.scale.x,
                        glm::vec3(mat[1]) / oNodeData.scale.y,
                        glm::vec3(mat[2]) / oNodeData.scale.z);
                glm::quat q         = glm::quat_cast(rotMat);
                oNodeData.rotation  = glm::degrees(glm::eulerAngles(q));
        }
        else {
                if (node.translation.size() == 3) {
                        oNodeData.translation = glm::vec3(
                                node.translation[0], node.translation[1], node.translation[2]);
                }
                if (node.scale.size() == 3) {
                        oNodeData.scale = glm::vec3(
                                node.scale[0], node.scale[1], node.scale[2]);
                }
                if (node.rotation.size() == 4) {
                        // glTF quaternion layout: [x, y, z, w]
                        glm::quat q(
                                static_cast<float>(node.rotation[3]),   // w
                                static_cast<float>(node.rotation[0]),   // x
                                static_cast<float>(node.rotation[1]),   // y
                                static_cast<float>(node.rotation[2]));  // z
                        oNodeData.rotation = glm::degrees(glm::eulerAngles(q));
                }
        }

        ++oCtx.node_cnt;

        if (node.mesh >= 0) {
                ret_code_t res = ImportGLTFMesh(
                                model, node.mesh, oNodeData, oCtx, oResStash, sBaseDir);
                if (res != uSUCCESS) return res;
        }

        for (int child_idx : node.children) {
                ret_code_t res = ImportGLTFNode(
                                model, child_idx, oNodeData, oCtx, oResStash, sBaseDir);
                if (res != uSUCCESS) return res;
        }

        oParent.vChildren.emplace_back(std::move(oNodeData));
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
            [](tinygltf::Image * image, const int, std::string * err,
               std::string *, int, int,
               const unsigned char * bytes, int size, void *) -> bool {
                if (!image->uri.empty()) {
                    // External URI — we only need the path, skip decoding
                    return true;
                }
                // Embedded image — decode to RGBA8 with stb_image
                int w, h, comp;
                uint8_t * data = stbi_load_from_memory(bytes, size, &w, &h, &comp, 4);
                if (!data) {
                    if (err) *err = stbi_failure_reason();
                    return false;
                }
                image->width     = w;
                image->height    = h;
                image->component = 4;
                image->image.assign(data, data + w * h * 4);
                stbi_image_free(data);
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
        oRoot.scale = glm::vec3(1.0f);

        int scene_idx = model.defaultScene >= 0 ? model.defaultScene : 0;
        const tinygltf::Scene & scene = model.scenes[scene_idx];

        for (int node_idx : scene.nodes) {
                ret_code_t res = ImportGLTFNode(
                                model, node_idx, oRoot, oCtx, oResStash, sBaseDir);
                if (res != uSUCCESS) return res;
        }

        return uSUCCESS;
}

}
}
