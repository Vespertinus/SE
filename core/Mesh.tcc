
#include <fstream>
#include <GeometryUtil.h>

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

        Import(oStoreStrategySettings, oLoadStrategySettings);
}

template <class StoreStrategyList, class LoadStrategyList>
        Mesh<StoreStrategyList, LoadStrategyList>::Mesh(
                const std::string & sName,
                const rid_t new_rid,
                const MeshSettings & oNewMeshSettings) :
        ResourceHolder(new_rid, sName),
        oMeshCtx{},
        pTransform(nullptr),
        oMeshSettings(oNewMeshSettings) {

        boost::filesystem::path oPath(sName);
        std::string sExt = oPath.extension().string();
        std::transform(sExt.begin(), sExt.end(), sExt.begin(), ::tolower);

        if (sExt == ".sems") {
                log_d("file '{}' ext '{}', call FBLoader", sName, sExt);
                Load();
        }
        else {
                Import(typename TDefaultStoreStrategy::Settings(), typename TDefaultLoadStrategy::Settings());
        }
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
                Mesh<StoreStrategyList, LoadStrategyList>::Import(
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
                throw (std::runtime_error( "Mesh::Import: Loading failed, err_code = " + std::to_string(err_code)));
        }

        err_code = oStoreStrategy.Store(oMeshStock, oMeshCtx);
        if (err_code) {
                throw (std::runtime_error( "Mesh::Import: Storing failed, err_code = " + std::to_string(err_code)));
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


template <class StoreStrategyList, class LoadStrategyList>
        void Mesh<StoreStrategyList, LoadStrategyList>::
                Load() {

        static const size_t max_file_size = 1024 * 1024 * 10;

        auto file_size = boost::filesystem::file_size(sName);
        if (file_size > max_file_size) {
                throw(std::runtime_error(
                                        "too big file size, allowed max = " +
                                        std::to_string(max_file_size) +
                                        ", got " +
                                        std::to_string(file_size) +
                                        " bytes"));
        }

        std::vector<char> vBuffer(file_size);
        log_d("buffer size: {}", vBuffer.size());

        //TODO rewrite on os wrappers that in linux case call mmap
        {
                std::ifstream oInput(sName, std::ios::binary | std::ios::in);
                if(!oInput.is_open()) {
                        throw(std::runtime_error("failed to open file: " + sName));
                }
                oInput.read(&vBuffer[0], file_size);
        }
        flatbuffers::Verifier oVerifier(reinterpret_cast<uint8_t *>(&vBuffer[0]), file_size);
        if (SE::FlatBuffers::VerifyMeshBuffer(oVerifier) != true) {
                throw(std::runtime_error("failed to verify data in: " + sName));
        }

        Load(SE::FlatBuffers::GetMesh(&vBuffer[0]));
}

template <class StoreStrategyList, class LoadStrategyList>
        void Mesh<StoreStrategyList, LoadStrategyList>::
                Load(const SE::FlatBuffers::Mesh * pMesh) {

        auto * pShapesFB        = pMesh->shapes();
        size_t shapes_cnt       = pShapesFB->Length();
        auto * pMin             = pMesh->min();
        auto * pMax             = pMesh->max();


        oMeshCtx.stride         = ((pMesh->skip_normals()) ? VERTEX_BASE_SIZE : VERTEX_SIZE) * sizeof(float);
        oMeshCtx.min            = glm::vec3(pMin->x(), pMin->y(), pMin->z());
        oMeshCtx.max            = glm::vec3(pMax->x(), pMax->y(), pMax->z());
        oMeshCtx.skip_normals   = pMesh->skip_normals();

        log_d("mesh: shape cnt = {}, stride = {}, skip_normals = {}, min ({}, {}, {}), max({}, {}, {})",
                        shapes_cnt,
                        oMeshCtx.stride,
                        oMeshCtx.skip_normals,
                        oMeshCtx.min.x,
                        oMeshCtx.min.y,
                        oMeshCtx.min.z,
                        oMeshCtx.max.x,
                        oMeshCtx.max.y,
                        oMeshCtx.max.z);
        log_d("ext_material = {}", oMeshSettings.ext_material);

        for (size_t i = 0; i < shapes_cnt; ++i) {

                ShapeCtx oShape{};
                auto pCurShape          = pShapesFB->Get(i);

                oShape.triangles_cnt    = pCurShape->triangles_cnt();
                oShape.sName            = pCurShape->name()->c_str();
                auto * pMin             = pCurShape->min();
                oShape.min              = glm::vec3(pMin->x(), pMin->y(), pMin->z());
                auto * pMax             = pCurShape->max();
                oShape.max              = glm::vec3(pMax->x(), pMax->y(), pMax->z());

                std::string sTexPath    = pCurShape->texture()->c_str();
                if (!sTexPath.empty() && !oMeshSettings.ext_material) {
                        //TODO tex settings ?
                        oShape.pTex     = CreateResource<SE::TTexture>(sTexPath);
                }


                auto * pVertices        = pCurShape->vertices();

                glGenBuffers(1, &oShape.buf_id);
                glBindBuffer(GL_ARRAY_BUFFER, oShape.buf_id);
                glBufferData(GL_ARRAY_BUFFER,
                             pVertices->Length() * sizeof(float),
                             pVertices->Data(),
                             GL_STATIC_DRAW);

                log_d("shape[{}] name = '{}', triangles cnt = {}, buf_id = {}, texture id = {}, min x = {}, y = {}, z = {}, max x = {}, y = {}, z = {}",
                                i,
                                oShape.sName,
                                oShape.triangles_cnt,
                                oShape.buf_id,
                                (oShape.pTex) ? oShape.pTex->GetID() : 0,
                                oShape.min.x, oShape.min.y, oShape.min.z,
                                oShape.max.x, oShape.max.y, oShape.max.z);

                oMeshCtx.vShapes.emplace_back(std::move(oShape));
        }

}

}



