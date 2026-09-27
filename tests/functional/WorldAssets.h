
#ifndef SE_TEST_WORLD_ASSETS_H
#define SE_TEST_WORLD_ASSETS_H

// ---------------------------------------------------------------------------
// Inline FlatBuffers object-api builders for the world suites
// (AnimationAssets.h convention: one <Subsystem>Assets.h per subsystem).
// Builders never touch the filesystem; scene/world writers return serialized
// bytes so tests write them to scratch paths themselves (asset I/O stays
// inside test bodies — gtest_discover_tests runs binaries from the repo root).
// ---------------------------------------------------------------------------

#include <flatbuffers/flatbuffers.h>
#include <World_generated.h>
#include <SceneTree_generated.h>
#include <Component_generated.h>
#include <Resources_generated.h>

#include <fstream>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace se_test {

// ---- world builders ---------------------------------------------------------

inline std::unique_ptr<SE::FlatBuffers::WorldT> MakeWorld(const std::string & id) {
        auto pWorld = std::make_unique<SE::FlatBuffers::WorldT>();
        pWorld->id = id;
        return pWorld;
}

// Streaming knobs for the Step 2 suites (defaults match misc/World.fbs).
inline void SetStreaming(SE::FlatBuffers::WorldT & oWorld,
                         const bool enabled,
                         const float load_radius,
                         const float unload_radius,
                         const uint32_t unload_frame_delay = 60,
                         const float main_budget_ms        = 2.f,
                         const uint32_t max_activations    = 1,
                         const float cell_size             = 32.f) {
        auto pS = std::make_unique<SE::FlatBuffers::StreamingConfigT>();
        pS->enabled                   = enabled;
        pS->cell_size                 = cell_size;
        pS->load_radius               = load_radius;
        pS->unload_radius             = unload_radius;
        pS->unload_frame_delay        = unload_frame_delay;
        pS->main_budget_ms            = main_budget_ms;
        pS->max_activations_per_frame = max_activations;
        oWorld.streaming = std::move(pS);
}

inline SE::FlatBuffers::SubLevelRefT * AddSubLevel(
                SE::FlatBuffers::WorldT & oWorld,
                const std::string & sPath,
                const SE::FlatBuffers::Vec3 & vMin,
                const SE::FlatBuffers::Vec3 & vMax,
                const uint32_t flags = 0) {

        auto pRef = std::make_unique<SE::FlatBuffers::SubLevelRefT>();
        pRef->path       = sPath;
        pRef->bounds_min = std::make_unique<SE::FlatBuffers::Vec3>(vMin);
        pRef->bounds_max = std::make_unique<SE::FlatBuffers::Vec3>(vMax);
        pRef->flags      = flags;
        oWorld.sub_levels.push_back(std::move(pRef));
        return oWorld.sub_levels.back().get();
}

inline std::vector<uint8_t> BakeWorld(const SE::FlatBuffers::WorldT & oWorld) {
        flatbuffers::FlatBufferBuilder oBuilder;
        auto offset = SE::FlatBuffers::World::Pack(oBuilder, &oWorld);
        SE::FlatBuffers::FinishWorldBuffer(oBuilder, offset);
        const uint8_t * p = oBuilder.GetBufferPointer();
        return std::vector<uint8_t>(p, p + oBuilder.GetSize());
}

// ---- scene builders --------------------------------------------------------

inline SE::FlatBuffers::NodeT MakeNode(
                const std::string & sName,
                const glm::vec3 vPos    = {},
                const glm::vec3 vRotDeg = {},
                const glm::vec3 vScale  = {1.f, 1.f, 1.f}) {

        SE::FlatBuffers::NodeT oNode;
        oNode.name        = sName;
        oNode.translation = std::make_unique<SE::FlatBuffers::Vec3>(vPos.x, vPos.y, vPos.z);
        oNode.rotation    = std::make_unique<SE::FlatBuffers::Vec3>(vRotDeg.x, vRotDeg.y, vRotDeg.z);
        oNode.scale       = std::make_unique<SE::FlatBuffers::Vec3>(vScale.x, vScale.y, vScale.z);
        return oNode;
}

inline SE::FlatBuffers::StateMachineHolderT MakeStateMachineRef(const std::string & sPath) {
        SE::FlatBuffers::StateMachineHolderT oHolder;
        oHolder.path = sPath;
        return oHolder;
}

