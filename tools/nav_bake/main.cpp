
// ---------------------------------------------------------------------------
// nav_bake — offline navigation bake tool.
//
// Consumes a JSON config whose input is either a compiled .seworld (mode A —
// the exact bytes the runtime loads: sub-level list, bounds, flags, terrain
// grid) or an explicit .sesc list (mode B — mesh-only, worldless). Geometry is
// decoded headless from the baked .sesc/.sems/.seterrain/.setile binaries and
// handed to the shared NavBake library (the same code the runtime tests
// exercise). Writes one .senm per agent class. Byte-deterministic.
//
// Usage: nav_bake <config.json> [--level L] [--log DEST]
//
// Config example (resource/nav/demo.json) — paths are relative to the process
// working directory (the repo root, like every other asset path):
// {
//     "world": "resource/world/demo.seworld",
//     "classes": [
//         { "name": "default", "radius": 0.5, "height": 2.0,
//           "max_slope_deg": 50.0, "step_height": 0.4 }
//     ],
//     "tile_dim": 16.0,
//     "cell_size": 64.0,
//     "voxel_size": 0.2,
//     "voxel_height": 0.2,
//     "output": "resource/nav/demo"
// }
//
// Mode B replaces "world" with: "sesc": ["resource/levels/arena.sesc", ...]
// (group bounds are then computed from the geometry; the streaming cell_size
// divisibility check is a world-mode feature).
// ---------------------------------------------------------------------------

#define SE_IMPL
#include <nav/NavBake.h>
#include <nav/NavBake.tcc>

#include <Component_generated.h>
#include <Mesh_generated.h>
#include <NavMesh_generated.h>
#include <SceneTree_generated.h>
#include <Terrain_generated.h>
#include <World_generated.h>

#include <Logging.h>
#include <boost/program_options.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <unordered_map>

std::shared_ptr<spdlog::logger> gLogger;

namespace po = boost::program_options;
using nlohmann::json;
namespace FlatBuffers = SE::FlatBuffers;
namespace NavBake = SE::NavBake;

// ---------------------------------------------------------------------------

static std::vector<uint8_t> ReadFileBytes(const std::string & sPath) {

        std::ifstream f(sPath, std::ios::binary);
        if (!f) { throw std::runtime_error("cannot open file: " + sPath); }
        f.seekg(0, std::ios::end);
        const std::streamoff size = f.tellg();
        if (size <= 0) { throw std::runtime_error("empty file: " + sPath); }
        f.seekg(0, std::ios::beg);
        std::vector<uint8_t> vBytes(static_cast<size_t>(size));
        f.read(reinterpret_cast<char *>(vBytes.data()), size);
        if (!f) { throw std::runtime_error("short read: " + sPath); }
        return vBytes;
}

static std::string DirOf(const std::string & sPath) {

        const size_t pos = sPath.find_last_of('/');
        return (pos == std::string::npos) ? std::string() : sPath.substr(0, pos + 1);
}

static glm::vec3 FbVec(const FlatBuffers::Vec3 * pV, const glm::vec3 & vDefault = glm::vec3(0.0f)) {

        return pV ? glm::vec3(pV->x(), pV->y(), pV->z()) : vDefault;
}

static glm::mat4 LocalMatrix(const glm::vec3 & vT, const glm::vec3 & vRdeg, const glm::vec3 & vS) {

        const glm::quat qRotation(glm::radians(vRdeg));
        return glm::translate(glm::mat4(1.0f), vT) * glm::toMat4(qRotation) * glm::scale(glm::mat4(1.0f), vS);
}

// ---------------------------------------------------------------------------
// tool-side group: NavBake input plus the authoring strings it must resolve
// ---------------------------------------------------------------------------

struct SToolGroup {

        NavBake::SGroup oGroup;
        std::vector<std::string> vAreaNames;      // parallel to oGroup.vAreaBoxes
        std::vector<std::string> vLinkAreas;      // parallel to oGroup.vLinks
        std::vector<glm::mat4>   vLinkNodeWorld;  // authoring node's world matrix per link

        struct SPendingEnd {
                uint32_t     link_index = 0;
                std::string  sNode;       // end_node name (empty → use vOffset)
                glm::vec3    vOffset;     // node-local offset fallback
        };
        std::vector<SPendingEnd> vPendingEnds;

        std::map<std::string, glm::vec3> mNodePositions;   // first match wins
};

struct SToolContext {

        std::vector<SToolGroup> vGroups;

        std::map<std::string, std::shared_ptr<std::vector<uint8_t>>> mFileBytes;
        std::map<std::string, const FlatBuffers::Mesh *> mMeshes;
        std::map<std::string, std::map<std::string, const FlatBuffers::Mesh *>> mSceneMeshes;
        std::set<std::string> sTerrains;

        /** Index a scene's inline (name-flavor) mesh resources so StaticModel
         *  holders that reference meshes by name resolve offline too. */
        void IndexSceneResources(const std::string & sScene, const FlatBuffers::SceneTree * pTree) {
                auto & oIndex = mSceneMeshes[sScene];
                if (!oIndex.empty() || !pTree->resources()) { return; }
                for (const FlatBuffers::ResourceRef * pRef : *pTree->resources()) {
                        if (pRef == nullptr) { continue; }
                        if (pRef->resource_type() != FlatBuffers::ResourceU::MeshHolder) { continue; }
                        const auto * pHolder = pRef->resource_as_MeshHolder();
                        if (pHolder == nullptr || pHolder->mesh() == nullptr) { continue; }
                        if (pHolder->name() && pHolder->name()->size() > 0) {
                                oIndex[pHolder->name()->str()] = pHolder->mesh();
                        }
                }
        }

