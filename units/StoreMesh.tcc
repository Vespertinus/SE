namespace SE {


StoreMesh::StoreMesh(const Settings & oNewSettings) : oSettings(oNewSettings) { ;; }



StoreMesh::~StoreMesh() throw() { }



ret_code_t StoreMesh::Store(MeshStock & oMeshStock, std::vector<MeshData> & vMeshData) {

        if (!oMeshStock.vShapes.size() || !oMeshStock.vTextures.size()) {

                fprintf(stderr, "StoreMesh::Store: empty mesh data\n");
                return uWRONG_INPUT_DATA;
        }

        if (oMeshStock.vShapes.size() != oMeshStock.vTextures.size()) {
                fprintf(stderr, "StoreMesh::Store: wrong input data, mesh cnt = %zu, textures cnt = %zu\n",
                                oMeshStock.vShapes.size(),
                                oMeshStock.vTextures.size());
                return uWRONG_INPUT_DATA;
        }

        for (size_t i = 0; i < oMeshStock.vShapes.size(); ++i) {
                std::vector <float> & vShape = oMeshStock.vShapes[i];

                uint32_t buf_id         = 0;
                uint32_t triangles_cnt      = 0;
        
                glGenBuffers(1, &buf_id);
                glBindBuffer(GL_ARRAY_BUFFER, buf_id);
                glBufferData(GL_ARRAY_BUFFER, 
                             vShape.size() * sizeof(float), 
                             &vShape.at(0),
                             GL_STATIC_DRAW);
                triangles_cnt = vShape.size() / (3 + 3 + 3 + 2) / 3; // 3:vtx, 3:normal, 3:col, 2:texcoord

                printf("shape[%zu] triangles cnt = %u, texture id = %u\n", i, triangles_cnt, (oMeshStock.vTextures[i]) ? oMeshStock.vTextures[i]->GetID() : 0);

                vMeshData.emplace_back(MeshData{ buf_id, triangles_cnt, oMeshStock.vTextures[i] } );
        }

        return uSUCCESS; 
}


} // namespace SE

