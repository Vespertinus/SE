/*
        FILE based on tinyobjloader:viewer.cc
*/

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <Global.h> 
#include <GlobalTypes.h>


namespace SE {

static std::string GetBaseDir(const std::string & filepath) {
        if (filepath.find_last_of("/\\") != std::string::npos)
                return filepath.substr(0, filepath.find_last_of("/\\"));
        return "";
}


static void CalcNormal(float normals[3], float v0[3], float v1[3], float v2[3]) {
        float v10[3];
        v10[0] = v1[0] - v0[0];
        v10[1] = v1[1] - v0[1];
        v10[2] = v1[2] - v0[2];

        float v20[3];
        v20[0] = v2[0] - v0[0];
        v20[1] = v2[1] - v0[1];
        v20[2] = v2[2] - v0[2];

        normals[0] = v20[1] * v10[2] - v20[2] * v10[1];
        normals[1] = v20[2] * v10[0] - v20[0] * v10[2];
        normals[2] = v20[0] * v10[1] - v20[1] * v10[0];

        float len2 = normals[0] * normals[0] + normals[1] * normals[1] + normals[2] * normals[2];
        if (len2 > 0.0f) {
                float len = sqrtf(len2);

                normals[0] /= len;
                normals[1] /= len;
        }
}



OBJLoader::OBJLoader(const Settings & oSettings) { ;; }


OBJLoader::~OBJLoader() throw() { ;; }

ret_code_t OBJLoader::Load(const std::string sPath, MeshStock & oStock) {

        std::string                             err;
        tinyobj::attrib_t                       oAttrib;
        std::vector<tinyobj::shape_t>           vShapes;
        std::vector<tinyobj::material_t>        vMaterials;
        std::vector<SE::TTexture *>             vTextures;

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
                fprintf(stderr, "OBJLoader::Load: failed to load obj file '%s', reason: '%s'\n", sPath.c_str(), err.c_str());
                return uREAD_FILE_ERROR;
        }
        if (!ret) {
                fprintf(stderr, "OBJLoader::Load: failed to load obj file '%s'\n", sPath.c_str());
                return uREAD_FILE_ERROR;
        }

        //TODO rewrite to debug log
        printf("OBJLoader::Load: file: '%s'\n", sPath.c_str());
        printf("OBJLoader::Load: vertices  = %d\n", (int)(oAttrib.vertices.size()) / 3);
        printf("OBJLoader::Load: normals   = %d\n", (int)(oAttrib.normals.size()) / 3);
        printf("OBJLoader::Load: texcoords = %d\n", (int)(oAttrib.texcoords.size()) / 2);
        printf("OBJLoader::Load: materials = %d\n", (int)vMaterials.size());
        printf("OBJLoader::Load: shapes    = %d\n", (int)vShapes.size());

        vMaterials.push_back(tinyobj::material_t());

