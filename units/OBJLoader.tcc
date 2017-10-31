/*
        FILE based on tinyobjloader:viewer.cc
*/

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>


namespace SE {

static std::string GetBaseDir(const std::string & filepath) {
        if (filepath.find_last_of("/\\") != std::string::npos)
                return filepath.substr(0, filepath.find_last_of("/\\")) + '/';
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
                log_e("failed to load obj file '{}', reason: '{}'", sPath, err);
                return uREAD_FILE_ERROR;
        }
        if (!ret) {
                log_e("failed to load obj file '{}'", sPath);
                return uREAD_FILE_ERROR;
        }

        log_d("file: '{}'", sPath.c_str());
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
                        vTextures.emplace_back(nullptr);
                        continue; 
                }

                SE::TTexture * pTex = SE::TResourceManager::Instance().Create<SE::TTexture>(sBaseDir + pMat->diffuse_texname);
                if (pTex) {
                        vTextures.emplace_back(pTex);
                }
                else {
                      char buf[256];
                      snprintf(buf, sizeof(buf), "OBJLoader::Load: failed to load texture: '%s'", pMat->diffuse_texname.c_str());
                      log_e("{}", buf);
                      throw (std::runtime_error(buf));
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
                                assert(oAttrib.texcoords.size() > (size_t)(2 * idx0.texcoord_index + 1));
                                assert(oAttrib.texcoords.size() > (size_t)(2 * idx1.texcoord_index + 1));
                                assert(oAttrib.texcoords.size() > (size_t)(2 * idx2.texcoord_index + 1));
/*
                                // Flip Y coord.
                                tex_coord[0][0] = oAttrib.texcoords[2 * idx0.texcoord_index];
                                tex_coord[0][1] = 1.0f - oAttrib.texcoords[2 * idx0.texcoord_index + 1];
                                tex_coord[1][0] = oAttrib.texcoords[2 * idx1.texcoord_index];
                                tex_coord[1][1] = 1.0f - oAttrib.texcoords[2 * idx1.texcoord_index + 1];
                                tex_coord[2][0] = oAttrib.texcoords[2 * idx2.texcoord_index];
                                tex_coord[2][1] = 1.0f - oAttrib.texcoords[2 * idx2.texcoord_index + 1];
*/
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
                        /*
                        for (int k = 0; k < 3; k++) {
                                int f0 = idx0.vertex_index;
                                int f1 = idx1.vertex_index;
                                int f2 = idx2.vertex_index;
                                assert(f0 >= 0);
                                assert(f1 >= 0);
                                assert(f2 >= 0);

                                //vert[0][k] = oAttrib.vertices[3 * f0 + k];
                                //vert[1][k] = oAttrib.vertices[3 * f1 + k];
                                //vert[2][k] = oAttrib.vertices[3 * f2 + k];
                                
                                vert[0][k] = oAttrib.vertices[3 * f0 + k];
                                vert[1][k] = oAttrib.vertices[3 * f1 + k];
                                vert[2][k] = oAttrib.vertices[3 * f2 + k];
                                
                        }*/
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
                                //TODO remove color data for each vertice, move to material settings
//                                float normal_factor = 0.2;
//                                float diffuse_factor = 1 - normal_factor;
                                float c[3] = {
                                        /*
                                        normals[k][0] * normal_factor + diffuse[0] * diffuse_factor,
                                        normals[k][1] * normal_factor + diffuse[1] * diffuse_factor,
                                        normals[k][2] * normal_factor + diffuse[2] * diffuse_factor
*/
                                        diffuse[0],
                                        diffuse[1],
                                        diffuse[2]
                                };
                                float len2 = c[0] * c[0] + c[1] * c[1] + c[2] * c[2];
                                if (len2 > 0.0f) {
                                        float len = sqrtf(len2);

                                        c[0] /= len;
                                        c[1] /= len;
                                        c[2] /= len;
                                }
                                //CHECK ... --> turn off

                                vMeshData.push_back(c[0] /* * 0.5 + 0.5*/);
                                vMeshData.push_back(c[1] /* * 0.5 + 0.5*/);
                                vMeshData.push_back(c[2] /* * 0.5 + 0.5*/);

                                vMeshData.push_back(tex_coord[k][0]);
                                vMeshData.push_back(tex_coord[k][1]);
                        }
                }

                if (vMeshData.size() == 0) {
                        log_e("empty shape, ind = {}", s);
                        continue;
                }

                if (vShapes[s].mesh.material_ids.size() > 0 && vShapes[s].mesh.material_ids[0] >= 0 /*&& vShapes[s].mesh.material_ids.size() > s*/) {
                        oStock.vTextures.emplace_back(vTextures[vShapes[s].mesh.material_ids[0] ]);
                        log_d("shape[{}] name = '{}', material ind id = {}", s, vShapes[s].name.c_str(), vShapes[s].mesh.material_ids[0] );
                } else {
                        oStock.vTextures.emplace_back(nullptr);
                        log_d("shape[{}] name = '{}' empty material", s, vShapes[s].name.c_str());
                }
                
                oStock.vShapes.emplace_back(std::make_tuple(std::move(vMeshData), vShapes[s].name));
        } 

        return uSUCCESS;
}


} //namespace SE
