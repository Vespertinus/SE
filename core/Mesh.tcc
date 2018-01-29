
namespace SE  {

template <class StoreStrategyList, class LoadStrategyList>
        template <class TStoreStrategySettings,  class TLoadStrategySettings>
                Mesh<StoreStrategyList, LoadStrategyList>::Mesh(
                        const std::string & oName,
                        const rid_t new_rid,
                        const TStoreStrategySettings & oStoreStrategySettings,
                        const TLoadStrategySettings & oLoadStrategySettings,
                        const MeshSettings & oNewMeshSettings) :
                ResourceHolder(new_rid),
                oMeshSettings(oNewMeshSettings),
                stride ((oMeshSettings.skip_normals) ? (3 + 2) * sizeof(float) : (3 + 3 + 2) * sizeof(float) ) {

        Create(oName, oStoreStrategySettings, oLoadStrategySettings);
}

template <class StoreStrategyList, class LoadStrategyList>
        Mesh<StoreStrategyList, LoadStrategyList>::Mesh(
                const std::string & oName,
                const rid_t new_rid,
                const MeshSettings & oNewMeshSettings) :
                        Mesh(oName,
                             rid,
                             typename TDefaultStoreStrategy::Settings(),
                             typename TDefaultLoadStrategy::Settings(),
                             oNewMeshSettings
                             ) {
}

template <class StoreStrategyList, class LoadStrategyList>
        template <class TConcreateSettings, std::enable_if_t< MP::InnerContain<StoreStrategyList, TConcreateSettings>::value, TConcreateSettings> * >
                Mesh<StoreStrategyList, LoadStrategyList>::Mesh(
                        const std::string & oName,
                        const rid_t new_rid,
                        const TConcreateSettings & oSettings,
                        const MeshSettings & oNewMeshSettings) :
                                Mesh(oName,
                                     rid,
                                     oSettings,
                                     typename TDefaultLoadStrategy::Settings(),
                                     oNewMeshSettings) {
}

template <class StoreStrategyList, class LoadStrategyList>
        template <class TConcreateSettings, std::enable_if_t< MP::InnerContain<LoadStrategyList, TConcreateSettings>::value, TConcreateSettings> * >
                Mesh<StoreStrategyList, LoadStrategyList>::Mesh(
                        const std::string & oName,
                        const rid_t new_rid,
                        const TConcreateSettings & oSettings,
                        const MeshSettings & oNewMeshSettings) :
                                Mesh(oName,
                                     rid,
                                     typename TDefaultStoreStrategy::Settings(),
                                     oSettings,
                                     oNewMeshSettings) {
}

template <class StoreStrategyList, class LoadStrategyList> Mesh<StoreStrategyList, LoadStrategyList>::~Mesh() throw() { ;; }



template <class StoreStrategyList, class LoadStrategyList>
        template <class TStoreStrategySettings,  class TLoadStrategySettings> void
                Mesh<StoreStrategyList, LoadStrategyList>::Create(
                                const std::string oName,
                                const TStoreStrategySettings & oStoreStrategySettings,
                                const TLoadStrategySettings & oLoadStrategySettings) {

        typedef typename MP::InnerSearch<StoreStrategyList, TStoreStrategySettings>::Result TStoreStrategy;
        typedef typename MP::InnerSearch<LoadStrategyList,  TLoadStrategySettings >::Result TLoadStrategy;

        MeshStock    oMeshStock(oMeshSettings);
        ret_code_t   err_code;

        TLoadStrategy   oLoadStrategy(oLoadStrategySettings);
        TStoreStrategy  oStoreStrategy(oStoreStrategySettings);

        log_d("skip_normals = {}, ext_material = {}", oMeshSettings.skip_normals, oMeshSettings.ext_material);

        err_code = oLoadStrategy.Load(oName, oMeshStock);
        if (err_code) {
                char buf[256];
                snprintf(buf, sizeof(buf), "Mesh::Create: Loading failed, err_code = %u", err_code);
                log_e("{}", buf);
                throw (std::runtime_error(buf));
        }

        err_code = oStoreStrategy.Store(oMeshStock, vMeshData);
        if (err_code) {
                char buf[256];
                snprintf(buf, sizeof(buf), "Mesh::Create: Storing failed, err_code = %u\n", err_code);
                log_e("{}", buf);
                throw (std::runtime_error(buf));
        }

        if (vMeshData.size() == 1) {
                min = vMeshData[0].min;
                max = vMeshData[0].max;
        }
        else if (vMeshData.size() > 1) {
                min = glm::vec3(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
                max = glm::vec3(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());

                for (auto oMeshData : vMeshData) {
                        min.x = std::min(oMeshData.min.x, min.x);
                        min.y = std::min(oMeshData.min.y, min.y);
                        min.z = std::min(oMeshData.min.z, min.z);

                        max.x = std::max(oMeshData.max.x, max.x);
                        max.y = std::max(oMeshData.max.y, max.y);
                        max.z = std::max(oMeshData.max.z, max.z);
                }
        }
}

template <class StoreStrategyList, class LoadStrategyList> uint32_t Mesh<StoreStrategyList, LoadStrategyList>::GetShapesCnt() const {
        return vMeshData.size();
}

template <class StoreStrategyList, class LoadStrategyList> uint32_t Mesh<StoreStrategyList, LoadStrategyList>::GetTrianglesCnt() const {
        uint32_t total_triangles_cnt = 0;
        for (auto item : vMeshData) {
                total_triangles_cnt += item.triangles_cnt;
        }
        return total_triangles_cnt;
}


template <class StoreStrategyList, class LoadStrategyList>
        void Mesh<StoreStrategyList, LoadStrategyList>::
                SetPos(const float x, const float y, const float z) {
        pos[0] = x;
        pos[1] = y;
        pos[2] = z;
}


template <class StoreStrategyList, class LoadStrategyList>
        void Mesh<StoreStrategyList, LoadStrategyList>::
                SetRotation(const float x, const float y, const float z) {
        rot[0] = x;
        rot[1] = y;
        rot[2] = z;
}


template <class StoreStrategyList, class LoadStrategyList>
        void Mesh<StoreStrategyList, LoadStrategyList>::
                DrawShape(const MeshData & oMeshData) const {

        if (oMeshData.buf_id < 1) {
               return;
        }

        glBindBuffer(GL_ARRAY_BUFFER, oMeshData.buf_id);

        if (!oMeshSettings.ext_material) {

                if (oMeshData.pTex != nullptr) {
                        glBindTexture(GL_TEXTURE_2D, oMeshData.pTex->GetID());
                }
                else {
                        glBindTexture(GL_TEXTURE_2D, 0);
                }

                glColor3f(1, 1, 1);

        }

        glVertexPointer(3, GL_FLOAT,   stride, (const void*)0);
        if (!oMeshSettings.skip_normals) {
                glNormalPointer(GL_FLOAT,      stride, (const void*)(sizeof(float) * 3));
                glTexCoordPointer(2, GL_FLOAT, stride, (const void*)(sizeof(float) * 6));
        }
        else {
                glTexCoordPointer(2, GL_FLOAT, stride, (const void*)(sizeof(float) * 3));
        }

        glDrawArrays(GL_TRIANGLES, 0, 3 * oMeshData.triangles_cnt);
}


template <class StoreStrategyList, class LoadStrategyList>
        void Mesh<StoreStrategyList, LoadStrategyList>::
                Draw() const {

        glEnableClientState(GL_VERTEX_ARRAY);
        if (!oMeshSettings.skip_normals) {
                glEnableClientState(GL_NORMAL_ARRAY);
        }
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);

        for (auto oMeshData : vMeshData) {
                DrawShape(oMeshData);
        }
}


template <class StoreStrategyList, class LoadStrategyList>
        void Mesh<StoreStrategyList, LoadStrategyList>::
                Draw(const size_t shape_ind) const {

        if (shape_ind >= vMeshData.size()) {
                return;
        }

        glEnableClientState(GL_VERTEX_ARRAY);
        if (!oMeshSettings.skip_normals) {
                glEnableClientState(GL_NORMAL_ARRAY);
        }
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);