inline void AddStateMachineComponent(SE::FlatBuffers::NodeT & oNode, const std::string & sPath) {
        auto pComp = std::make_unique<SE::FlatBuffers::ComponentT>();
        auto pSM   = std::make_unique<SE::FlatBuffers::StateMachineT>();
        pSM->hms   = std::make_unique<SE::FlatBuffers::StateMachineHolderT>(MakeStateMachineRef(sPath));
        // union Set takes the payload by value/move (not a unique_ptr to it)
        pComp->component.Set(std::move(*pSM));
        oNode.components.push_back(std::move(pComp));
}

// A node carrying one StateMachine (path flavor) — composing N of these with
// distinct paths yields a declared dependency table with N separately
// budgetable CreateResource+Retain units (the Step 2 budget fixtures).
inline SE::FlatBuffers::NodeT MakeNodeWithSM(
                const std::string & sName, const std::string & sSmPath,
                const glm::vec3 vPos = {}) {
        auto oNode = MakeNode(sName, vPos);
        AddStateMachineComponent(oNode, sSmPath);
        return oNode;
}

inline void AddBoxRigidBodyComponent(SE::FlatBuffers::NodeT & oNode, const glm::vec3 vHalfExtents) {
        auto pComp  = std::make_unique<SE::FlatBuffers::ComponentT>();
        auto pRB    = std::make_unique<SE::FlatBuffers::RigidBodyT>();
        auto pBox   = std::make_unique<SE::FlatBuffers::BoxColliderT>();
        pBox->half_extents = std::make_unique<SE::FlatBuffers::Vec3>(vHalfExtents.x, vHalfExtents.y, vHalfExtents.z);
        pRB->collider.Set(std::move(*pBox));
        pComp->component.Set(std::move(*pRB));
        oNode.components.push_back(std::move(pComp));
}

// Stage 3 Step 4: gameplay spatial-index marker (tag/radius/track_node).
inline void AddSpatialEntryComponent(SE::FlatBuffers::NodeT & oNode,
                                     const std::string & sTag,
                                     const float radius = 0.f,
                                     const bool track_node = false) {
        auto pComp   = std::make_unique<SE::FlatBuffers::ComponentT>();
        auto pEntry  = std::make_unique<SE::FlatBuffers::SpatialEntryT>();
        pEntry->tag        = sTag;
        pEntry->radius     = radius;
        pEntry->track_node = track_node;
        pComp->component.Set(std::move(*pEntry));
        oNode.components.push_back(std::move(pComp));
}

// A node carrying one SpatialEntry — the authored point-of-interest fixture.
inline SE::FlatBuffers::NodeT MakeNodeWithSpatialEntry(
                const std::string & sName, const std::string & sTag,
                const glm::vec3 vPos = {}, const float radius = 0.f,
                const bool track_node = false) {
        auto oNode = MakeNode(sName, vPos);
        AddSpatialEntryComponent(oNode, sTag, radius, track_node);
        return oNode;
}

// Assemble a scene root: node becomes the root; resources are deduplicated from
// the state-machine paths the helper collected (mirrors what compose_scene emits).
inline std::vector<uint8_t> BakeSceneWithStateMachineResources(SE::FlatBuffers::NodeT oRoot) {

        SE::FlatBuffers::SceneTreeT oTree;
        oTree.root = std::make_unique<SE::FlatBuffers::NodeT>(std::move(oRoot));

        // collect distinct StateMachineHolder paths, mirroring the writer-side dedup
        std::vector<std::string> vPaths;

        std::vector<SE::FlatBuffers::NodeT *> vStack{ oTree.root.get() };
        while (!vStack.empty()) {
                SE::FlatBuffers::NodeT * pNode = vStack.back();
                vStack.pop_back();
                for (const auto & pChild : pNode->children) {
                        if (pChild) { vStack.push_back(pChild.get()); }
                }
                for (const auto & pComp : pNode->components) {
                        if (!pComp) { continue; }
                        if (auto * pSM = pComp->component.AsStateMachine()) {
                                if (pSM->hms && !pSM->hms->path.empty()) {
                                        const auto & sPath = pSM->hms->path;
                                        if (std::find(vPaths.begin(), vPaths.end(), sPath) == vPaths.end()) {
                                                vPaths.push_back(sPath);
                                        }
                                }
                        }
                }
        }

        for (const auto & sPath : vPaths) {
                auto pRef = std::make_unique<SE::FlatBuffers::ResourceRefT>();
                auto pHolder = std::make_unique<SE::FlatBuffers::StateMachineHolderT>();
                pHolder->path = sPath;
                pRef->resource.Set(std::move(*pHolder));
                oTree.resources.push_back(std::move(pRef));
        }

        flatbuffers::FlatBufferBuilder oBuilder;
        auto offset = SE::FlatBuffers::SceneTree::Pack(oBuilder, &oTree);
        SE::FlatBuffers::FinishSceneTreeBuffer(oBuilder, offset);
        const uint8_t * p = oBuilder.GetBufferPointer();
        return std::vector<uint8_t>(p, p + oBuilder.GetSize());
}

