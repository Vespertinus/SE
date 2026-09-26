
#ifdef SE_IMPL

#include <nav/NavBake.h>

#include <Recast.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>

#include <Logging.h>

#include <terrain/TerrainQuery.h>   // IsWalkable — shared with runtime (step-3 contract)

#include <algorithm>
#include <cmath>
#include <cstring>

namespace SE {
namespace NavBake {

namespace {

// Surface Recast's error-level logs through the house logger; progress spam stays silent.
struct SRecastContext : rcContext {
        void doLog(const rcLogCategory cat, const char * msg, const int len) override {
                (void)len;
                if (cat == RC_LOG_ERROR) { log_e("NavBake/Recast: {}", msg); }
        }
};

// Poly area bytes in tile data: 0 = null (excluded), registry_index + 1 otherwise.
constexpr unsigned char kGroundAreaByte = 1;

constexpr float kClaimEps = 1e-4f;

float SnapDown(float v, float dim) {
        return std::floor(v / dim) * dim;
}

glm::vec3 TriNormal(const glm::vec3 & a, const glm::vec3 & b, const glm::vec3 & c) {
        return glm::normalize(glm::cross(b - a, c - a));
}

bool PointInBox(const glm::vec3 & p, const glm::vec3 & center, const glm::vec3 & half) {
        return glm::all(glm::greaterThanEqual(p, center - half)) &&
               glm::all(glm::lessThanEqual(p, center + half));
}

bool TriOverlapAabb(const glm::vec3 & ta, const glm::vec3 & tb, const glm::vec3 & tc,
                    const float * bmin, const float * bmax) {
        const float tminx = glm::min(ta.x, glm::min(tb.x, tc.x));
        const float tmaxx = glm::max(ta.x, glm::max(tb.x, tc.x));
        const float tminy = glm::min(ta.y, glm::min(tb.y, tc.y));
        const float tmaxy = glm::max(ta.y, glm::max(tb.y, tc.y));
        const float tminz = glm::min(ta.z, glm::min(tb.z, tc.z));
        const float tmaxz = glm::max(ta.z, glm::max(tb.z, tc.z));
        return tminx <= bmax[0] && tmaxx >= bmin[0] &&
               tminy <= bmax[1] && tmaxy >= bmin[1] &&
               tminz <= bmax[2] && tmaxz >= bmin[2];
}

// Height at (wx, wz) + walkability of the containing quad (both triangle normals
// must pass the shared slope predicate — an agent never crosses a too-steep part
// of a quad). Returns false when the point is outside every terrain tile.
bool SampleTerrain(const std::vector<const SGeomHeightfield *> & vHfs, float wx, float wz,
                   float max_slope_deg, float & out_h, bool & out_walkable,
                   const SGeomHeightfield ** out_hf, uint32_t & out_cell) {

        for (const SGeomHeightfield * p : vHfs) {
                const SGeomHeightfield & hf = *p;
                if (hf.resolution < 2 || hf.vHeights.empty()) { continue; }
                if (wx < hf.origin.x || wz < hf.origin.z) { continue; }
                const float step = hf.tile_size / static_cast<float>(hf.resolution - 1);
                if (wx >= hf.origin.x + hf.tile_size || wz >= hf.origin.z + hf.tile_size) { continue; }

                const float fx = (wx - hf.origin.x) / step;
                const float fz = (wz - hf.origin.z) / step;
                const uint32_t j = glm::min(static_cast<uint32_t>(fx), hf.resolution - 2u);
                const uint32_t i = glm::min(static_cast<uint32_t>(fz), hf.resolution - 2u);
                const float tx = fx - static_cast<float>(j);
                const float tz = fz - static_cast<float>(i);

                const float h00 = hf.vHeights[static_cast<size_t>(i) * hf.resolution + j];
                const float h10 = hf.vHeights[static_cast<size_t>(i) * hf.resolution + j + 1];
                const float h01 = hf.vHeights[static_cast<size_t>(i + 1) * hf.resolution + j];
                const float h11 = hf.vHeights[static_cast<size_t>(i + 1) * hf.resolution + j + 1];

                const float h0 = h00 + (h10 - h00) * tx;
                const float h1 = h01 + (h11 - h01) * tx;
                out_h = h0 + (h1 - h0) * tz;

                // quad corners on the Terrain.fbs corner grid; diagonal p00 -> p11
                const glm::vec3 p00(0.f, h00, 0.f);
                const glm::vec3 p10(step, h10, 0.f);
                const glm::vec3 p01(0.f, h01, step);
                const glm::vec3 p11(step, h11, step);
                out_walkable = TerrainQuery::IsWalkable(TriNormal(p00, p01, p11), max_slope_deg) &&
                               TerrainQuery::IsWalkable(TriNormal(p00, p11, p10), max_slope_deg);

                *out_hf = &hf;
                out_cell = i * (hf.resolution - 1u) + j;
                return true;
        }
        return false;
}

// Bake-wide geometry view for one coexisting-flag class (normal / interior).
// Every tile build samples all same-flag groups' geometry inside its build
// bbox (tile + borderSize ring): Detour links two grid-adjacent tiles only
// through portal edges on their shared boundary, and erosion pulls walkable
// space walkableRadius back from the edge of the build input — so a group
// seam links only when walkable data reaches past the boundary on BOTH sides.
// Terrain partitioned between disjoint sub-level boxes provides that data to
// neither side on its own; the neighbor's ground in the border band does.
struct SScopeGeom {

