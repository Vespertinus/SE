
#ifndef SE_TEST_WORLD_DEMO_WORLD_BUILDER_H
#define SE_TEST_WORLD_DEMO_WORLD_BUILDER_H

// ---------------------------------------------------------------------------
// WorldDemoWorldBuilder — builds the headless demo-mirror world on disk:
// the same layout, streaming topology, populations, triggers, and nav group
// structure as the committed world_demo assets, but component-only scenes
// (no meshes/materials — those need a GL context, which Tier C tests and the
// bench must not require). The committed .sebb/.sebt AI assets are reused by
// path. Files land under resource/{world,nav}/gen/worlddemo/.
//
// Layout mirrors scripts/village/build_village.py:
//   village [-40,40]² (index 0) | inn interior (1, Manual+NoNav+NoSpawn)
//   forest_w [-120,-40]×[-80,80] (2, camp) | forest_e [40,120]×[-80,80] (3)
//   farm [-40,40]×[40,80] (4) | tower [-40,40]×[-80,-40] (5)
// ---------------------------------------------------------------------------

#include <Logging.h>

#include <flatbuffers/flatbuffers.h>

#include <World_generated.h>
#include <SceneTree_generated.h>
#include <Component_generated.h>
#include <Spawner_generated.h>
#include <NavMesh_generated.h>
#include <Blackboard_generated.h>
#include <nav/NavBake.h>
#ifdef SE_IMPL
#include <nav/NavBake.tcc>   // Bake() body (this header is only used under SE_IMPL)
#endif

#include <cstdio>
#include <boost/filesystem.hpp>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace se_test {

struct DemoWorldPaths {
        std::string world    = "resource/world/gen/worlddemo/world_demo_test.seworld";
        std::string senm     = "resource/nav/gen/worlddemo/world_demo_test.senm";
        std::string village  = "resource/world/gen/worlddemo/wd_village.sesc";
        std::string inn      = "resource/world/gen/worlddemo/wd_inn_interior.sesc";
        std::string forest_w = "resource/world/gen/worlddemo/wd_forest_w.sesc";
        std::string forest_e = "resource/world/gen/worlddemo/wd_forest_e.sesc";
        std::string farm     = "resource/world/gen/worlddemo/wd_farm.sesc";
        std::string tower    = "resource/world/gen/worlddemo/wd_tower.sesc";
};

inline bool WriteDemoBytes(const std::string & sPath, const std::vector<uint8_t> & vBytes) {
        std::ofstream f(sPath, std::ios::binary | std::ios::trunc);
        if (!f.is_open()) {
                log_e("worlddemo builder: cannot open '{}' (dirs created?)", sPath);
                return false;
        }
        f.write(reinterpret_cast<const char *>(vBytes.data()),
                static_cast<std::streamsize>(vBytes.size()));
        const bool bOk = static_cast<size_t>(f.tellp()) == vBytes.size();
        if (!bOk) { log_e("worlddemo builder: short write on '{}'", sPath); }
        return bOk;
}

inline SE::FlatBuffers::NodeT MakeDemoNode(const std::string & sName,
                                           const glm::vec3 & vPos = {}) {
        SE::FlatBuffers::NodeT oNode;
        oNode.name        = sName;
        oNode.translation = std::make_unique<SE::FlatBuffers::Vec3>(vPos.x, vPos.y, vPos.z);
        oNode.rotation    = std::make_unique<SE::FlatBuffers::Vec3>(0.f, 0.f, 0.f);
        oNode.scale       = std::make_unique<SE::FlatBuffers::Vec3>(1.f, 1.f, 1.f);
        return oNode;
}

inline void AddTriggerVolume(SE::FlatBuffers::NodeT & oNode,
                             const glm::vec3 & vHalfExtents,
                             const std::string & sEnterEvent) {
        auto pComp = std::make_unique<SE::FlatBuffers::ComponentT>();
        auto pTV   = std::make_unique<SE::FlatBuffers::TriggerVolumeT>();
        auto pBox  = std::make_unique<SE::FlatBuffers::BoxColliderT>();
        pBox->half_extents = std::make_unique<SE::FlatBuffers::Vec3>(vHalfExtents.x, vHalfExtents.y, vHalfExtents.z);
        pTV->collider.Set(std::move(*pBox));
        pTV->on_enter_event = sEnterEvent;
        pComp->component.Set(std::move(*pTV));
        oNode.components.push_back(std::move(pComp));
}

