
#ifdef SE_IMPL

#include <terrain/Terrain.h>

#include <Logging.h>

#include <GlobalTypes.h>
#include <GLUtil.h>
#include <Material.h>
#include <terrain/TerrainAsset.h>
#include <terrain/TerrainSystem.h>

#include <Terrain_generated.h>
#include <flatbuffers/flatbuffers.h>

#include <cmath>

#ifdef SE_TERRAIN_ENABLED
// NOTE: no application.h here — its SDLWindow → GL/glx → X11 chain defines
// the `None` macro, which breaks gtest headers in TUs that mix both
#include <Camera.h>
#endif

namespace SE {

namespace {

// interleaved PosNormUV: pos3 tile-local + normal3 + uv2 (world-scaled)
constexpr size_t kVertexFloats = 8;
constexpr size_t kVertexBytes  = kVertexFloats * sizeof(float);

struct SVertexAccum {
        float x, y, z;
        float nx, ny, nz;
        float u, v;
};
static_assert(sizeof(SVertexAccum) == kVertexBytes, "terrain vertex layout");

} // namespace

// ---------------------------------------------------------------------------

Terrain::Terrain(TSceneTree::TSceneNodeExact * pNewNode, const SDesc & oNewDesc)
        : pNode(pNewNode),
          oDesc(oNewDesc) {
}

Terrain::Terrain(TSceneTree::TSceneNodeExact * pNewNode, const FlatBuffers::TerrainComponent * pFb)
        : pNode(pNewNode) {

        if (pFb && pFb->manifest()) {
                oDesc.sManifest  = pFb->manifest()->str();
                oDesc.b_grid_mode = pFb->grid_mode();
        }
}

Terrain::~Terrain() noexcept {
        Disable();
}

void Terrain::EnsureAsset() {

        if (hAsset.IsValid()) { return; }
        hAsset = CreateResource<TerrainAsset>(oDesc.sManifest);
        if (!hAsset.IsValid()) {
                log_e("Terrain: failed to load manifest '{}'", oDesc.sManifest);
                return;
        }
        uv_scale = GetResource(hAsset)->GetMaterial().uv_scale;
}

void Terrain::Enable() {

        if (enabled) { return; }
        enabled = true;

        EnsureAsset();
        if (!hAsset.IsValid()) { return; }

        auto & oSystem = GetSystem<TerrainSystem>();
        reg_id = oSystem.AttachTerrain(hAsset, oDesc.b_grid_mode);
        if (reg_id == 0) {
                log_e("Terrain: failed to register '{}'", oDesc.sManifest);
                return;
        }
        oSystem.LinkComponent(reg_id, oDesc.b_renderable ? this : nullptr);

        const uint32_t tiles = uint32_t(GetResource(hAsset)->GetTiles().size());
        for (auto & p : vTileGpu) { p.reset(); }
        vTileGpu.resize(tiles);
        oTileTransforms.resize(tiles);   // reserved once below; refs handed to RenderCommands
        oTileTransforms.reserve(tiles);
        // The tile Transforms are deliberately parentless: baked vertices already
        // carry the world offset, and raw Transforms get no parent→child dirty
        // propagation (a moved node would leave stale tile matrices). The rule
        // is that the owning node must be identity — enforce it loudly instead
        // of silently diverging (two sources of truth for tile placement).
        if (pNode) {
                const Transform & oNodeT = pNode->GetTransform();
                const glm::vec3 vPos = oNodeT.GetPos();
                if (glm::dot(vPos, vPos) != 0.f || oNodeT.GetScale() != glm::vec3(1.f)
                    || oNodeT.GetRotation() != glm::quat(1.f, 0.f, 0.f, 0.f)) {
                        log_w("Terrain: node '{}' carries a transform — terrain ignores it "
                              "(bake world-space tiles into an identity node)",
                              pNode->GetName());
                }
        }
        vSelectedLod.assign(tiles, 0);
        vLodOverride.assign(tiles, -1);
        quadtree_dirty = true;

        if (!oDesc.b_renderable) { return; }

#ifdef SE_TERRAIN_ENABLED
        TEngine::Instance().Get<TRenderer>().AddRenderable(this);
#endif

        const auto & oMat = GetResource(hAsset)->GetMaterial();
        if (!oMat.sPath.empty()) {
                try {
                        hMaterial = CreateResource<Material>(GetSystem<Config>().sResourceDir + oMat.sPath);
                }
                catch (const std::exception & ex) {
                        log_e("Terrain: material '{}' failed: {} (render disabled)",
                              oMat.sPath, ex.what());
                }
        }

        oSystem.SetGpuSink(reg_id,
                        [this](const uint32_t idx, const STileRuntime & rt) { OnTileGpuReady(idx, rt); },
                        [this](const uint32_t idx) { OnTileGpuRelease(idx); });
}

void Terrain::Disable() {

        if (!enabled) { return; }
        enabled = false;

        if (reg_id != 0) {
            auto & oSystem = GetSystem<TerrainSystem>();
#ifdef SE_TERRAIN_ENABLED
            if (oDesc.b_renderable) { TEngine::Instance().Get<TRenderer>().RemoveRenderable(this); }
#endif
            oSystem.SetGpuSink(reg_id, {}, {});
            oSystem.UnlinkComponent(reg_id);
            oSystem.Detach(reg_id);
            reg_id = 0;
        }

        ReleaseGpu();
        vRenderCommands.clear();
        hMaterial = {};
}

void Terrain::ReleaseGpu() {

        for (auto & pTile : vTileGpu) {
                if (!pTile) { continue; }
                for (const uint32_t vao : pTile->vVaos) {
                        if (vao != 0) { glDeleteVertexArrays(1, const_cast<uint32_t *>(&vao)); }
                }
                pTile.reset();
        }
        vTileGpu.resize(vTileGpu.size());
        mIndexByLod.clear();
        oQuadtree.Clear();
        vRenderCommands.clear();
}

// ---------------------------------------------------------------------------
// GPU upload (main thread, from TerrainSystem tile builds)
// ---------------------------------------------------------------------------

Terrain::SSharedIndex & Terrain::GetSharedIndex(const uint32_t lod, const uint16_t resolution) {

        if (mIndexByLod.size() <= lod) { mIndexByLod.resize(lod + 1); }
        SSharedIndex & oShared = mIndexByLod[lod];
        if (oShared.pBuffer) { return oShared; }

        // grid + skirt indices, 32-bit (res > 255 cells exceeds the 16-bit range)
        const uint32_t cells = uint32_t(resolution - 1);
        const uint32_t grid_count = cells * cells * 6;
        const uint32_t skirt_count = cells * 4 * 2 * 3;   // 4 edges, 2 tris per cell... per edge cell
        std::vector<uint32_t> vIndices;
        vIndices.reserve(grid_count + skirt_count);

        const uint32_t res = resolution;
        auto IndexOf = [res](uint32_t i, uint32_t j) { return i * res + j; };

        for (uint32_t i = 0; i < cells; ++i) {
                for (uint32_t j = 0; j < cells; ++j) {
                        const uint32_t v00 = IndexOf(i, j);
                        const uint32_t v01 = IndexOf(i, j + 1);
                        const uint32_t v10 = IndexOf(i + 1, j);
                        const uint32_t v11 = IndexOf(i + 1, j + 1);
                        // CCW seen from +Y: (00,10,11) and (00,11,01)
                        vIndices.insert(vIndices.end(), { v00, v10, v11, v00, v11, v01 });
                }
        }

        // Skirt: 4*res duplicated edge vertices dropped by skirt_depth, walked CCW
        // around the patch (e0: j asc at i=0, e1: i asc at j=res-1, e2: j desc at
        // i=res-1, e3: i desc at j=0). Triangles (a0, b1, b0) + (a0, a1, b1) face
        // OUTWARD on every edge with that ordering (verified by cross products).
        const uint32_t skirt_base = res * res;
        auto SkirtIndex = [res, skirt_base](uint32_t edge, uint32_t k) {
                return skirt_base + edge * res + k;
        };
        auto EmitSkirtStrip = [&](uint32_t edge, auto && InteriorA) {
                for (uint32_t k = 0; k < res - 1; ++k) {
                        const uint32_t a0 = InteriorA(k);
                        const uint32_t a1 = InteriorA(k + 1);
                        const uint32_t b0 = SkirtIndex(edge, k);
                        const uint32_t b1 = SkirtIndex(edge, k + 1);
                        vIndices.insert(vIndices.end(), { a0, b1, b0, a0, a1, b1 });
                }
        };
        EmitSkirtStrip(0, [&](uint32_t k) { return IndexOf(0, k); });
        EmitSkirtStrip(1, [&](uint32_t k) { return IndexOf(k, res - 1); });
        EmitSkirtStrip(2, [&](uint32_t k) { return IndexOf(res - 1, res - 1 - k); });
        EmitSkirtStrip(3, [&](uint32_t k) { return IndexOf(res - 1 - k, 0); });

        oShared.count = uint32_t(vIndices.size());
        oShared.pBuffer = std::make_unique<VertexBuffer>(vIndices.size() * sizeof(uint32_t), VertexBuffer::UpdateFreq::STATIC);
        oShared.pBuffer->ReplaceData(vIndices.data(), vIndices.size() * sizeof(uint32_t));
        oShared.pBuffer->UploadToGPU();
        return oShared;
}

void Terrain::OnTileGpuReady(const uint32_t tile_index, const STileRuntime & oRuntime) {

        if (!oDesc.b_renderable || !hAsset.IsValid()) { return; }
        if (tile_index >= vTileGpu.size()) { return; }

        const auto & oRef = GetResource(hAsset)->GetTiles()[tile_index];

        // patch transform: vertices are tile-local (x, h - min.y, z) — float precision
        // survives at any world distance through the standard MVMatrix path
        Transform & oTransform = oTileTransforms[tile_index];
        oTransform.SetWorldPos(oRef.vBoundsMin);

        auto pGpu = std::make_unique<STileGpu>();
        pGpu->vVaos.reserve(oRuntime.vLods.size());
        pGpu->vGeoms.reserve(oRuntime.vLods.size());

        for (uint32_t lod = 0; lod < uint32_t(oRuntime.vLods.size()); ++lod) {

                const SDecodedLod & oLod = oRuntime.vLods[lod];
                const uint32_t res = oLod.resolution;
                const float step = TerrainQuery::SampleStep(res, oRuntime.tile_size);
                const auto & oShared = GetSharedIndex(lod, res);

                // skirt depth scales with this LOD's world step: the crack a
                // skirt must cover is where a coarser neighbor's straight edge
                // chords cut across a finer surface, and that gap grows with the
                // coarse step (the old global-range 0.2% rule left visible seams
                // at LOD2/LOD3 borders)
                const float skirt_depth = std::max(1.0f, step);

                // vertices: interior grid + 4*res skirt ring
                vVertexScratch.resize((size_t(res) * res + size_t(res) * 4) * kVertexBytes);
                auto * pVerts = reinterpret_cast<SVertexAccum *>(vVertexScratch.data());

                auto IndexOf = [res](uint32_t i, uint32_t j) { return i * res + j; };
                for (uint32_t i = 0; i < res; ++i) {
                        for (uint32_t j = 0; j < res; ++j) {
                                const size_t k = size_t(i) * res + j;
                                const size_t n_off = k * 2;
                                const glm::vec3 vNormal = OctDecodeNormal(OctNormal16{
                                        oLod.vNormals[n_off], oLod.vNormals[n_off + 1] });
                                SVertexAccum v;
                                v.x  = j * step;
                                v.z  = i * step;
                                v.y  = oRuntime.Dequantize(oLod.vHeights[k]) - oRef.vBoundsMin.y;
                                v.nx = vNormal.x; v.ny = vNormal.y; v.nz = vNormal.z;
                                v.u = (oRef.vBoundsMin.x + v.x) * uv_scale;
                                v.v = (oRef.vBoundsMin.z + v.z) * uv_scale;
                                pVerts[k] = v;
                        }
                }

                // skirt: duplicate edge vertices dropped by skirt_depth (normals kept —
                // the silhouette hides the wall; UVs keep tiling continuity). Edge
                // ordering matches the index generator (CCW around the patch).
                const uint32_t skirt_base = res * res;
                for (uint32_t k = 0; k < res; ++k) {
                        auto CopyDropped = [&](uint32_t edge, uint32_t src_index) {
                                SVertexAccum v = pVerts[src_index];
                                v.y -= skirt_depth;
                                pVerts[skirt_base + edge * res + k] = v;
                        };
                        CopyDropped(0, IndexOf(0, k));
                        CopyDropped(1, IndexOf(k, res - 1));
                        CopyDropped(2, IndexOf(res - 1, res - 1 - k));
                        CopyDropped(3, IndexOf(res - 1 - k, 0));
                }

                // VAO + VBO
                uint32_t vao = 0;
                glGenVertexArrays(1, &vao);
                glBindVertexArray(vao);

                auto pBuffer = std::make_unique<VertexBuffer>(vVertexScratch.size(), VertexBuffer::UpdateFreq::STATIC);
                pBuffer->ReplaceData(vVertexScratch.data(), vVertexScratch.size());
                pBuffer->UploadToGPU();
                glBindBuffer(GL_ARRAY_BUFFER, pBuffer->ID());

                glVertexAttribPointer(mAttributeLocation["Position"], 3, GL_FLOAT, false,
                                      GLsizei(kVertexBytes), (const void *)offsetof(SVertexAccum, x));
                glEnableVertexAttribArray(mAttributeLocation["Position"]);
                glVertexAttribPointer(mAttributeLocation["Normal"], 3, GL_FLOAT, false,
                                      GLsizei(kVertexBytes), (const void *)offsetof(SVertexAccum, nx));
                glEnableVertexAttribArray(mAttributeLocation["Normal"]);
                glVertexAttribPointer(mAttributeLocation["TexCoord0"], 2, GL_FLOAT, false,
                                      GLsizei(kVertexBytes), (const void *)offsetof(SVertexAccum, u));
                glEnableVertexAttribArray(mAttributeLocation["TexCoord0"]);

                // element buffer: shared per resolution (bind into this VAO)
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, oShared.pBuffer->ID());

                glBindVertexArray(0);

                pGpu->vVaos.push_back(vao);
                pGpu->vGeoms.push_back(GeometryEntity(vao, oShared.count, 0,
                                                     VertexIndexType::INT, GL_TRIANGLES, 0));
        }

