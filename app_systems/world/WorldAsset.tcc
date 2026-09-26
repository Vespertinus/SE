
#ifdef SE_IMPL

#include <world/WorldAsset.h>
#include <Logging.h>

#include <flatbuffers/flatbuffers.h>
#include <fstream>
#include <stdexcept>

namespace SE {

WorldAsset::WorldAsset(const std::string& sName, rid_t rid)
        : ResourceHolder(rid, sName) {

        std::ifstream f(sName, std::ios::binary | std::ios::ate);
        if (!f.is_open()) {
                throw std::runtime_error(fmt::format("WorldAsset: failed to open '{}'", sName));
        }

        const size_t sz = static_cast<size_t>(f.tellg());
        f.seekg(0);
        std::vector<uint8_t> raw(sz);
        f.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(sz));

        flatbuffers::Verifier verifier(raw.data(), raw.size());
        if (!SE::FlatBuffers::VerifyWorldBuffer(verifier)) {
                throw std::runtime_error(
                                fmt::format("WorldAsset: FlatBuffer verify failed for '{}'", sName));
        }

        Build(SE::FlatBuffers::GetWorld(raw.data()));
        size = static_cast<uint32_t>(sz);

        log_d("WorldAsset: loaded '{}' ({} bytes, {} sub-levels)",
                        sName, sz, vSubLevels.size());
}

WorldAsset::WorldAsset(const std::string& sName, rid_t rid, const FlatBuffers::World* pWorld)
        : ResourceHolder(rid, sName) {

        Build(pWorld);
        log_d("WorldAsset: built inline '{}' ({} sub-levels)", sName, vSubLevels.size());
}

