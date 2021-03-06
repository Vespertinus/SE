

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <Logging.h>
#include <GeometryUtil.h>
#include "OBJReader.h"

#include <Mesh_generated.h>

namespace SE {
namespace TOOLS {

static std::string GetBaseDir(const std::string & sPath) {
        if (sPath.find_last_of("/\\") != std::string::npos)
                return sPath.substr(0, sPath.find_last_of("/\\")) + '/';
        return "";
}

SE::ret_code_t ReadOBJ(const std::string & sPath,
                       ModelData & oModel,
                       ImportCtx & oCtx) {

        std::string                             err;
        tinyobj::attrib_t                       oAttrib;
        std::vector<tinyobj::shape_t>           vShapes;
        std::vector<tinyobj::material_t>        vMaterials;
        std::vector<std::string>                vTextures;
        VertexIndex                             oVertexIndex;
        std::vector<float>                      vVertexData;
        /** pos(3float), normal(3float), tex(2float) */
        std::vector<float>                      vVertices;

        vVertexData.reserve(8);

        oModel.pMesh->sName  = sPath;

        std::string sBaseDir = GetBaseDir(sPath);
        if (sBaseDir.empty()) {
                sBaseDir = "./";
        }

        bool ret = tinyobj::LoadObj(&oAttrib,
                                    &vShapes,
                                    &vMaterials,
                                    &err,
                                    sPath.c_str(),
                                    sBaseDir.c_str());
        if (!err.empty()) {
                log_e("failed to load obj file '{}', reason: '{}'", sPath, err);
                return SE::uREAD_FILE_ERROR;
        }
        if (!ret) {
                log_e("failed to load obj file '{}'", sPath);
                return SE::uREAD_FILE_ERROR;
        }

        log_d("file: '{}'", sPath);
        log_d("vertices  = {}", (int)(oAttrib.vertices.size()) / 3);
        log_d("normals   = {}", (int)(oAttrib.normals.size()) / 3);
        log_d("texcoords = {}", (int)(oAttrib.texcoords.size()) / 2);
        log_d("materials = {}", (int)vMaterials.size());
        log_d("shapes    = {}", (int)vShapes.size());

        vMaterials.push_back(tinyobj::material_t());

        for (size_t i = 0; i < vMaterials.size(); i++) {
                tinyobj::material_t * pMat = & vMaterials[i];
                log_d("material[{}], name = '{}', diffuse_texname = '{}'", i, pMat->name, pMat->diffuse_texname);

                if (pMat->diffuse_texname.length() == 0) {
                        vTextures.emplace_back("");
                        continue;
                }

                vTextures.emplace_back(sBaseDir + pMat->diffuse_texname);
        }

        oModel.pMesh->vShapes.reserve(vShapes.size());
        uint8_t  stride           = ((oCtx.skip_normals) ? VERTEX_BASE_SIZE : VERTEX_SIZE) * sizeof(float);
        uint32_t index_start      = 0;
        uint32_t total_index_size = 0;

        for (size_t s = 0; s < vShapes.size(); ++s) {

                total_index_size += vShapes[s].mesh.indices.size();
        }
        vVertices.reserve(total_index_size  * (3 * 3 + 3 * 3 + 3 * 2 ));
        TPackVertexIndex Pack      = PackVertexIndexInit(total_index_size, oModel.pMesh->oIndex);

        for (size_t s = 0; s < vShapes.size(); ++s) {

                uint32_t                cur_index = 0;
                BoundingBox             oBBox;

                uint32_t triangles_cnt = vShapes[s].mesh.indices.size() / 3;

                for (size_t f = 0; f < triangles_cnt; f++) {
                        tinyobj::index_t idx0 = vShapes[s].mesh.indices[3 * f + 0];
                        tinyobj::index_t idx1 = vShapes[s].mesh.indices[3 * f + 1];
                        tinyobj::index_t idx2 = vShapes[s].mesh.indices[3 * f + 2];

                        int current_material_id = vShapes[s].mesh.material_ids[f];

                        log_d("face: {}:1, vert index: pos = {}, normal = {}, tex coord = {}",
                                        f,
                                        vShapes[s].mesh.indices[3 * f + 0].vertex_index,
                                        vShapes[s].mesh.indices[3 * f + 0].normal_index,
                                        vShapes[s].mesh.indices[3 * f + 0].texcoord_index);
                        log_d("face: {}:2, vert index: pos = {}, normal = {}, tex coord = {}",
                                        f,
                                        vShapes[s].mesh.indices[3 * f + 1].vertex_index,
                                        vShapes[s].mesh.indices[3 * f + 1].normal_index,
                                        vShapes[s].mesh.indices[3 * f + 1].texcoord_index);
                        log_d("face: {}:3, vert index: pos = {}, normal = {}, tex coord = {}",
                                        f,
                                        vShapes[s].mesh.indices[3 * f + 2].vertex_index,
                                        vShapes[s].mesh.indices[3 * f + 2].normal_index,
                                        vShapes[s].mesh.indices[3 * f + 2].texcoord_index);

                        if ((current_material_id < 0) || (current_material_id >= static_cast<int>(vMaterials.size()))) {
                                // Invaid material ID. Use default material.
                                current_material_id = vMaterials.size() - 1; // Default material is added to the last item in `vMaterials`.
                        }

                        float tex_coord[3][2];
                        if (oAttrib.texcoords.size() > 0) {
                                assert(oAttrib.texcoords.size() > (size_t)(2 * idx0.texcoord_index + 1));
                                assert(oAttrib.texcoords.size() > (size_t)(2 * idx1.texcoord_index + 1));
                                assert(oAttrib.texcoords.size() > (size_t)(2 * idx2.texcoord_index + 1));

                                tex_coord[0][0] = oAttrib.texcoords[2 * idx0.texcoord_index];
                                tex_coord[0][1] = oAttrib.texcoords[2 * idx0.texcoord_index + 1];
                                tex_coord[1][0] = oAttrib.texcoords[2 * idx1.texcoord_index];
                                tex_coord[1][1] = oAttrib.texcoords[2 * idx1.texcoord_index + 1];
                                tex_coord[2][0] = oAttrib.texcoords[2 * idx2.texcoord_index];
                                tex_coord[2][1] = oAttrib.texcoords[2 * idx2.texcoord_index + 1];

                        } else {
                                tex_coord[0][0] = 0.0f;
                                tex_coord[0][1] = 0.0f;
                                tex_coord[1][0] = 0.0f;
                                tex_coord[1][1] = 0.0f;
                                tex_coord[2][0] = 0.0f;
                                tex_coord[2][1] = 0.0f;
                        }

                        float vert[3][3];
                        int f0 = idx0.vertex_index;
                        int f1 = idx1.vertex_index;
                        int f2 = idx2.vertex_index;

                        vert[0][0] = oAttrib.vertices[3 * f0 + 0];
                        vert[1][0] = oAttrib.vertices[3 * f1 + 0];
                        vert[2][0] = oAttrib.vertices[3 * f2 + 0];

                        vert[0][1] = - oAttrib.vertices[3 * f0 + 2];
                        vert[1][1] = - oAttrib.vertices[3 * f1 + 2];
                        vert[2][1] = - oAttrib.vertices[3 * f2 + 2];

                        vert[0][2] = oAttrib.vertices[3 * f0 + 1];
                        vert[1][2] = oAttrib.vertices[3 * f1 + 1];
                        vert[2][2] = oAttrib.vertices[3 * f2 + 1];

                        float normals[3][3];
                        if (!oCtx.skip_normals) {
                                if (oAttrib.normals.size() > 0) {
                                        int f0 = idx0.normal_index;
                                        int f1 = idx1.normal_index;
                                        int f2 = idx2.normal_index;
                                        assert(f0 >= 0);
                                        assert(f1 >= 0);
                                        assert(f2 >= 0);
                                        for (int k = 0; k < 3; k++) {
                                                normals[0][k] = oAttrib.normals[3 * f0 + k];
                                                normals[1][k] = oAttrib.normals[3 * f1 + k];
                                                normals[2][k] = oAttrib.normals[3 * f2 + k];
                                        }
                                } else {
                                        CalcNormal(normals[0], vert[0], vert[1], vert[2]);
                                        normals[1][0] = normals[0][0];
                                        normals[1][1] = normals[0][1];
                                        normals[1][2] = normals[0][2];
                                        normals[2][0] = normals[0][0];
                                        normals[2][1] = normals[0][1];
                                        normals[2][2] = normals[0][2];
                                }
                        }

                        for (int k = 0; k < 3; k++) {
                                vVertexData.clear();

                                vVertexData.push_back(vert[k][0]);
                                vVertexData.push_back(vert[k][1]);
                                vVertexData.push_back(vert[k][2]);

                                oBBox.Concat(*((glm::vec3 *)&vert[k][0]));

                                if (!oCtx.skip_normals) {
                                        vVertexData.push_back(normals[k][0]);
                                        vVertexData.push_back(normals[k][1]);
                                        vVertexData.push_back(normals[k][2]);
                                }

                                vVertexData.push_back(tex_coord[k][0]);
                                vVertexData.push_back(tex_coord[k][1]);

                                if (oVertexIndex.Get(vVertexData, cur_index)) {

                                        log_d("new index = {}, pos ({}, {}, {}), rot ({}, {}, {}), uv ({}, {})",
                                                        cur_index,
                                                        vVertexData[0], vVertexData[1], vVertexData[2],
                                                        (oCtx.skip_normals) ? 0 : vVertexData[3],
                                                        (oCtx.skip_normals) ? 0 : vVertexData[4],
                                                        (oCtx.skip_normals) ? 0 : vVertexData[5],
                                                        vVertexData[6], vVertexData[7]);

                                        vVertices.insert(vVertices.end(), vVertexData.begin(), vVertexData.end());
                                        ++oCtx.total_vertices_cnt;
                                }
                                else {
                                        log_d("old index = {}, pos ({}, {}, {}), rot ({}, {}, {}), uv ({}, {})",
                                                        cur_index,
                                                        vVertexData[0], vVertexData[1], vVertexData[2],
                                                        (oCtx.skip_normals) ? 0 : vVertexData[3],
                                                        (oCtx.skip_normals) ? 0 : vVertexData[4],
                                                        (oCtx.skip_normals) ? 0 : vVertexData[5],
                                                        vVertexData[6], vVertexData[7]);
                                }
                                Pack(oModel.pMesh->oIndex, cur_index);
                        }
                }

                oCtx.total_triangles_cnt += triangles_cnt;

                oModel.pMesh->vShapes.emplace_back(
                                index_start,
                                static_cast<uint32_t>(vShapes[s].mesh.indices.size()),
                                std::move(oBBox));
                index_start = vShapes[s].mesh.indices.size();
        }

        oModel.pMesh->vVertexBuffers.emplace_back(MeshData::VertexBuffer { std::move(vVertices), stride });

        uint16_t next_offset = 3;

        oModel.pMesh->vAttributes.emplace_back(MeshData::VertexAttribute{
                        "Position",
                        0,
                        3,
                        0 });
        if (!oCtx.skip_normals) {
                oModel.pMesh->vAttributes.emplace_back(MeshData::VertexAttribute{
                                "Normal",
                                static_cast<uint16_t>(3 * sizeof(float)),
                                3,
                                0 });
                next_offset = 6;
        }
        oModel.pMesh->vAttributes.emplace_back(MeshData::VertexAttribute{
                        "TexCoord0",
                        static_cast<uint16_t>(next_offset * sizeof(float)),
                        2,
                        0 });

        //TODO fill oModel materials and textures

        ++oCtx.mesh_cnt;

        for (auto & oShape : oModel.pMesh->vShapes) {
                oModel.pMesh->oBBox.Concat(oShape.oBBox);
        }

        return SE::uSUCCESS;

}

}
}