        // Cached draw command per LOD: built once here, never per frame. The
        // tile cull sphere (half-diagonal of the manifest AABB) lets the shadow
        // pass skip tiles outside the light ortho without any bounds system.
        if (hMaterial.IsValid()) {
                Material * pMat = GetResource(hMaterial);
                const float cull_radius = glm::length(oRef.vBoundsMax - oRef.vBoundsMin) * 0.5f;
                pGpu->vLodCmds.reserve(pGpu->vGeoms.size());
                for (const auto & oGeom : pGpu->vGeoms) {
                        pGpu->vLodCmds.emplace_back(&oGeom, pMat, oTileTransforms[tile_index]);
                        pGpu->vLodCmds.back().SetCullRadius(cull_radius);
                }
        }

        vTileGpu[tile_index] = std::move(pGpu);
        quadtree_dirty = true;
        // tile gained GPU buffers after the last visibility build — the cached
        // command lists must re-gather (this is what makes streamed-in tiles
        // appear without waiting for a camera move)
#ifdef SE_TERRAIN_ENABLED
        TEngine::Instance().Get<TRenderer>().MarkVisibleDirty();
#endif
}

void Terrain::OnTileGpuRelease(const uint32_t tile_index) {

        if (tile_index >= vTileGpu.size()) { return; }
        std::unique_ptr<STileGpu> & pTile = vTileGpu[tile_index];
        if (!pTile) { return; }
        for (const uint32_t vao : pTile->vVaos) {
                if (vao != 0) { uint32_t h = vao; glDeleteVertexArrays(1, &h); }
        }
        pTile.reset();
        quadtree_dirty = true;
#ifdef SE_TERRAIN_ENABLED
        TEngine::Instance().Get<TRenderer>().MarkVisibleDirty();
#endif
}