        std::shared_ptr<std::vector<uint8_t>> FetchBytes(const std::string & sPath) {

                auto it = mFileBytes.find(sPath);
                if (it != mFileBytes.end()) { return it->second; }
                auto pBytes = std::make_shared<std::vector<uint8_t>>(ReadFileBytes(sPath));
                mFileBytes[sPath] = pBytes;
                return pBytes;
        }
};

// ---------------------------------------------------------------------------
// .sems — headless mesh decode (never SE::Mesh: it creates GL objects)
// ---------------------------------------------------------------------------

static void AppendMesh(const FlatBuffers::Mesh * pMesh, const glm::mat4 & mWorld,
                NavBake::SGeomMesh & out, const std::string & sSource) {

        if (pMesh->primitive_type() != FlatBuffers::PrimitiveType::GEOM_TRIANGLES) {
                log_w("nav_bake: mesh '{}' is not GEOM_TRIANGLES — skipped", sSource);
                return;
        }

        const FlatBuffers::VertexAttribute * pPos = nullptr;
        if (pMesh->attributes()) {
                for (const FlatBuffers::VertexAttribute * pA : *pMesh->attributes()) {
                        if (pA && pA->name() && pA->name()->str() == "Position") { pPos = pA; break; }
                }
        }
        if (pPos == nullptr || pPos->elem_size() != 3) {
                log_w("nav_bake: mesh '{}' has no 3-float Position attribute — skipped", sSource);
                return;
        }
        const FlatBuffers::VertexBuffer * pVB =
                (pMesh->vertices() && pMesh->vertices()->size() > pPos->buffer_ind())
                        ? pMesh->vertices()->Get(pPos->buffer_ind()) : nullptr;
        if (pVB == nullptr || pVB->buf_type() != FlatBuffers::VertexBufferU::FloatVector ||
            pVB->buf() == nullptr) {
                log_w("nav_bake: mesh '{}' Position buffer is not a FloatVector — skipped", sSource);
                return;
        }
        const auto * pFloatData = static_cast<const FlatBuffers::FloatVector *>(pVB->buf())->data();
        if (pFloatData == nullptr || pFloatData->size() == 0) {
                log_w("nav_bake: mesh '{}' has an empty Position buffer — skipped", sSource);
                return;
        }
        const float * pData = pFloatData->data();
        const size_t float_count = pFloatData->size();
        const size_t stride = (pVB->stride() >= 12) ? pVB->stride() : 32;
        if (static_cast<size_t>(pPos->offset()) + 12 > stride) {
                log_w("nav_bake: mesh '{}' Position offset/stride invalid — skipped", sSource);
                return;
        }

        const FlatBuffers::IndexBuffer * pIB = pMesh->index();
        if (pIB == nullptr || pIB->buf() == nullptr) {
                log_w("nav_bake: mesh '{}' has no index buffer — skipped", sSource);
                return;
        }
        auto IndexCount = [pIB]() -> uint32_t {
                switch (pIB->buf_type()) {
                case FlatBuffers::IndexBufferU::Uint8Vector: {
                        const auto * p = static_cast<const FlatBuffers::Uint8Vector *>(pIB->buf())->data();
                        return p ? static_cast<uint32_t>(p->size()) : 0;
                }
                case FlatBuffers::IndexBufferU::Uint16Vector: {
                        const auto * p = static_cast<const FlatBuffers::Uint16Vector *>(pIB->buf())->data();
                        return p ? static_cast<uint32_t>(p->size()) : 0;
                }
                default: {
                        const auto * p = static_cast<const FlatBuffers::Uint32Vector *>(pIB->buf())->data();
                        return p ? static_cast<uint32_t>(p->size()) : 0;
                }
                }
        };
        auto FetchIndex = [pIB](uint32_t i) -> uint32_t {
                switch (pIB->buf_type()) {
                case FlatBuffers::IndexBufferU::Uint8Vector:
                        return static_cast<const FlatBuffers::Uint8Vector *>(pIB->buf())->data()->Get(i);
                case FlatBuffers::IndexBufferU::Uint16Vector:
                        return static_cast<const FlatBuffers::Uint16Vector *>(pIB->buf())->data()->Get(i);
                default:
                        return static_cast<const FlatBuffers::Uint32Vector *>(pIB->buf())->data()->Get(i);
                }
        };
        const uint32_t idx_count = IndexCount();

        // triangle ranges from shapes (falls back to the whole buffer)
        std::vector<std::pair<uint32_t, uint32_t>> vRanges;
        if (pMesh->shapes() && pMesh->shapes()->size() > 0) {
                for (const FlatBuffers::Shape * pS : *pMesh->shapes()) {
                        if (pS == nullptr) { continue; }
                        const uint32_t start = pS->index_elem_start();
                        uint32_t count = pS->index_elem_count();
                        if (count == 0) { count = idx_count - std::min(start, idx_count); }
                        if (start + count > idx_count) { continue; }
                        vRanges.emplace_back(start, count);
                }
        } else {
                vRanges.emplace_back(0, idx_count);
        }

        // lazy vertex remap: world-transform each referenced vertex exactly once
        std::unordered_map<uint32_t, uint32_t> mRemap;
        auto VertexBase = [&](uint32_t idx) -> uint32_t {
                auto it = mRemap.find(idx);
                if (it != mRemap.end()) { return it->second; }
                const size_t first_float = static_cast<size_t>(idx) * stride / 4 + pPos->offset() / 4;
                if (idx * stride + pPos->offset() + 12 > float_count * 4) { return 0xffffffffu; }
                const glm::vec4 vWorld = mWorld * glm::vec4(pData[first_float], pData[first_float + 1], pData[first_float + 2], 1.0f);
                const uint32_t new_idx = static_cast<uint32_t>(out.vVertices.size() / 3);
                out.vVertices.push_back(vWorld.x);
                out.vVertices.push_back(vWorld.y);
                out.vVertices.push_back(vWorld.z);
                mRemap.emplace(idx, new_idx);
                return new_idx;
        };

        uint32_t dropped = 0;
        for (const auto & range : vRanges) {
                if (range.second < 3) { continue; }
                for (uint32_t i = 0; i + 2 < range.second; i += 3) {
                        const uint32_t a = VertexBase(FetchIndex(range.first + i + 0));
                        const uint32_t b = VertexBase(FetchIndex(range.first + i + 1));
                        const uint32_t c = VertexBase(FetchIndex(range.first + i + 2));
                        if (a == 0xffffffffu || b == 0xffffffffu || c == 0xffffffffu) { ++dropped; continue; }
                        out.vIndices.insert(out.vIndices.end(), { a, b, c });
                }
        }
        if (dropped > 0) {
                log_w("nav_bake: mesh '{}': {} triangles referenced out-of-range vertices — dropped", sSource, dropped);
        }
}

