
namespace SE  {

template <class StoreStrategyList, class LoadStrategyList>
        template <class TStoreStrategySettings,  class TLoadStrategySettings>
                Mesh<StoreStrategyList, LoadStrategyList>::Mesh(
                        const std::string & sName,
                        const rid_t new_rid,
                        const TStoreStrategySettings & oStoreStrategySettings,
                        const TLoadStrategySettings & oLoadStrategySettings,
                        const MeshSettings & oNewMeshSettings) :
                ResourceHolder(new_rid, sName),
                oMeshCtx{},
                pTransform(nullptr),
                oMeshSettings(oNewMeshSettings) {

        Create(oStoreStrategySettings, oLoadStrategySettings);
}

template <class StoreStrategyList, class LoadStrategyList>
        Mesh<StoreStrategyList, LoadStrategyList>::Mesh(
                const std::string & sName,
                const rid_t new_rid,
                const MeshSettings & oNewMeshSettings) :
                        Mesh(sName,
                             rid,
                             typename TDefaultStoreStrategy::Settings(),
                             typename TDefaultLoadStrategy::Settings(),
                             oNewMeshSettings
                             ) {
}

template <class StoreStrategyList, class LoadStrategyList>
        template <class TConcreateSettings, std::enable_if_t< MP::InnerContain<StoreStrategyList, TConcreateSettings>::value, TConcreateSettings> * >
                Mesh<StoreStrategyList, LoadStrategyList>::Mesh(
                        const std::string & sName,
                        const rid_t new_rid,
                        const TConcreateSettings & oSettings,
                        const MeshSettings & oNewMeshSettings) :
                                Mesh(sName,
                                     rid,
                                     oSettings,
                                     typename TDefaultLoadStrategy::Settings(),
                                     oNewMeshSettings) {
}

template <class StoreStrategyList, class LoadStrategyList>
        template <class TConcreateSettings, std::enable_if_t< MP::InnerContain<LoadStrategyList, TConcreateSettings>::value, TConcreateSettings> * >
                Mesh<StoreStrategyList, LoadStrategyList>::Mesh(
                        const std::string & sName,
                        const rid_t new_rid,
                        const TConcreateSettings & oSettings,
                        const MeshSettings & oNewMeshSettings) :
                                Mesh(sName,
                                     rid,
                                     typename TDefaultStoreStrategy::Settings(),
                                     oSettings,
                                     oNewMeshSettings) {
}

template <class StoreStrategyList, class LoadStrategyList> Mesh<StoreStrategyList, LoadStrategyList>::~Mesh() throw() { ;; }



template <class StoreStrategyList, class LoadStrategyList>
        template <class TStoreStrategySettings,  class TLoadStrategySettings> void
                Mesh<StoreStrategyList, LoadStrategyList>::Create(
                                const TStoreStrategySettings & oStoreStrategySettings,
                                const TLoadStrategySettings & oLoadStrategySettings) {

        typedef typename MP::InnerSearch<StoreStrategyList, TStoreStrategySettings>::Result TStoreStrategy;
        typedef typename MP::InnerSearch<LoadStrategyList,  TLoadStrategySettings >::Result TLoadStrategy;

        MeshStock    oMeshStock(oMeshSettings);
        ret_code_t   err_code;

        TLoadStrategy   oLoadStrategy(oLoadStrategySettings);
        TStoreStrategy  oStoreStrategy(oStoreStrategySettings);

        log_d("ext_material = {}", oMeshSettings.ext_material);

        err_code = oLoadStrategy.Load(sName, oMeshStock);
        if (err_code) {
                char buf[256];
                snprintf(buf, sizeof(buf), "Mesh::Create: Loading failed, err_code = %u", err_code);
                log_e("{}", buf);
                throw (std::runtime_error(buf));
        }

        err_code = oStoreStrategy.Store(oMeshStock, oMeshCtx);
        if (err_code) {
                char buf[256];
                snprintf(buf, sizeof(buf), "Mesh::Create: Storing failed, err_code = %u\n", err_code);
                log_e("{}", buf);
                throw (std::runtime_error(buf));
        }

}

template <class StoreStrategyList, class LoadStrategyList> uint32_t Mesh<StoreStrategyList, LoadStrategyList>::GetShapesCnt() const {
        return oMeshCtx.vShapes.size();
}

template <class StoreStrategyList, class LoadStrategyList> uint32_t Mesh<StoreStrategyList, LoadStrategyList>::GetTrianglesCnt() const {
        uint32_t total_triangles_cnt = 0;
        for (auto item : oMeshCtx.vShapes) {
                total_triangles_cnt += item.triangles_cnt;
        }
        return total_triangles_cnt;
}


template <class StoreStrategyList, class LoadStrategyList>
        void Mesh<StoreStrategyList, LoadStrategyList>::
                SetTransform(Transform const * const pNewTransform) {

        pTransform = pNewTransform;
}

template <class StoreStrategyList, class LoadStrategyList>
        void Mesh<StoreStrategyList, LoadStrategyList>::
                DrawShape(const ShapeCtx & oShapeCtx) const {

        if (oShapeCtx.buf_id < 1) {
               return;
        }

        glBindBuffer(GL_ARRAY_BUFFER, oShapeCtx.buf_id);

        if (!oMeshSettings.ext_material) {

                if (oShapeCtx.pTex != nullptr) {
                        glBindTexture(GL_TEXTURE_2D, oShapeCtx.pTex->GetID());
                }
                else {
                        glBindTexture(GL_TEXTURE_2D, 0);
                }

                glColor3f(1, 1, 1);

        }

        glVertexPointer(3, GL_FLOAT,   oMeshCtx.stride, (const void*)0);
        if (!oMeshCtx.skip_normals) {
                glNormalPointer(GL_FLOAT,      oMeshCtx.stride, (const void*)(sizeof(float) * 3));
                glTexCoordPointer(2, GL_FLOAT, oMeshCtx.stride, (const void*)(sizeof(float) * 6));
        }
        else {
                glTexCoordPointer(2, GL_FLOAT, oMeshCtx.stride, (const void*)(sizeof(float) * 3));
        }

        glDrawArrays(GL_TRIANGLES, 0, 3 * oShapeCtx.triangles_cnt);
}


template <class StoreStrategyList, class LoadStrategyList>
        void Mesh<StoreStrategyList, LoadStrategyList>::
                Draw() const {

        glEnableClientState(GL_VERTEX_ARRAY);
        if (!oMeshCtx.skip_normals) {
                glEnableClientState(GL_NORMAL_ARRAY);
        }
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);