// ---------------------------------------------------------------------------
// selection + renderable interface
// ---------------------------------------------------------------------------

float Terrain::Boundary(const uint32_t lod, const float tile_size) {
        // Distance where LOD (lod-1) hands over to LOD lod. With base 1.0 the
        // first boundary sits at one tile_size: LOD0 covers only the nearest
        // ring and every streamed-out-to-load-radius ring uses the coarser
        // chain (previously factor^lod put LOD0 at tile_size * 2^lod — 1024 m
        // at the demo's 512 m tiles — so ALL streamed tiles rendered LOD0 and
        // the baked chain was dead residency).
        return tile_size * kDistanceBase * std::pow(kDistanceFactor, float(lod) - 1.0f);
}

uint32_t Terrain::SelectLod(const uint32_t tile_index, const float distance) {

        const uint32_t lod_count = hAsset.IsValid() ? uint32_t(GetResource(hAsset)->GetLodCount()) : 1u;
        const float ts = hAsset.IsValid() ? GetResource(hAsset)->GetTileSize() : 512.0f;

        uint32_t target = 0;
        while (target + 1 < lod_count && distance >= Boundary(target + 1, ts)) { ++target; }

        // hysteresis band: no flip-flopping at a boundary
        uint8_t & cur = vSelectedLod[tile_index];
        const float hyst = kSwitchHysteresis;
        if (target > cur && distance < Boundary(cur + 1, ts) * (1.0f + hyst)) { target = cur; }
        if (target < cur && distance > Boundary(cur, ts) * (1.0f - hyst))     { target = cur; }
        cur = uint8_t(target);
        return target;
}