        for (size_t i = 0; i < vMaterials.size(); i++) {
                tinyobj::material_t * pMat = & vMaterials[i];
                printf("OBJLoader::Load: material[%d].diffuse_texname = %s\n", int(i), pMat->diffuse_texname.c_str());
                
                if (pMat->diffuse_texname.length() == 0) { continue; }

                SE::TTexture * pTex = SE::TResourceManager::Instance().Create<SE::TTexture>(pMat->diffuse_texname);
                if (pTex) {
                        vTextures.emplace_back(pTex);
                }
                else {
                      fprintf(stderr, "OBJLoader::Load: failed to load texture: '%s'\n", pMat->diffuse_texname.c_str());
                }
        }
        
        
        for (size_t s = 0; s < vShapes.size(); s++) {
                std::vector<float> vMeshData;  // pos(3float), normal(3float), color(3float)
                vMeshData.reserve((vShapes[s].mesh.indices.size() / 3) * (3 * 3 + 3 * 3 + 3 * 2 )); //CHECK
                
                for (size_t f = 0; f < vShapes[s].mesh.indices.size() / 3; f++) {
                        tinyobj::index_t idx0 = vShapes[s].mesh.indices[3 * f + 0];
                        tinyobj::index_t idx1 = vShapes[s].mesh.indices[3 * f + 1];
                        tinyobj::index_t idx2 = vShapes[s].mesh.indices[3 * f + 2];

                        int current_material_id = vShapes[s].mesh.material_ids[f];

                        if ((current_material_id < 0) || (current_material_id >= static_cast<int>(vMaterials.size()))) {
                                // Invaid material ID. Use default material.
                                current_material_id = vMaterials.size() - 1; // Default material is added to the last item in `vMaterials`.
                        }
                        
                        float diffuse[3];
                        for (size_t i = 0; i < 3; i++) {
                                diffuse[i] = vMaterials[current_material_id].diffuse[i];
                        }

                        float tex_coord[3][2];
                        if (oAttrib.texcoords.size() > 0) {
                                assert(oAttrib.texcoords.size() > 2 * idx0.texcoord_index + 1);
                                assert(oAttrib.texcoords.size() > 2 * idx1.texcoord_index + 1);
                                assert(oAttrib.texcoords.size() > 2 * idx2.texcoord_index + 1);

                                // Flip Y coord.
                                tex_coord[0][0] = oAttrib.texcoords[2 * idx0.texcoord_index];
                                tex_coord[0][1] = 1.0f - oAttrib.texcoords[2 * idx0.texcoord_index + 1];
                                tex_coord[1][0] = oAttrib.texcoords[2 * idx1.texcoord_index];
                                tex_coord[1][1] = 1.0f - oAttrib.texcoords[2 * idx1.texcoord_index + 1];
                                tex_coord[2][0] = oAttrib.texcoords[2 * idx2.texcoord_index];
                                tex_coord[2][1] = 1.0f - oAttrib.texcoords[2 * idx2.texcoord_index + 1];
                        } else {
                                tex_coord[0][0] = 0.0f;
                                tex_coord[0][1] = 0.0f;
                                tex_coord[1][0] = 0.0f;
                                tex_coord[1][1] = 0.0f;
                                tex_coord[2][0] = 0.0f;
                                tex_coord[2][1] = 0.0f;
                        }
                        
                        float vert[3][3];
                        for (int k = 0; k < 3; k++) {
                                int f0 = idx0.vertex_index;
                                int f1 = idx1.vertex_index;
                                int f2 = idx2.vertex_index;
                                assert(f0 >= 0);
                                assert(f1 >= 0);
                                assert(f2 >= 0);

                                vert[0][k] = oAttrib.vertices[3 * f0 + k];
                                vert[1][k] = oAttrib.vertices[3 * f1 + k];
                                vert[2][k] = oAttrib.vertices[3 * f2 + k];
                        }

                        float normals[3][3];
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
                                // compute geometric normal
                                CalcNormal(normals[0], vert[0], vert[1], vert[2]);
                                normals[1][0] = normals[0][0];
                                normals[1][1] = normals[0][1];
                                normals[1][2] = normals[0][2];
                                normals[2][0] = normals[0][0];
                                normals[2][1] = normals[0][1];
                                normals[2][2] = normals[0][2];
                        }
                        for (int k = 0; k < 3; k++) {
                                vMeshData.push_back(vert[k][0]);
                                vMeshData.push_back(vert[k][1]);
                                vMeshData.push_back(vert[k][2]);
                                vMeshData.push_back(normals[k][0]);
                                vMeshData.push_back(normals[k][1]);
                                vMeshData.push_back(normals[k][2]);

                                //CHECK ... --> turn off
                                // Combine normal and diffuse to get color.
                                float normal_factor = 0.2;
                                float diffuse_factor = 1 - normal_factor;
                                float c[3] = {
                                        normals[k][0] * normal_factor + diffuse[0] * diffuse_factor,
                                        normals[k][1] * normal_factor + diffuse[1] * diffuse_factor,
                                        normals[k][2] * normal_factor + diffuse[2] * diffuse_factor
                                };
                                float len2 = c[0] * c[0] + c[1] * c[1] + c[2] * c[2];
                                if (len2 > 0.0f) {
                                        float len = sqrtf(len2);

                                        c[0] /= len;
                                        c[1] /= len;
                                        c[2] /= len;
                                }
                                //CHECK ... --> turn off

                                vMeshData.push_back(c[0] * 0.5 + 0.5);
                                vMeshData.push_back(c[1] * 0.5 + 0.5);
                                vMeshData.push_back(c[2] * 0.5 + 0.5);

                                vMeshData.push_back(tex_coord[k][0]);
                                vMeshData.push_back(tex_coord[k][1]);
                        }
                }

                if (vMeshData.size() == 0) {
                        fprintf(stderr, "OBJLoader::Load: empty shape, ind = %zu\n", s);
                        continue;
                }

                if (vShapes[s].mesh.material_ids.size() > 0 && vShapes[s].mesh.material_ids.size() > s) {
                        oStock.vTextures.emplace_back(vTextures[vShapes[s].mesh.material_ids[0] ]);
                        printf("OBJLoader::Load: shape[%zu] material id = %d\n", s, vShapes[s].mesh.material_ids[0] );
                } else {
                        oStock.vTextures.emplace_back(nullptr);
                        printf("OBJLoader::Load: shape[%zu] empty material\n", s);
                }
                
/*
                if (vMeshData.size() > 0) {
                        glGenBuffers(1, &o.vb_id);
                        glBindBuffer(GL_ARRAY_BUFFER, o.vb_id);
                        glBufferData(GL_ARRAY_BUFFER, vMeshData.size() * sizeof(float), &vMeshData.at(0),
                                        GL_STATIC_DRAW);
                        o.numTriangles = vMeshData.size() / (3 + 3 + 3 + 2) / 3; // 3:vtx, 3:normal, 3:col, 2:texcoord

                        printf("shape[%d] # of triangles = %d\n", static_cast<int>(s),
                                        o.numTriangles);
                }
*/
                oStock.vShapes.emplace_back(std::move(vMeshData));
        } 

        return uSUCCESS;
}


} //namespace SE