void WorldAsset::Build(const FlatBuffers::World* pWorld) {

        if (!pWorld || !pWorld->id()) {
                throw std::runtime_error("WorldAsset: null world or missing id");
        }

        id = StrID(pWorld->id()->str());

        if (const auto* pS = pWorld->streaming()) {
                streaming.enabled                   = pS->enabled();
                streaming.cell_size                 = pS->cell_size();
                streaming.load_radius               = pS->load_radius();
                streaming.unload_radius             = pS->unload_radius();
                streaming.max_activations_per_frame = pS->max_activations_per_frame();
                streaming.main_budget_ms            = pS->main_budget_ms();
                streaming.unload_frame_delay        = pS->unload_frame_delay();

                if (streaming.cell_size <= 0.f) {
                        throw std::runtime_error(
                                        fmt::format("WorldAsset '{}': streaming.cell_size must be > 0", sName));
                }
                if (streaming.load_radius <= 0.f || streaming.unload_radius < streaming.load_radius) {
                        throw std::runtime_error(fmt::format(
                                        "WorldAsset '{}': need 0 < load_radius <= unload_radius (hysteresis band)",
                                        sName));
                }
                if (streaming.max_activations_per_frame == 0) {
                        throw std::runtime_error(fmt::format(
                                        "WorldAsset '{}': streaming.max_activations_per_frame must be >= 1",
                                        sName));
                }
                if (streaming.unload_frame_delay == 0) {
                        throw std::runtime_error(fmt::format(
                                        "WorldAsset '{}': streaming.unload_frame_delay must be >= 1",
                                        sName));
                }
                if (streaming.main_budget_ms < 0.f) {
                        throw std::runtime_error(fmt::format(
                                        "WorldAsset '{}': streaming.main_budget_ms must be >= 0",
                                        sName));
                }
        }

        if (pWorld->terrain_grid()) {
                if (pWorld->terrain_grid()->manifest() && pWorld->terrain_grid()->manifest()->size() > 0) {
                        sTerrainGridManifest = pWorld->terrain_grid()->manifest()->str();
                }
        }

        if (pWorld->nav()) {
                const auto * pNav = pWorld->nav();

                if (pNav->default_class()) {
                        sDefaultNavClass = pNav->default_class()->str();
                }

                if (pNav->meshes()) {
                        vNavMeshes.reserve(pNav->meshes()->size());

                        for (const auto * pEntry : *pNav->meshes()) {
                                if (!pEntry || !pEntry->agent_class() || pEntry->agent_class()->size() == 0) {
                                        throw std::runtime_error(
                                                        fmt::format("WorldAsset '{}': nav mesh entry has no agent_class", sName));
                                }
                                const auto * pHolder = pEntry->mesh();
                                if (!pHolder) {
                                        throw std::runtime_error(fmt::format(
                                                        "WorldAsset '{}': nav mesh '{}' has no holder", sName,
                                                        pEntry->agent_class()->str()));
                                }

                                NavMeshEntry e;
                                e.agent_class = pEntry->agent_class()->str();

                                if (pHolder->path() && pHolder->path()->size() > 0) {
                                        e.path = pHolder->path()->str();
                                } else if (pHolder->mesh()) {
                                        // Inline embedding: the holder points into this (about to be
                                        // discarded) world buffer, so repack the sub-table into a
                                        // standalone .senm blob now; the bridge creates the resource
                                        // from it under the holder's cache name.
                                        if (!pHolder->name() || pHolder->name()->size() == 0) {
                                                throw std::runtime_error(fmt::format(
                                                                "WorldAsset '{}': inline nav mesh '{}' requires a name (resource cache key)",
                                                                sName, e.agent_class));
                                        }
                                        e.cache_name = pHolder->name()->str();

                                        FlatBuffers::NavMeshAssetT oNative;
                                        pHolder->mesh()->UnPackTo(&oNative);
                                        flatbuffers::FlatBufferBuilder oFbb;
                                        // Finish variant writes the SENM identifier — the
                                        // NavMeshAsset verifier demands it
                                        FlatBuffers::FinishNavMeshAssetBuffer(
                                                        oFbb, FlatBuffers::CreateNavMeshAsset(oFbb, &oNative));
                                        e.pInlineData = std::make_shared<std::vector<uint8_t>>(
                                                        oFbb.GetBufferPointer(), oFbb.GetBufferPointer() + oFbb.GetSize());
                                } else {
                                        throw std::runtime_error(fmt::format(
                                                        "WorldAsset '{}': nav mesh '{}' holder has neither path nor inline data",
                                                        sName, e.agent_class));
                                }

                                vNavMeshes.push_back(std::move(e));
                        }
                }
        }

        if (pWorld->global_blackboard()) {
                const auto* pHolder = pWorld->global_blackboard();

                if (pHolder->path() && pHolder->path()->size() > 0) {
                        oGlobalBlackboard.path = pHolder->path()->str();
                } else if (pHolder->blackboard()) {
                        // Inline embedding: repack the sub-table into a standalone
                        // .sebb blob now (the world buffer is about to be discarded);
                        // BlackboardSystem creates the resource from it under the
                        // holder's cache name.
                        if (!pHolder->name() || pHolder->name()->size() == 0) {
                                throw std::runtime_error(fmt::format(
                                                "WorldAsset '{}': inline global blackboard requires a name "
                                                "(resource cache key)", sName));
                        }
                        oGlobalBlackboard.cache_name = pHolder->name()->str();

                        FlatBuffers::BlackboardDefinitionT oNative;
                        pHolder->blackboard()->UnPackTo(&oNative);
                        flatbuffers::FlatBufferBuilder oFbb;
                        // Finish variant writes the SEBB identifier — the
                        // BlackboardAsset verifier demands it
                        FlatBuffers::FinishBlackboardDefinitionBuffer(
                                        oFbb, FlatBuffers::CreateBlackboardDefinition(oFbb, &oNative));
                        oGlobalBlackboard.pInlineData = std::make_shared<std::vector<uint8_t>>(
                                        oFbb.GetBufferPointer(), oFbb.GetBufferPointer() + oFbb.GetSize());
                } else {
                        throw std::runtime_error(fmt::format(
                                        "WorldAsset '{}': global_blackboard holder has neither path nor "
                                        "inline data", sName));
                }
        }

        if (pWorld->sub_levels()) {
                vSubLevels.reserve(pWorld->sub_levels()->size());

                for (uint32_t idx = 0; idx < pWorld->sub_levels()->size(); ++idx) {
                        const auto* pRef = pWorld->sub_levels()->Get(idx);
                        if (!pRef || !pRef->path() || pRef->path()->size() == 0) {
                                throw std::runtime_error(
                                                fmt::format("WorldAsset '{}': sub-level #{} has no path", sName, idx));
                        }

                        SubLevelEntry e;
                        e.path  = pRef->path()->str();
                        e.name  = StrID(e.path + "#" + std::to_string(idx));

                        if (const auto* pB = pRef->bounds_min()) { e.bounds_min = { pB->x(), pB->y(), pB->z() }; }
                        if (const auto* pB = pRef->bounds_max()) { e.bounds_max = { pB->x(), pB->y(), pB->z() }; }
                        if (const auto* pT = pRef->translation()) { e.translation = { pT->x(), pT->y(), pT->z() }; }
                        if (const auto* pR = pRef->rotation())    { e.rotation    = { pR->x(), pR->y(), pR->z() }; }

                        if (glm::any(glm::greaterThan(e.bounds_min, e.bounds_max))) {
                                throw std::runtime_error(
                                                fmt::format("WorldAsset '{}': sub-level #{} ('{}') has bounds_min > bounds_max",
                                                            sName, idx, e.path));
                        }

                        e.flags  = pRef->flags();
                        e.layer  = pRef->layer() ? StrID(pRef->layer()->str()) : StrID{};
                        e.weight = pRef->weight();

                        vSubLevels.push_back(std::move(e));
                }
        }

        if (pWorld->spawn()) {
                vSpawnPopulations.reserve(pWorld->spawn()->size());
                for (const auto* pDef : *pWorld->spawn()) {
                        if (pDef && pDef->population()) {
                                vSpawnPopulations.push_back(StrID(pDef->population()->str()));
                        }
                }
        }

        if (const auto* pF = pWorld->initial_focus()) { vInitialFocus = { pF->x(), pF->y(), pF->z() }; }
        seed = pWorld->seed();

        BuildCellMembership();
}