inline std::vector<uint8_t> BakeScene(SE::FlatBuffers::NodeT oRoot) {
        SE::FlatBuffers::SceneTreeT oTree;
        oTree.root = std::make_unique<SE::FlatBuffers::NodeT>(std::move(oRoot));

        flatbuffers::FlatBufferBuilder oBuilder;
        auto offset = SE::FlatBuffers::SceneTree::Pack(oBuilder, &oTree);
        SE::FlatBuffers::FinishSceneTreeBuffer(oBuilder, offset);
        const uint8_t * p = oBuilder.GetBufferPointer();
        return std::vector<uint8_t>(p, p + oBuilder.GetSize());
}

inline bool WriteBytes(const std::string & sPath, const std::vector<uint8_t> & vBytes) {
        std::ofstream f(sPath, std::ios::binary | std::ios::trunc);
        if (!f.is_open()) { return false; }
        f.write(reinterpret_cast<const char *>(vBytes.data()),
                static_cast<std::streamsize>(vBytes.size()));
        return f.good();
}

// ---- spawner builders (Stage 3 Step 5) --------------------------------------

#include <Spawner_generated.h>

inline std::unique_ptr<SE::FlatBuffers::SpawnMemberT> MakeSpawnMember(
                const std::string & sTemplateId,
                const uint32_t count = 1,
                const std::string & sGeometry = {},
                const float weight = 1.f,
                const float initial_delay_s = 0.f) {
        auto pMember  = std::make_unique<SE::FlatBuffers::SpawnMemberT>();
        pMember->template_id = sTemplateId;
        pMember->count       = count;
        pMember->geometry    = sGeometry;
        pMember->weight      = weight;
        pMember->initial_delay_s = initial_delay_s;
        return pMember;
}

inline void AddSpawnPointComponent(SE::FlatBuffers::NodeT & oNode,
                                   const std::string & sTemplateId,
                                   const float initial_delay_s = 0.f,
                                   const uint32_t max_spawns   = 1,
                                   const float respawn_delay_s = -1.f,
                                   const SE::FlatBuffers::SpawnSnap snap = SE::FlatBuffers::SpawnSnap::None) {
        auto pComp = std::make_unique<SE::FlatBuffers::ComponentT>();
        auto pSP   = std::make_unique<SE::FlatBuffers::SpawnPointT>();
        pSP->template_id     = sTemplateId;
        pSP->initial_delay_s = initial_delay_s;
        pSP->max_spawns      = max_spawns;
        pSP->respawn_delay_s = respawn_delay_s;
        pSP->snap            = snap;
        pComp->component.Set(std::move(*pSP));
        oNode.components.push_back(std::move(pComp));
}

inline void AddSpawnVolumeComponent(SE::FlatBuffers::NodeT & oNode,
                                    const glm::vec3 vSize,
                                    const SE::FlatBuffers::SpawnDistribution dist = SE::FlatBuffers::SpawnDistribution::Uniform,
                                    const uint32_t candidates = 8) {
        auto pComp = std::make_unique<SE::FlatBuffers::ComponentT>();
        auto pVol  = std::make_unique<SE::FlatBuffers::SpawnVolumeT>();
        pVol->size            = std::make_unique<SE::FlatBuffers::Vec3>(vSize.x, vSize.y, vSize.z);
        pVol->distribution    = dist;
        pVol->candidate_count = candidates;
        pComp->component.Set(std::move(*pVol));
        oNode.components.push_back(std::move(pComp));
}

