
#ifndef APP_TERRAIN_COMPONENT_H
#define APP_TERRAIN_COMPONENT_H 1

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include <Component_generated.h>
#include <GeometryEntity.h>
#include <RenderCommand.h>
#include <ResourceHandle.h>
#include <Transform.h>
#include <VertexBuffer.h>
#include <terrain/TerrainQuadtree.h>

namespace SE {

class TerrainAsset;
class Material;

// ---------------------------------------------------------------------------
// Terrain — the ONE component per terrain-bearing sub-level (component mode)
// or per world-level grid node (grid mode). All patches, LOD selection and
// draw batching live inside TerrainSystem + this component's GPU buffers; the
// generic scene graph never sees a per-patch node.
//
// Render path (SE_TERRAIN_ENABLED targets): the component is a registered
// renderable — plain RenderCommand/GeometryEntity draws through GeometryPass
// and ShadowPass. AllVisible rebuilds when the camera moves, and
// GetRenderCommands() re-runs quadtree culling + LOD selection at that point.
//
// Headless (tests): construct with SDesc::b_renderable = false — no renderer,
// no GL, no material; the component still drives TerrainSystem tile lifecycle.
// ---------------------------------------------------------------------------

class Terrain {
public:
        using TSerialized = FlatBuffers::TerrainComponent;

        struct SDesc {
                std::string sManifest;        // .seterrain (resource-dir relative, like world paths)
                bool        b_grid_mode = false;   // true: tiles page by distance (world-level node)
                bool        b_renderable = true;   // false in headless tests
        };

        Terrain(TSceneTree::TSceneNodeExact * pNewNode, const SDesc & oDesc);
        Terrain(TSceneTree::TSceneNodeExact * pNewNode, const FlatBuffers::TerrainComponent * pFb);
        ~Terrain() noexcept;

        void Enable();
        void Disable();

        // --- renderable interface ------------------------------------------------
        /// Re-runs quadtree culling + LOD selection (against the active camera) and
        /// returns one command per visible patch. Commands are built ONCE per
        /// (tile, LOD) at GPU-ready and cached in STileGpu — the per-frame gather
        /// only selects pointers (no RenderCommand/ShaderProgramState churn).
        /// Called at most once per frame.
        const std::vector<const RenderCommand *> & GetRenderCommands();
        /// DebugRenderer overlay: tile AABBs colored by selected LOD.
        void DrawDebug() const;

        uint64_t    GetRegistration() const { return reg_id; }
        /** Commands emitted by the last GetRenderCommands gather (diagnostic:
         *  stable while tiles flicker ⇒ the problem is draw-side, not culling). */
        uint32_t    GetLastVisibleCount() const { return last_visible_count; }
        std::string Str() const;

        // --- debug: per-tile LOD override + gather diagnostics -------------------
        /// Force a tile's LOD (0..GetLodCount()-1); -1 restores auto selection.
        void     SetLodOverride(uint32_t tile_index, int8_t lod);
        int8_t   GetLodOverride(uint32_t tile_index) const;
        uint32_t GetTileCount() const;
        uint32_t GetLodCount() const;
        /// Triangles one full tile renders at the given LOD (grid + skirt; every
        /// tile shares the same per-LOD index-buffer topology).
        uint64_t GetLodTriCount(uint32_t lod) const;
        bool     HasTileGpu(uint32_t tile_index) const;
        /// LOD actually rendered at the last gather (also updated by overrides).
        uint8_t  GetTileSelectedLod(uint32_t tile_index) const;
        /// Debug: render every tile with the wireframe material instead of the
        /// assigned one — makes the baked LOD chain visible. Main pass only.
        void     SetDebugWireframe(const bool wireframe);
        bool     IsDebugWireframe() const { return wireframe_mode; }
        /// Ground truth from the last gather: tiles whose manifest AABB is inside
        /// the frustum (independent of the quadtree), how many of them have no
        /// GPU buffers yet, and the total triangles emitted.
        uint32_t GetLastInViewCount() const { return last_in_view_count; }
        uint32_t GetLastNoGpuCount() const  { return last_no_gpu_count; }
        uint64_t GetLastTriCount() const    { return last_tri_count; }
        /** VisibilityManager opt-in: sphere around the manifest extent
         *  (conservative bound over every possible tile). */
        bool     GetCullSphere(glm::vec3 & vCenter, float & fRadius) const {
                if (!enabled || cull_radius <= 0.f) { return false; }
                vCenter = vCullCenter;
                fRadius = cull_radius;
                return true;
        }