// ---------------------------------------------------------------------------
// .seterrain/.setile — headless heightfield decode (LOD0, dequantized)
// ---------------------------------------------------------------------------

// Appends every nav-enabled tile's LOD0 grid. Outputs the extent covered
// (xz from tile positions, y from the per-tile height ranges) so callers can
// size group bounds; bounds are used for the tile y-range, so terrain heights
// must be covered or spans clip at the tile AABB.
static bool AppendTerrainManifest(const std::string & sManifest, SToolGroup & oGroup, SToolContext & oCtx,
                           glm::vec3 & out_min, glm::vec3 & out_max,
                           const std::function<bool(const glm::vec3 &)> & oTileFilter = {}) {


        const auto pBytes = oCtx.FetchBytes(sManifest);
        flatbuffers::Verifier v(pBytes->data(), pBytes->size());
        if (!v.VerifyBuffer<FlatBuffers::Terrain>(FlatBuffers::TerrainIdentifier())) {
                throw std::runtime_error("invalid .seterrain: " + sManifest);
        }
        const FlatBuffers::Terrain * pTerrain = FlatBuffers::GetTerrain(pBytes->data());
        const glm::vec3 vOrigin = FbVec(pTerrain->origin());
        const float tile_size = pTerrain->tile_size();
        const std::string sTileDir = DirOf(sManifest);

        out_min = glm::vec3(FLT_MAX);
        out_max = glm::vec3(-FLT_MAX);

        if (pTerrain->tiles()) {
                for (const FlatBuffers::TerrainTileRef * pRef : *pTerrain->tiles()) {
                        if (pRef == nullptr || pRef->path() == nullptr) { continue; }
                        if ((pRef->flags() & static_cast<uint16_t>(FlatBuffers::TerrainTileFlags::NoNav)) != 0) {
                                continue;   // heightfield nav bake skips this tile
                        }

                        const std::string sTilePath = sTileDir + pRef->path()->str();
                        const auto pTileBytes = oCtx.FetchBytes(sTilePath);
                        flatbuffers::Verifier tv(pTileBytes->data(), pTileBytes->size());
                        if (!tv.VerifyBuffer<FlatBuffers::TerrainTileData>(FlatBuffers::TerrainIdentifier())) {
                                throw std::runtime_error("invalid .setile: " + sTilePath);
                        }
                        const FlatBuffers::TerrainTileData * pTile = flatbuffers::GetRoot<FlatBuffers::TerrainTileData>(pTileBytes->data());
                        if (pTile->lods() == nullptr || pTile->lods()->size() == 0) { continue; }
                        if (oTileFilter &&
                            !oTileFilter(vOrigin + glm::vec3(static_cast<float>(pTile->x()) * tile_size + tile_size * 0.5f, 0.0f,
                                                             static_cast<float>(pTile->y()) * tile_size + tile_size * 0.5f))) {
                                continue;   // tile claimed elsewhere (component-merged terrain)
                        }
                        const FlatBuffers::TerrainLod * pLod0 = pTile->lods()->Get(0);
                        if (pLod0 == nullptr || pLod0->heights() == nullptr || pLod0->resolution() < 2) { continue; }

                        NavBake::SGeomHeightfield oHf;
                        oHf.origin = vOrigin + glm::vec3(static_cast<float>(pTile->x()) * tile_size, 0.0f,
                                                         static_cast<float>(pTile->y()) * tile_size);
                        oHf.tile_size = tile_size;
                        oHf.resolution = pLod0->resolution();

                        const size_t sample_count = static_cast<size_t>(oHf.resolution) * oHf.resolution;
                        oHf.vHeights.resize(sample_count);
                        const float offset = pTile->height_offset();
                        const float scale = pTile->height_scale();
                        for (size_t s = 0; s < sample_count; ++s) {
                                oHf.vHeights[s] = offset + scale * static_cast<float>(pLod0->heights()->Get(static_cast<uint32_t>(s)));
                        }
                        if (pTile->hole_mask() && pTile->hole_mask()->size() > 0) {
                                oHf.vHoleMask.assign(pTile->hole_mask()->data(),
                                                     pTile->hole_mask()->data() + pTile->hole_mask()->size());
                        }

                        out_min = glm::min(out_min, oHf.origin + glm::vec3(0.0f, pRef->height_min(), 0.0f));
                        out_max = glm::max(out_max, oHf.origin + glm::vec3(tile_size, pRef->height_max(), tile_size));
                        oGroup.oGroup.vHeightfields.push_back(std::move(oHf));
                }
        }

        oCtx.sTerrains.insert(sManifest);
        return true;
}

