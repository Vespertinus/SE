
#ifdef SE_IMPL

#include <terrain/TerrainAsset.h>

#include <Logging.h>

#include <flatbuffers/flatbuffers.h>

#include <fstream>

namespace SE {

TerrainAsset::TerrainAsset(const std::string & sName, rid_t rid)
        : ResourceHolder(rid, sName) {

        std::ifstream f(sName, std::ios::binary | std::ios::ate);
        if (!f.is_open()) {
                log_e("TerrainAsset: failed to open '{}'", sName);
                return;
        }

        const size_t sz = static_cast<size_t>(f.tellg());
        f.seekg(0);
        std::vector<uint8_t> raw(sz);
        f.read(reinterpret_cast<char *>(raw.data()), static_cast<std::streamsize>(sz));

        flatbuffers::Verifier verifier(raw.data(), raw.size());
        if (!SE::FlatBuffers::VerifyTerrainBuffer(verifier)) {
                log_e("TerrainAsset: FlatBuffer verify failed for '{}'", sName);
                return;
        }

        Build(SE::FlatBuffers::GetTerrain(raw.data()));
        size = static_cast<uint32_t>(sz);

        log_d("TerrainAsset: loaded '{}' ({} bytes, {}x{} tiles, {} LODs)",
              sName, sz, tiles_x, tiles_y, vLodResolutions.size());
}

TerrainAsset::TerrainAsset(const std::string & sName, rid_t rid, const FlatBuffers::Terrain * pFb)
        : ResourceHolder(rid, sName) {

        Build(pFb);
        log_d("TerrainAsset: built inline '{}' ({}x{} tiles)", sName, tiles_x, tiles_y);
}

const TerrainAsset::STileRef * TerrainAsset::FindTile(uint16_t x, uint16_t y) const {

        const auto it = mTileLookup.find(PackCoords(x, y));
        if (it == mTileLookup.end()) return nullptr;
        return &vTiles[it->second];
}

int32_t TerrainAsset::TileIndexAt(uint16_t x, uint16_t y) const {

        const auto it = mTileLookup.find(PackCoords(x, y));
        return it != mTileLookup.end() ? static_cast<int32_t>(it->second) : -1;
}

void TerrainAsset::Build(const FlatBuffers::Terrain * pFb) {

        if (!pFb) return;

        if (pFb->id()) sId = pFb->id()->str();

        tile_size = pFb->tile_size();
        if (tile_size <= 0.0f) {
                log_e("TerrainAsset: '{}': invalid tile_size {}", sName, tile_size);
                return;
        }
        if (const auto * oOrigin = pFb->origin()) {
                vOrigin = glm::vec3(oOrigin->x(), oOrigin->y(), oOrigin->z());
        }

        if (pFb->lod_resolutions()) {
                for (const auto r : *pFb->lod_resolutions()) {
                        if (r < 2) {
                                log_e("TerrainAsset: '{}': LOD resolution {} < 2", sName, r);
                                return;
                        }
                        vLodResolutions.push_back(r);
                }
        }
        if (vLodResolutions.empty()) {
                log_e("TerrainAsset: '{}': no LOD resolutions", sName);
                return;
        }

        if (pFb->tiles()) {
                for (const auto * t : *pFb->tiles()) {
                        if (!t || !t->path() || t->path()->size() == 0) {
                                log_e("TerrainAsset: '{}': tile ref without path", sName);
                                return;
                        }
                        if (t->x() >= tiles_x) tiles_x = static_cast<uint16_t>(t->x() + 1);
                        if (t->y() >= tiles_y) tiles_y = static_cast<uint16_t>(t->y() + 1);

                        STileRef oRef;
                        oRef.x = t->x();
                        oRef.y = t->y();
                        oRef.sPath = t->path()->str();
                        if (const auto * oMin = t->bounds_min()) oRef.vBoundsMin = glm::vec3(oMin->x(), oMin->y(), oMin->z());
                        if (const auto * oMax = t->bounds_max()) oRef.vBoundsMax = glm::vec3(oMax->x(), oMax->y(), oMax->z());
                        oRef.height_min = t->height_min();
                        oRef.height_max = t->height_max();
                        oRef.flags = t->flags();

                        const uint64_t key = PackCoords(oRef.x, oRef.y);
                        if (mTileLookup.count(key) > 0) {
                                log_e("TerrainAsset: '{}': duplicate tile ({}, {})", sName, oRef.x, oRef.y);
                                return;
                        }
                        mTileLookup[key] = static_cast<uint32_t>(vTiles.size());
                        vTiles.push_back(std::move(oRef));
                }
        }
        if (vTiles.empty()) {
                log_e("TerrainAsset: '{}': no tiles", sName);
                return;
        }

        if (pFb->material()) {
                if (pFb->material()->material_path()) {
                        oMaterial.sPath = pFb->material()->material_path()->str();
                }
                oMaterial.uv_scale = pFb->material()->uv_scale();
        }
        if (pFb->physics()) {
                oPhysics.friction = pFb->physics()->friction();
                oPhysics.max_slope_deg = pFb->physics()->max_slope_deg();
        }

        height_min = pFb->height_min();
        height_max = pFb->height_max();
}

std::string TerrainAsset::Str() const {
        return fmt::format("TerrainAsset['{}' tiles={}x{} lods={} tile_size={}]",
                        sName, tiles_x, tiles_y, vLodResolutions.size(), tile_size);
}

} // namespace SE

#endif // SE_IMPL
