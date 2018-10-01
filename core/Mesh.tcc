
#include <fstream>
#include <GeometryUtil.h>

namespace SE  {


Mesh::Mesh(
                const std::string & sName,
                const rid_t new_rid,
                const SE::FlatBuffers::Mesh * pMesh,
                const MeshSettings & oNewMeshSettings) :
        ResourceHolder(new_rid, sName),
        oMeshCtx{},
        oMeshSettings(oNewMeshSettings) {

        Load(pMesh);
}

Mesh::Mesh(
                const std::string & sName,
                const rid_t new_rid,
                const MeshSettings & oNewMeshSettings) :
        ResourceHolder(new_rid, sName),
        oMeshCtx{},
        oMeshSettings(oNewMeshSettings) {

        Load();
}

Mesh::~Mesh() noexcept {

        Clean();
}

uint32_t Mesh::GetShapesCnt() const {
        return oMeshCtx.vShapes.size();
}

uint32_t Mesh::GetTrianglesCnt() const {

        uint32_t total_triangles_cnt = 0;
        for (auto item : oMeshCtx.vShapes) {
                total_triangles_cnt += item.triangles_cnt;
        }
        return total_triangles_cnt;
}


void Mesh::DrawShape(const ShapeCtx & oShapeCtx) const {

        if (!oMeshSettings.ext_material) {
                TRenderState::Instance().SetShaderProgram(oShapeCtx.pShader);

                //TODO move into material manipulation
                if (oShapeCtx.pTex != nullptr) {
                        //glBindTexture(GL_TEXTURE_2D, oShapeCtx.pTex->GetID());
                        TRenderState::Instance().SetTexture(SE::TextureUnit::DIFFUSE, oShapeCtx.pTex);
                }
        }

        TRenderState::Instance().Draw(oShapeCtx.vao_id, oShapeCtx.triangles_cnt * 3, oShapeCtx.gl_index_type);
}



void Mesh::Draw() const {

        for (auto & oShapeCtx : oMeshCtx.vShapes) {
                DrawShape(oShapeCtx);
        }
}



void Mesh::Draw(const size_t shape_ind) const {

        if (shape_ind >= oMeshCtx.vShapes.size()) {
                return;
        }

        DrawShape(oMeshCtx.vShapes[shape_ind]);
}

typename Mesh::TShapesInfo Mesh::GetShapesInfo() const {

        TShapesInfo vInfo;
        vInfo.reserve(oMeshCtx.vShapes.size());
        for (uint32_t i = 0; i < oMeshCtx.vShapes.size(); ++i) {
                vInfo.emplace_back(i, oMeshCtx.vShapes[i].sName);
        }

        return vInfo;
}


glm::vec3 Mesh::GetCenter(const size_t shape_ind) const {

        if (shape_ind >= oMeshCtx.vShapes.size()) {
                log_w("wrong shape ind = {}, mesh rid = {}", shape_ind, rid);
                return glm::vec3();
        }

        return oMeshCtx.vShapes[shape_ind].oBBox.Center();
}


glm::vec3 Mesh::GetCenter() const {

        return oMeshCtx.oBBox.Center();
}


const BoundingBox & Mesh::GetBBox() const {

        return oMeshCtx.oBBox;
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

        auto                  * pShapesFB        = pMesh->shapes();
        size_t                  shapes_cnt       = pShapesFB->Length();

        uint32_t                index_buf_id;
        std::vector<uint32_t>   vBuffersGLType;
        std::vector<uint32_t>   vBuffersID;

        oMeshCtx.oBBox = BoundingBox(
                        *reinterpret_cast<const glm::vec3 *>(&pMesh->bbox()->min()),
                        *reinterpret_cast<const glm::vec3 *>(&pMesh->bbox()->max())
                        );

        log_d("mesh: shape cnt = {}, min ({}, {}, {}), max({}, {}, {})",
                        shapes_cnt,
                        oMeshCtx.oBBox.Min().x,
                        oMeshCtx.oBBox.Min().y,
                        oMeshCtx.oBBox.Min().z,
                        oMeshCtx.oBBox.Max().x,
                        oMeshCtx.oBBox.Max().y,
                        oMeshCtx.oBBox.Max().z);
        log_d("ext_material = {}", oMeshSettings.ext_material);

        auto LocalClean = [&index_buf_id, &vBuffersID](uint32_t cur_vao_id) {

                glBindVertexArray(0);

                if (index_buf_id) {
                        glDeleteBuffers(1, &index_buf_id);
                }
                if (vBuffersID.size()) {
                        glDeleteBuffers(vBuffersID.size(), &vBuffersID[0]);
                }

                if (cur_vao_id) {
                        glDeleteVertexArrays(1, &cur_vao_id);
                }
        };

        for (size_t shape_num = 0; shape_num < shapes_cnt; ++shape_num) {

                ShapeCtx oShape{};
                auto pCurShape          = pShapesFB->Get(shape_num);
                auto * pNameFB          = pCurShape->name();

                oShape.sName            = (pNameFB != nullptr) ? pNameFB->c_str() : "";
                oShape.triangles_cnt    = pCurShape->triangles_cnt();
                oShape.oBBox = BoundingBox(
                                *reinterpret_cast<const glm::vec3 *>(&pCurShape->bbox()->min()),
                                *reinterpret_cast<const glm::vec3 *>(&pCurShape->bbox()->max())
                                );

                auto * pTexNameFB       = pCurShape->texture();
                std::string sTexPath    = (pTexNameFB != nullptr) ? pTexNameFB->c_str() : "";

                index_buf_id            = 0;
                vBuffersID.clear();
                vBuffersGLType.clear();

                // ___Start___ FIXME basic material manipulation, move into separate class
                if (!oMeshSettings.ext_material) {

                        if (!sTexPath.empty()) {
                                oShape.pTex     = CreateResource<SE::TTexture>(sTexPath);
                                oShape.pShader  = CreateResource<SE::ShaderProgram>(
                                                "resource/shader_program/simple_tex.sesp",
                                                SE::ShaderProgram::Settings{"resource/shader/"}
                                                );
                        }
                        else {
                                oShape.pShader  = CreateResource<SE::ShaderProgram>(
                                                "resource/shader_program/wireframe.sesp",
                                                //"resource/shader_program/simple.sesp",
                                                SE::ShaderProgram::Settings{"resource/shader/"}
                                                );
                        }
                }
                // ___End_____ FIXME basic material manipulation, move into separate class

                uint32_t index_size             = 0;
                uint32_t index_type_size        = 0;
                const void * index_data         = nullptr;
                SE::FlatBuffers::IndexBuffer index_type = pCurShape->index_type();

                switch (index_type) {

                        case SE::FlatBuffers::IndexBuffer::Uint8Vector:
                                {
                                        auto pIndexVec          = pCurShape->index_as_Uint8Vector()->data();
                                        index_size              = pIndexVec->Length();
                                        index_type_size         = sizeof(uint8_t);
                                        index_data              = pIndexVec->Data();
                                        oShape.gl_index_type    = GL_UNSIGNED_BYTE;
                                }
                                break;
                        case SE::FlatBuffers::IndexBuffer::Uint16Vector:
                                {
                                        auto pIndexVec          = pCurShape->index_as_Uint16Vector()->data();
                                        index_size              = pIndexVec->Length();
                                        index_type_size         = sizeof(uint16_t);
                                        index_data              = pIndexVec->Data();
                                        oShape.gl_index_type    = GL_UNSIGNED_SHORT;
                                }
                                break;
                        case SE::FlatBuffers::IndexBuffer::Uint32Vector:
                                {
                                        auto pIndexVec          = pCurShape->index_as_Uint32Vector()->data();
                                        index_size              = pIndexVec->Length();
                                        index_type_size         = sizeof(uint32_t);
                                        index_data              = pIndexVec->Data();
                                        oShape.gl_index_type    = GL_UNSIGNED_INT;
                                }
                                break;
                        default:
                                LocalClean(oShape.vao_id);
                                Clean();
                                throw(std::runtime_error("unknown IndexBuffer type: " +
                                                        std::to_string(static_cast<uint8_t>(index_type)) +
                                                        ", file: " +
                                                        sName));
                }

                glGenVertexArrays(1, &oShape.vao_id);
                glBindVertexArray(oShape.vao_id);

                glGenBuffers(1, &index_buf_id);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buf_id);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                             index_size * index_type_size,
                             index_data,
                             GL_STATIC_DRAW);