// ---------------------------------------------------------------------------
// .sesc — node walk (TRS composition exactly per core/Transform.tcc)
// ---------------------------------------------------------------------------

static const FlatBuffers::Mesh * ResolveMesh(const FlatBuffers::MeshHolder * pHolder, SToolContext & oCtx,
                                      const std::string & sScene) {

        if (pHolder == nullptr) { return nullptr; }
        if (pHolder->mesh()) { return pHolder->mesh(); }
        if (pHolder->path() && pHolder->path()->size() > 0) {
                const std::string sPath = pHolder->path()->str();
                auto it = oCtx.mMeshes.find(sPath);
                if (it != oCtx.mMeshes.end()) { return it->second; }
                const auto pBytes = oCtx.FetchBytes(sPath);
                flatbuffers::Verifier v(pBytes->data(), pBytes->size());
                if (!v.VerifyBuffer<FlatBuffers::Mesh>(FlatBuffers::MeshIdentifier())) {
                        throw std::runtime_error("invalid .sems: " + sPath);
                }
                const FlatBuffers::Mesh * pMesh = FlatBuffers::GetMesh(pBytes->data());
                oCtx.mMeshes[sPath] = pMesh;
                return pMesh;
        }
        // name-flavor: resolve against the scene's inline resource table
        if (pHolder->name() && pHolder->name()->size() > 0) {
                auto itScene = oCtx.mSceneMeshes.find(sScene);
                if (itScene != oCtx.mSceneMeshes.end()) {
                        auto it = itScene->second.find(pHolder->name()->str());
                        if (it != itScene->second.end()) { return it->second; }
                }
        }
        return nullptr;
}

static void WalkNode(const FlatBuffers::Node * pNode, const glm::mat4 & mParentWorld,
              SToolGroup & oGroup, SToolContext & oCtx, const std::string & sScene) {

        if (pNode == nullptr) { return; }

        const glm::mat4 mWorld = mParentWorld * LocalMatrix(FbVec(pNode->translation()),
                                                            FbVec(pNode->rotation()),
                                                            FbVec(pNode->scale(), glm::vec3(1.0f)));
        const glm::vec3 vWorldPos(mWorld[3]);

        if (pNode->name() && pNode->name()->size() > 0) {
                oGroup.mNodePositions.emplace(pNode->name()->str(), vWorldPos);
        }

        if (pNode->components()) {
                for (const FlatBuffers::Component * pComp : *pNode->components()) {
                        if (pComp == nullptr) { continue; }
                        switch (pComp->component_type()) {
                        case FlatBuffers::ComponentU::StaticModel: {
                                const FlatBuffers::MeshHolder * pHolder = pComp->component_as_StaticModel()->mesh();
                                const FlatBuffers::Mesh * pMesh = ResolveMesh(pHolder, oCtx, sScene);
                                if (pMesh != nullptr) {
                                        oGroup.oGroup.vMeshes.emplace_back();
                                        AppendMesh(pMesh, mWorld, oGroup.oGroup.vMeshes.back(), sScene);
                                } else {
                                        log_w("nav_bake: scene '{}': StaticModel mesh '{}' unresolved — skipped",
                                              sScene, (pHolder && pHolder->name()) ? pHolder->name()->str() : "?");
                                }
                                break;
                        }
                        case FlatBuffers::ComponentU::NavArea: {
                                const FlatBuffers::NavArea * pFB = pComp->component_as_NavArea();
                                // world-axis-aligned box around the node; a rotated node yields its
                                // conservative world AABB
                                const glm::mat3 mRot(mWorld);
                                const glm::vec3 vLocalHalf = FbVec(pFB->size()) * 0.5f;
                                const glm::vec3 vHalf = glm::abs(mRot[0]) * vLocalHalf.x +
                                                        glm::abs(mRot[1]) * vLocalHalf.y +
                                                        glm::abs(mRot[2]) * vLocalHalf.z;
                                NavBake::SAreaBox oBox;
                                oBox.center = vWorldPos;
                                oBox.half_extents = vHalf;
                                oBox.exclusion = (pFB->area() == nullptr || pFB->area()->size() == 0);
                                oGroup.oGroup.vAreaBoxes.push_back(oBox);
                                oGroup.vAreaNames.push_back(pFB->area() ? pFB->area()->str() : "");
                                break;
                        }
                        case FlatBuffers::ComponentU::NavLink: {
                                const FlatBuffers::NavLink * pFB = pComp->component_as_NavLink();
                                NavBake::SLink oLink;
                                oLink.start = vWorldPos;
                                oLink.radius = pFB->radius();
                                oLink.bidirectional = pFB->bidirectional();
                                oGroup.oGroup.vLinks.push_back(oLink);
                                oGroup.vLinkAreas.push_back(pFB->area() ? pFB->area()->str() : "");
                                oGroup.vLinkNodeWorld.push_back(mWorld);

                                SToolGroup::SPendingEnd oPending;
                                oPending.link_index = static_cast<uint32_t>(oGroup.oGroup.vLinks.size() - 1);
                                if (pFB->end_node() && pFB->end_node()->size() > 0) {
                                        oPending.sNode = pFB->end_node()->str();
                                } else {
                                        oPending.vOffset = FbVec(pFB->end());
                                }
                                oGroup.vPendingEnds.push_back(std::move(oPending));
                                break;
                        }
                        case FlatBuffers::ComponentU::TerrainComponent: {
                                const FlatBuffers::TerrainComponent * pFB = pComp->component_as_TerrainComponent();
                                if (pFB->manifest() && pFB->manifest()->size() > 0) {
                                        glm::vec3 tmin, tmax;
                                        AppendTerrainManifest(pFB->manifest()->str(), oGroup, oCtx, tmin, tmax);
                                        // expand the group's y-range to the terrain heights (claims stay xz-driven)
                                        oGroup.oGroup.bounds_min.y = glm::min(oGroup.oGroup.bounds_min.y, tmin.y);
                                        oGroup.oGroup.bounds_max.y = glm::max(oGroup.oGroup.bounds_max.y, tmax.y);
                                }
                                break;
                        }
                        default:
                                break;   // not nav-relevant
                        }
                }
        }

        if (pNode->children()) {
                for (const FlatBuffers::Node * pChild : *pNode->children()) {
                        WalkNode(pChild, mWorld, oGroup, oCtx, sScene);
                }
        }
}

