
#include <fstream>
#include <GeometryEntity.h>

namespace SE  {


Mesh::Mesh(
                const std::string & sName,
                const rid_t new_rid,
                const SE::FlatBuffers::Mesh * pMesh) :
        ResourceHolder(new_rid, sName),
        vao_id(0) {

        Load(pMesh);
}

Mesh::Mesh(
                const std::string & sName,
                const rid_t new_rid) :
        ResourceHolder(new_rid, sName),
        vao_id(0) {

        Load();
}

Mesh::~Mesh() noexcept {

        if (vao_id) {
                glDeleteVertexArrays(1, &vao_id);
        }
}

uint32_t Mesh::GetShapesCnt() const {
        return vSubMeshes.size();
}

uint32_t Mesh::GetIndicesCnt() const {

        uint32_t total_cnt = 0;
        for (auto & item : vSubMeshes) {
                total_cnt += item.GetIndicesCnt();
        }
        return total_cnt;
}

const std::vector<GeometryEntity> & Mesh::GetShapes() const {

        return vSubMeshes;
}

const std::vector<BoundingBox> & Mesh::GetBBoxes() const {
        return vBBoxes;
}


glm::vec3 Mesh::GetCenter(const size_t shape_ind) const {

        if (shape_ind >= vSubMeshes.size()) {
                log_w("wrong shape ind = {}, mesh rid = {}", shape_ind, rid);
                return glm::vec3();
        }

        return vBBoxes[shape_ind].Center();
}


glm::vec3 Mesh::GetCenter() const {

        return vBBoxes.back().Center();
}


const BoundingBox & Mesh::GetBBox() const {

        return vBBoxes.back();
}


void Mesh::Load() {

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


void Mesh::Load(const SE::FlatBuffers::Mesh * pMesh) {

        uint32_t                index_buf_id;
        /** |gl_type: 0 --> buffers_cnt| gl_id: buffers_cnt --> buffers_cnt * 2| stride: buffers_cnt * 2 --> buffers_cnt * 3 */
        std::vector<uint32_t> vBuffers;
        uint32_t buffers_cnt    = 0;
        uint32_t * pGLTypes     = nullptr;
        uint32_t * pGLID        = nullptr;
        uint32_t * pStride      = nullptr;

        auto LocalClean = [&index_buf_id, &pGLID, &buffers_cnt](uint32_t cur_vao_id) {

                glBindVertexArray(0);

                if (index_buf_id) {
                        glDeleteBuffers(1, &index_buf_id);

                }

                if (buffers_cnt) {
                        glDeleteBuffers(buffers_cnt, &pGLID[0]);
                }

                if (cur_vao_id) {
                        glDeleteVertexArrays(1, &cur_vao_id);
                }
        };

        index_buf_id                    = 0;
        uint32_t index_size             = 0;
        uint32_t index_type_size        = 0;
        uint32_t se_index_type;
        const void * index_data         = nullptr;
        SE::FlatBuffers::IndexBufferU index_type = pMesh->index()->buf_type();

        switch (index_type) {

                case SE::FlatBuffers::IndexBufferU::Uint8Vector:
                        {
                                auto pIndexVec          = pMesh->index()->buf_as_Uint8Vector()->data();
                                index_size              = pIndexVec->Length();
                                index_type_size         = sizeof(uint8_t);
                                index_data              = pIndexVec->Data();
                                se_index_type           = VertexIndexType::BYTE;
                        }
                        break;
                case SE::FlatBuffers::IndexBufferU::Uint16Vector:
                        {
                                auto pIndexVec          = pMesh->index()->buf_as_Uint16Vector()->data();
                                index_size              = pIndexVec->Length();
                                index_type_size         = sizeof(uint16_t);
                                index_data              = pIndexVec->Data();
                                se_index_type           = VertexIndexType::SHORT;
                        }
                        break;
                case SE::FlatBuffers::IndexBufferU::Uint32Vector:
                        {
                                auto pIndexVec          = pMesh->index()->buf_as_Uint32Vector()->data();
                                index_size              = pIndexVec->Length();
                                index_type_size         = sizeof(uint32_t);
                                index_data              = pIndexVec->Data();
                                se_index_type           = VertexIndexType::INT;
                        }
                        break;
                default:
                        LocalClean(vao_id);
                        throw(std::runtime_error("unknown IndexBuffer type: " +
                                                std::to_string(static_cast<uint8_t>(index_type)) +
                                                ", file: " +
                                                sName));
        }

        glGenVertexArrays(1, &vao_id);
        glBindVertexArray(vao_id);

        glGenBuffers(1, &index_buf_id);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buf_id);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                        index_size * index_type_size,
                        index_data,
                        GL_STATIC_DRAW);