inline void AddTriggerBinding(SE::FlatBuffers::NodeT & oNode,
                              const std::string & sEvent,
                              const std::string & sKey,
                              const bool bValue) {
        auto pComp = std::make_unique<SE::FlatBuffers::ComponentT>();
        auto pTB   = std::make_unique<SE::FlatBuffers::TriggerBindingT>();
        auto pDef  = std::make_unique<SE::FlatBuffers::TriggerBindingDefT>();
        pDef->on_event = sEvent;
        pDef->key      = sKey;
        pDef->value.Set(SE::FlatBuffers::Bool{bValue});
        pTB->bindings.push_back(std::move(pDef));
        pComp->component.Set(std::move(*pTB));
        oNode.components.push_back(std::move(pComp));
}

inline void AddSpawnVolume(SE::FlatBuffers::NodeT & oNode,
                           const glm::vec3 & vSize,
                           const uint32_t seed_offset = 0) {
        auto pComp = std::make_unique<SE::FlatBuffers::ComponentT>();
        auto pSV   = std::make_unique<SE::FlatBuffers::SpawnVolumeT>();
        pSV->size        = std::make_unique<SE::FlatBuffers::Vec3>(vSize.x, vSize.y, vSize.z);
        pSV->distribution = SE::FlatBuffers::SpawnDistribution::BestCandidate;
        pSV->seed_offset = seed_offset;
        pComp->component.Set(std::move(*pSV));
        oNode.components.push_back(std::move(pComp));
}

inline SE::FlatBuffers::SpawnMemberT MakeMember(const std::string & sTemplate,
                                                const uint32_t count,
                                                const std::string & sGeometry) {
        SE::FlatBuffers::SpawnMemberT m;
        m.template_id = sTemplate;
        m.count       = count;
        m.geometry    = sGeometry;
        return m;
}

inline void AddPopulation(SE::FlatBuffers::NodeT & oNode,
                          const std::string & sId,
                          std::vector<SE::FlatBuffers::SpawnMemberT> vMembers,
                          std::vector<SE::FlatBuffers::WaveT> vWaves) {
        auto pComp = std::make_unique<SE::FlatBuffers::ComponentT>();
        auto pPop  = std::make_unique<SE::FlatBuffers::PopulationT>();
        pPop->id          = sId;
        pPop->max_active  = 0;
        pPop->snap        = SE::FlatBuffers::SpawnSnap::NavMesh;
        for (auto & m : vMembers) {
                auto pCopy = std::make_unique<SE::FlatBuffers::SpawnMemberT>();
                pCopy->template_id = m.template_id;
                pCopy->count       = m.count;
                pCopy->geometry    = m.geometry;
                pPop->members.push_back(std::move(pCopy));
        }
        for (auto & w : vWaves) {
                auto pWave = std::make_unique<SE::FlatBuffers::WaveT>();
                pWave->on_event = w.on_event;
                for (auto & m : w.members) {
                        auto pCopy = std::make_unique<SE::FlatBuffers::SpawnMemberT>();
                        pCopy->template_id = m->template_id;
                        pCopy->count       = m->count;
                        pCopy->geometry    = m->geometry;
                        pWave->members.push_back(std::move(pCopy));
                }
                pPop->waves.push_back(std::move(pWave));
        }
        pComp->component.Set(std::move(*pPop));
        oNode.components.push_back(std::move(pComp));
}

inline std::vector<uint8_t> BakeSceneRoot(const std::string & sName,
                                          std::vector<SE::FlatBuffers::NodeT> vChildren) {
        SE::FlatBuffers::NodeT oRoot = MakeDemoNode(sName);
        for (auto & c : vChildren) oRoot.children.push_back(std::make_unique<SE::FlatBuffers::NodeT>(std::move(c)));

        SE::FlatBuffers::SceneTreeT oTree;
        oTree.root = std::make_unique<SE::FlatBuffers::NodeT>(std::move(oRoot));

        flatbuffers::FlatBufferBuilder oBuilder;
        auto offset = SE::FlatBuffers::SceneTree::Pack(oBuilder, &oTree);
        SE::FlatBuffers::FinishSceneTreeBuffer(oBuilder, offset);
        const uint8_t * p = oBuilder.GetBufferPointer();
        return std::vector<uint8_t>(p, p + oBuilder.GetSize());
}