// Resolves pending link ends: end_node → sibling node world pos, else the
// node-local offset composed through the authoring node's full transform.
static void ResolveLinks(SToolGroup & oGroup) {

        for (const SToolGroup::SPendingEnd & oPending : oGroup.vPendingEnds) {
                NavBake::SLink & oLink = oGroup.oGroup.vLinks[oPending.link_index];
                if (!oPending.sNode.empty()) {
                        auto it = oGroup.mNodePositions.find(oPending.sNode);
                        if (it == oGroup.mNodePositions.end()) {
                                throw std::runtime_error("NavLink end_node '" + oPending.sNode + "' not found in group '" +
                                                         oGroup.oGroup.id + "'");
                        }
                        oLink.end = it->second;
                } else {
                        oLink.end = glm::vec3(oGroup.vLinkNodeWorld[oPending.link_index] * glm::vec4(oPending.vOffset, 1.0f));
                }
        }
        oGroup.vPendingEnds.clear();
}

// ---------------------------------------------------------------------------
// input loading
// ---------------------------------------------------------------------------

// Parses + verifies the .seworld; returns the streaming cell_size (schema default).
static float ParseWorld(const std::string & sWorldPath, SToolContext & oCtx,
                 const FlatBuffers::World *& out_world) {

        const auto pBytes = oCtx.FetchBytes(sWorldPath);
        flatbuffers::Verifier v(pBytes->data(), pBytes->size());
        if (!v.VerifyBuffer<FlatBuffers::World>(FlatBuffers::WorldIdentifier())) {
                throw std::runtime_error("invalid .seworld: " + sWorldPath);
        }
        out_world = FlatBuffers::GetWorld(pBytes->data());
        return out_world->streaming() ? out_world->streaming()->cell_size() : 64.0f;
}