// Population with base members and optional waves, in one step.
inline void AddPopulationComponent(
                SE::FlatBuffers::NodeT & oNode,
                const std::string & sId,
                std::vector<std::unique_ptr<SE::FlatBuffers::SpawnMemberT>> vMembers,
                const float tick_interval_s = 0.05f,
                const float respawn_delay_s = -1.f,
                const SE::FlatBuffers::SpawnLifetime lifetime = SE::FlatBuffers::SpawnLifetime::SubLevel,
                const SE::FlatBuffers::SpawnSnap snap = SE::FlatBuffers::SpawnSnap::None) {
        auto pComp = std::make_unique<SE::FlatBuffers::ComponentT>();
        auto pPop  = std::make_unique<SE::FlatBuffers::PopulationT>();
        pPop->id              = sId;
        pPop->members         = std::move(vMembers);
        pPop->tick_interval_s = tick_interval_s;
        pPop->respawn_delay_s = respawn_delay_s;
        pPop->lifetime        = lifetime;
        pPop->snap            = snap;
        pComp->component.Set(std::move(*pPop));
        oNode.components.push_back(std::move(pComp));
}

// Wave on an already-built PopulationT.
inline void AddWaveToPopulation(SE::FlatBuffers::PopulationT & oPop,
                                std::vector<std::unique_ptr<SE::FlatBuffers::SpawnMemberT>> vMembers,
                                const std::string & sOnEvent = {},
                                const float at_time_s = -1.f) {
        auto pWave = std::make_unique<SE::FlatBuffers::WaveT>();
        pWave->on_event  = sOnEvent;
        pWave->at_time_s = at_time_s;
        pWave->members   = std::move(vMembers);
        oPop.waves.push_back(std::move(pWave));
}

// FocusNear arming condition on an already-built PopulationT.
inline void SetPopulationFocusNear(SE::FlatBuffers::PopulationT & oPop, const float radius) {
        auto pCond = std::make_unique<SE::FlatBuffers::SpawnConditionT>();
        auto pFocus = std::make_unique<SE::FlatBuffers::FocusNearT>();
        pFocus->radius = radius;
        pCond->type.Set(std::move(*pFocus));
        auto pFocusValue = std::make_unique<SE::FlatBuffers::FocusNearT>();
        pFocusValue->radius = radius;
        pCond->value.Set(std::move(*pFocusValue));
        oPop.conditions.push_back(std::move(pCond));
}

// WorldFlag arming condition on an already-built PopulationT (Step 9: global
// blackboard bool key; requires BlackboardSystem composed).
inline void SetPopulationWorldFlag(SE::FlatBuffers::PopulationT & oPop, const std::string & sKey) {
        auto pCond = std::make_unique<SE::FlatBuffers::SpawnConditionT>();
        auto pFlag = std::make_unique<SE::FlatBuffers::WorldFlagT>();
        pFlag->key = sKey;
        pCond->type.Set(std::move(*pFlag));
        auto pFlagValue = std::make_unique<SE::FlatBuffers::WorldFlagT>();
        pFlagValue->key = sKey;
        pCond->value.Set(std::move(*pFlagValue));
        oPop.conditions.push_back(std::move(pCond));
}

// AtTime arming condition on an already-built PopulationT.
inline void SetPopulationAtTime(SE::FlatBuffers::PopulationT & oPop, const float at_s) {
        auto pCond = std::make_unique<SE::FlatBuffers::SpawnConditionT>();
        auto pAt = std::make_unique<SE::FlatBuffers::AtTimeT>();
        pAt->at_s = at_s;
        pCond->type.Set(std::move(*pAt));
        auto pAtValue = std::make_unique<SE::FlatBuffers::AtTimeT>();
        pAtValue->at_s = at_s;
        pCond->value.Set(std::move(*pAtValue));
        oPop.conditions.push_back(std::move(pCond));
}

// Fetch the Population component payload of a node builder (to tweak it in place).
inline SE::FlatBuffers::PopulationT * GetPopulationPayload(SE::FlatBuffers::NodeT & oNode) {
        for (auto & pComp : oNode.components) {
                if (auto * pPop = pComp->component.AsPopulation()) {
                        return pPop;
                }
        }
        return nullptr;
}

} // namespace se_test

#endif // SE_TEST_WORLD_ASSETS_H