        struct SHfRef {
                const SGeomHeightfield * p;
                glm::vec3 bmin;
                glm::vec3 bmax;
        };
        struct SMeshRef {
                const SGeomMesh * p;
                glm::vec3 bmin;
                glm::vec3 bmax;
        };

        std::vector<SHfRef>            vHfs;
        std::vector<SMeshRef>          vMeshes;
        std::vector<const SAreaBox *>  pExclusions;
};

// One nav tile: rasterize/fill, build the poly mesh, serialize to Detour bytes.
// The build input is the owner's scope (all same-flag groups' geometry within
// the build bbox); oGroup still decides named areas, off-mesh links, and the
// output tile's group id. Returns false (and logs) on internal bake errors;
// produces no output when the tile ends up with zero polygons (empty space).
bool BakeTile(const SGroup & oGroup, const SScopeGeom & oScope, uint16_t layer,
              uint32_t tx, uint32_t ty,
              const SConfig & oCfg, const glm::vec3 & grid_origin,
              float y_min, float y_max,
              const std::vector<SAreaBox> & vAutoExclude,
              SNavTileBytes & out, uint32_t & polys_out, uint32_t & verts_out) {

        const SAgentClass & agent = oCfg.agent;

        rcConfig cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.cs  = oCfg.voxel_size;
        cfg.ch  = oCfg.voxel_height;
        cfg.walkableSlopeAngle = agent.max_slope_deg;
        cfg.walkableHeight = static_cast<int>(std::ceil(agent.height / oCfg.voxel_height));
        cfg.walkableClimb  = static_cast<int>(std::floor(agent.step_height / oCfg.voxel_height));
        cfg.walkableRadius = static_cast<int>(std::ceil(agent.radius / oCfg.voxel_size));
        cfg.maxEdgeLen = static_cast<int>(oCfg.max_edge_len / oCfg.voxel_size);
        cfg.maxSimplificationError = oCfg.max_contour_error;
        cfg.minRegionArea  = static_cast<int>(oCfg.min_region_area);
        cfg.mergeRegionArea = static_cast<int>(oCfg.merge_region_area);
        cfg.maxVertsPerPoly = 6;
        cfg.tileSize = static_cast<int>(oCfg.tile_dim / oCfg.voxel_size);
        cfg.borderSize = cfg.walkableRadius + 3;
        cfg.width  = cfg.tileSize + cfg.borderSize * 2;
        cfg.height = cfg.tileSize + cfg.borderSize * 2;

        const float ox = grid_origin.x + static_cast<float>(tx) * oCfg.tile_dim;
        const float oz = grid_origin.z + static_cast<float>(ty) * oCfg.tile_dim;
        cfg.bmin[0] = ox - cfg.borderSize * oCfg.voxel_size;
        cfg.bmin[1] = y_min - 1.0f;
        cfg.bmin[2] = oz - cfg.borderSize * oCfg.voxel_size;
        cfg.bmax[0] = ox + oCfg.tile_dim + cfg.borderSize * oCfg.voxel_size;
        cfg.bmax[1] = y_max + 1.0f;
        cfg.bmax[2] = oz + oCfg.tile_dim + cfg.borderSize * oCfg.voxel_size;

        SRecastContext ctx;

        rcHeightfield * pSolid = rcAllocHeightfield();
        if (!rcCreateHeightfield(&ctx, *pSolid, cfg.width, cfg.height, cfg.bmin, cfg.bmax, cfg.cs, cfg.ch)) {
                log_e("NavBake: tile ({}, {}) [{}]: rcCreateHeightfield failed", tx, ty, oGroup.id);
                rcFreeHeightField(pSolid);
                return false;
        }

        // --- heightfield spans (direct fill, no triangle rasterization) ---------
        {
                // scope prefilter: terrain tiles overlapping the build bbox XZ
                std::vector<const SGeomHeightfield *> vLocal;
                for (const SScopeGeom::SHfRef & r : oScope.vHfs) {
                        if (r.bmin.x >= cfg.bmax[0] || r.bmax.x <= cfg.bmin[0] ||
                            r.bmin.z >= cfg.bmax[2] || r.bmax.z <= cfg.bmin[2]) { continue; }
                        vLocal.push_back(r.p);
                }
                for (int vz = 0; vz < cfg.height; ++vz) {
                        for (int vx = 0; vx < cfg.width; ++vx) {
                                const float wx = cfg.bmin[0] + (static_cast<float>(vx) + 0.5f) * cfg.cs;
                                const float wz = cfg.bmin[2] + (static_cast<float>(vz) + 0.5f) * cfg.cs;

                                float h = 0.0f;
                                bool walkable = false;
                                const SGeomHeightfield * pHf = nullptr;
                                uint32_t cell = 0;
                                if (!SampleTerrain(vLocal, wx, wz, agent.max_slope_deg, h, walkable, &pHf, cell)) {
                                        continue;
                                }
                                if (!walkable) {
                                        continue;   // unwalkable quad (cliff wall): no walkable span here
                                }

                                // hole mask: bit per cell, 1 = hole (deferred consumer, honored when authored)
                                if (!pHf->vHoleMask.empty()) {
                                        if ((pHf->vHoleMask[cell >> 3] & (1u << (cell & 7u))) != 0) { continue; }
                                }

                                int smax = static_cast<int>(std::floor((h - cfg.bmin[1]) / cfg.ch));
                                if (smax < 1) { smax = 1; }
                                if (smax > RC_SPAN_MAX_HEIGHT) { smax = RC_SPAN_MAX_HEIGHT; }
                                rcAddSpan(&ctx, *pSolid, vx, vz, 0, static_cast<unsigned short>(smax), kGroundAreaByte, 1);
                        }
                }
        }

        // --- mesh triangles ------------------------------------------------------
        {
                std::vector<float>          vVerts;
                std::vector<int>            vTris;
                std::vector<unsigned char>  vAreas;
                vVerts.reserve(4096 * 3);

                for (const SScopeGeom::SMeshRef & r : oScope.vMeshes) {
                        // scope prefilter: mesh AABB against the build bbox
                        if (r.bmin.x >= cfg.bmax[0] || r.bmax.x <= cfg.bmin[0] ||
                            r.bmin.y >= cfg.bmax[1] || r.bmax.y <= cfg.bmin[1] ||
                            r.bmin.z >= cfg.bmax[2] || r.bmax.z <= cfg.bmin[2]) { continue; }
                        const SGeomMesh & oMesh = *r.p;
                        const size_t tri_count = oMesh.vIndices.size() / 3;
                        for (size_t t = 0; t < tri_count; ++t) {
                                const uint32_t ia = oMesh.vIndices[t * 3 + 0];
                                const uint32_t ib = oMesh.vIndices[t * 3 + 1];
                                const uint32_t ic = oMesh.vIndices[t * 3 + 2];
                                const glm::vec3 va(oMesh.vVertices[ia * 3], oMesh.vVertices[ia * 3 + 1], oMesh.vVertices[ia * 3 + 2]);
                                const glm::vec3 vb(oMesh.vVertices[ib * 3], oMesh.vVertices[ib * 3 + 1], oMesh.vVertices[ib * 3 + 2]);
                                const glm::vec3 vc(oMesh.vVertices[ic * 3], oMesh.vVertices[ic * 3 + 1], oMesh.vVertices[ic * 3 + 2]);

                                if (!TriOverlapAabb(va, vb, vc, cfg.bmin, cfg.bmax)) { continue; }

                                const int base = static_cast<int>(vVerts.size() / 3);
                                vVerts.insert(vVerts.end(), { va.x, va.y, va.z, vb.x, vb.y, vb.z, vc.x, vc.y, vc.z });
                                vTris.insert(vTris.end(), { base, base + 1, base + 2 });
                                vAreas.push_back(kGroundAreaByte);
                        }
                }

                if (!vTris.empty()) {
                        rcClearUnwalkableTriangles(&ctx, cfg.walkableSlopeAngle, vVerts.data(),
                                                   static_cast<int>(vVerts.size() / 3), vTris.data(),
                                                   static_cast<int>(vTris.size() / 3), vAreas.data());
                        rcRasterizeTriangles(&ctx, vVerts.data(), static_cast<int>(vVerts.size() / 3),
                                             vTris.data(), vAreas.data(),
                                             static_cast<int>(vTris.size() / 3), *pSolid, 1);
                }
        }

        // --- exclusion volumes (column-granular, tessellation-independent) -------
        // Nulls spans whose TOP lies inside the volume: y-range respected (an
        // elevated box leaves the ground under it walkable; a ground box kills it).
        auto ExcludeColumns = [&cfg, pSolid](const SAreaBox & oBox) {
                for (int vz = 0; vz < cfg.height; ++vz) {
                        for (int vx = 0; vx < cfg.width; ++vx) {
                                const float wx = cfg.bmin[0] + (static_cast<float>(vx) + 0.5f) * cfg.cs;
                                const float wz = cfg.bmin[2] + (static_cast<float>(vz) + 0.5f) * cfg.cs;
                                if (wx < oBox.center.x - oBox.half_extents.x || wx > oBox.center.x + oBox.half_extents.x ||
                                    wz < oBox.center.z - oBox.half_extents.z || wz > oBox.center.z + oBox.half_extents.z) {
                                        continue;
                                }
                                for (rcSpan * s = pSolid->spans[vz * cfg.width + vx]; s != nullptr; s = s->next) {
                                        const float top = cfg.bmin[1] + static_cast<float>(s->smax) * cfg.ch;
                                        if (top >= oBox.center.y - oBox.half_extents.y && top <= oBox.center.y + oBox.half_extents.y) {
                                                s->area = RC_NULL_AREA;
                                        }
                                }
                        }
                }
        };
        for (const SAreaBox * pBox : oScope.pExclusions) { ExcludeColumns(*pBox); }
        for (const SAreaBox & oBox : vAutoExclude) { ExcludeColumns(oBox); }

        // --- shared pipeline ------------------------------------------------------
        rcFilterLowHangingWalkableObstacles(&ctx, cfg.walkableClimb, *pSolid);
        rcFilterLedgeSpans(&ctx, cfg.walkableHeight, cfg.walkableClimb, *pSolid);
        rcFilterWalkableLowHeightSpans(&ctx, cfg.walkableHeight, *pSolid);

        rcCompactHeightfield * pChf = rcAllocCompactHeightfield();
        if (!rcBuildCompactHeightfield(&ctx, cfg.walkableHeight, cfg.walkableClimb, *pSolid, *pChf)) {
                log_e("NavBake: tile ({}, {}) [{}]: rcBuildCompactHeightfield failed", tx, ty, oGroup.id);
                rcFreeHeightField(pSolid);
                rcFreeCompactHeightfield(pChf);
                return false;
        }
        rcFreeHeightField(pSolid);

        if (!rcErodeWalkableArea(&ctx, cfg.walkableRadius, *pChf)) {
                log_e("NavBake: tile ({}, {}) [{}]: rcErodeWalkableArea failed", tx, ty, oGroup.id);
                rcFreeCompactHeightfield(pChf);
                return false;
        }
        if (!rcBuildDistanceField(&ctx, *pChf)) {
                log_e("NavBake: tile ({}, {}) [{}]: rcBuildDistanceField failed", tx, ty, oGroup.id);
                rcFreeCompactHeightfield(pChf);
                return false;
        }
        // borderSize here is what marks border regions — without it no edge ever
        // becomes a portal, and separately-baked tiles never link (spike-verified)
        if (!rcBuildRegions(&ctx, *pChf, cfg.borderSize, cfg.minRegionArea, cfg.mergeRegionArea)) {
                log_e("NavBake: tile ({}, {}) [{}]: rcBuildRegions failed", tx, ty, oGroup.id);
                rcFreeCompactHeightfield(pChf);
                return false;
        }

        rcContourSet * pCset = rcAllocContourSet();
        if (!rcBuildContours(&ctx, *pChf, cfg.maxSimplificationError, cfg.maxEdgeLen, *pCset, RC_CONTOUR_TESS_WALL_EDGES)) {
                log_e("NavBake: tile ({}, {}) [{}]: rcBuildContours failed", tx, ty, oGroup.id);
                rcFreeCompactHeightfield(pChf);
                rcFreeContourSet(pCset);
                return false;
        }

        rcPolyMesh * pMesh = rcAllocPolyMesh();
        if (!rcBuildPolyMesh(&ctx, *pCset, cfg.maxVertsPerPoly, *pMesh)) {
                log_e("NavBake: tile ({}, {}) [{}]: rcBuildPolyMesh failed", tx, ty, oGroup.id);
                rcFreeCompactHeightfield(pChf);
                rcFreeContourSet(pCset);
                rcFreePolyMesh(pMesh);
                return false;
        }

        if (pMesh->npolys == 0) {
                rcFreePolyMesh(pMesh);
                rcFreeContourSet(pCset);
                rcFreeCompactHeightfield(pChf);
                return true;   // empty tile — omitted from output
        }

        rcPolyMeshDetail * pDetail = rcAllocPolyMeshDetail();
        rcBuildPolyMeshDetail(&ctx, *pMesh, *pChf, oCfg.voxel_size * 6.0f, oCfg.voxel_height * 1.0f, *pDetail);

        // --- named area tagging (poly centers) -----------------------------------
        // rcPolyMesh verts are unsigned-short voxel coords relative to pmesh->bmin
        // (dtCreateNavMeshData converts them back with bmin + iv * cs/ch)
        for (int p = 0; p < pMesh->npolys; ++p) {
                const unsigned short * pv = &pMesh->polys[p * cfg.maxVertsPerPoly * 2];
                glm::vec3 center(0.0f);
                int nv = 0;
                for (int j = 0; j < cfg.maxVertsPerPoly; ++j) {
                        if (pv[j] == RC_MESH_NULL_IDX) { break; }
                        const unsigned short vi = pv[j];
                        center.x += pMesh->bmin[0] + static_cast<float>(pMesh->verts[vi * 3 + 0]) * pMesh->cs;
                        center.y += pMesh->bmin[1] + static_cast<float>(pMesh->verts[vi * 3 + 1]) * pMesh->ch;
                        center.z += pMesh->bmin[2] + static_cast<float>(pMesh->verts[vi * 3 + 2]) * pMesh->cs;
                        ++nv;
                }
                if (nv == 0) { continue; }
                center /= static_cast<float>(nv);

                for (const SAreaBox & oBox : oGroup.vAreaBoxes) {
                        if (!oBox.exclusion && PointInBox(center, oBox.center, oBox.half_extents)) {
                                pMesh->areas[p] = static_cast<unsigned char>(oBox.registry_index + 1);
                                break;
                        }
                }
        }

        // poly flags must be non-zero or query filters reject every poly
        for (int p = 0; p < pMesh->npolys; ++p) {
                pMesh->flags[p] = 1;   // walk
        }

        // --- off-mesh connections starting in this tile ---------------------------
        std::vector<float>          vConVerts;
        std::vector<float>          vConRads;
        std::vector<unsigned char>  vConDirs;
        std::vector<unsigned char>  vConAreas;
        std::vector<unsigned short> vConFlags;
        std::vector<unsigned int>   vConUsers;

        for (size_t li = 0; li < oGroup.vLinks.size(); ++li) {
                const SLink & oLink = oGroup.vLinks[li];
                if (oLink.start.x < ox || oLink.start.x >= ox + oCfg.tile_dim ||
                    oLink.start.z < oz || oLink.start.z >= oz + oCfg.tile_dim) {
                        continue;
                }
                vConVerts.insert(vConVerts.end(), { oLink.start.x, oLink.start.y, oLink.start.z,
                                                    oLink.end.x,   oLink.end.y,   oLink.end.z });
                vConRads.push_back(oLink.radius);
                vConDirs.push_back(oLink.bidirectional ? static_cast<unsigned char>(DT_OFFMESH_CON_BIDIR) : 0);
                vConAreas.push_back(static_cast<unsigned char>(oLink.registry_index + 1));
                vConFlags.push_back(oLink.flags);
                vConUsers.push_back(static_cast<unsigned int>(li));
        }

        // --- serialize -------------------------------------------------------------
        dtNavMeshCreateParams params;
        memset(&params, 0, sizeof(params));
        params.verts = pMesh->verts;
        params.vertCount = pMesh->nverts;
        params.polys = pMesh->polys;
        params.polyAreas = pMesh->areas;
        params.polyFlags = pMesh->flags;
        params.polyCount = pMesh->npolys;
        params.nvp = pMesh->nvp;
        if (pDetail && pDetail->ntris > 0) {
                params.detailMeshes = pDetail->meshes;
                params.detailVerts = pDetail->verts;
                params.detailVertsCount = pDetail->nverts;
                params.detailTris = pDetail->tris;
                params.detailTriCount = pDetail->ntris;
        }
        params.offMeshConVerts = vConVerts.data();
        params.offMeshConRad = vConRads.data();
        params.offMeshConDir = vConDirs.data();
        params.offMeshConAreas = vConAreas.data();
        params.offMeshConFlags = vConFlags.data();
        params.offMeshConUserID = vConUsers.data();
        params.offMeshConCount = static_cast<int>(vConRads.size());
        params.walkableHeight = agent.height;
        params.walkableRadius = agent.radius;
        params.walkableClimb = agent.step_height;
        memcpy(params.bmin, pMesh->bmin, sizeof(float) * 3);
        memcpy(params.bmax, pMesh->bmax, sizeof(float) * 3);
        params.cs = cfg.cs;
        params.ch = cfg.ch;
        params.tileX = static_cast<int>(tx);
        params.tileY = static_cast<int>(ty);
        params.tileLayer = layer;
        params.buildBvTree = true;

        // The builder rejects off-mesh connections whose start Y lies outside the
        // tile's tight height window (detail/poly vert heights ± walkableClimb).
        // Quantized surface heights can sit a fraction of a voxel off the authored
        // endpoint Y, so clamp the endpoints into the same window deterministically.
        if (!vConVerts.empty()) {
                float hmin = FLT_MAX, hmax = -FLT_MAX;
                if (pDetail && pDetail->nverts > 0) {
                        for (int v = 0; v < pDetail->nverts; ++v) {
                                hmin = glm::min(hmin, pDetail->verts[v * 3 + 1]);
                                hmax = glm::max(hmax, pDetail->verts[v * 3 + 1]);
                        }
                } else {
                        for (int v = 0; v < pMesh->nverts; ++v) {
                                const float h = params.bmin[1] + static_cast<float>(pMesh->verts[v * 3 + 1]) * pMesh->ch;
                                hmin = glm::min(hmin, h);
                                hmax = glm::max(hmax, h);
                        }
                }
                const float eps = agent.step_height * 0.25f;
                for (size_t c = 0; c < vConRads.size(); ++c) {
                        vConVerts[c * 6 + 1] = glm::clamp(vConVerts[c * 6 + 1], hmin + eps, hmax - eps);
                        vConVerts[c * 6 + 4] = glm::clamp(vConVerts[c * 6 + 4], hmin + eps, hmax - eps);
                }
        }

        unsigned char * pNavData = nullptr;
        int nav_size = 0;
        const bool ok = dtCreateNavMeshData(&params, &pNavData, &nav_size);
        if (ok) {
                out.x = tx;
                out.y = ty;
                out.layer = layer;
                out.group = oGroup.id;
                out.vBytes.assign(pNavData, pNavData + nav_size);
                polys_out += static_cast<uint32_t>(pMesh->npolys);
                verts_out += static_cast<uint32_t>(pMesh->nverts);
                dtFree(pNavData);
        } else {
                log_e("NavBake: tile ({}, {}) [{}]: dtCreateNavMeshData failed", tx, ty, oGroup.id);
        }

        rcFreePolyMeshDetail(pDetail);
        rcFreePolyMesh(pMesh);
        rcFreeContourSet(pCset);
        rcFreeCompactHeightfield(pChf);
        return ok;
}

} // namespace

// -----------------------------------------------------------------------------

bool Bake(const SBakeInput & oIn, const SConfig & oCfg, SOutput & out) {

        out = SOutput{};

        // --- config validation ---
        if (oCfg.agent.radius <= 0.0f || oCfg.agent.height <= 0.0f || oCfg.agent.step_height < 0.0f ||
            oCfg.agent.max_slope_deg <= 0.0f || oCfg.agent.max_slope_deg >= 90.0f) {
                log_e("NavBake: invalid agent class '{}' params", oCfg.agent.name);
                return false;
        }
        if (oCfg.tile_dim <= 0.0f || oCfg.voxel_size <= 0.0f || oCfg.voxel_height <= 0.0f) {
                log_e("NavBake: invalid tile/voxel dims");
                return false;
        }
        if (oCfg.cell_size > 0.0f) {
                const float frac = oCfg.cell_size / oCfg.tile_dim;
                if (std::abs(frac - std::round(frac)) > kClaimEps) {
                        log_e("NavBake: tile_dim {} must divide streaming cell_size {} integrally",
                              oCfg.tile_dim, oCfg.cell_size);
                        return false;
                }
        }
        if (oCfg.vAreas.empty() || !oCfg.vAreas[0].empty()) {
                log_e("NavBake: area registry must start with the ground entry \"\"");
                return false;
        }
        if (oCfg.vAreas.size() + 1 > 63) {
                log_e("NavBake: too many areas ({}); Recast allows 63 walkable ids", oCfg.vAreas.size());
                return false;
        }

        // --- input validation + geometry-bounds sanity ---
        for (const SGroup & oGroup : oIn.vGroups) {
                if (glm::any(glm::greaterThan(oGroup.bounds_min, oGroup.bounds_max))) {
                        log_e("NavBake: group '{}': bounds_min > bounds_max", oGroup.id);
                        return false;
                }
                for (const SAreaBox & oBox : oGroup.vAreaBoxes) {
                        if (glm::any(glm::lessThanEqual(oBox.half_extents, glm::vec3(0.0f)))) {
                                log_e("NavBake: group '{}': area box with empty half_extents", oGroup.id);
                                return false;
                        }
                        if (!oBox.exclusion && oBox.registry_index >= oCfg.vAreas.size()) {
                                log_e("NavBake: group '{}': area box registry index {} out of range",
                                      oGroup.id, oBox.registry_index);
                                return false;
                        }
                }
                for (const SLink & oLink : oGroup.vLinks) {
                        if (oLink.radius <= 0.0f || oLink.registry_index >= oCfg.vAreas.size()) {
                                log_e("NavBake: group '{}': invalid link (radius/area)", oGroup.id);
                                return false;
                        }
                }

                if (oGroup.vMeshes.empty() && oGroup.vHeightfields.empty()) { continue; }
                glm::vec3 gmin(FLT_MAX), gmax(-FLT_MAX);
                for (const SGeomMesh & oMesh : oGroup.vMeshes) {
                        for (size_t v = 0; v * 3 + 2 < oMesh.vVertices.size(); ++v) {
                                gmin = glm::min(gmin, glm::vec3(oMesh.vVertices[v * 3], oMesh.vVertices[v * 3 + 1], oMesh.vVertices[v * 3 + 2]));
                                gmax = glm::max(gmax, glm::vec3(oMesh.vVertices[v * 3], oMesh.vVertices[v * 3 + 1], oMesh.vVertices[v * 3 + 2]));
                        }
                }
                for (const SGeomHeightfield & hf : oGroup.vHeightfields) {
                        gmin = glm::min(gmin, hf.origin);
                        gmax = glm::max(gmax, hf.origin + glm::vec3(hf.tile_size, 0.0f, hf.tile_size));
                }
                if (glm::any(glm::lessThan(gmin, oGroup.bounds_min)) || glm::any(glm::greaterThan(gmax, oGroup.bounds_max))) {
                        log_w("NavBake: group '{}': geometry exceeds authored bounds — tiles may clip it "
                              "(content [{:.2f} {:.2f} {:.2f}..{:.2f} {:.2f} {:.2f}] vs bounds [{:.2f} {:.2f} {:.2f}..{:.2f} {:.2f} {:.2f}])",
                              oGroup.id,
                              gmin.x, gmin.y, gmin.z, gmax.x, gmax.y, gmax.z,
                              oGroup.bounds_min.x, oGroup.bounds_min.y, oGroup.bounds_min.z,
                              oGroup.bounds_max.x, oGroup.bounds_max.y, oGroup.bounds_max.z);
                }
                const_cast<SGroup &>(oGroup).content_min = gmin;
                const_cast<SGroup &>(oGroup).content_max = gmax;
                const_cast<SGroup &>(oGroup).bHasContent = true;
        }

        // --- grid extent from group bounds ---
        glm::vec3 bmin(FLT_MAX), bmax(-FLT_MAX);
        for (const SGroup & oGroup : oIn.vGroups) {
                bmin = glm::min(bmin, oGroup.bounds_min);
                bmax = glm::max(bmax, oGroup.bounds_max);
        }
        if (oIn.vGroups.empty() || bmin.x > bmax.x) {
                log_e("NavBake: no content groups to bake");
                return false;
        }

        out.tile_dim = oCfg.tile_dim;
        out.origin = glm::vec3(SnapDown(bmin.x, oCfg.tile_dim), 0.0f, SnapDown(bmin.z, oCfg.tile_dim));
        out.tiles_x = static_cast<uint32_t>(std::ceil((bmax.x - out.origin.x) / oCfg.tile_dim - kClaimEps));
        out.tiles_y = static_cast<uint32_t>(std::ceil((bmax.z - out.origin.z) / oCfg.tile_dim - kClaimEps));

        // --- tile claims ------------------------------------------------------------
        // per cell: one normal owner (exclusive), at most one coexisting group
        const size_t cell_count = static_cast<size_t>(out.tiles_x) * out.tiles_y;
        std::vector<int32_t> vOwner(cell_count, -1);
        std::vector<int32_t> vCoexist(cell_count, -1);

        uint16_t next_layer = 0;
        std::vector<uint16_t> vGroupLayers(oIn.vGroups.size(), 0);

        for (size_t g = 0; g < oIn.vGroups.size(); ++g) {
                const SGroup & oGroup = oIn.vGroups[g];
                if (oGroup.coexisting) {
                        if (next_layer == 0) { next_layer = 1; }
                        vGroupLayers[g] = next_layer++;
                        if (next_layer == 0) {
                                log_e("NavBake: too many coexisting groups (> {} layers)", 0xffff - 1);
                                return false;
                        }
                }
        }

        auto CellIndex = [&](uint32_t cx, uint32_t cy) -> size_t {
                return static_cast<size_t>(cy) * out.tiles_x + cx;
        };

        for (size_t g = 0; g < oIn.vGroups.size(); ++g) {
                const SGroup & oGroup = oIn.vGroups[g];

                int32_t cx0 = static_cast<int32_t>(std::floor((oGroup.bounds_min.x - out.origin.x) / oCfg.tile_dim + kClaimEps));
                int32_t cy0 = static_cast<int32_t>(std::floor((oGroup.bounds_min.z - out.origin.z) / oCfg.tile_dim + kClaimEps));
                int32_t cx1 = static_cast<int32_t>(std::ceil((oGroup.bounds_max.x - out.origin.x) / oCfg.tile_dim - kClaimEps));
                int32_t cy1 = static_cast<int32_t>(std::ceil((oGroup.bounds_max.z - out.origin.z) / oCfg.tile_dim - kClaimEps));
                cx0 = glm::clamp(cx0, 0, static_cast<int32_t>(out.tiles_x));
                cy0 = glm::clamp(cy0, 0, static_cast<int32_t>(out.tiles_y));
                cx1 = glm::clamp(cx1, 0, static_cast<int32_t>(out.tiles_x));
                cy1 = glm::clamp(cy1, 0, static_cast<int32_t>(out.tiles_y));

                // heightfield-bearing groups claim per content tile: a world
                // grid group keeps only the tiles not merged into sub-levels,
                // so claiming by its (sparse) bounding box would over-claim
                const bool bByContent = !oGroup.vHeightfields.empty();

                for (int32_t cy = cy0; cy < cy1; ++cy) {
                        for (int32_t cx = cx0; cx < cx1; ++cx) {
                                // claim by tile center inside bounds + content
                                const float wx = out.origin.x + (static_cast<float>(cx) + 0.5f) * oCfg.tile_dim;
                                const float wz = out.origin.z + (static_cast<float>(cy) + 0.5f) * oCfg.tile_dim;
                                if (wx < oGroup.bounds_min.x || wx > oGroup.bounds_max.x ||
                                    wz < oGroup.bounds_min.z || wz > oGroup.bounds_max.z) {
                                        continue;
                                }
                                if (bByContent) {
                                        bool bInsideHf = false;
                                        for (const SGeomHeightfield & hf : oGroup.vHeightfields) {
                                                if (wx >= hf.origin.x && wx <= hf.origin.x + hf.tile_size &&
                                                    wz >= hf.origin.z && wz <= hf.origin.z + hf.tile_size) {
                                                        bInsideHf = true;
                                                        break;
                                                }
                                        }
                                        if (!bInsideHf) { continue; }
                                }
                                const size_t ci = CellIndex(static_cast<uint32_t>(cx), static_cast<uint32_t>(cy));
                                if (oGroup.coexisting) {
                                        if (vCoexist[ci] >= 0 && vCoexist[ci] != static_cast<int32_t>(g)) {
                                                log_e("NavBake: coexisting groups '{}' and '{}' both claim tile ({}, {})",
                                                      oIn.vGroups[vCoexist[ci]].id, oGroup.id, cx, cy);
                                                return false;
                                        }
                                        vCoexist[ci] = static_cast<int32_t>(g);
                                } else {
                                        if (vOwner[ci] >= 0 && vOwner[ci] != static_cast<int32_t>(g)) {
                                                log_e("NavBake: normal sub-levels '{}' and '{}' both claim tile ({}, {}) — "
                                                      "align bounds to the tile grid, mark one Manual, or add NoNav",
                                                      oIn.vGroups[vOwner[ci]].id, oGroup.id, cx, cy);
                                                return false;
                                        }
                                        vOwner[ci] = static_cast<int32_t>(g);
                                }
                        }
                }
        }

        // --- per-tile bake (row-major + layer, deterministic order) ---
        float y_min = FLT_MAX, y_max = -FLT_MAX;
        for (const SGroup & oGroup : oIn.vGroups) {
                y_min = glm::min(y_min, oGroup.bounds_min.y);
                y_max = glm::max(y_max, oGroup.bounds_max.y);
        }
        if (y_min > y_max) { y_min = 0.0f; y_max = 0.0f; }

        // auto-exclusion volumes: coexisting groups' authored bounds remove their
        // footprint from every normal group's bake (bake-time overlap resolution)
        std::vector<SAreaBox> vAutoExclude;
        for (const SGroup & oGroup : oIn.vGroups) {
                if (!oGroup.coexisting) { continue; }
                const glm::vec3 center = (oGroup.bounds_min + oGroup.bounds_max) * 0.5f;
                const glm::vec3 half = (oGroup.bounds_max - oGroup.bounds_min) * 0.5f;
                vAutoExclude.push_back(SAreaBox{center, half, true, 0});
        }

        // --- border-sampling scopes (see SScopeGeom) ------------------------------
        // [0] = normal groups, [1] = coexisting groups; determinism = input order
        SScopeGeom vScopes[2];
        for (const SGroup & oGroup : oIn.vGroups) {
                SScopeGeom & oScope = vScopes[oGroup.coexisting ? 1 : 0];
                for (const SGeomHeightfield & hf : oGroup.vHeightfields) {
                        if (hf.resolution < 2 || hf.vHeights.empty()) { continue; }
                        oScope.vHfs.push_back(SScopeGeom::SHfRef{
                                        &hf, hf.origin,
                                        hf.origin + glm::vec3(hf.tile_size, 0.0f, hf.tile_size)});
                }
                for (const SGeomMesh & oMesh : oGroup.vMeshes) {
                        if (oMesh.vIndices.empty() || oMesh.vVertices.empty()) { continue; }
                        glm::vec3 mmin(FLT_MAX), mmax(-FLT_MAX);
                        for (size_t v = 0; v * 3 + 2 < oMesh.vVertices.size(); ++v) {
                                mmin = glm::min(mmin, glm::vec3(oMesh.vVertices[v * 3],
                                                                oMesh.vVertices[v * 3 + 1],
                                                                oMesh.vVertices[v * 3 + 2]));
                                mmax = glm::max(mmax, glm::vec3(oMesh.vVertices[v * 3],
                                                                oMesh.vVertices[v * 3 + 1],
                                                                oMesh.vVertices[v * 3 + 2]));
                        }
                        oScope.vMeshes.push_back(SScopeGeom::SMeshRef{&oMesh, mmin, mmax});
                }
                for (const SAreaBox & oBox : oGroup.vAreaBoxes) {
                        if (oBox.exclusion) { oScope.pExclusions.push_back(&oBox); }
                }
        }

        for (uint32_t ty = 0; ty < out.tiles_y; ++ty) {
                for (uint32_t tx = 0; tx < out.tiles_x; ++tx) {
                        const size_t ci = CellIndex(tx, ty);

                        if (vOwner[ci] >= 0) {
                                const SGroup & oGroup = oIn.vGroups[vOwner[ci]];
                                SNavTileBytes oTile;
                                uint32_t polys = 0, verts = 0;
                                if (!BakeTile(oGroup, vScopes[oGroup.coexisting ? 1 : 0], 0, tx, ty,
                                              oCfg, out.origin, y_min, y_max, vAutoExclude, oTile, polys, verts)) {
                                        return false;
                                }
                                if (!oTile.vBytes.empty()) {
                                        out.poly_count += polys;
                                        out.vert_count += verts;
                                        out.vTiles.push_back(std::move(oTile));
                                }
                        }
                        if (vCoexist[ci] >= 0) {
                                const SGroup & oGroup = oIn.vGroups[vCoexist[ci]];
                                SNavTileBytes oTile;
                                uint32_t polys = 0, verts = 0;
                                if (!BakeTile(oGroup, vScopes[1], vGroupLayers[vCoexist[ci]], tx, ty,
                                              oCfg, out.origin, y_min, y_max, {}, oTile, polys, verts)) {
                                        return false;
                                }
                                if (!oTile.vBytes.empty()) {
                                        out.poly_count += polys;
                                        out.vert_count += verts;
                                        out.vTiles.push_back(std::move(oTile));
                                }
                        }
                }
        }

        // deterministic (y, x, layer) order
        std::sort(out.vTiles.begin(), out.vTiles.end(), [](const SNavTileBytes & a, const SNavTileBytes & b) {
                if (a.y != b.y) { return a.y < b.y; }
                if (a.x != b.x) { return a.x < b.x; }
                return a.layer < b.layer;
        });

        // --- link table ---
        out.vLinks.reserve(oIn.vGroups.size());
        for (const SGroup & oGroup : oIn.vGroups) {
                for (const SLink & oLink : oGroup.vLinks) {
                        SLinkRecord oRec;
                        oRec.start = oLink.start;
                        oRec.end = oLink.end;
                        oRec.radius = oLink.radius;
                        oRec.registry_index = oLink.registry_index;
                        oRec.flags = oLink.flags;
                        oRec.bidirectional = oLink.bidirectional;
                        oRec.group = oGroup.id;
                        out.vLinks.push_back(std::move(oRec));
                }
        }

        log_i("NavBake: world '{}' agent '{}' baked: {} tiles ({}x{} grid, tile {}m), {} polys, {} verts, {} links, {} areas",
              oCfg.world_id, oCfg.agent.name, out.vTiles.size(), out.tiles_x, out.tiles_y, oCfg.tile_dim,
              out.poly_count, out.vert_count, out.vLinks.size(), oCfg.vAreas.size());
        return true;
}

} // namespace NavBake
} // namespace SE

#endif // SE_IMPL