                log_d("index size = {}, type size = {}", index_size, index_type_size);

                auto pVertices          = pCurShape->vertices();
                auto pVerticesTypes     = pCurShape->vertices_type();
                uint32_t buffers_cnt    = pVertices->Length();

                if ((buffers_cnt != pVerticesTypes->Length()) || (buffers_cnt == 0)) {

                        LocalClean(oShape.vao_id);
                        Clean();
                        throw(std::runtime_error("uneven vectors size, or empty vertex buffers, vertices size: " +
                                                std::to_string(buffers_cnt) +
                                                ", vertices types size: " +
                                                 std::to_string(pVerticesTypes->Length()) ));
                }

                vBuffersGLType.resize(buffers_cnt, 0);
                vBuffersID.resize(buffers_cnt, 0);
                glGenBuffers(buffers_cnt, &vBuffersID[0]);

                uint32_t buffer_size             = 0;
                uint32_t buffer_type_size        = 0;
                const void * buffer_data         = nullptr;

                for (uint32_t i = 0; i < buffers_cnt; ++i) {
                        auto buffer_type = static_cast<SE::FlatBuffers::VertexBuffer>(pVerticesTypes->Get(i));
                        switch (buffer_type) {
                                case SE::FlatBuffers::VertexBuffer::FloatVector:
                                        {
                                        auto pBufferVec  = static_cast<const SE::FlatBuffers::FloatVector *>(pVertices->Get(i))->data();
                                        buffer_size      = pBufferVec->Length();
                                        buffer_type_size = sizeof(float);
                                        buffer_data      = pBufferVec->Data();
                                        vBuffersGLType[i] = GL_FLOAT;
                                        }
                                        break;
                                case SE::FlatBuffers::VertexBuffer::ByteVector:
                                        {
                                        auto pBufferVec  = static_cast<const SE::FlatBuffers::ByteVector *>(pVertices->Get(i))->data();
                                        buffer_size      = pBufferVec->Length();
                                        buffer_type_size = sizeof(uint8_t);
                                        buffer_data      = pBufferVec->Data();
                                        vBuffersGLType[i] = GL_UNSIGNED_BYTE;
                                        }
                                        break;

                                case SE::FlatBuffers::VertexBuffer::Uint32Vector:
                                        {
                                        auto pBufferVec  = static_cast<const SE::FlatBuffers::Uint32Vector *>(pVertices->Get(i))->data();
                                        buffer_size      = pBufferVec->Length();
                                        buffer_type_size = sizeof(uint32_t);
                                        buffer_data      = pBufferVec->Data();
                                        vBuffersGLType[i] = GL_UNSIGNED_INT;
                                        }
                                        break;
                                default:
                                        LocalClean(oShape.vao_id);
                                        Clean();
                                        throw(std::runtime_error("unknown VertexBuffer type: " +
                                                                std::to_string(static_cast<uint8_t>(buffer_type)) ));
                        }

                        /*
                        if (buffer_size != index_size) {
                                LocalClean(oShape.vao_id);
                                Clean();
                                throw(std::runtime_error("buffer[" +
                                                        std::to_string(i) +
                                                        "] size (" +
                                                        std::to_string(buffer_size) +
                                                        ") mismatch index size (" +
                                                        std::to_string(index_size) +
                                                        ")" ));
                        }
                        */

                        glBindBuffer(GL_ARRAY_BUFFER, vBuffersID[i]);
                        glBufferData(GL_ARRAY_BUFFER,
                                        buffer_size * buffer_type_size,
                                        buffer_data,
                                        GL_STATIC_DRAW);

                        log_d("buffer[{}] size = {}, type size = {}", i, buffer_size, buffer_type_size);
                }

