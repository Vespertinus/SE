
#ifdef SE_IMPL

#include <MeshGen.h>
#include <glm/glm.hpp>

#include <cmath>
#include <vector>

namespace SE {
namespace MeshGen {

// ---------------------------------------------------------------------------
// Box
// ---------------------------------------------------------------------------
void Box(MeshBuilder & b, glm::vec3 half) {
        const float half_x = half.x, half_y = half.y, half_z = half.z;

        struct FaceDesc {
                float v[4][3]; // positions (CCW viewed from outside)
                float n[3];    // normal
                float t[3];    // tangent (dP/dU)
        };

        static const float uvs[4][2] = { {0,0},{1,0},{1,1},{0,1} };

        const FaceDesc faces[6] = {
                // Top (+Y)
                {{{ -half_x,  half_y,  half_z },{ half_x,  half_y,  half_z },{ half_x,  half_y,-half_z },{ -half_x, half_y,-half_z }}, {0,1,0},{1,0,0}},
                // Bottom (-Y)
                {{{ -half_x,-half_y,-half_z },{ half_x,-half_y,-half_z },{ half_x,-half_y, half_z },{ -half_x,-half_y, half_z }}, {0,-1,0},{1,0,0}},
                // Front (+Z)
                {{{ -half_x,-half_y, half_z },{ half_x,-half_y, half_z },{ half_x, half_y, half_z },{ -half_x, half_y, half_z }}, {0,0,1},{1,0,0}},
                // Back (-Z)
                {{{  half_x,-half_y,-half_z },{ -half_x,-half_y,-half_z },{ -half_x, half_y,-half_z },{ half_x, half_y,-half_z }}, {0,0,-1},{-1,0,0}},
                // Right (+X)
                {{{  half_x,-half_y, half_z },{ half_x,-half_y,-half_z },{ half_x, half_y,-half_z },{ half_x, half_y, half_z }}, {1,0,0},{0,0,-1}},
                // Left (-X)
                {{{ -half_x,-half_y,-half_z },{ -half_x,-half_y, half_z },{ -half_x, half_y, half_z },{ -half_x, half_y,-half_z }}, {-1,0,0},{0,0,1}},
        };

        for (int f = 0; f < 6; ++f) {
                const uint32_t base = b.CurrentVertexIndex();
                // interleaved data: pos(3)+norm(3)+tan(3)+uv(2) = 11 floats
                float vert_data[4][11];
                for (int vi = 0; vi < 4; ++vi) {
                        vert_data[vi][0] = faces[f].v[vi][0];
                        vert_data[vi][1] = faces[f].v[vi][1];
                        vert_data[vi][2] = faces[f].v[vi][2];
                        vert_data[vi][3] = faces[f].n[0];
                        vert_data[vi][4] = faces[f].n[1];
                        vert_data[vi][5] = faces[f].n[2];
                        vert_data[vi][6] = faces[f].t[0];
                        vert_data[vi][7] = faces[f].t[1];
                        vert_data[vi][8] = faces[f].t[2];
                        vert_data[vi][9] = uvs[vi][0];
                        vert_data[vi][10]= uvs[vi][1];
                }
                b.AppendVertices(vert_data, 4);
                b.Quad(base+0, base+1, base+2, base+3);
        }
}

// ---------------------------------------------------------------------------
// Sphere
// ---------------------------------------------------------------------------
void Sphere(MeshBuilder & b, float radius, int latDiv, int lonDiv) {
        const int rings   = latDiv;
        const int sectors = lonDiv;
        const float pi    = static_cast<float>(M_PI);
        const float two_pi = 2.0f * pi;

        const uint32_t base = b.CurrentVertexIndex();

        for (int r = 0; r <= rings; ++r) {
                float phi = pi * r / rings;
                float sp  = std::sin(phi);
                float cp  = std::cos(phi);

                for (int s = 0; s <= sectors; ++s) {
                        float theta = two_pi * s / sectors;
                        float st    = std::sin(theta);
                        float ct    = std::cos(theta);

                        float nx = sp * ct;
                        float ny = cp;
                        float nz = sp * st;

                        float tx, ty, tz;
                        if (sp < 1e-5f) {
                                tx = 1.0f; ty = 0.0f; tz = 0.0f;
                        } else {
                                tx = -st; ty = 0.0f; tz = ct;
                        }

                        float u = static_cast<float>(s) / sectors;
                        float v = static_cast<float>(r) / rings;

                        b.Position({ nx * radius, ny * radius, nz * radius })
                         .Normal  ({ nx, ny, nz })
                         .Tangent ({ tx, ty, tz })
                         .UV      ({ u, v })
                         .EndVertex();
                }
        }

        const int verts_per_row = sectors + 1;
        for (int r = 0; r < rings; ++r) {
                for (int s = 0; s < sectors; ++s) {
                        uint32_t tl = base + r       * verts_per_row + s;
                        uint32_t tr = base + r       * verts_per_row + s + 1;
                        uint32_t bl = base + (r + 1) * verts_per_row + s;
                        uint32_t br = base + (r + 1) * verts_per_row + s + 1;
                        b.Triangle(tl, bl, tr);
                        b.Triangle(tr, bl, br);
                }
        }
}

// ---------------------------------------------------------------------------
// Cylinder
// ---------------------------------------------------------------------------
void Cylinder(MeshBuilder & b, float radius, float height, int div) {
        const float pi    = static_cast<float>(M_PI);
        const float two_pi = 2.0f * pi;
        const float half_y = height * 0.5f;
        const uint32_t base = b.CurrentVertexIndex();

        // Side verts: two rings (bottom ring index 0..div, top ring index div+1..2*div+1)
        for (int side = 0; side < 2; ++side) {
                float y   = (side == 0) ? -half_y : half_y;
                float v   = (side == 0) ? 1.0f : 0.0f;
                for (int i = 0; i <= div; ++i) {
                        float theta = two_pi * i / div;
                        float ct = std::cos(theta), st = std::sin(theta);
                        b.Position({ ct * radius, y, st * radius })
                         .Normal  ({ ct, 0.0f, st })
                         .Tangent ({ -st, 0.0f, ct })
                         .UV      ({ static_cast<float>(i) / div, v })
                         .EndVertex();
                }
        }

        const int ring_size = div + 1;
        // Side quads
        for (int i = 0; i < div; ++i) {
                uint32_t bl = base + i;
                uint32_t br = base + i + 1;
                uint32_t tl = base + ring_size + i;
                uint32_t tr = base + ring_size + i + 1;
                b.Quad(bl, br, tr, tl);
        }

        // Cap centers
        uint32_t bot_center = b.CurrentVertexIndex();
        b.Position({0.0f, -half_y, 0.0f}).Normal({0,-1,0}).Tangent({1,0,0}).UV({0.5f,0.5f}).EndVertex();
        uint32_t top_center = b.CurrentVertexIndex();
        b.Position({0.0f,  half_y, 0.0f}).Normal({0, 1,0}).Tangent({1,0,0}).UV({0.5f,0.5f}).EndVertex();

        // Cap verts
        uint32_t bot_rim = b.CurrentVertexIndex();
        for (int i = 0; i <= div; ++i) {
                float theta = two_pi * i / div;
                float ct = std::cos(theta), st = std::sin(theta);
                b.Position({ ct*radius, -half_y, st*radius }).Normal({0,-1,0}).Tangent({1,0,0})
                 .UV({ ct*0.5f+0.5f, st*0.5f+0.5f }).EndVertex();
        }
        uint32_t top_rim = b.CurrentVertexIndex();
        for (int i = 0; i <= div; ++i) {
                float theta = two_pi * i / div;
                float ct = std::cos(theta), st = std::sin(theta);
                b.Position({ ct*radius,  half_y, st*radius }).Normal({0,1,0}).Tangent({1,0,0})
                 .UV({ ct*0.5f+0.5f, st*0.5f+0.5f }).EndVertex();
        }

        for (int i = 0; i < div; ++i) {
                b.Triangle(bot_center, bot_rim + i + 1, bot_rim + i);
                b.Triangle(top_center, top_rim + i,     top_rim + i + 1);
        }
}

// ---------------------------------------------------------------------------
// Cone
// ---------------------------------------------------------------------------
void Cone(MeshBuilder & b, float radius, float height, int div) {
        const float pi     = static_cast<float>(M_PI);
        const float two_pi = 2.0f * pi;
        const float half_y = height * 0.5f;
        const float slope  = radius / height; // for normal calculation

        const uint32_t base = b.CurrentVertexIndex();

        // Side verts at base ring
        for (int i = 0; i <= div; ++i) {
                float theta = two_pi * i / div;
                float ct = std::cos(theta), st = std::sin(theta);
                // Normal tilted by slope
                glm::vec3 n = glm::normalize(glm::vec3(ct, slope, st));
                b.Position({ ct * radius, -half_y, st * radius })
                 .Normal  (n)
                 .Tangent ({ -st, 0.0f, ct })
                 .UV      ({ static_cast<float>(i) / div, 1.0f })
                 .EndVertex();
        }
        // Apex — one vert per sector for correct normals
        for (int i = 0; i < div; ++i) {
                float theta = two_pi * (i + 0.5f) / div;
                float ct = std::cos(theta), st = std::sin(theta);
                glm::vec3 n = glm::normalize(glm::vec3(ct, slope, st));
                b.Position({ 0.0f, half_y, 0.0f })
                 .Normal  (n)
                 .Tangent ({ -st, 0.0f, ct })
                 .UV      ({ (static_cast<float>(i) + 0.5f) / div, 0.0f })
                 .EndVertex();
        }

        const int apex_start = static_cast<int>(base) + (div + 1);
        for (int i = 0; i < div; ++i) {
                b.Triangle(base + i, base + i + 1, static_cast<uint32_t>(apex_start + i));
        }

        // Bottom cap
        uint32_t bot_center = b.CurrentVertexIndex();
        b.Position({0.0f, -half_y, 0.0f}).Normal({0,-1,0}).Tangent({1,0,0}).UV({0.5f,0.5f}).EndVertex();
        uint32_t bot_rim = b.CurrentVertexIndex();
        for (int i = 0; i <= div; ++i) {
                float theta = two_pi * i / div;
                float ct = std::cos(theta), st = std::sin(theta);
                b.Position({ ct*radius, -half_y, st*radius }).Normal({0,-1,0}).Tangent({1,0,0})
                 .UV({ ct*0.5f+0.5f, st*0.5f+0.5f }).EndVertex();
        }
        for (int i = 0; i < div; ++i) {
                b.Triangle(bot_center, bot_rim + i + 1, bot_rim + i);
        }
}

// ---------------------------------------------------------------------------
// Capsule
// ---------------------------------------------------------------------------
void Capsule(MeshBuilder & b, float radius, float halfHeight, int div) {
        const float pi     = static_cast<float>(M_PI);
        const float two_pi = 2.0f * pi;
        const int   rings  = div / 2;   // hemisphere rings

        const uint32_t base = b.CurrentVertexIndex();

        // Bottom hemisphere (phi from PI/2 to PI)
        for (int r = 0; r <= rings; ++r) {
                float phi = pi * 0.5f + pi * 0.5f * r / rings;
                float sp  = std::sin(phi);
                float cp  = std::cos(phi);
                for (int s = 0; s <= div; ++s) {
                        float theta = two_pi * s / div;
                        float ct = std::cos(theta), st = std::sin(theta);
                        float nx = sp * ct, ny = cp, nz = sp * st;
                        float tx = (sp < 1e-5f) ? 1.0f : -st;
                        float tz = (sp < 1e-5f) ? 0.0f :  ct;
                        float u  = static_cast<float>(s) / div;
                        float v  = 0.5f * r / rings;
                        b.Position({ nx*radius, ny*radius - halfHeight, nz*radius })
                         .Normal  ({ nx, ny, nz })
                         .Tangent ({ tx, 0.0f, tz })
                         .UV      ({ u, v })
                         .EndVertex();
                }
        }

        // Top hemisphere (phi from 0 to PI/2)
        for (int r = 0; r <= rings; ++r) {
                float phi = pi * 0.5f * r / rings;
                float sp  = std::sin(phi);
                float cp  = std::cos(phi);
                for (int s = 0; s <= div; ++s) {
                        float theta = two_pi * s / div;
                        float ct = std::cos(theta), st = std::sin(theta);
                        float nx = sp * ct, ny = cp, nz = sp * st;
                        float tx = (sp < 1e-5f) ? 1.0f : -st;
                        float tz = (sp < 1e-5f) ? 0.0f :  ct;
                        float u  = static_cast<float>(s) / div;
                        float v  = 0.5f + 0.5f * r / rings;
                        b.Position({ nx*radius, ny*radius + halfHeight, nz*radius })
                         .Normal  ({ nx, ny, nz })
                         .Tangent ({ tx, 0.0f, tz })
                         .UV      ({ u, v })
                         .EndVertex();
                }
        }

        const int verts_per_row = div + 1;
        const int total_rings   = (rings + 1) * 2;
        for (int r = 0; r < total_rings - 1; ++r) {
                for (int s = 0; s < div; ++s) {
                        uint32_t tl = base + r       * verts_per_row + s;
                        uint32_t tr = base + r       * verts_per_row + s + 1;
                        uint32_t bl = base + (r + 1) * verts_per_row + s;
                        uint32_t br = base + (r + 1) * verts_per_row + s + 1;
                        b.Triangle(tl, bl, tr);
                        b.Triangle(tr, bl, br);
                }
        }
}

// ---------------------------------------------------------------------------
// Torus
// ---------------------------------------------------------------------------
void Torus(MeshBuilder & b, float major, float minor, int majDiv, int minDiv) {
        const float pi     = static_cast<float>(M_PI);
        const float two_pi = 2.0f * pi;

        const uint32_t base = b.CurrentVertexIndex();

        for (int i = 0; i <= majDiv; ++i) {
                float u   = two_pi * i / majDiv;
                float cu  = std::cos(u), su = std::sin(u);

                for (int j = 0; j <= minDiv; ++j) {
                        float v   = two_pi * j / minDiv;
                        float cv  = std::cos(v), sv = std::sin(v);

                        float px  = (major + minor * cv) * cu;
                        float py  = minor * sv;
                        float pz  = (major + minor * cv) * su;

                        // Normal points from ring centre to vertex
                        float nx  = cv * cu;
                        float ny  = sv;
                        float nz  = cv * su;

                        // Tangent along major circle
                        float tx  = -su;
                        float ty  = 0.0f;
                        float tz  =  cu;

                        b.Position({ px, py, pz })
                         .Normal  ({ nx, ny, nz })
                         .Tangent ({ tx, ty, tz })
                         .UV      ({ static_cast<float>(i) / majDiv, static_cast<float>(j) / minDiv })
                         .EndVertex();
                }
        }

        const int row = minDiv + 1;
        for (int i = 0; i < majDiv; ++i) {
                for (int j = 0; j < minDiv; ++j) {
                        uint32_t a = base + i       * row + j;
                        uint32_t b2= base + i       * row + j + 1;
                        uint32_t c = base + (i + 1) * row + j;
                        uint32_t d = base + (i + 1) * row + j + 1;
                        b.Quad(a, b2, d, c);
                }
        }
}

// ---------------------------------------------------------------------------
// Quad (flat XZ-plane, facing +Y)
// ---------------------------------------------------------------------------
void Quad(MeshBuilder & b, glm::vec2 halfSize) {
        const float half_w = halfSize.x, half_d = halfSize.y;
        const uint32_t base = b.CurrentVertexIndex();

        b.Position({-half_w, 0.0f,  half_d}).Normal({0,1,0}).Tangent({1,0,0}).UV({0.0f, 0.0f}).EndVertex();
        b.Position({ half_w, 0.0f,  half_d}).Normal({0,1,0}).Tangent({1,0,0}).UV({1.0f, 0.0f}).EndVertex();
        b.Position({ half_w, 0.0f, -half_d}).Normal({0,1,0}).Tangent({1,0,0}).UV({1.0f, 1.0f}).EndVertex();
        b.Position({-half_w, 0.0f, -half_d}).Normal({0,1,0}).Tangent({1,0,0}).UV({0.0f, 1.0f}).EndVertex();
        b.Quad(base+0, base+1, base+2, base+3);
}

// ---------------------------------------------------------------------------
// FullscreenTri
// ---------------------------------------------------------------------------
void FullscreenTri(MeshBuilder & b) {
        const uint32_t base = b.CurrentVertexIndex();
        b.Position({-1.0f,-1.0f, 0.0f}).Normal({0,0,1}).Tangent({1,0,0}).UV({0.0f, 0.0f}).EndVertex();
        b.Position({ 3.0f,-1.0f, 0.0f}).Normal({0,0,1}).Tangent({1,0,0}).UV({2.0f, 0.0f}).EndVertex();
        b.Position({-1.0f, 3.0f, 0.0f}).Normal({0,0,1}).Tangent({1,0,0}).UV({0.0f, 2.0f}).EndVertex();
        b.Triangle(base+0, base+1, base+2);
}

// ---------------------------------------------------------------------------
// ComputeNormals
// ---------------------------------------------------------------------------
void ComputeNormals(MeshBuilder & b) {
        const VertexLayout &        layout    = b.Layout();
        const VertexAttributeDesc * pPosAttr  = layout.Find(VertexAttrib::Position);
        const VertexAttributeDesc * pNrmAttr  = layout.Find(VertexAttrib::Normal);
        if (!pPosAttr || !pNrmAttr) return;

        const uint32_t vert_count  = b.CurrentVertexIndex();
        const uint32_t idx_count   = b.IndexCount();
        const uint32_t stride      = layout.stride;
        const uint8_t  * pVertData = b.VertexData();
        uint8_t        * pVertMut  = b.VertexDataMutable();
        const uint32_t * pIdxData  = b.IndexData();

        // Reset normals
        for (uint32_t i = 0; i < vert_count; ++i) {
                float z = 0.0f;
                std::memcpy(pVertMut + i * stride + pNrmAttr->offset + 0, &z, 4);
                std::memcpy(pVertMut + i * stride + pNrmAttr->offset + 4, &z, 4);
                std::memcpy(pVertMut + i * stride + pNrmAttr->offset + 8, &z, 4);
        }

        // Accumulate face normals
        for (uint32_t t = 0; t + 2 < idx_count; t += 3) {
                uint32_t i0 = pIdxData[t], i1 = pIdxData[t+1], i2 = pIdxData[t+2];

                auto ReadPos = [&](uint32_t idx) -> glm::vec3 {
                        float x, y, z;
                        std::memcpy(&x, pVertData + idx * stride + pPosAttr->offset + 0, 4);
                        std::memcpy(&y, pVertData + idx * stride + pPosAttr->offset + 4, 4);
                        std::memcpy(&z, pVertData + idx * stride + pPosAttr->offset + 8, 4);
                        return { x, y, z };
                };

                glm::vec3 p0 = ReadPos(i0), p1 = ReadPos(i1), p2 = ReadPos(i2);
                glm::vec3 fn = glm::cross(p1 - p0, p2 - p0);

                for (uint32_t idx : { i0, i1, i2 }) {
                        float nx, ny, nz;
                        std::memcpy(&nx, pVertMut + idx * stride + pNrmAttr->offset + 0, 4);
                        std::memcpy(&ny, pVertMut + idx * stride + pNrmAttr->offset + 4, 4);
                        std::memcpy(&nz, pVertMut + idx * stride + pNrmAttr->offset + 8, 4);
                        nx += fn.x; ny += fn.y; nz += fn.z;
                        std::memcpy(pVertMut + idx * stride + pNrmAttr->offset + 0, &nx, 4);
                        std::memcpy(pVertMut + idx * stride + pNrmAttr->offset + 4, &ny, 4);
                        std::memcpy(pVertMut + idx * stride + pNrmAttr->offset + 8, &nz, 4);
                }
        }

        // Normalize
        for (uint32_t i = 0; i < vert_count; ++i) {
                float nx, ny, nz;
                std::memcpy(&nx, pVertMut + i * stride + pNrmAttr->offset + 0, 4);
                std::memcpy(&ny, pVertMut + i * stride + pNrmAttr->offset + 4, 4);
                std::memcpy(&nz, pVertMut + i * stride + pNrmAttr->offset + 8, 4);
                float len = std::sqrt(nx*nx + ny*ny + nz*nz);
                if (len > 1e-8f) { nx /= len; ny /= len; nz /= len; }
                std::memcpy(pVertMut + i * stride + pNrmAttr->offset + 0, &nx, 4);
                std::memcpy(pVertMut + i * stride + pNrmAttr->offset + 4, &ny, 4);
                std::memcpy(pVertMut + i * stride + pNrmAttr->offset + 8, &nz, 4);
        }
}

// ---------------------------------------------------------------------------
// ComputeTangents
// ---------------------------------------------------------------------------
void ComputeTangents(MeshBuilder & b) {
        const VertexLayout &        layout    = b.Layout();
        const VertexAttributeDesc * pPosAttr  = layout.Find(VertexAttrib::Position);
        const VertexAttributeDesc * pUvAttr   = layout.Find(VertexAttrib::TexCoord0);
        const VertexAttributeDesc * pTanAttr  = layout.Find(VertexAttrib::Tangent);
        if (!pPosAttr || !pUvAttr || !pTanAttr) return;

        const uint32_t vert_count  = b.CurrentVertexIndex();
        const uint32_t idx_count   = b.IndexCount();
        const uint32_t stride      = layout.stride;
        const uint8_t  * pVertData = b.VertexData();
        uint8_t        * pVertMut  = b.VertexDataMutable();
        const uint32_t * pIdxData  = b.IndexData();

        // Reset tangents
        for (uint32_t i = 0; i < vert_count; ++i) {
                float z = 0.0f;
                std::memcpy(pVertMut + i * stride + pTanAttr->offset + 0, &z, 4);
                std::memcpy(pVertMut + i * stride + pTanAttr->offset + 4, &z, 4);
                std::memcpy(pVertMut + i * stride + pTanAttr->offset + 8, &z, 4);
        }

        auto ReadVec3 = [&](const uint8_t * pBase, uint32_t off) -> glm::vec3 {
                float x, y, z;
                std::memcpy(&x, pBase + off + 0, 4);
                std::memcpy(&y, pBase + off + 4, 4);
                std::memcpy(&z, pBase + off + 8, 4);
                return { x, y, z };
        };
        auto ReadVec2 = [&](const uint8_t * pBase, uint32_t off) -> glm::vec2 {
                float x, y;
                std::memcpy(&x, pBase + off + 0, 4);
                std::memcpy(&y, pBase + off + 4, 4);
                return { x, y };
        };

        for (uint32_t t = 0; t + 2 < idx_count; t += 3) {
                uint32_t i0 = pIdxData[t], i1 = pIdxData[t+1], i2 = pIdxData[t+2];

                glm::vec3 p0 = ReadVec3(pVertData, i0 * stride + pPosAttr->offset);
                glm::vec3 p1 = ReadVec3(pVertData, i1 * stride + pPosAttr->offset);
                glm::vec3 p2 = ReadVec3(pVertData, i2 * stride + pPosAttr->offset);
                glm::vec2 u0 = ReadVec2(pVertData, i0 * stride + pUvAttr->offset);
                glm::vec2 u1 = ReadVec2(pVertData, i1 * stride + pUvAttr->offset);
                glm::vec2 u2 = ReadVec2(pVertData, i2 * stride + pUvAttr->offset);

                glm::vec3 dP1 = p1 - p0, dP2 = p2 - p0;
                glm::vec2 dU1 = u1 - u0, dU2 = u2 - u0;

                float det = dU1.x * dU2.y - dU1.y * dU2.x;
                glm::vec3 oTan(0.0f);
                if (std::abs(det) > 1e-8f) {
                        float inv = 1.0f / det;
                        oTan = inv * (dU2.y * dP1 - dU1.y * dP2);
                }

                for (uint32_t idx : { i0, i1, i2 }) {
                        glm::vec3 oCur = ReadVec3(pVertMut, idx * stride + pTanAttr->offset);
                        oCur += oTan;
                        std::memcpy(pVertMut + idx * stride + pTanAttr->offset + 0, &oCur.x, 4);
                        std::memcpy(pVertMut + idx * stride + pTanAttr->offset + 4, &oCur.y, 4);
                        std::memcpy(pVertMut + idx * stride + pTanAttr->offset + 8, &oCur.z, 4);
                }
        }

        // Normalize
        for (uint32_t i = 0; i < vert_count; ++i) {
                glm::vec3 oT = ReadVec3(pVertMut, i * stride + pTanAttr->offset);
                float len = std::sqrt(oT.x*oT.x + oT.y*oT.y + oT.z*oT.z);
                if (len > 1e-8f) { oT.x /= len; oT.y /= len; oT.z /= len; }
                std::memcpy(pVertMut + i * stride + pTanAttr->offset + 0, &oT.x, 4);
                std::memcpy(pVertMut + i * stride + pTanAttr->offset + 4, &oT.y, 4);
                std::memcpy(pVertMut + i * stride + pTanAttr->offset + 8, &oT.z, 4);
        }
}

} // namespace MeshGen
} // namespace SE

#endif // SE_IMPL
