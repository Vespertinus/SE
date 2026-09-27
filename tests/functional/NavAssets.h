
#ifndef SE_NAV_TEST_ASSETS_H
#define SE_NAV_TEST_ASSETS_H 1

// In-memory fixtures for the NavBake tests (TerrainAssets.h pattern): analytic
// geometry, no committed binary assets. Query helpers wrap a dtNavMesh built
// from SOutput tile bytes — the exact bytes Step 7's NavMeshAsset will hand to
// dtNavMesh::addTile.

#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>
#include <DetourNavMeshBuilder.h>

#include <nav/NavBake.h>

#include <glm/glm.hpp>

#include <cmath>
#include <cstring>
#include <set>
#include <memory>
#include <vector>

namespace se_nav_test {

using namespace SE;

// ---------------------------------------------------------------------------
// geometry
// ---------------------------------------------------------------------------

// Horizontal quad (2 CCW-up triangles) covering [x0..x1] x [z0..z1] at height y.
inline void AddQuad(NavBake::SGeomMesh & mesh, float x0, float z0, float x1, float z1, float y) {

        const uint32_t base = static_cast<uint32_t>(mesh.vVertices.size() / 3);
        mesh.vVertices.insert(mesh.vVertices.end(), {
                x0, y, z0,   x1, y, z0,   x1, y, z1,   x0, y, z1,
        });
        mesh.vIndices.insert(mesh.vIndices.end(), { base + 0u, base + 2u, base + 1u, base + 0u, base + 3u, base + 2u });
}

// Axis-aligned box with a top face plus 4 sides (walls matter for nav height).
inline void AddBox(NavBake::SGeomMesh & mesh, float x0, float z0, float x1, float z1, float y_top, float y_bottom = -1.0f) {

        AddQuad(mesh, x0, z0, x1, z1, y_top);
        // sides (winding: normals point outward; only the up-facing test cares about the top)
        const uint32_t b = static_cast<uint32_t>(mesh.vVertices.size() / 3);
        const float v[8][3] = {
                { x0, y_top, z0 }, { x1, y_top, z0 }, { x1, y_top, z1 }, { x0, y_top, z1 },
                { x0, y_bottom, z0 }, { x1, y_bottom, z0 }, { x1, y_bottom, z1 }, { x0, y_bottom, z1 },
        };
        for (const auto & p : v) {
                mesh.vVertices.insert(mesh.vVertices.end(), { p[0], p[1], p[2] });
        }
        const uint32_t quads[4][4] = { { 0, 1, 5, 4 }, { 1, 2, 6, 5 }, { 2, 3, 7, 6 }, { 3, 0, 4, 7 } };
        for (const auto & q : quads) {
                mesh.vIndices.insert(mesh.vIndices.end(), { b + q[0], b + q[1], b + q[2], b + q[0], b + q[2], b + q[3] });
        }
}

// Heightfield cells triangulated with the SAME diagonal the bake's slope test
// uses (p00 → p11) — the mesh-mode side of the cross-mode agreement test.
inline NavBake::SGeomMesh HeightfieldToMesh(const NavBake::SGeomHeightfield & hf) {

        NavBake::SGeomMesh out;
        const float step = hf.tile_size / static_cast<float>(hf.resolution - 1);
        for (uint32_t i = 0; i < hf.resolution; ++i) {
                for (uint32_t j = 0; j < hf.resolution; ++j) {
                        out.vVertices.push_back(hf.origin.x + j * step);
                        out.vVertices.push_back(hf.vHeights[size_t(i) * hf.resolution + j]);
                        out.vVertices.push_back(hf.origin.z + i * step);
                }
        }
        for (uint32_t i = 0; i + 1 < hf.resolution; ++i) {
                for (uint32_t j = 0; j + 1 < hf.resolution; ++j) {
                        const uint32_t p00 = i * hf.resolution + j;
                        const uint32_t p10 = i * hf.resolution + j + 1;
                        const uint32_t p01 = (i + 1) * hf.resolution + j;
                        const uint32_t p11 = (i + 1) * hf.resolution + j + 1;
                        out.vIndices.insert(out.vIndices.end(), { p00, p01, p11, p00, p11, p10 });
                }
        }
        return out;
}

// ---------------------------------------------------------------------------
// config / bake
// ---------------------------------------------------------------------------

inline NavBake::SAgentClass DefaultAgent(float radius = 0.5f) {

        NavBake::SAgentClass a;
        a.name = "default";
        a.radius = radius;
        a.height = 2.0f;
        a.max_slope_deg = 45.0f;
        a.step_height = 0.4f;
        return a;
}

inline NavBake::SConfig MakeConfig(float tile_dim = 8.0f, std::vector<std::string> vAreas = { "" }) {

        NavBake::SConfig cfg;
        cfg.world_id = "nav_test";
        cfg.agent = DefaultAgent();
        cfg.tile_dim = tile_dim;
        cfg.cell_size = 0.0f;   // divisibility is a world-mode concern; tests bake worldless
        cfg.vAreas = std::move(vAreas);
        return cfg;
}

inline NavBake::SGroup GroundGroup(float x0, float z0, float x1, float z1, float y = 0.0f) {

        NavBake::SGroup g;
        g.id = "ground";
        g.bounds_min = glm::vec3(x0, y - 1.0f, z0);
        g.bounds_max = glm::vec3(x1, y + 3.0f, z1);
        AddQuad(g.vMeshes.emplace_back(), x0, z0, x1, z1, y);
        return g;
}

// ---------------------------------------------------------------------------
// runtime side: build a dtNavMesh from baked bytes + query helpers
// ---------------------------------------------------------------------------

struct SNavRuntime {
        std::unique_ptr<dtNavMesh> pMesh;
        std::unique_ptr<dtNavMeshQuery> pQuery;
        std::vector<std::pair<uint32_t, uint32_t>> vCells;   // claimed cells (x, y)
};

inline SNavRuntime InitRuntime(const NavBake::SOutput & out) {

        SNavRuntime rt;
        dtNavMeshParams params;
        memset(&params, 0, sizeof(params));
        params.orig[0] = out.origin.x;
        params.orig[1] = out.origin.y;
        params.orig[2] = out.origin.z;
        params.tileWidth = out.tile_dim;
        params.tileHeight = out.tile_dim;
        params.maxTiles = 4096;
        params.maxPolys = 32768;
        rt.pMesh.reset(dtAllocNavMesh());
        (void)rt.pMesh->init(&params);

        for (const NavBake::SNavTileBytes & t : out.vTiles) {
                // Detour takes ownership: allocate in its allocator, mark FREE_DATA
                unsigned char * pData = reinterpret_cast<unsigned char *>(dtAlloc(t.vBytes.size(), DT_ALLOC_PERM));
                memcpy(pData, t.vBytes.data(), t.vBytes.size());
                dtTileRef ref = 0;
                (void)rt.pMesh->addTile(pData, static_cast<int>(t.vBytes.size()), DT_TILE_FREE_DATA, 0, &ref);
                rt.vCells.emplace_back(t.x, t.y);
        }
        rt.pQuery.reset(dtAllocNavMeshQuery());
        rt.pQuery->init(rt.pMesh.get(), 4096);
        return rt;
}

inline dtPolyRef Nearest(const SNavRuntime & rt, const glm::vec3 & pos, const glm::vec3 & ext = glm::vec3(1.0f, 2.0f, 1.0f)) {

        dtQueryFilter filter;
        filter.setIncludeFlags(0xffff);
        filter.setExcludeFlags(0);
        float pt[3] = { pos.x, pos.y, pos.z };
        const float e[3] = { ext.x, ext.y, ext.z };
        dtPolyRef ref = 0;
        float out[3];
        rt.pQuery->findNearestPoly(pt, e, &filter, &ref, out);
        return ref;
}

// Returns false when the path does not reach the target.
inline bool FindPath(const SNavRuntime & rt, const glm::vec3 & from, const glm::vec3 & to) {

        dtQueryFilter filter;
        filter.setIncludeFlags(0xffff);
        filter.setExcludeFlags(0);
        const float a[3] = { from.x, from.y, from.z };
        const float b[3] = { to.x, to.y, to.z };
        dtPolyRef sref = 0, eref = 0;
        float spt[3], ept[3];
        const float e[3] = { 1.0f, 4.0f, 1.0f };
        rt.pQuery->findNearestPoly(a, e, &filter, &sref, spt);
        rt.pQuery->findNearestPoly(b, e, &filter, &eref, ept);
        if (!sref || !eref) { return false; }
        dtPolyRef path[256];
        int count = 0;
        rt.pQuery->findPath(sref, eref, spt, ept, &filter, path, &count, 256);
        return count > 0 && path[count - 1] == eref;
}

// Distinct poly area bytes across all added tiles (layer 0 excluded automatically
// by the caller's expectations — layers are part of the tile set).
inline std::set<unsigned char> AreaBytes(const SNavRuntime & rt) {

        std::set<unsigned char> areas;
        const dtMeshTile * tiles[16];
        for (const auto & cell : rt.vCells) {
                const int n = rt.pMesh->getTilesAt(static_cast<int>(cell.first), static_cast<int>(cell.second), tiles, 16);
                for (int i = 0; i < n; ++i) {
                        for (int p = 0; p < tiles[i]->header->polyCount; ++p) {
                                areas.insert(tiles[i]->polys[p].getArea());
                        }
                }
        }
        return areas;
}

// Poly centers (world) across all tiles — for hole assertions.
inline std::vector<glm::vec3> PolyCenters(const SNavRuntime & rt) {

        std::vector<glm::vec3> centers;
        const dtMeshTile * tiles[16];
        for (const auto & cell : rt.vCells) {
                const int n = rt.pMesh->getTilesAt(static_cast<int>(cell.first), static_cast<int>(cell.second), tiles, 16);
                for (int i = 0; i < n; ++i) {
                        const dtMeshTile * pTile = tiles[i];
                        for (int p = 0; p < pTile->header->polyCount; ++p) {
                                const dtPoly & poly = pTile->polys[p];
                                glm::vec3 acc(0.0f);
                                for (int j = 0; j < poly.vertCount; ++j) {
                                        const float * v = &pTile->verts[poly.verts[j] * 3];
                                        acc += glm::vec3(v[0], v[1], v[2]);
                                }
                                centers.push_back(acc / static_cast<float>(poly.vertCount));
                        }
                }
        }
        return centers;
}

} // namespace se_nav_test

#endif