                auto pAttributes = pCurShape->attributes();

                for (uint32_t i = 0; i < pAttributes->Length(); ++i) {

                        auto pCurAttrubute = pAttributes->Get(i);
                        auto itLocation = mAttributeLocation.find(pCurAttrubute->name()->c_str());
                        if (itLocation == mAttributeLocation.end()) {
                                LocalClean(oShape.vao_id);
                                Clean();
                                throw(std::runtime_error("unknown vertex attribute name: '" +
                                                        pCurAttrubute->name()->str() +
                                                        "', shape num = " +
                                                        std::to_string(shape_num) ));
                        }
                        std::uintptr_t offset = pCurAttrubute->offset();
                        /*
                        log_d("elem_size = {}, gl_type = {}, stride = {}, offset = {}",
                                        pCurAttrubute->elem_size(),
                                        vBuffersGLType[pCurAttrubute->buffer_ind()],
                                        pCurShape->stride(),
                                        offset);
                        */
                        glVertexAttribPointer(itLocation->second,
                                        pCurAttrubute->elem_size(),
                                        vBuffersGLType[pCurAttrubute->buffer_ind()],
                                        false,
                                        pCurShape->stride(), //FIXME move to buffer info after switching serialization from flatbuffers union to table
                                        (const void *)offset);
                        glEnableVertexAttribArray(itLocation->second);

                }

                glBindVertexArray(0);
                glDeleteBuffers(1, &index_buf_id);
                glDeleteBuffers(buffers_cnt, &vBuffersID[0]);

                log_d("shape[{}] name = '{}', triangles cnt = {}, vao_id = {}, texture id = {}, buffers_cnt = {}, vert attributes cnt = {}, min x = {}, y = {}, z = {}, max x = {}, y = {}, z = {}",
                                shape_num,
                                oShape.sName,
                                oShape.triangles_cnt,
                                oShape.vao_id,
                                (oShape.pTex) ? oShape.pTex->GetID() : 0,
                                buffers_cnt,
                                pAttributes->Length(),
                                oShape.oBBox.Min().x, oShape.oBBox.Min().y, oShape.oBBox.Min().z,
                                oShape.oBBox.Max().x, oShape.oBBox.Max().y, oShape.oBBox.Max().z);

                oMeshCtx.vShapes.emplace_back(std::move(oShape));
        }
}


void Mesh::Clean() {

        for (auto & oShape : oMeshCtx.vShapes) {

                glDeleteVertexArrays(1, &oShape.vao_id);
        }
}

std::string Mesh::Str() const {

        return fmt::format("Mesh name: '{}', shapes cnt: {}, bbox: {}", sName, oMeshCtx.vShapes.size(), oMeshCtx.oBBox.Str());
}

}