        DrawShape(vMeshData[shape_ind]);
}


template <class StoreStrategyList, class LoadStrategyList>
        typename Mesh<StoreStrategyList, LoadStrategyList>::TShapesInfo Mesh<StoreStrategyList, LoadStrategyList>::
                GetShapesInfo() const {

        TShapesInfo vInfo;
        vInfo.reserve(vMeshData.size());
        for (uint32_t i = 0; i < vMeshData.size(); ++i) {
                vInfo.emplace_back(i, vMeshData[i].sName);
        }

        return vInfo;
}



template <class StoreStrategyList, class LoadStrategyList>
        glm::vec3 Mesh<StoreStrategyList, LoadStrategyList>::
                GetCenter(const size_t shape_ind) const {

        if (shape_ind >= vMeshData.size()) {
                log_w("wrong shape ind = {}, mesh rid = {}", shape_ind, rid);
                return glm::vec3();
        }

        return glm::vec3((vMeshData[shape_ind].max.x + vMeshData[shape_ind].min.x) / 2,
                         (vMeshData[shape_ind].max.y + vMeshData[shape_ind].min.y) / 2,
                         (vMeshData[shape_ind].max.z + vMeshData[shape_ind].min.z) / 2);
}


template <class StoreStrategyList, class LoadStrategyList>
        glm::vec3 Mesh<StoreStrategyList, LoadStrategyList>::
                GetCenter() const {

        return glm::vec3((max.x + min.x) / 2,
                         (max.y + min.y) / 2,
                         (max.z + min.z) / 2);
}


template <class StoreStrategyList, class LoadStrategyList>
        void Mesh<StoreStrategyList, LoadStrategyList>::
                DrawBBox(const size_t shape_ind) const {

        if (shape_ind >= vMeshData.size()) {
                log_w("wrong shape ind = {}, mesh rid = {}", shape_ind, rid);
                return;
        }

        const glm::vec3 & cur_min = vMeshData[shape_ind].min;
        const glm::vec3 & cur_max = vMeshData[shape_ind].max;

        HELPERS::DrawBBox(cur_min, cur_max);
}


template <class StoreStrategyList, class LoadStrategyList>
        void Mesh<StoreStrategyList, LoadStrategyList>::
                DrawBBox() const {

        HELPERS::DrawBBox(min, max);
}



template <class StoreStrategyList, class LoadStrategyList>
        std::tuple<const glm::vec3 &, const glm::vec3 &> Mesh<StoreStrategyList, LoadStrategyList>::
                GetBBox() const {
/*
        log_d("min x = {}, y = {}, z = {}, max x = {}, y = {}, z = {}",
                        min.x, min.y, min.z,
                        max.x, max.y, max.z);
        */
        //return std::make_tuple(std::cref(min), std::cref(max));
        //return std::tie(min, max);
        //return std::forward_as_tuple(min, max);
        return {min, max};
}


}