static void LoadWorldGroups(const FlatBuffers::World * pWorld, SToolContext & oCtx) {

        if (pWorld->sub_levels() == nullptr) { return; }

        // component-merged terrain: when the world declares a terrain grid, its
        // heightfield tiles ride the normal sub-level groups that cover them
        // ("component-merged terrain" — NavigationSystem::ActivateTerrainTile
        // tolerates the manifest being absent as a group), and the world-level
        // terrain group keeps only the unclaimed tiles.
        const std::string sTerrainManifest =
                (pWorld->terrain_grid() && pWorld->terrain_grid()->manifest())
                        ? pWorld->terrain_grid()->manifest()->str() : "";
        std::vector<glm::vec3> vClaimedMin, vClaimedMax;

        for (const FlatBuffers::SubLevelRef * pRef : *pWorld->sub_levels()) {
                if (pRef == nullptr || pRef->path() == nullptr) { continue; }
                const uint32_t flags = pRef->flags();
                if ((flags & static_cast<uint32_t>(FlatBuffers::SubLevelFlags::NoNav)) != 0) {
                        log_i("nav_bake: sub-level '{}' flagged NoNav — skipped", pRef->path()->str());
                        continue;
                }

                SToolGroup oGroup;
                oGroup.oGroup.id = pRef->path()->str();
                oGroup.oGroup.coexisting = (flags & static_cast<uint32_t>(FlatBuffers::SubLevelFlags::Manual)) != 0;
                oGroup.oGroup.bounds_min = FbVec(pRef->bounds_min());
                oGroup.oGroup.bounds_max = FbVec(pRef->bounds_max());
                const bool bNormal = !oGroup.oGroup.coexisting;
                const glm::vec3 vSubMin = oGroup.oGroup.bounds_min;
                const glm::vec3 vSubMax = oGroup.oGroup.bounds_max;

                // instance placement: the wrapper transform WorldSystem applies on activation
                const glm::mat4 mWrapper = LocalMatrix(FbVec(pRef->translation()), FbVec(pRef->rotation()), glm::vec3(1.0f));
                const std::string sScene = pRef->path()->str();
                const auto pSceneBytes = oCtx.FetchBytes(sScene);
                flatbuffers::Verifier sv(pSceneBytes->data(), pSceneBytes->size());
                if (!sv.VerifyBuffer<FlatBuffers::SceneTree>(FlatBuffers::SceneTreeIdentifier())) {
                        throw std::runtime_error("invalid .sesc: " + sScene);
                }
                oCtx.IndexSceneResources(sScene, FlatBuffers::GetSceneTree(pSceneBytes->data()));
                WalkNode(FlatBuffers::GetSceneTree(pSceneBytes->data())->root(), mWrapper, oGroup, oCtx, sScene);
                ResolveLinks(oGroup);

                if (bNormal && !sTerrainManifest.empty()) {
                        glm::vec3 tmin(FLT_MAX), tmax(-FLT_MAX);
                        AppendTerrainManifest(sTerrainManifest, oGroup, oCtx, tmin, tmax,
                                              [&vSubMin, &vSubMax](const glm::vec3 & vCenter) {
                                                      return vCenter.x >= vSubMin.x && vCenter.x <= vSubMax.x &&
                                                             vCenter.z >= vSubMin.z && vCenter.z <= vSubMax.z;
                                              });
                        if (!oGroup.oGroup.vHeightfields.empty()) {
                                oGroup.oGroup.bounds_min = glm::min(oGroup.oGroup.bounds_min, tmin);
                                oGroup.oGroup.bounds_max = glm::max(oGroup.oGroup.bounds_max, tmax);
                                vClaimedMin.push_back(tmin);
                                vClaimedMax.push_back(tmax);
                                log_i("nav_bake: merged {} terrain tile(s) of '{}' into group '{}'",
                                      oGroup.oGroup.vHeightfields.size(), sTerrainManifest, oGroup.oGroup.id);
                        }
                }
                oCtx.vGroups.push_back(std::move(oGroup));
        }

        // world-level terrain grid (flight-sim consumption mode): its own group —
        // minus tiles already merged into normal sub-level groups above
        if (pWorld->terrain_grid() && pWorld->terrain_grid()->manifest()) {
                const std::string sManifest = pWorld->terrain_grid()->manifest()->str();
                SToolGroup oGroup;
                oGroup.oGroup.id = sManifest;
                glm::vec3 tmin, tmax;
                if (!vClaimedMin.empty()) {
                        AppendTerrainManifest(sManifest, oGroup, oCtx, tmin, tmax,
                                              [&vClaimedMin, &vClaimedMax](const glm::vec3 & vCenter) {
                                                      for (size_t k = 0; k < vClaimedMin.size(); ++k) {
                                                              if (vCenter.x >= vClaimedMin[k].x && vCenter.x <= vClaimedMax[k].x &&
                                                                  vCenter.z >= vClaimedMin[k].z && vCenter.z <= vClaimedMax[k].z) {
                                                                      return false;
                                                              }
                                                      }
                                                      return true;
                                              });
                } else {
                        AppendTerrainManifest(sManifest, oGroup, oCtx, tmin, tmax);
                }
                if (oGroup.oGroup.vHeightfields.empty()) {
                        log_i("nav_bake: terrain grid '{}' fully merged into sub-level groups", sManifest);
                } else {
                        oGroup.oGroup.bounds_min = tmin;
                        oGroup.oGroup.bounds_max = tmax;
                        oCtx.vGroups.push_back(std::move(oGroup));
                }
        }
}