        log_d("index size = {}, type size = {}", index_size, index_type_size);

        buffers_cnt    = pMesh->vertices()->Length();

        if (buffers_cnt == 0) {

                LocalClean(vao_id);
                throw(std::runtime_error("empty vertex buffers"));
        }


        vBuffers.resize(buffers_cnt * 3, 0);
        pGLTypes = &vBuffers[0];
        pGLID    = &vBuffers[buffers_cnt];
        pStride  = &vBuffers[buffers_cnt * 2];

        glGenBuffers(buffers_cnt, &pGLID[0]);

        uint32_t buffer_size             = 0;
        uint32_t buffer_type_size        = 0;
        const void * buffer_data         = nullptr;

        for (uint32_t i = 0; i < buffers_cnt; ++i) {
                auto pBufferFB   = pMesh->vertices()->Get(i);
                auto buffer_type = static_cast<SE::FlatBuffers::VertexBufferU>(pBufferFB->buf_type());
                pStride[i] = pBufferFB->stride();

                switch (buffer_type) {
                        case SE::FlatBuffers::VertexBufferU::FloatVector:
                                {
                                        auto pBufferVec  = pBufferFB->buf_as_FloatVector()->data();
                                        buffer_size      = pBufferVec->Length();
                                        buffer_type_size = sizeof(float);
                                        buffer_data      = pBufferVec->Data();
                                        pGLTypes[i]      = GL_FLOAT;
                                }
                                break;
                        case SE::FlatBuffers::VertexBufferU::ByteVector:
                                {
                                        auto pBufferVec  = pBufferFB->buf_as_ByteVector()->data();
                                        buffer_size      = pBufferVec->Length();
                                        buffer_type_size = sizeof(uint8_t);
                                        buffer_data      = pBufferVec->Data();
                                        pGLTypes[i]      = GL_UNSIGNED_BYTE;
                                }
                                break;

                        case SE::FlatBuffers::VertexBufferU::Uint32Vector:
                                {
                                        auto pBufferVec  = pBufferFB->buf_as_Uint32Vector()->data();
                                        buffer_size      = pBufferVec->Length();
                                        buffer_type_size = sizeof(uint32_t);
                                        buffer_data      = pBufferVec->Data();
                                        pGLTypes[i]      = GL_UNSIGNED_INT;
                                }
                                break;
                        default:
                                LocalClean(vao_id);
                                throw(std::runtime_error("unknown VertexBuffer type: " +
                                                        std::to_string(static_cast<uint8_t>(buffer_type)) ));
                }

                glBindBuffer(GL_ARRAY_BUFFER, pGLID[i]);
                glBufferData(GL_ARRAY_BUFFER,
                                buffer_size * buffer_type_size,
                                buffer_data,
                                GL_STATIC_DRAW);

                log_d("buffer[{}] size = {}, type size = {}", i, buffer_size, buffer_type_size);
        }

        uint32_t last_binded_buffer = buffers_cnt - 1;
        auto pAttributes = pMesh->attributes();

