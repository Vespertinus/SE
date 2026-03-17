
#ifdef SE_IMPL

#include <MeshBuilder.h>
#include <Mesh_generated.h>
#include <glm/gtc/matrix_inverse.hpp>

#include <algorithm>
#include <vector>
#include <cmath>

namespace SE {

MeshBuilder::MeshBuilder(const VertexLayout & layout)
        : oLayout(layout)
{
        std::memset(staging, 0, sizeof(staging));
}

void MeshBuilder::WriteAttr(VertexAttrib attrib, const float * data, uint32_t n) {
        const VertexAttributeDesc * pAttr = oLayout.Find(attrib);
        if (!pAttr) return;
        uint32_t copy_n = (n < pAttr->elem_size) ? n : pAttr->elem_size;
        std::memcpy(staging + pAttr->offset, data, copy_n * sizeof(float));
}

MeshBuilder & MeshBuilder::Position(glm::vec3 p) { WriteAttr(VertexAttrib::Position,  &p.x,  3); return *this; }
MeshBuilder & MeshBuilder::Normal  (glm::vec3 n) { WriteAttr(VertexAttrib::Normal,    &n.x,  3); return *this; }
MeshBuilder & MeshBuilder::Tangent (glm::vec3 t) { WriteAttr(VertexAttrib::Tangent,   &t.x,  3); return *this; }
MeshBuilder & MeshBuilder::UV      (glm::vec2 uv){ WriteAttr(VertexAttrib::TexCoord0, &uv.x, 2); return *this; }
MeshBuilder & MeshBuilder::Color   (glm::vec4 c) { WriteAttr(VertexAttrib::Color,     &c.x,  4); return *this; }

MeshBuilder & MeshBuilder::EndVertex() {
        const size_t old_size = vVerts.size();
        vVerts.resize(old_size + oLayout.stride);
        std::memcpy(vVerts.data() + old_size, staging, oLayout.stride);

        const VertexAttributeDesc * pPosAttr = oLayout.Find(VertexAttrib::Position);
        if (pPosAttr) {
                float px, py, pz;
                std::memcpy(&px, staging + pPosAttr->offset + 0, 4);
                std::memcpy(&py, staging + pPosAttr->offset + 4, 4);
                std::memcpy(&pz, staging + pPosAttr->offset + 8, 4);
                if (px < vBboxMin.x) vBboxMin.x = px;
                if (py < vBboxMin.y) vBboxMin.y = py;
                if (pz < vBboxMin.z) vBboxMin.z = pz;
                if (px > vBboxMax.x) vBboxMax.x = px;
                if (py > vBboxMax.y) vBboxMax.y = py;
                if (pz > vBboxMax.z) vBboxMax.z = pz;
        }

        std::memset(staging, 0, sizeof(staging));
        ++vert_count;
        return *this;
}

MeshBuilder & MeshBuilder::Triangle(uint32_t a, uint32_t b, uint32_t c) {
        vIndices.push_back(a);
        vIndices.push_back(b);
        vIndices.push_back(c);
        return *this;
}

MeshBuilder & MeshBuilder::Quad(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
        vIndices.push_back(a); vIndices.push_back(b); vIndices.push_back(c);
        vIndices.push_back(a); vIndices.push_back(c); vIndices.push_back(d);
        return *this;
}

MeshBuilder & MeshBuilder::AppendVertices(const void * data, uint32_t count) {
        const uint8_t * src = static_cast<const uint8_t *>(data);
        vVerts.insert(vVerts.end(), src, src + count * oLayout.stride);

        const VertexAttributeDesc * pPosAttr = oLayout.Find(VertexAttrib::Position);
        if (pPosAttr) {
                for (uint32_t i = 0; i < count; ++i) {
                        const uint8_t * v = src + i * oLayout.stride;
                        float px, py, pz;
                        std::memcpy(&px, v + pPosAttr->offset + 0, 4);
                        std::memcpy(&py, v + pPosAttr->offset + 4, 4);
                        std::memcpy(&pz, v + pPosAttr->offset + 8, 4);
                        if (px < vBboxMin.x) vBboxMin.x = px;
                        if (py < vBboxMin.y) vBboxMin.y = py;
                        if (pz < vBboxMin.z) vBboxMin.z = pz;
                        if (px > vBboxMax.x) vBboxMax.x = px;
                        if (py > vBboxMax.y) vBboxMax.y = py;
                        if (pz > vBboxMax.z) vBboxMax.z = pz;
                }
        }

        vert_count += count;
        return *this;
}

MeshBuilder & MeshBuilder::AppendIndices(const uint32_t * idx, uint32_t count) {
        vIndices.insert(vIndices.end(), idx, idx + count);
        return *this;
}

MeshBuilder & MeshBuilder::ApplyTransform(const glm::mat4 & m) {
        const VertexAttributeDesc * pPosAttr = oLayout.Find(VertexAttrib::Position);
        const VertexAttributeDesc * pNrmAttr = oLayout.Find(VertexAttrib::Normal);
        const VertexAttributeDesc * pTanAttr = oLayout.Find(VertexAttrib::Tangent);

        const glm::mat3 mNrmMat = glm::mat3(glm::inverseTranspose(m));
        const glm::mat3 mTanMat = glm::mat3(m);

        vBboxMin = glm::vec3( 1e30f);
        vBboxMax = glm::vec3(-1e30f);

        for (uint32_t i = 0; i < vert_count; ++i) {
                uint8_t * v = vVerts.data() + i * oLayout.stride;

                if (pPosAttr) {
                        float px, py, pz;
                        std::memcpy(&px, v + pPosAttr->offset + 0, 4);
                        std::memcpy(&py, v + pPosAttr->offset + 4, 4);
                        std::memcpy(&pz, v + pPosAttr->offset + 8, 4);
                        glm::vec3 oTp = glm::vec3(m * glm::vec4(px, py, pz, 1.0f));
                        std::memcpy(v + pPosAttr->offset + 0, &oTp.x, 4);
                        std::memcpy(v + pPosAttr->offset + 4, &oTp.y, 4);
                        std::memcpy(v + pPosAttr->offset + 8, &oTp.z, 4);
                        if (oTp.x < vBboxMin.x) vBboxMin.x = oTp.x;
                        if (oTp.y < vBboxMin.y) vBboxMin.y = oTp.y;
                        if (oTp.z < vBboxMin.z) vBboxMin.z = oTp.z;
                        if (oTp.x > vBboxMax.x) vBboxMax.x = oTp.x;
                        if (oTp.y > vBboxMax.y) vBboxMax.y = oTp.y;
                        if (oTp.z > vBboxMax.z) vBboxMax.z = oTp.z;
                }

                if (pNrmAttr) {
                        float nx, ny, nz;
                        std::memcpy(&nx, v + pNrmAttr->offset + 0, 4);
                        std::memcpy(&ny, v + pNrmAttr->offset + 4, 4);
                        std::memcpy(&nz, v + pNrmAttr->offset + 8, 4);
                        glm::vec3 oTn = glm::normalize(mNrmMat * glm::vec3(nx, ny, nz));
                        std::memcpy(v + pNrmAttr->offset + 0, &oTn.x, 4);
                        std::memcpy(v + pNrmAttr->offset + 4, &oTn.y, 4);
                        std::memcpy(v + pNrmAttr->offset + 8, &oTn.z, 4);
                }

                if (pTanAttr) {
                        float tx, ty, tz;
                        std::memcpy(&tx, v + pTanAttr->offset + 0, 4);
                        std::memcpy(&ty, v + pTanAttr->offset + 4, 4);
                        std::memcpy(&tz, v + pTanAttr->offset + 8, 4);
                        glm::vec3 oTt = glm::normalize(mTanMat * glm::vec3(tx, ty, tz));
                        std::memcpy(v + pTanAttr->offset + 0, &oTt.x, 4);
                        std::memcpy(v + pTanAttr->offset + 4, &oTt.y, 4);
                        std::memcpy(v + pTanAttr->offset + 8, &oTt.z, 4);
                }
        }

        return *this;
}

MeshBuilder & MeshBuilder::Merge(const MeshBuilder & other) {
        const uint32_t base = vert_count;
        vVerts.insert(vVerts.end(), other.vVerts.begin(), other.vVerts.end());
        vert_count += other.vert_count;

        for (uint32_t idx : other.vIndices) {
                vIndices.push_back(idx + base);
        }

        if (other.vert_count > 0) {
                if (other.vBboxMin.x < vBboxMin.x) vBboxMin.x = other.vBboxMin.x;
                if (other.vBboxMin.y < vBboxMin.y) vBboxMin.y = other.vBboxMin.y;
                if (other.vBboxMin.z < vBboxMin.z) vBboxMin.z = other.vBboxMin.z;
                if (other.vBboxMax.x > vBboxMax.x) vBboxMax.x = other.vBboxMax.x;
                if (other.vBboxMax.y > vBboxMax.y) vBboxMax.y = other.vBboxMax.y;
                if (other.vBboxMax.z > vBboxMax.z) vBboxMax.z = other.vBboxMax.z;
        }

        return *this;
}

uint32_t MeshBuilder::CurrentVertexIndex() const { return vert_count; }
uint32_t MeshBuilder::IndexCount()         const { return static_cast<uint32_t>(vIndices.size()); }

void MeshBuilder::Clear() {
        vVerts.clear();
        vIndices.clear();
        vert_count = 0;
        vBboxMin = glm::vec3( 1e30f);
        vBboxMax = glm::vec3(-1e30f);
        std::memset(staging, 0, sizeof(staging));
}

H<TMesh> MeshBuilder::Upload(const char * name) {
        flatbuffers::FlatBufferBuilder oFb(1024 * 64);

        // Vertex buffer
        const uint32_t float_count  = static_cast<uint32_t>(vVerts.size()) / sizeof(float);
        const float *  pFloatPtr    = reinterpret_cast<const float *>(vVerts.data());
        std::vector<float> vFloatData(pFloatPtr, pFloatPtr + float_count);

        auto oFvData = oFb.CreateVector(vFloatData);
        auto oFvOff  = SE::FlatBuffers::CreateFloatVector(oFb, oFvData);
        auto oVbOff  = SE::FlatBuffers::CreateVertexBuffer(
                        oFb,
                        SE::FlatBuffers::VertexBufferU::FloatVector,
                        oFvOff.Union(),
                        static_cast<uint8_t>(oLayout.stride));

        // Index buffer: uint16 when possible, uint32 otherwise
        flatbuffers::Offset<SE::FlatBuffers::IndexBuffer> oIbOff;
        if (vert_count <= 65535u) {
                std::vector<uint16_t> vIdx16;
                vIdx16.reserve(vIndices.size());
                for (uint32_t i : vIndices) vIdx16.push_back(static_cast<uint16_t>(i));
                auto oIdxData = oFb.CreateVector(vIdx16);
                auto oU16Off  = SE::FlatBuffers::CreateUint16Vector(oFb, oIdxData);
                oIbOff = SE::FlatBuffers::CreateIndexBuffer(
                                oFb,
                                SE::FlatBuffers::IndexBufferU::Uint16Vector,
                                oU16Off.Union());
        } else {
                auto oIdxData = oFb.CreateVector(vIndices);
                auto oU32Off  = SE::FlatBuffers::CreateUint32Vector(oFb, oIdxData);
                oIbOff = SE::FlatBuffers::CreateIndexBuffer(
                                oFb,
                                SE::FlatBuffers::IndexBufferU::Uint32Vector,
                                oU32Off.Union());
        }

        // Vertex attributes
        std::vector<flatbuffers::Offset<SE::FlatBuffers::VertexAttribute>> vAttrVec;
        vAttrVec.reserve(oLayout.attr_count);
        for (uint32_t i = 0; i < oLayout.attr_count; ++i) {
                const auto & a = oLayout.pAttrs[i];
                vAttrVec.push_back(SE::FlatBuffers::CreateVertexAttributeDirect(
                                oFb,
                                VertexAttribName(a.attrib),
                                static_cast<uint16_t>(a.offset),
                                static_cast<uint8_t>(a.elem_size),
                                0, 0,
                                SE::FlatBuffers::AttribDestType::DEST_FLOAT));
        }

        // Bounding box + shape
        SE::FlatBuffers::BoundingBox oBbox(
                        SE::FlatBuffers::Vec3(vBboxMin.x, vBboxMin.y, vBboxMin.z),
                        SE::FlatBuffers::Vec3(vBboxMax.x, vBboxMax.y, vBboxMax.z));
        auto oShapeOff = SE::FlatBuffers::CreateShape(
                        oFb, &oBbox, 0,
                        static_cast<uint32_t>(vIndices.size()));

        std::vector<flatbuffers::Offset<SE::FlatBuffers::VertexBuffer>> vVbVec     = { oVbOff };
        std::vector<flatbuffers::Offset<SE::FlatBuffers::Shape>>        vShapesVec = { oShapeOff };

        auto oVbVecOff   = oFb.CreateVector(vVbVec);
        auto oAttrVecOff = oFb.CreateVector(vAttrVec);
        auto oShapesOff  = oFb.CreateVector(vShapesVec);

        auto oMeshOff = SE::FlatBuffers::CreateMesh(
                        oFb,
                        oIbOff,
                        oVbVecOff,
                        oAttrVecOff,
                        SE::FlatBuffers::PrimitiveType::GEOM_TRIANGLES,
                        oShapesOff,
                        &oBbox);

        oFb.Finish(oMeshOff);

        auto * pFBMesh = SE::FlatBuffers::GetMesh(oFb.GetBufferPointer());
        return CreateResource<TMesh>(name, pFBMesh);
}

} // namespace SE

#endif // SE_IMPL
