#ifndef __PHYSICS_MESH_BUILDERS_H__
#define __PHYSICS_MESH_BUILDERS_H__

#include <cmath>
#include <vector>
#include <Mesh_generated.h>
#include <glm/vec3.hpp>

namespace SE {

// Build a box FlatBuffer mesh with Position(3), Normal(3), Tangent(3), TexCoord0(2)
// Stride = 44 bytes. half = per-axis half-extents.
inline H<TMesh> CreateBoxMesh(glm::vec3 half, const char * name) {

        // 6 faces, 4 verts each = 24 verts, 6 indices each = 36 indices
        // For each face: 4 positions, common normal + tangent, UVs (0,0)(1,0)(1,1)(0,1)
        // Triangles: (0,1,2) and (0,2,3) per face using base offset

        const float half_x = half.x, half_y = half.y, half_z = half.z;

        struct FaceDesc {
                // 4 vertices in CCW order (viewed from outside = normal direction)
                float v[4][3];   // positions
                float n[3];      // normal
                float t[3];      // tangent (dP/dU direction)
        };

        // Faces ordered so that cross(v1-v0, v2-v0) = normal direction
        const FaceDesc faces[6] = {
                // Top (+Y): n=(0,1,0), t=(1,0,0)
                {{{ -half_x,  half_y,  half_z }, {  half_x,  half_y,  half_z }, {  half_x,  half_y, -half_z }, { -half_x,  half_y, -half_z }},
                 { 0, 1, 0 }, { 1, 0, 0 }},
                // Bottom (-Y): n=(0,-1,0), t=(1,0,0)
                {{{ -half_x, -half_y, -half_z }, {  half_x, -half_y, -half_z }, {  half_x, -half_y,  half_z }, { -half_x, -half_y,  half_z }},
                 { 0,-1, 0 }, { 1, 0, 0 }},
                // Front (+Z): n=(0,0,1), t=(1,0,0)
                {{{ -half_x, -half_y,  half_z }, {  half_x, -half_y,  half_z }, {  half_x,  half_y,  half_z }, { -half_x,  half_y,  half_z }},
                 { 0, 0, 1 }, { 1, 0, 0 }},
                // Back (-Z): n=(0,0,-1), t=(-1,0,0)
                {{{  half_x, -half_y, -half_z }, { -half_x, -half_y, -half_z }, { -half_x,  half_y, -half_z }, {  half_x,  half_y, -half_z }},
                 { 0, 0,-1 }, {-1, 0, 0 }},
                // Right (+X): n=(1,0,0), t=(0,0,-1)
                {{{  half_x, -half_y,  half_z }, {  half_x, -half_y, -half_z }, {  half_x,  half_y, -half_z }, {  half_x,  half_y,  half_z }},
                 { 1, 0, 0 }, { 0, 0,-1 }},
                // Left (-X): n=(-1,0,0), t=(0,0,1)
                {{{ -half_x, -half_y, -half_z }, { -half_x, -half_y,  half_z }, { -half_x,  half_y,  half_z }, { -half_x,  half_y, -half_z }},
                 {-1, 0, 0 }, { 0, 0, 1 }},
        };

        const float uvs[4][2] = { {0,0},{1,0},{1,1},{0,1} };

        std::vector<float>    oVerts;
        oVerts.reserve(24 * 11);
        std::vector<uint16_t> oIndices;
        oIndices.reserve(36);

        for (int f = 0; f < 6; ++f) {
                const uint16_t base = static_cast<uint16_t>(f * 4);
                for (int v = 0; v < 4; ++v) {
                        // position
                        oVerts.push_back(faces[f].v[v][0]);
                        oVerts.push_back(faces[f].v[v][1]);
                        oVerts.push_back(faces[f].v[v][2]);
                        // normal
                        oVerts.push_back(faces[f].n[0]);
                        oVerts.push_back(faces[f].n[1]);
                        oVerts.push_back(faces[f].n[2]);
                        // tangent
                        oVerts.push_back(faces[f].t[0]);
                        oVerts.push_back(faces[f].t[1]);
                        oVerts.push_back(faces[f].t[2]);
                        // uv
                        oVerts.push_back(uvs[v][0]);
                        oVerts.push_back(uvs[v][1]);
                }
                // Two CCW triangles
                oIndices.push_back(base + 0); oIndices.push_back(base + 1); oIndices.push_back(base + 2);
                oIndices.push_back(base + 0); oIndices.push_back(base + 2); oIndices.push_back(base + 3);
        }

        flatbuffers::FlatBufferBuilder oBuilder(1024 * 16);

        auto oFloatData = oBuilder.CreateVector(oVerts);
        auto oFvOff     = SE::FlatBuffers::CreateFloatVector(oBuilder, oFloatData);
        auto oVbOff     = SE::FlatBuffers::CreateVertexBuffer(
                        oBuilder,
                        SE::FlatBuffers::VertexBufferU::FloatVector,
                        oFvOff.Union(),
                        44);

        auto oIdxData = oBuilder.CreateVector(oIndices);
        auto oU16Off  = SE::FlatBuffers::CreateUint16Vector(oBuilder, oIdxData);
        auto oIbOff   = SE::FlatBuffers::CreateIndexBuffer(
                        oBuilder,
                        SE::FlatBuffers::IndexBufferU::Uint16Vector,
                        oU16Off.Union());

        auto oAttrPos = SE::FlatBuffers::CreateVertexAttributeDirect(oBuilder, "Position",   0, 3, 0, 0, SE::FlatBuffers::AttribDestType::DEST_FLOAT);
        auto oAttrNrm = SE::FlatBuffers::CreateVertexAttributeDirect(oBuilder, "Normal",    12, 3, 0, 0, SE::FlatBuffers::AttribDestType::DEST_FLOAT);
        auto oAttrTan = SE::FlatBuffers::CreateVertexAttributeDirect(oBuilder, "Tangent",   24, 3, 0, 0, SE::FlatBuffers::AttribDestType::DEST_FLOAT);
        auto oAttrUv  = SE::FlatBuffers::CreateVertexAttributeDirect(oBuilder, "TexCoord0", 36, 2, 0, 0, SE::FlatBuffers::AttribDestType::DEST_FLOAT);

        std::vector<flatbuffers::Offset<SE::FlatBuffers::VertexBuffer>>    oVbVec   = { oVbOff };
        std::vector<flatbuffers::Offset<SE::FlatBuffers::VertexAttribute>> oAttrVec = { oAttrPos, oAttrNrm, oAttrTan, oAttrUv };

        SE::FlatBuffers::BoundingBox bbox(
                        SE::FlatBuffers::Vec3(-half_x, -half_y, -half_z),
                        SE::FlatBuffers::Vec3( half_x,  half_y,  half_z));

        auto oShapeOff    = SE::FlatBuffers::CreateShape(oBuilder, &bbox, 0, static_cast<uint32_t>(oIndices.size()));
        std::vector<flatbuffers::Offset<SE::FlatBuffers::Shape>> oShapesVec = { oShapeOff };

        auto oVbVecOff   = oBuilder.CreateVector(oVbVec);
        auto oAttrVecOff = oBuilder.CreateVector(oAttrVec);
        auto oShapesOff  = oBuilder.CreateVector(oShapesVec);

        auto oMeshOff = SE::FlatBuffers::CreateMesh(
                        oBuilder,
                        oIbOff,
                        oVbVecOff,
                        oAttrVecOff,
                        SE::FlatBuffers::PrimitiveType::GEOM_TRIANGLES,
                        oShapesOff,
                        &bbox);

        oBuilder.Finish(oMeshOff);

        auto * pFBMesh = SE::FlatBuffers::GetMesh(oBuilder.GetBufferPointer());
        return CreateResource<TMesh>(name, pFBMesh);
}

// Build a flat XZ-plane FlatBuffer mesh (facing +Y).
// w, d = total width and depth. Mesh centred at origin.
inline H<TMesh> CreatePlaneMesh(float w, float d, const char * name) {

        const float half_w = w * 0.5f, half_d = d * 0.5f;

        // Single quad, CCW from above (+Y)
        // v0=(-half_w,0,half_d), v1=(half_w,0,half_d), v2=(half_w,0,-half_d), v3=(-half_w,0,-half_d)
        // e1=v1-v0=(w,0,0), e2=v2-v0=(w,0,-2*half_d) → e1×e2=(0,+,0) → +Y ✓
        const float vert_data[4][11] = {
                { -half_w, 0.0f,  half_d,   0,1,0,  1,0,0,  0.0f, 0.0f },
                {  half_w, 0.0f,  half_d,   0,1,0,  1,0,0,  1.0f, 0.0f },
                {  half_w, 0.0f, -half_d,   0,1,0,  1,0,0,  1.0f, 1.0f },
                { -half_w, 0.0f, -half_d,   0,1,0,  1,0,0,  0.0f, 1.0f },
        };
        const uint16_t idx_data[6] = { 0,1,2, 0,2,3 };

        std::vector<float>    oVerts(vert_data[0], vert_data[0] + 4 * 11);
        std::vector<uint16_t> oIndices(idx_data, idx_data + 6);

        flatbuffers::FlatBufferBuilder oBuilder(1024 * 4);

        auto oFloatData = oBuilder.CreateVector(oVerts);
        auto oFvOff     = SE::FlatBuffers::CreateFloatVector(oBuilder, oFloatData);
        auto oVbOff     = SE::FlatBuffers::CreateVertexBuffer(
                        oBuilder,
                        SE::FlatBuffers::VertexBufferU::FloatVector,
                        oFvOff.Union(),
                        44);

        auto oIdxData = oBuilder.CreateVector(oIndices);
        auto oU16Off  = SE::FlatBuffers::CreateUint16Vector(oBuilder, oIdxData);
        auto oIbOff   = SE::FlatBuffers::CreateIndexBuffer(
                        oBuilder,
                        SE::FlatBuffers::IndexBufferU::Uint16Vector,
                        oU16Off.Union());

        auto oAttrPos = SE::FlatBuffers::CreateVertexAttributeDirect(oBuilder, "Position",   0, 3, 0, 0, SE::FlatBuffers::AttribDestType::DEST_FLOAT);
        auto oAttrNrm = SE::FlatBuffers::CreateVertexAttributeDirect(oBuilder, "Normal",    12, 3, 0, 0, SE::FlatBuffers::AttribDestType::DEST_FLOAT);
        auto oAttrTan = SE::FlatBuffers::CreateVertexAttributeDirect(oBuilder, "Tangent",   24, 3, 0, 0, SE::FlatBuffers::AttribDestType::DEST_FLOAT);
        auto oAttrUv  = SE::FlatBuffers::CreateVertexAttributeDirect(oBuilder, "TexCoord0", 36, 2, 0, 0, SE::FlatBuffers::AttribDestType::DEST_FLOAT);

        std::vector<flatbuffers::Offset<SE::FlatBuffers::VertexBuffer>>    oVbVec   = { oVbOff };
        std::vector<flatbuffers::Offset<SE::FlatBuffers::VertexAttribute>> oAttrVec = { oAttrPos, oAttrNrm, oAttrTan, oAttrUv };

        SE::FlatBuffers::BoundingBox bbox(
                        SE::FlatBuffers::Vec3(-half_w, -0.001f, -half_d),
                        SE::FlatBuffers::Vec3( half_w,  0.001f,  half_d));

        auto oShapeOff    = SE::FlatBuffers::CreateShape(oBuilder, &bbox, 0, 6u);
        std::vector<flatbuffers::Offset<SE::FlatBuffers::Shape>> oShapesVec = { oShapeOff };

        auto oVbVecOff   = oBuilder.CreateVector(oVbVec);
        auto oAttrVecOff = oBuilder.CreateVector(oAttrVec);
        auto oShapesOff  = oBuilder.CreateVector(oShapesVec);

        auto oMeshOff = SE::FlatBuffers::CreateMesh(
                        oBuilder,
                        oIbOff,
                        oVbVecOff,
                        oAttrVecOff,
                        SE::FlatBuffers::PrimitiveType::GEOM_TRIANGLES,
                        oShapesOff,
                        &bbox);

        oBuilder.Finish(oMeshOff);

        auto * pFBMesh = SE::FlatBuffers::GetMesh(oBuilder.GetBufferPointer());
        return CreateResource<TMesh>(name, pFBMesh);
}

} // namespace SE

#endif