        for (uint32_t i = 0; i < pAttributes->Length(); ++i) {

                auto pCurAttrubute = pAttributes->Get(i);
                auto itLocation = mAttributeLocation.find(pCurAttrubute->name()->c_str());
                if (itLocation == mAttributeLocation.end()) {
                        LocalClean(vao_id);
                        throw(std::runtime_error("unknown vertex attribute name: '" +
                                                pCurAttrubute->name()->str() + "'" ));
                }
                std::uintptr_t offset = pCurAttrubute->offset();

                if (pCurAttrubute->buffer_ind() != last_binded_buffer) {
                        last_binded_buffer = pCurAttrubute->buffer_ind();
                        glBindBuffer(GL_ARRAY_BUFFER, pGLID[last_binded_buffer]);
                }

                if (pCurAttrubute->destination() == SE::FlatBuffers::AttribDestType::DEST_FLOAT) {
                        /*
                           log_d("elem_size = {}, gl_type = {}, stride = {}, offset = {}",
                           pCurAttrubute->elem_size(),
                           vBuffersGLType[pCurAttrubute->buffer_ind()],
                           pCurShape->stride(),
                           offset);
                         */
                        glVertexAttribPointer(
                                        itLocation->second,
                                        pCurAttrubute->elem_size(),
                                        pGLTypes[pCurAttrubute->buffer_ind()],
                                        false,
                                        pStride[pCurAttrubute->buffer_ind()],
                                        (const void *)offset);
                }
                else if (pCurAttrubute->destination() == SE::FlatBuffers::AttribDestType::DEST_INT) {
                        glVertexAttribIPointer(
                                        itLocation->second,
                                        pCurAttrubute->elem_size(),
                                        pGLTypes[pCurAttrubute->buffer_ind()],
                                        pStride[pCurAttrubute->buffer_ind()],
                                        (const void *)offset);

                }
                else {
                        LocalClean(vao_id);
                        throw(std::runtime_error(fmt::format("wrong destination: '{}' in vertex attribute name: '",
                                                        static_cast<uint8_t>(pCurAttrubute->destination()),
                                                        pCurAttrubute->name()->str()) ));
                }

                glEnableVertexAttribArray(itLocation->second);

                vAttrInfo.emplace_back(pCurAttrubute->name()->c_str(), pCurAttrubute->custom());
        }

        glBindVertexArray(0);
        glDeleteBuffers(1, &index_buf_id);
        glDeleteBuffers(buffers_cnt, &pGLID[0]);

        //___Start___ fill sub meshes
        auto                  * pShapesFB        = pMesh->shapes();
        size_t                  shapes_cnt       = pShapesFB->Length();
        uint32_t                primitive_type;

        //TODO rewrite
        switch (pMesh->primitive_type()) {

                case FlatBuffers::PrimitiveType::GEOM_TRIANGLES:
                        primitive_type = GL_TRIANGLES;
                        break;
                case FlatBuffers::PrimitiveType::GEOM_LINES:
                        primitive_type = GL_LINES;
                        break;
                case FlatBuffers::PrimitiveType::GEOM_POINTS:
                        primitive_type = GL_POINTS;
                        break;
                default:
                        LocalClean(vao_id);
                        throw(std::runtime_error(
                                                fmt::format("unknown primitive_type: {}, mesh: '{}'",
                                                        FlatBuffers::EnumNamePrimitiveType(pMesh->primitive_type()),
                                                        sName)
                                                ));
        };

        for (size_t shape_num = 0; shape_num < shapes_cnt; ++shape_num) {

                auto pCurShape = pShapesFB->Get(shape_num);

                vSubMeshes.emplace_back(
                                vao_id,
                                pCurShape->index_elem_count(),
                                pCurShape->index_elem_start(),
                                se_index_type,
                                primitive_type );

                vBBoxes.emplace_back(
                                *reinterpret_cast<const glm::vec3 *>(&pCurShape->bbox()->min()),
                                *reinterpret_cast<const glm::vec3 *>(&pCurShape->bbox()->max())
                                );
        }

        /** if we have more than one sub mesh, we need to store concatenated bounding box for mesh */
        if (shapes_cnt > 1) {
                vBBoxes.emplace_back(
                        *reinterpret_cast<const glm::vec3 *>(&pMesh->bbox()->min()),
                        *reinterpret_cast<const glm::vec3 *>(&pMesh->bbox()->max())
                                );
        }
        //___End_____ fill sub meshes

        const BoundingBox & oMeshBBox = vBBoxes.back();
        log_d("shapes cnt = {}, triangles cnt = {}, vao_id = {}, buffers cnt = {}, vert attributes cnt = {}, min ({}, {}, {}), max({}, {}, {})",
                        shapes_cnt,
                        index_size / 3,
                        vao_id,
                        buffers_cnt,
                        pAttributes->Length(),
                        oMeshBBox.Min().x,
                        oMeshBBox.Min().y,
                        oMeshBBox.Min().z,
                        oMeshBBox.Max().x,
                        oMeshBBox.Max().y,
                        oMeshBBox.Max().z
                        );

}

std::string Mesh::Str() const {

        return fmt::format("Mesh name: '{}', triangles: {}, shapes: {}, bbox: {}", sName, GetIndicesCnt() / 3, vSubMeshes.size(), GetBBox().Str());
}


const std::vector <std::pair<StrID, uint32_t>> & Mesh::GetAttrInfo() const {

        return vAttrInfo;
}

}


