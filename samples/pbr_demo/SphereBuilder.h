#ifndef __PBR_SPHERE_BUILDER_H__
#define __PBR_SPHERE_BUILDER_H__

#include <cmath>
#include <vector>
#include <Mesh_generated.h>

namespace SE {

// Build a unit sphere FlatBuffer mesh with Position(3), Normal(3), Tangent(3), TexCoord0(2)
// Stride = 44 bytes (11 floats * 4)
// Indices are uint16; rings×sectors×6 = total index count
inline TMesh * CreateSphereMesh(int rings, int sectors) {

        const int   verts_per_row = sectors + 1;
        const int   num_verts     = (rings + 1) * verts_per_row;
        const int   num_indices   = rings * sectors * 6;
        const float PI            = static_cast<float>(M_PI);
        const float TWO_PI        = 2.0f * PI;

        // 11 floats per vertex: pos(3) + normal(3) + tangent(3) + uv(2)
        std::vector<float>    verts;
        verts.reserve(num_verts * 11);

        std::vector<uint16_t> indices;
        indices.reserve(num_indices);

        for (int r = 0; r <= rings; ++r) {
                float phi   = PI * r / rings;          // 0 .. PI
                float sp    = std::sin(phi);
                float cp    = std::cos(phi);

                for (int s = 0; s <= sectors; ++s) {
                        float theta = TWO_PI * s / sectors; // 0 .. 2PI
                        float st    = std::sin(theta);
                        float ct    = std::cos(theta);

                        // position == normal for unit sphere
                        float nx = sp * ct;
                        float ny = cp;
                        float nz = sp * st;

                        // tangent = dP/dtheta (normalised), fallback at poles
                        float tx, ty, tz;
                        if (sp < 1e-5f) {
                                tx = 1.0f; ty = 0.0f; tz = 0.0f;
                        } else {
                                tx = -st;   // -sin(theta)
                                ty = 0.0f;
                                tz =  ct;   //  cos(theta)
                                            // already unit length
                        }

                        float u = static_cast<float>(s) / sectors;
                        float v = static_cast<float>(r) / rings;

                        verts.push_back(nx);  // position x
                        verts.push_back(ny);  // position y
                        verts.push_back(nz);  // position z
                        verts.push_back(nx);  // normal x
                        verts.push_back(ny);  // normal y
                        verts.push_back(nz);  // normal z
                        verts.push_back(tx);  // tangent x
                        verts.push_back(ty);  // tangent y
                        verts.push_back(tz);  // tangent z
                        verts.push_back(u);   // uv.x
                        verts.push_back(v);   // uv.y
                }
        }

        // Indices: two CCW triangles per quad
        for (int r = 0; r < rings; ++r) {
                for (int s = 0; s < sectors; ++s) {
                        uint16_t tl = static_cast<uint16_t>(r       * verts_per_row + s);
                        uint16_t tr = static_cast<uint16_t>(r       * verts_per_row + s + 1);
                        uint16_t bl = static_cast<uint16_t>((r + 1) * verts_per_row + s);
                        uint16_t br = static_cast<uint16_t>((r + 1) * verts_per_row + s + 1);
                        // triangle 1
                        indices.push_back(tl);
                        indices.push_back(bl);
                        indices.push_back(tr);
                        // triangle 2
                        indices.push_back(tr);
                        indices.push_back(bl);
                        indices.push_back(br);
                }
        }

        flatbuffers::FlatBufferBuilder builder(1024 * 64);

        // Vertex buffer
        auto float_data = builder.CreateVector(verts);
        auto fv_off     = SE::FlatBuffers::CreateFloatVector(builder, float_data);
        auto vb_off     = SE::FlatBuffers::CreateVertexBuffer(
                        builder,
                        SE::FlatBuffers::VertexBufferU::FloatVector,
                        fv_off.Union(),
                        44);  // stride in bytes

        // Index buffer
        auto idx_data = builder.CreateVector(indices);
        auto u16_off  = SE::FlatBuffers::CreateUint16Vector(builder, idx_data);
        auto ib_off   = SE::FlatBuffers::CreateIndexBuffer(
                        builder,
                        SE::FlatBuffers::IndexBufferU::Uint16Vector,
                        u16_off.Union());

        // Vertex attributes
        auto attr_pos  = SE::FlatBuffers::CreateVertexAttributeDirect(builder, "Position",  0,  3, 0, 0, SE::FlatBuffers::AttribDestType::DEST_FLOAT);
        auto attr_nrm  = SE::FlatBuffers::CreateVertexAttributeDirect(builder, "Normal",   12,  3, 0, 0, SE::FlatBuffers::AttribDestType::DEST_FLOAT);
        auto attr_tan  = SE::FlatBuffers::CreateVertexAttributeDirect(builder, "Tangent",  24,  3, 0, 0, SE::FlatBuffers::AttribDestType::DEST_FLOAT);
        auto attr_uv   = SE::FlatBuffers::CreateVertexAttributeDirect(builder, "TexCoord0",36,  2, 0, 0, SE::FlatBuffers::AttribDestType::DEST_FLOAT);

        std::vector<flatbuffers::Offset<SE::FlatBuffers::VertexBuffer>>    vb_vec   = { vb_off };
        std::vector<flatbuffers::Offset<SE::FlatBuffers::VertexAttribute>> attr_vec = { attr_pos, attr_nrm, attr_tan, attr_uv };

        // Shape
        SE::FlatBuffers::BoundingBox bbox(
                        SE::FlatBuffers::Vec3(-1.f, -1.f, -1.f),
                        SE::FlatBuffers::Vec3( 1.f,  1.f,  1.f));

        auto shape_off = SE::FlatBuffers::CreateShape(builder, &bbox, 0, static_cast<uint32_t>(num_indices));

        std::vector<flatbuffers::Offset<SE::FlatBuffers::Shape>> shapes_vec = { shape_off };

        // Mesh
        auto vb_vec_off   = builder.CreateVector(vb_vec);
        auto attr_vec_off = builder.CreateVector(attr_vec);
        auto shapes_off   = builder.CreateVector(shapes_vec);

        auto mesh_off = SE::FlatBuffers::CreateMesh(
                        builder,
                        ib_off,
                        vb_vec_off,
                        attr_vec_off,
                        SE::FlatBuffers::PrimitiveType::GEOM_TRIANGLES,
                        shapes_off,
                        &bbox);

        builder.Finish(mesh_off);

        auto * pFBMesh = SE::FlatBuffers::GetMesh(builder.GetBufferPointer());
        return CreateResource<TMesh>("pbr_sphere", pFBMesh);
}

} // namespace SE

#endif