void Terrain::RebuildQuadtree() {

        if (!hAsset.IsValid()) { return; }
        glm::vec3 vMin{ 0.0f }, vMax{ 0.0f };
        bool bFirst = true;
        for (const auto & t : GetResource(hAsset)->GetTiles()) {
                if (bFirst) { vMin = t.vBoundsMin; vMax = t.vBoundsMax; bFirst = false; }
                else {
                        vMin = glm::min(vMin, t.vBoundsMin);
                        vMax = glm::max(vMax, t.vBoundsMax);
                }
        }
        oQuadtree.Build(vMin, vMax, GetResource(hAsset)->GetTilesX(), GetResource(hAsset)->GetTilesY());
        quadtree_dirty = false;

        // manifest-extent cull sphere for the visibility manager
        vCullCenter = (vMin + vMax) * 0.5f;
        cull_radius = glm::length(vMax - vMin) * 0.5f;
}

const std::vector<const RenderCommand *> & Terrain::GetRenderCommands() {

        vRenderCommands.clear();
        last_visible_count = 0;
        last_in_view_count  = 0;
        last_no_gpu_count   = 0;
        last_tri_count     = 0;
        if (!oDesc.b_renderable || !hAsset.IsValid() || reg_id == 0) { return vRenderCommands; }

#ifdef SE_TERRAIN_ENABLED
        Camera * pCamera = TEngine::Instance().Get<TRenderer>().GetCamera();
        if (!pCamera || !hMaterial.IsValid()) { return vRenderCommands; }

        if (quadtree_dirty) { RebuildQuadtree(); }

        const SFrustum oFrustum = SFrustum::FromVP(pCamera->GetWorldMVP());
        const glm::vec3 vCamPos = pCamera->GetWorldPos();

        // Ground-truth cross-check of the quadtree — O(ALL manifest tiles) per
        // gather, so it is debug-only (the TerrainDebugger reads the counters).
        // It used to run in production every frame, scaling with world size.
#ifdef SE_TERRAIN_DEBUG_GROUND_TRUTH
        {
                const auto & vRefs = GetResource(hAsset)->GetTiles();
                for (uint32_t i = 0; i < vRefs.size(); ++i) {
                        if (!oFrustum.IntersectsAABB(vRefs[i].vBoundsMin, vRefs[i].vBoundsMax)) { continue; }
                        ++last_in_view_count;
                        if (i >= vTileGpu.size() || !vTileGpu[i]) { ++last_no_gpu_count; }
                }
        }
#endif

        oQuadtree.CollectVisible(oFrustum, vVisibleScratch);

        // Diagnostics: the quadtree's candidate count stands in for the (now
        // debug-only) ground-truth in-view count; no-gpu counts inside gather.
        last_in_view_count = uint32_t(vVisibleScratch.size());

        vRenderCommands.reserve(vVisibleScratch.size());
        for (const uint32_t flat_index : vVisibleScratch) {
                const int32_t tile_index = GetResource(hAsset)->TileIndexAt(
                                uint16_t(flat_index % GetResource(hAsset)->GetTilesX()),
                                uint16_t(flat_index / GetResource(hAsset)->GetTilesX()));
                if (tile_index < 0) { continue; }
                const STileGpu * pTile = vTileGpu[size_t(tile_index)].get();
                if (!pTile || pTile->vLodCmds.empty()) { ++last_no_gpu_count; continue; }

                const auto & oRef = GetResource(hAsset)->GetTiles()[size_t(tile_index)];
                const float d = StreamingDirector::DistanceToBounds(vCamPos, oRef.vBoundsMin, oRef.vBoundsMax);

                const int8_t forced = vLodOverride[size_t(tile_index)];
                uint32_t lod;
                if (forced >= 0) {
                        lod = std::min(uint32_t(forced), GetResource(hAsset)->GetLodCount() - 1);
                        vSelectedLod[size_t(tile_index)] = uint8_t(lod);
                }
                else {
                        lod = SelectLod(uint32_t(tile_index), d);
                }
                const uint32_t lod_safe = std::min(lod, uint32_t(pTile->vLodCmds.size() - 1));

                last_tri_count += GetLodTriCount(lod_safe);
                vRenderCommands.push_back(&pTile->vLodCmds[lod_safe]);
        }
        last_visible_count = uint32_t(vRenderCommands.size());
#endif
        return vRenderCommands;
}

