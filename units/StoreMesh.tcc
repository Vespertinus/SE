namespace SE {


StoreMesh::StoreMesh(const Settings & oNewSettings) : oSettings(oNewSettings) { ;; }



StoreMesh::~StoreMesh() throw() { }



ret_code_t StoreMesh::Store(MeshStock & oMeshStock, std::vector<MeshData> & vMeshData) {

        if (!oMeshStock.vShapes.size() || !oMeshStock.vTextures.size()) {

                log_e("empty mesh data");
                return uWRONG_INPUT_DATA;
        }

        if (oMeshStock.vShapes.size() != oMeshStock.vTextures.size()) {
                log_e("wrong input data, mesh cnt = {}, textures cnt = {}",
                                oMeshStock.vShapes.size(),
                                oMeshStock.vTextures.size());
                return uWRONG_INPUT_DATA;
        }

        for (size_t i = 0; i < oMeshStock.vShapes.size(); ++i) {
                std::vector <float> & vShape = std::get<0>(oMeshStock.vShapes[i]);
                std::string & sName = std::get<1>(oMeshStock.vShapes[i]);

                glm::vec3 min(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
                glm::vec3 max(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());

                uint32_t buf_id         = 0;
                uint32_t triangles_cnt  = 0;
        
                glGenBuffers(1, &buf_id);
                glBindBuffer(GL_ARRAY_BUFFER, buf_id);
                glBufferData(GL_ARRAY_BUFFER, 
                             vShape.size() * sizeof(float), 
                             &vShape.at(0),
                             GL_STATIC_DRAW);
                triangles_cnt = vShape.size() / (3 + 3 + 3 + 2) / 3; // 3:vtx, 3:normal, 3:col, 2:texcoord

                for (uint32_t i = 0; i < vShape.size(); i += (3 + 3 + 3 + 2)) {
                        min.x = std::min(vShape[i    ], min.x);
                        min.y = std::min(vShape[i + 1], min.y);
                        min.z = std::min(vShape[i + 2], min.z);
                        
                        max.x = std::max(vShape[i    ], max.x);
                        max.y = std::max(vShape[i + 1], max.y);
                        max.z = std::max(vShape[i + 2], max.z);
                }

                log_d("shape[{}u] name = '{}', triangles cnt = {}, texture id = {}, min x = {}, y = {}, z = {}, max x = {}, y = {}, z = {}",
                                i, 
                                sName.c_str(), 
                                triangles_cnt, 
                                (oMeshStock.vTextures[i]) ? oMeshStock.vTextures[i]->GetID() : 0,
                                min.x, min.y, min.z,
                                max.x, max.y, max.z);

                vMeshData.emplace_back(MeshData{ buf_id, triangles_cnt, oMeshStock.vTextures[i], std::get<1>(oMeshStock.vShapes[i]), min, max } );
        }

        return uSUCCESS;
}


} // namespace SE