        // LOD selection policy (b_i = tile_size * kDistanceBase * kDistanceFactor^(i-1)
        // is the distance where LOD i-1 hands over to LOD i — LOD0 holds only the
        // NEAREST ring (one tile_size), so the baked chain actually renders past it;
        // 3D distance-to-AABB already covers flight-view altitude — looking straight
        // down from height H puts every tile at d >= H).
        static constexpr float kDistanceBase   = 1.0f;
        static constexpr float kDistanceFactor = 2.0f;
        static constexpr float kSwitchHysteresis = 0.1f;

private:
        struct STileGpu {
                std::vector<uint32_t>       vVaos;    // one per LOD
                std::vector<GeometryEntity> vGeoms;   // one per LOD; refs the shared index buffer
                /// Cached draw command per LOD (built once at GPU-ready; shares the
                /// tile transform + material; carries the tile cull radius).
                std::vector<RenderCommand>  vLodCmds;
        };

        struct SSharedIndex {
                std::unique_ptr<VertexBuffer> pBuffer;   // GL_ELEMENT_ARRAY_BUFFER
                uint32_t                      count = 0;
        };

        Terrain(const Terrain &) = delete;
        Terrain & operator = (const Terrain &) = delete;

        void EnsureAsset();
        void RebuildQuadtree();
        void OnTileGpuReady(uint32_t tile_index, const STileRuntime & oRuntime);
        void OnTileGpuRelease(uint32_t tile_index);
        uint32_t SelectLod(uint32_t tile_index, float distance);
        static float Boundary(uint32_t lod, float tile_size);
        SSharedIndex & GetSharedIndex(uint32_t lod, uint16_t resolution);
        void ReleaseGpu();

        TSceneTree::TSceneNodeExact * pNode = nullptr;
        SDesc        oDesc;
        H<TerrainAsset> hAsset;
        H<Material>  hMaterial;
        H<Material>  hWireframe;      // debug material, lazy-loaded on first toggle
        bool         wireframe_mode = false;
        uint64_t     reg_id = 0;
        bool         enabled = false;
        float        uv_scale = 0.05f;

        // GPU state (populated only when b_renderable)
        std::vector<std::unique_ptr<STileGpu>> vTileGpu;   // indexed by tile index
        std::vector<Transform>                 oTileTransforms;   // reserved once; RenderCommand holds refs
        std::vector<SSharedIndex>              mIndexByLod;       // indexed by lod
        std::vector<uint8_t>                   vSelectedLod;      // per tile, hysteresis state
        std::vector<int8_t>                    vLodOverride;      // per tile; -1 = auto
        TerrainQuadtree                        oQuadtree;
        bool                                   quadtree_dirty = true;
        std::vector<uint32_t>                  vVisibleScratch;
        std::vector<uint8_t>                   vVertexScratch;    // interleaved PosNormUV build scratch
        /** Pointers to the cached per-(tile,LOD) commands of the last gather. */
        std::vector<const RenderCommand *>     vRenderCommands;
        uint32_t                               last_visible_count = 0;
        uint32_t                               last_in_view_count = 0;
        uint32_t                               last_no_gpu_count = 0;
        uint64_t                               last_tri_count = 0;
        // manifest-extent cull sphere (set in RebuildQuadtree)
        glm::vec3                              vCullCenter {0.f};
        float                                  cull_radius = 0.f;
};

} // namespace SE

#endif