// ---------------------------------------------------------------------------
// debug: per-tile LOD override + gather diagnostics
// ---------------------------------------------------------------------------

void Terrain::SetLodOverride(const uint32_t tile_index, const int8_t lod) {

        if (tile_index >= vLodOverride.size()) { return; }
        if (lod >= 0 && hAsset.IsValid()) {
                vLodOverride[tile_index] = int8_t(std::min<int32_t>(lod, int32_t(GetResource(hAsset)->GetLodCount()) - 1));
        }
        else {
                vLodOverride[tile_index] = -1;
        }
        // the override takes effect on the next gather
#ifdef SE_TERRAIN_ENABLED
        if (oDesc.b_renderable) { TEngine::Instance().Get<TRenderer>().MarkVisibleDirty(); }
#endif
}

int8_t Terrain::GetLodOverride(const uint32_t tile_index) const {
        return tile_index < vLodOverride.size() ? vLodOverride[tile_index] : int8_t(-1);
}

void Terrain::SetDebugWireframe(const bool wireframe) {

        if (wireframe == wireframe_mode) { return; }

        if (!hWireframe.IsValid()) {
                hWireframe = CreateResource<Material>(
                                GetSystem<Config>().sResourceDir + "material/wireframe.semt");
        }
        Material * pNormal = hMaterial.IsValid() ? GetResource(hMaterial) : nullptr;
        Material * pWire   = GetResource(hWireframe);
        Material * pTarget = wireframe ? pWire : pNormal;
        if (!pTarget) { return; }

        wireframe_mode = wireframe;

        // swap the material on every cached per-(tile, LOD) command in place —
        // commands, geometry and transforms stay; only the material slot moves
        for (auto & pGpu : vTileGpu) {
                if (!pGpu) { continue; }
                for (auto & cmd : pGpu->vLodCmds) { cmd.SetMaterial(pTarget); }
        }
        // the blend mode may differ between the two materials — force a regather
#ifdef SE_TERRAIN_ENABLED
        if (oDesc.b_renderable) { TEngine::Instance().Get<TRenderer>().MarkVisibleDirty(); }
#endif
}