// Builds every file; returns false (+ logs) on the first I/O failure.
inline bool BuildDemoMirrorWorld(const DemoWorldPaths & paths = {}) {

        auto stage = [](const char * sz) {
                log_i("worlddemo builder: {}", sz);
                std::fflush(stdout);
        };
        boost::filesystem::create_directories("resource/world/gen/worlddemo");
        boost::filesystem::create_directories("resource/nav/gen/worlddemo");
        stage("dirs");

        // --- sub-level scenes ---------------------------------------------------
        // village: bell + inn door triggers, villager population on 2 volumes
        {
                stage("village scene");
        SE::FlatBuffers::NodeT oBell = MakeDemoNode("BellPost", glm::vec3(0.f, 2.f, 10.f));
                AddTriggerVolume(oBell, glm::vec3(1.2f, 1.5f, 1.2f), "bell_ring");

                {
                        // ONE TriggerBinding component carrying both writes
                        // (the loader rejects duplicate component types per node)
                        auto pComp = std::make_unique<SE::FlatBuffers::ComponentT>();
                        auto pTB   = std::make_unique<SE::FlatBuffers::TriggerBindingT>();
                        auto pPos  = std::make_unique<SE::FlatBuffers::TriggerBindingDefT>();
                        pPos->on_event = "bell_ring";
                        pPos->key      = "noise_pos";
                        pPos->write_position = true;
                        pTB->bindings.push_back(std::move(pPos));
                        auto pAlert = std::make_unique<SE::FlatBuffers::TriggerBindingDefT>();
                        pAlert->on_event = "bell_ring";
                        pAlert->key      = "alert";
                        pAlert->value.Set(SE::FlatBuffers::Bool{true});
                        pTB->bindings.push_back(std::move(pAlert));
                        pComp->component.Set(std::move(*pTB));
                        oBell.components.push_back(std::move(pComp));
                }

                SE::FlatBuffers::NodeT oInnDoor = MakeDemoNode("InnDoor", glm::vec3(10.f, 2.f, -4.6f));
                AddTriggerVolume(oInnDoor, glm::vec3(1.2f, 1.4f, 0.9f), "inn_door");


                SE::FlatBuffers::NodeT oVP1 = MakeDemoNode("VP1");
                AddSpawnVolume(oVP1, glm::vec3(1.6f, 1.f, 1.6f), 11);
                SE::FlatBuffers::NodeT oVP2 = MakeDemoNode("VP2");
                AddSpawnVolume(oVP2, glm::vec3(1.6f, 1.f, 1.6f), 12);

                SE::FlatBuffers::NodeT oPop = MakeDemoNode("Villagers");
                AddPopulation(oPop, "villagers",
                              {MakeMember("villager_a", 2, "VP1"), MakeMember("villager_b", 2, "VP2")},
                              {});

                        // spawner geometry nodes live UNDER the population node
                        oPop.children.push_back(std::make_unique<SE::FlatBuffers::NodeT>(std::move(oVP1)));
                        oPop.children.push_back(std::make_unique<SE::FlatBuffers::NodeT>(std::move(oVP2)));

                if (!WriteDemoBytes(paths.village, BakeSceneRoot("Village",
                                {std::move(oBell), std::move(oInnDoor), std::move(oPop)}))) { return false; }
        }

        // inn interior: exit trigger only
        {
                stage("inn scene");
        SE::FlatBuffers::NodeT oExit = MakeDemoNode("InnExit");
                AddTriggerVolume(oExit, glm::vec3(1.2f, 1.5f, 1.2f), "inn_exit");

                if (!WriteDemoBytes(paths.inn, BakeSceneRoot("InnInterior", {std::move(oExit)}))) { return false; }
        }

        // forest west: camp volume + guard wave + alarm trigger/binding
        {
                stage("forest scene");
        SE::FlatBuffers::NodeT oCampVol = MakeDemoNode("CampVolume", glm::vec3(-76.f, 2.f, 4.f));
                AddSpawnVolume(oCampVol, glm::vec3(8.f, 2.f, 8.f), 31);

                SE::FlatBuffers::NodeT oPop = MakeDemoNode("CampGuards");
                AddPopulation(oPop, "camp_guards", {},
                              {[] {
                                      SE::FlatBuffers::WaveT w;
                                      w.on_event = "camp_alarm";
                                      auto pMember = std::make_unique<SE::FlatBuffers::SpawnMemberT>(
                                                      MakeMember("guard", 8, "CampVolume"));
                                      w.members.push_back(std::move(pMember));
                                      return w;
                              }()});

                SE::FlatBuffers::NodeT oTrigger = MakeDemoNode("CampTrigger", glm::vec3(-62.f, 2.f, 4.f));
                AddTriggerVolume(oTrigger, glm::vec3(13.f, 2.f, 13.f), "camp_alarm");

                AddTriggerBinding(oTrigger, "camp_alarm", "alarm_raised", true);

                        oPop.children.push_back(std::make_unique<SE::FlatBuffers::NodeT>(std::move(oCampVol)));

                if (!WriteDemoBytes(paths.forest_w, BakeSceneRoot("ForestW",
                                {std::move(oPop), std::move(oTrigger)}))) { return false; }
        }

        // forest east / farm / tower: markers + farm population
        {
                stage("forest_e scene");
        SE::FlatBuffers::NodeT oMarker = MakeDemoNode("ForestEMarker");
                if (!WriteDemoBytes(paths.forest_e, BakeSceneRoot("ForestE", {std::move(oMarker)}))) { return false; }
        }
        {
                stage("farm scene");
        SE::FlatBuffers::NodeT oFP1 = MakeDemoNode("FP1");
                AddSpawnVolume(oFP1, glm::vec3(1.6f, 1.f, 1.6f), 61);
                SE::FlatBuffers::NodeT oFP2 = MakeDemoNode("FP2");
                AddSpawnVolume(oFP2, glm::vec3(1.6f, 1.f, 1.6f), 62);
                SE::FlatBuffers::NodeT oPop = MakeDemoNode("Farmers");
                AddPopulation(oPop, "farmers",
                              {MakeMember("farmer", 1, "FP1"), MakeMember("farmer", 1, "FP2")}, {});
                        oPop.children.push_back(std::make_unique<SE::FlatBuffers::NodeT>(std::move(oFP1)));
                        oPop.children.push_back(std::make_unique<SE::FlatBuffers::NodeT>(std::move(oFP2)));

                if (!WriteDemoBytes(paths.farm, BakeSceneRoot("Farm", {std::move(oPop)}))) { return false; }
        }
        {
                stage("tower scene");
        SE::FlatBuffers::NodeT oMarker = MakeDemoNode("TowerMarker");
                if (!WriteDemoBytes(paths.tower, BakeSceneRoot("Tower", {std::move(oMarker)}))) { return false; }
        }

        // --- nav: one ground group per nav-enabled sub-level ----------------------
        {
                using namespace SE::NavBake;
                auto add_quad = [](SGeomMesh & mesh, float x0, float z0, float x1, float z1, float y) {
                        const std::vector<float> v = {x0, y, z0, x1, y, z0, x1, y, z1, x0, y, z1};
                        const std::vector<uint32_t> idx = {0, 2, 1, 0, 3, 2};
                        mesh.vVertices.insert(mesh.vVertices.end(), v.begin(), v.end());
                        mesh.vIndices.insert(mesh.vIndices.end(), idx.begin(), idx.end());
                };
                auto ground = [&](const std::string & sId, float x0, float z0, float x1, float z1) {
                        SGroup g;
                        g.id = sId;
                        g.bounds_min = glm::vec3(x0, -1.f, z0);
                        g.bounds_max = glm::vec3(x1, 4.f, z1);
                        g.vMeshes.emplace_back();
                        add_quad(g.vMeshes.back(), x0, z0, x1, z1, 0.0f);
                        return g;
                };
                stage("nav bake");
        SBakeInput in;
                in.vGroups.push_back(ground(paths.village, -40.f, -40.f, 40.f, 40.f));
                in.vGroups.push_back(ground(paths.forest_w, -120.f, -80.f, -40.f, 80.f));
                in.vGroups.push_back(ground(paths.forest_e, 40.f, -80.f, 120.f, 80.f));
                in.vGroups.push_back(ground(paths.farm, -40.f, 40.f, 40.f, 80.f));
                in.vGroups.push_back(ground(paths.tower, -40.f, -80.f, 40.f, -40.f));

                SConfig cfg;
                cfg.tile_dim = 20.f;
                cfg.agent = {"default", 0.4f, 1.8f, 50.f, 0.4f};
                cfg.vAreas = {""};   // the ground entry must lead the registry

                SOutput out;
                if (!Bake(in, cfg, out)) {
                        log_e("worlddemo mirror nav bake failed");
                        return false;
                }

                SE::FlatBuffers::NavMeshAssetT a;
                a.agent_class = "default";
                a.version     = 1;
                a.origin      = std::make_unique<SE::FlatBuffers::Vec3>(out.origin.x, out.origin.y, out.origin.z);
                a.tile_dim    = out.tile_dim;
                a.voxel_size   = 0.2f;
                a.voxel_height = 0.2f;
                a.areas        = cfg.vAreas;
                for (const SNavTileBytes & t : out.vTiles) {
                        auto pTile  = std::make_unique<SE::FlatBuffers::NavTileT>();
                        pTile->x = t.x; pTile->y = t.y; pTile->layer = t.layer; pTile->group = t.group;
                        pTile->data = t.vBytes;
                        a.tiles.push_back(std::move(pTile));
                }
                flatbuffers::FlatBufferBuilder oBuilder;
                auto offset = SE::FlatBuffers::NavMeshAsset::Pack(oBuilder, &a);
                SE::FlatBuffers::FinishNavMeshAssetBuffer(oBuilder, offset);
                const uint8_t * p = oBuilder.GetBufferPointer();
                if (!WriteDemoBytes(paths.senm,
                                    std::vector<uint8_t>(p, p + oBuilder.GetSize()))) { return false; }
        }

        // --- the world -----------------------------------------------------------
        {
                stage("world json");
        auto pWorld = std::make_unique<SE::FlatBuffers::WorldT>();
                pWorld->id = "world_demo_test";
                {
                        auto pS = std::make_unique<SE::FlatBuffers::StreamingConfigT>();
                        pS->enabled = true;
                        pS->cell_size = 40.f;
                        pS->load_radius = 55.f;
                        pS->unload_radius = 85.f;
                        pS->max_activations_per_frame = 4;
                        pS->main_budget_ms = 8.f;
                        pS->unload_frame_delay = 3;
                        pWorld->streaming = std::move(pS);
                }
                auto add = [&](const std::string & sPath, const glm::vec3 & vMin,
                               const glm::vec3 & vMax, const uint32_t flags) {
                        auto pRef = std::make_unique<SE::FlatBuffers::SubLevelRefT>();
                        pRef->path = sPath;
                        pRef->bounds_min = std::make_unique<SE::FlatBuffers::Vec3>(vMin.x, vMin.y, vMin.z);
                        pRef->bounds_max = std::make_unique<SE::FlatBuffers::Vec3>(vMax.x, vMax.y, vMax.z);
                        pRef->flags = flags;
                        pWorld->sub_levels.push_back(std::move(pRef));
                };
                const uint32_t fInn = 2u | 4u | 8u;   // Manual | NoNav | NoSpawn
                add(paths.village,  {-42.f, 0.f, -42.f}, {42.f, 8.f, 42.f},    0);
                add(paths.inn,      {4.f,   0.f, -16.f}, {16.f, 5.f, -4.f},    fInn);
                add(paths.forest_w, {-122.f, 0.f, -82.f}, {-38.f, 8.f, 82.f},  0);
                add(paths.forest_e, {38.f,  0.f, -82.f}, {122.f, 8.f, 82.f},   0);
                add(paths.farm,     {-42.f, 0.f, 38.f},  {42.f, 8.f, 82.f},    0);
                add(paths.tower,    {-42.f, 0.f, -82.f}, {42.f, 8.f, -38.f},   0);

                pWorld->initial_focus = std::make_unique<SE::FlatBuffers::Vec3>(0.f, 2.f, 0.f);
                pWorld->seed = 20260917ull;

                {
                        auto pNav = std::make_unique<SE::FlatBuffers::NavConfigT>();
                        auto pEntry = std::make_unique<SE::FlatBuffers::NavMeshEntryT>();
                        pEntry->agent_class = "default";
                        pEntry->mesh = std::make_unique<SE::FlatBuffers::NavMeshHolderT>();
                        pEntry->mesh->path = paths.senm;
                        pNav->meshes.push_back(std::move(pEntry));
                        pNav->default_class = "default";
                        pWorld->nav = std::move(pNav);
                }
                {
                        auto pBB = std::make_unique<SE::FlatBuffers::BlackboardHolderT>();
                        pBB->path = "resource/ai/world_demo/bb_world.sebb";
                        pWorld->global_blackboard = std::move(pBB);
                }

                flatbuffers::FlatBufferBuilder oBuilder;
                auto offset = SE::FlatBuffers::World::Pack(oBuilder, pWorld.get());
                SE::FlatBuffers::FinishWorldBuffer(oBuilder, offset);
                const uint8_t * p = oBuilder.GetBufferPointer();
                if (!WriteDemoBytes(paths.world,
                                    std::vector<uint8_t>(p, p + oBuilder.GetSize()))) { return false; }
        }

        return true;
}

} // namespace se_test

#endif // SE_TEST_WORLD_DEMO_WORLD_BUILDER_H