static void LoadSescList(const std::vector<std::string> & vPaths, SToolContext & oCtx) {

        for (const std::string & sScene : vPaths) {
                const auto pSceneBytes = oCtx.FetchBytes(sScene);
                flatbuffers::Verifier sv(pSceneBytes->data(), pSceneBytes->size());
                if (!sv.VerifyBuffer<FlatBuffers::SceneTree>(FlatBuffers::SceneTreeIdentifier())) {
                        throw std::runtime_error("invalid .sesc: " + sScene);
                }
                SToolGroup oGroup;
                oGroup.oGroup.id = sScene;
                oCtx.IndexSceneResources(sScene, FlatBuffers::GetSceneTree(pSceneBytes->data()));
                WalkNode(FlatBuffers::GetSceneTree(pSceneBytes->data())->root(), glm::mat4(1.0f), oGroup, oCtx, sScene);
                ResolveLinks(oGroup);

                // worldless mode: bounds from content (meshes, terrain, links)
                glm::vec3 bmin(FLT_MAX), bmax(-FLT_MAX);
                for (const NavBake::SGeomMesh & m : oGroup.oGroup.vMeshes) {
                        for (size_t v = 0; v + 2 < m.vVertices.size(); v += 3) {
                                bmin = glm::min(bmin, glm::vec3(m.vVertices[v], m.vVertices[v + 1], m.vVertices[v + 2]));
                                bmax = glm::max(bmax, glm::vec3(m.vVertices[v], m.vVertices[v + 1], m.vVertices[v + 2]));
                        }
                }
                for (const NavBake::SGeomHeightfield & hf : oGroup.oGroup.vHeightfields) {
                        bmin = glm::min(bmin, hf.origin);
                        bmax = glm::max(bmax, hf.origin + glm::vec3(hf.tile_size, 0.0f, hf.tile_size));
                }
                for (const NavBake::SLink & l : oGroup.oGroup.vLinks) {
                        bmin = glm::min(bmin, glm::min(l.start, l.end));
                        bmax = glm::max(bmax, glm::max(l.start, l.end));
                }
                if (bmin.x > bmax.x) { throw std::runtime_error("scene '" + sScene + "' has no bakable geometry"); }
                oGroup.oGroup.bounds_min = bmin - glm::vec3(1.0f);
                oGroup.oGroup.bounds_max = bmax + glm::vec3(1.0f);
                oCtx.vGroups.push_back(std::move(oGroup));
        }
}

// ---------------------------------------------------------------------------
// output
// ---------------------------------------------------------------------------

static void WriteNavMesh(const std::string & sOutPath, const std::string & sClass,
                  const NavBake::SConfig & oCfg, const NavBake::SOutput & oOut) {

        FlatBuffers::NavMeshAssetT oAsset;
        oAsset.agent_class = sClass;
        oAsset.version = 1;
        oAsset.origin = std::make_unique<FlatBuffers::Vec3>(oOut.origin.x, oOut.origin.y, oOut.origin.z);
        oAsset.tile_dim = oOut.tile_dim;
        oAsset.voxel_size = oCfg.voxel_size;
        oAsset.voxel_height = oCfg.voxel_height;
        oAsset.areas = oCfg.vAreas;

        for (const NavBake::SNavTileBytes & oTile : oOut.vTiles) {
                auto pTile = std::make_unique<FlatBuffers::NavTileT>();
                pTile->x = oTile.x;
                pTile->y = oTile.y;
                pTile->layer = oTile.layer;
                pTile->group = oTile.group;
                pTile->data = oTile.vBytes;
                oAsset.tiles.push_back(std::move(pTile));
        }

        for (const NavBake::SLinkRecord & oRec : oOut.vLinks) {
                auto pLink = std::make_unique<FlatBuffers::OffMeshLinkT>();
                pLink->start = std::make_unique<FlatBuffers::Vec3>(oRec.start.x, oRec.start.y, oRec.start.z);
                pLink->end = std::make_unique<FlatBuffers::Vec3>(oRec.end.x, oRec.end.y, oRec.end.z);
                pLink->radius = oRec.radius;
                pLink->area = static_cast<uint16_t>(oRec.registry_index);
                pLink->flags = oRec.flags;
                pLink->bidirectional = oRec.bidirectional;
                pLink->group = oRec.group;
                oAsset.links.push_back(std::move(pLink));
        }

        flatbuffers::FlatBufferBuilder fbb(1 << 20);
        fbb.Finish(FlatBuffers::NavMeshAsset::Pack(fbb, &oAsset), FlatBuffers::NavMeshAssetIdentifier());

        const auto oParent = std::filesystem::path(sOutPath).parent_path();
        if (!oParent.empty()) { std::filesystem::create_directories(oParent); }
        std::ofstream f(sOutPath, std::ios::binary);
        if (!f) { throw std::runtime_error("cannot write output: " + sOutPath); }
        f.write(reinterpret_cast<const char *>(fbb.GetBufferPointer()), static_cast<std::streamsize>(fbb.GetSize()));

        log_i("nav_bake: wrote {} ({} tiles, {} polys, {} links, {} areas)",
              sOutPath, oAsset.tiles.size(), oOut.poly_count, oAsset.links.size(), oAsset.areas.size());
}

// ---------------------------------------------------------------------------