        if (pTransform) {
                const auto & world_mat = pTransform->GetWorld();
                glPushMatrix();
                glMultMatrixf(glm::value_ptr(world_mat));
        }

        for (auto & oShapeCtx : oMeshCtx.vShapes) {
                DrawShape(oShapeCtx);
        }

        if (pTransform) {
                glPopMatrix();
        }

}


template <class StoreStrategyList, class LoadStrategyList>
        void Mesh<StoreStrategyList, LoadStrategyList>::
                Draw(const size_t shape_ind) const {

        if (shape_ind >= oMeshCtx.vShapes.size()) {
                return;
        }

        glEnableClientState(GL_VERTEX_ARRAY);
        if (!oMeshCtx.skip_normals) {
                glEnableClientState(GL_NORMAL_ARRAY);
        }
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);

        DrawShape(oMeshCtx.vShapes[shape_ind]);
}


template <class StoreStrategyList, class LoadStrategyList>
        typename Mesh<StoreStrategyList, LoadStrategyList>::TShapesInfo Mesh<StoreStrategyList, LoadStrategyList>::
                GetShapesInfo() const {

        TShapesInfo vInfo;
        vInfo.reserve(oMeshCtx.vShapes.size());
        for (uint32_t i = 0; i < oMeshCtx.vShapes.size(); ++i) {
                vInfo.emplace_back(i, oMeshCtx.vShapes[i].sName);
        }

        return vInfo;
}



template <class StoreStrategyList, class LoadStrategyList>
        glm::vec3 Mesh<StoreStrategyList, LoadStrategyList>::
                GetCenter(const size_t shape_ind) const {
//TODO use transform
        if (shape_ind >= oMeshCtx.vShapes.size()) {
                log_w("wrong shape ind = {}, mesh rid = {}", shape_ind, rid);
                return glm::vec3();
        }

        return glm::vec3((oMeshCtx.vShapes[shape_ind].max.x + oMeshCtx.vShapes[shape_ind].min.x) / 2,
                         (oMeshCtx.vShapes[shape_ind].max.y + oMeshCtx.vShapes[shape_ind].min.y) / 2,
                         (oMeshCtx.vShapes[shape_ind].max.z + oMeshCtx.vShapes[shape_ind].min.z) / 2);
}


template <class StoreStrategyList, class LoadStrategyList>
        glm::vec3 Mesh<StoreStrategyList, LoadStrategyList>::
                GetCenter() const {
//TODO use transform
        return glm::vec3((oMeshCtx.max.x + oMeshCtx.min.x) / 2,
                         (oMeshCtx.max.y + oMeshCtx.min.y) / 2,
                         (oMeshCtx.max.z + oMeshCtx.min.z) / 2);
}


template <class StoreStrategyList, class LoadStrategyList>
        void Mesh<StoreStrategyList, LoadStrategyList>::
                DrawBBox(const size_t shape_ind) const {

        if (shape_ind >= oMeshCtx.vShapes.size()) {
                log_w("wrong shape ind = {}, mesh rid = {}", shape_ind, rid);
                return;
        }

        const glm::vec3 & cur_min = oMeshCtx.vShapes[shape_ind].min;
        const glm::vec3 & cur_max = oMeshCtx.vShapes[shape_ind].max;

        HELPERS::DrawBBox(cur_min, cur_max);
}


template <class StoreStrategyList, class LoadStrategyList>
        void Mesh<StoreStrategyList, LoadStrategyList>::
                DrawBBox() const {

        HELPERS::DrawBBox(oMeshCtx.min, oMeshCtx.max);
}



template <class StoreStrategyList, class LoadStrategyList>
        std::tuple<const glm::vec3 &, const glm::vec3 &> Mesh<StoreStrategyList, LoadStrategyList>::
                GetBBox() const {

        return { oMeshCtx.min, oMeshCtx.max };
}


}