void WorldAsset::BuildCellMembership() {

        mCellMembership.clear();

        for (uint32_t idx = 0; idx < vSubLevels.size(); ++idx) {
                const SubLevelEntry& e = vSubLevels[idx];

                const int32_t min_x = static_cast<int32_t>(std::floor(e.bounds_min.x / streaming.cell_size));
                const int32_t max_x = static_cast<int32_t>(std::floor(e.bounds_max.x / streaming.cell_size));
                const int32_t min_z = static_cast<int32_t>(std::floor(e.bounds_min.z / streaming.cell_size));
                const int32_t max_z = static_cast<int32_t>(std::floor(e.bounds_max.z / streaming.cell_size));

                for (int32_t cz = min_z; cz <= max_z; ++cz) {
                        for (int32_t cx = min_x; cx <= max_x; ++cx) {
                                mCellMembership[PackCell(cx, cz)].push_back(idx);
                        }
                }
        }
}

void WorldAsset::CellForPos(const glm::vec3& vPos, int32_t& cx, int32_t& cy) const {
        cx = static_cast<int32_t>(std::floor(vPos.x / streaming.cell_size));
        cy = static_cast<int32_t>(std::floor(vPos.z / streaming.cell_size));
}

bool WorldAsset::LevelsInCell(int32_t cx, int32_t cy, std::vector<uint32_t>& vOut) const {

        vOut.clear();

        const auto it = mCellMembership.find(PackCell(cx, cy));
        if (it == mCellMembership.end()) { return false; }

        vOut = it->second;
        return true;
}

std::string WorldAsset::Str() const {
        return fmt::format("world '{}': {} sub-levels, streaming {}, seed {}",
                        sName, vSubLevels.size(),
                        streaming.enabled ? "on" : "off", seed);
}

} // namespace SE

#endif