int main(int argc, char * argv[]) {

        gLogger = spdlog::stdout_logger_mt("Init");

        po::options_description oDesc("nav_bake — bake static scene geometry into tiled navmesh assets\n\nUsage");
        oDesc.add_options()
                ("help,h", "print help")
                ("config", po::value<std::string>()->required(), "path to bake config (JSON)")
                ("level",  po::value<std::string>()->default_value("info"), "log level: debug|info|warn|error")
                ("log",    po::value<std::string>()->default_value("stdout"), "log destination: stdout|stderr|<file>");

        po::positional_options_description oPos;
        oPos.add("config", 1);

        po::variables_map oVm;
        try {
                po::store(po::command_line_parser(argc, argv).options(oDesc).positional(oPos).run(), oVm);
                if (oVm.count("help")) { std::cout << oDesc << "\n"; return 0; }
                po::notify(oVm);
        }
        catch (const std::exception & ex) {
                std::cerr << ex.what() << "\n" << oDesc << "\n";
                return 1;
        }

        // log routing (house convention)
        {
                const std::string sLevel = oVm["level"].as<std::string>();
                const std::string sLog = oVm["log"].as<std::string>();
                if (sLog == "stderr") {
                        spdlog::drop("Init");
                        gLogger = spdlog::stderr_logger_mt("Init");
                } else if (sLog != "stdout") {
                        spdlog::drop("Init");
                        gLogger = spdlog::basic_logger_mt("Init", sLog);
                }
                gLogger->set_level(spdlog::level::from_str(sLevel));
        }

        try {
                // ---- config ------------------------------------------------------
                const std::string sConfigPath = oVm["config"].as<std::string>();
                json oJson;
                {
                        std::ifstream f(sConfigPath);
                        if (!f) { throw std::runtime_error("cannot open config: " + sConfigPath); }
                        f >> oJson;
                }

                const std::string sOutput = oJson.at("output").get<std::string>();
                const float tile_dim = oJson.value("tile_dim", 16.0f);
                const float voxel_size = oJson.value("voxel_size", 0.2f);
                const float voxel_height = oJson.value("voxel_height", 0.2f);

                SToolContext oCtx;
                std::string sWorldId = std::filesystem::path(sOutput).filename().string();
                float cell_size = 0.0f;   // world mode: from the .seworld; otherwise no check

                if (oJson.contains("world")) {
                        const std::string sWorldPath = oJson["world"].get<std::string>();
                        const FlatBuffers::World * pWorld = nullptr;
                        cell_size = ParseWorld(sWorldPath, oCtx, pWorld);
                        if (pWorld->id() && pWorld->id()->size() > 0) { sWorldId = pWorld->id()->str(); }
                        LoadWorldGroups(pWorld, oCtx);
                } else if (oJson.contains("sesc")) {
                        std::vector<std::string> vScenes;
                        for (const auto & s : oJson["sesc"]) { vScenes.push_back(s.get<std::string>()); }
                        LoadSescList(vScenes, oCtx);
                } else {
                        throw std::runtime_error("config needs either \"world\" or \"sesc\"");
                }

                // ---- area registry (sorted unique names, ground entry first) -------
                std::set<std::string> sNames;
                for (const SToolGroup & g : oCtx.vGroups) {
                        for (const std::string & s : g.vAreaNames) { if (!s.empty()) { sNames.insert(s); } }
                        for (const std::string & s : g.vLinkAreas) { if (!s.empty()) { sNames.insert(s); } }
                }
                std::vector<std::string> vAreas;
                vAreas.push_back("");
                for (const std::string & s : sNames) { vAreas.push_back(s); }
                auto RegistryIndex = [&vAreas](const std::string & s) -> uint32_t {
                        for (size_t i = 0; i < vAreas.size(); ++i) { if (vAreas[i] == s) { return static_cast<uint32_t>(i); } }
                        return 0;
                };
                for (SToolGroup & g : oCtx.vGroups) {
                        for (size_t i = 0; i < g.oGroup.vAreaBoxes.size(); ++i) {
                                if (!g.oGroup.vAreaBoxes[i].exclusion) {
                                        g.oGroup.vAreaBoxes[i].registry_index = RegistryIndex(g.vAreaNames[i]);
                                }
                        }
                        for (size_t i = 0; i < g.oGroup.vLinks.size(); ++i) {
                                g.oGroup.vLinks[i].registry_index = RegistryIndex(g.vLinkAreas[i]);
                        }
                }

                // ---- agent classes --------------------------------------------------
                std::vector<NavBake::SAgentClass> vClasses;
                if (oJson.contains("classes")) {
                        for (const auto & jc : oJson["classes"]) {
                                NavBake::SAgentClass c;
                                c.name = jc.value("name", "default");
                                c.radius = jc.value("radius", 0.5f);
                                c.height = jc.value("height", 2.0f);
                                c.max_slope_deg = jc.value("max_slope_deg", 50.0f);
                                c.step_height = jc.value("step_height", 0.4f);
                                vClasses.push_back(c);
                        }
                }
                if (vClasses.empty()) { vClasses.push_back(NavBake::SAgentClass{}); }

                // ---- bake + write (one .senm per class) ------------------------------
                for (const NavBake::SAgentClass & oClass : vClasses) {
                        NavBake::SConfig oCfg;
                        oCfg.world_id = sWorldId;
                        oCfg.agent = oClass;
                        oCfg.tile_dim = tile_dim;
                        oCfg.cell_size = cell_size;
                        oCfg.voxel_size = voxel_size;
                        oCfg.voxel_height = voxel_height;
                        oCfg.vAreas = vAreas;

                        std::vector<NavBake::SGroup> vLibGroups;
                        vLibGroups.reserve(oCtx.vGroups.size());
                        for (const SToolGroup & g : oCtx.vGroups) { vLibGroups.push_back(g.oGroup); }

                        NavBake::SBakeInput oIn;
                        oIn.vGroups = std::move(vLibGroups);
                        NavBake::SOutput oOut;
                        if (!NavBake::Bake(oIn, oCfg, oOut)) {
                                log_e("nav_bake: bake failed for agent class '{}'", oClass.name);
                                return 1;
                        }

                        WriteNavMesh(sOutput + "_" + oClass.name + ".senm", oClass.name, oCfg, oOut);
                }
        }
        catch (const std::exception & ex) {
                log_e("nav_bake: {}", ex.what());
                return 1;
        }

        return 0;
}