uint32_t Terrain::GetTileCount() const {
        return hAsset.IsValid() ? uint32_t(GetResource(hAsset)->GetTiles().size()) : 0;
}

uint32_t Terrain::GetLodCount() const {
        return hAsset.IsValid() ? GetResource(hAsset)->GetLodCount() : 0;
}

uint64_t Terrain::GetLodTriCount(const uint32_t lod) const {

        if (!hAsset.IsValid() || lod >= GetResource(hAsset)->GetLodCount()) { return 0; }
        const uint32_t res = GetResource(hAsset)->GetLodResolutions()[lod];
        const uint32_t cells = res - 1;
        // grid (2 tris per cell) + 4 skirt edges (2 tris per edge cell)
        return (uint64_t(cells) * cells) * 2 + uint64_t(cells) * 4 * 2;
}

bool Terrain::HasTileGpu(const uint32_t tile_index) const {
        return tile_index < vTileGpu.size() && vTileGpu[tile_index] != nullptr;
}

uint8_t Terrain::GetTileSelectedLod(const uint32_t tile_index) const {
        return tile_index < vSelectedLod.size() ? vSelectedLod[tile_index] : uint8_t(0);
}

void Terrain::DrawDebug() const {

        if (!hAsset.IsValid()) { return; }
        auto & oDebug = GetSystem<DebugRenderer>();
        static const glm::vec4 aColors[4] = {
                { 0.2f, 1.0f, 0.2f, 1.0f },   // LOD0 green
                { 1.0f, 1.0f, 0.2f, 1.0f },   // LOD1 yellow
                { 1.0f, 0.6f, 0.2f, 1.0f },   // LOD2 orange
                { 1.0f, 0.2f, 0.2f, 1.0f },   // LOD3+ red
        };
        static const glm::vec4 vForcedColor{ 1.0f, 1.0f, 1.0f, 1.0f };   // overridden tile
        for (size_t idx = 0; idx < GetResource(hAsset)->GetTiles().size(); ++idx) {
                if (idx >= vTileGpu.size() || !vTileGpu[idx]) { continue; }
                const auto & oRef = GetResource(hAsset)->GetTiles()[idx];
                // manifest bounds are world-space; DrawBBox transforms the corners,
                // so an identity transform here (the tile transform would double-
                // apply the tile origin and offset every box out of the grid)
                static const Transform oIdentity;
                const BoundingBox oBBox(oRef.vBoundsMin, oRef.vBoundsMax);
                const uint8_t lod = idx < vSelectedLod.size() ? vSelectedLod[idx] : 0;
                const int8_t forced = idx < vLodOverride.size() ? vLodOverride[idx] : int8_t(-1);
                oDebug.DrawBBox(oBBox, oIdentity, forced >= 0 ? vForcedColor : aColors[std::min<uint8_t>(lod, 3)]);
        }
}

std::string Terrain::Str() const {
        return fmt::format("Terrain['{}' reg={} grid={}]", oDesc.sManifest, reg_id, oDesc.b_grid_mode);
}

} // namespace SE

#endif // SE_IMPL
