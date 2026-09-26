
#ifdef SE_IMPL

#include <spawner/Population.h>
#include <spawner/SpawnerSystem.h>
#include <spawner/SpawnVolume.h>

#include <Logging.h>
#include <stdexcept>

namespace SE {

namespace {

PopulationRuntime::SMemberDesc ToRuntimeMember(const PopulationMemberDesc & d, bool wave_only) {
        PopulationRuntime::SMemberDesc rm;
        rm.template_id     = d.template_id;
        rm.geometry        = d.geometry;
        rm.weight          = d.weight;
        rm.count           = d.count;
        rm.initial_delay_s = d.initial_delay_s;
        rm.respawn_limit   = d.respawn_limit;
        rm.wave_only       = wave_only;
        return rm;
}

PopulationMemberDesc ParseMember(const FlatBuffers::SpawnMember * pFB) {
        PopulationMemberDesc d;
        d.template_id     = pFB->template_id() ? pFB->template_id()->str() : std::string{};
        d.geometry        = pFB->geometry()   ? pFB->geometry()->str()   : std::string{};
        d.weight          = pFB->weight();
        d.count           = pFB->count();
        d.initial_delay_s = pFB->initial_delay_s();
        d.respawn_limit   = pFB->respawn_limit();
        return d;
}

SpawnConditions::SConditionDesc ParseCondition(const FlatBuffers::SpawnCondition * pCond) {
        switch (pCond->type_type()) {
        case FlatBuffers::SpawnConditionU::FocusNear:
                return SpawnConditions::Parse(SpawnConditions::EKind::FocusNear,
                                pCond->value_as_FocusNear()->radius(), 0.f, {});
        case FlatBuffers::SpawnConditionU::TagNear:
                return SpawnConditions::Parse(SpawnConditions::EKind::TagNear,
                                pCond->value_as_TagNear()->radius(), 0.f,
                                pCond->value_as_TagNear()->tag()->str());
        case FlatBuffers::SpawnConditionU::AtTime:
                return SpawnConditions::Parse(SpawnConditions::EKind::AtTime,
                                0.f, pCond->value_as_AtTime()->at_s(), {});
        case FlatBuffers::SpawnConditionU::WorldFlag:
                return SpawnConditions::Parse(SpawnConditions::EKind::WorldFlag,
                                0.f, 0.f, {},
                                pCond->value_as_WorldFlag()->key()->str());
        default:
                throw std::runtime_error("Population: unknown condition type");
        }
}

} // namespace

// ---------------------------------------------------------------------------
// construction
// ---------------------------------------------------------------------------

Population::Population(TSceneTree::TSceneNodeExact * pNewNode, const PopulationDesc & oDesc)
        : pNode(pNewNode)
        , id(StrID(oDesc.id))
        , id_string(oDesc.id) {

        if (oDesc.id.empty()) {
                throw std::runtime_error("Population: id is required");
        }

        tick_interval  = oDesc.tick_interval_s > 0.f ? oDesc.tick_interval_s : 0.25f;
        despawn_radius = oDesc.despawn_radius;
        eLifetime      = oDesc.lifetime;
        eSnap          = oDesc.snap;
        tick_phase     = static_cast<float>(DeterministicRng::Hash64(oDesc.id) % 1024u) / 1024.f * tick_interval;

        PopulationRuntime::SDesc rd;
        rd.max_active      = oDesc.max_active;
        rd.respawn_delay_s = oDesc.respawn_delay_s;
        for (const auto & m : oDesc.members) {
                if (m.template_id.empty()) {
                        throw std::runtime_error("Population: member template_id is required");
                }
                rd.members.push_back(ToRuntimeMember(m, /*wave_only=*/false));
        }
        for (const auto & w : oDesc.waves) {
                PopulationRuntime::SWaveDesc rw;
                rw.at_time_s = w.at_time_s;
                rw.on_event  = w.on_event;
                rw.members.reserve(w.members.size());
                for (const auto & m : w.members) {
                        if (m.template_id.empty()) {
                                throw std::runtime_error("Population: wave member template_id is required");
                        }
                        rw.members.push_back(ToRuntimeMember(m, /*wave_only=*/true));
                }
                rd.waves.push_back(std::move(rw));
        }
        for (const auto & c : oDesc.conditions) {
                vConditions.push_back(SpawnConditions::Parse(c.kind, c.radius, c.at_s, c.tag, c.key));
        }

        oRuntime.Arm(rd);
        // children of this node don't exist yet during load — SpawnerSystem
        // re-resolves on the first spawn
        SyncGeometryNodes(false);
}

Population::Population(TSceneTree::TSceneNodeExact * pNewNode, const FlatBuffers::Population * pFB)
        : pNode(pNewNode) {

        if (!pFB->id() || pFB->id()->size() == 0) {
                throw std::runtime_error("Population: id is required");
        }
        id_string = pFB->id()->str();
        id        = StrID(id_string);

        tick_interval  = pFB->tick_interval_s() > 0.f ? pFB->tick_interval_s() : 0.25f;
        despawn_radius = pFB->despawn_radius();
        eLifetime      = pFB->lifetime();
        eSnap          = pFB->snap();
        tick_phase     = static_cast<float>(DeterministicRng::Hash64(id_string) % 1024u) / 1024.f * tick_interval;

        PopulationRuntime::SDesc rd;
        rd.max_active      = pFB->max_active();
        rd.respawn_delay_s = pFB->respawn_delay_s();

        if (const auto * pMembers = pFB->members()) {
                for (flatbuffers::uoffset_t i = 0; i < pMembers->size(); ++i) {
                        PopulationMemberDesc d = ParseMember(pMembers->Get(i));
                        if (d.template_id.empty()) {
                                throw std::runtime_error("Population: member template_id is required");
                        }
                        rd.members.push_back(ToRuntimeMember(d, /*wave_only=*/false));
                }
        }
        if (const auto * pWaves = pFB->waves()) {
                for (flatbuffers::uoffset_t i = 0; i < pWaves->size(); ++i) {
                        const auto * pWave = pWaves->Get(i);
                        PopulationRuntime::SWaveDesc rw;
                        rw.at_time_s = pWave->at_time_s();
                        rw.on_event  = pWave->on_event() ? pWave->on_event()->str() : std::string{};
                        if (const auto * pWaveMembers = pWave->members()) {
                                rw.members.reserve(pWaveMembers->size());
                                for (flatbuffers::uoffset_t j = 0; j < pWaveMembers->size(); ++j) {
                                        PopulationMemberDesc d = ParseMember(pWaveMembers->Get(j));
                                        if (d.template_id.empty()) {
                                                throw std::runtime_error("Population: wave member template_id is required");
                                        }
                                        rw.members.push_back(ToRuntimeMember(d, /*wave_only=*/true));
                                }
                        }
                        rd.waves.push_back(std::move(rw));
                }
        }
        if (const auto * pConditions = pFB->conditions()) {
                for (flatbuffers::uoffset_t i = 0; i < pConditions->size(); ++i) {
                        vConditions.push_back(ParseCondition(pConditions->Get(i)));
                }
        }

        oRuntime.Arm(rd);
        // children of this node don't exist yet during load — SpawnerSystem
        // re-resolves on the first spawn
        SyncGeometryNodes(false);
}

Population::~Population() noexcept {
        Disable();
}

// ---------------------------------------------------------------------------
// geometry: resolve member geometry names to nodes under the population subtree
// ---------------------------------------------------------------------------

bool Population::SyncGeometryNodes(bool bVerboseOnMiss) {

        vGeometryNodes.clear();
        vGeometryNodes.reserve(oRuntime.MemberCount());
        bool bAllResolved = true;
        for (uint32_t i = 0; i < oRuntime.MemberCount(); ++i) {
                const auto & m = oRuntime.Member(i);
                if (m.geometry.empty()) {
                        vGeometryNodes.push_back(pNode->weak_from_this());
                        continue;
                }
                if (auto pGeo = pNode->FindChild(StrID(m.geometry), /*recursive=*/true)) {
                        vGeometryNodes.push_back(pGeo);
                } else {
                        if (bVerboseOnMiss) {
                                log_e("Population '{}': geometry node '{}' not found — member spawns at origin",
                                      id_string, m.geometry);
                        }
                        bAllResolved = false;
                        vGeometryNodes.push_back(pNode->weak_from_this());
                }
        }
        return bAllResolved;
}

// ---------------------------------------------------------------------------
// lifecycle
// ---------------------------------------------------------------------------

void Population::Enable() {

        if (enabled) { return; }
        enabled = true;

        age         = 0.f;
        next_tick_t = tick_phase;

        // TagNear resolution (GetOrAdd in the index — arm-time only)
        if (auto * pIndex = GetSystem<SpawnerSystem>().GetIndex()) {
                for (auto & c : vConditions) {
                        SpawnConditions::ResolveTag(c, pIndex->Get());
                }
        }

        // WorldFlag resolution against the global blackboard (arm-time warn)
        if (const auto * pBlackboard = GetSystem<SpawnerSystem>().GetBlackboard()) {
                for (auto & c : vConditions) {
                        SpawnConditions::ResolveWorldFlag(c, pBlackboard);
                }
        } else {
                for (auto & c : vConditions) {
                        SpawnConditions::ResolveWorldFlag(c, nullptr);
                }
        }

        // deterministic PRNG: world seed ^ population id ^ seed offset
        const uint64_t world_seed = GetSystem<SpawnerSystem>().WorldSeed();
        oRng = DeterministicRng(world_seed ^ DeterministicRng::Hash64(id_string) ^ id);

        GetSystem<SpawnerSystem>().AddComponent(this);
}

void Population::Disable() {

        if (!enabled) { return; }
        enabled = false;

        // Owned spawns die with the population (deferred — this may run in a node
        // dtor during teardown, never destroy directly here). Persistent-lifetime
        // spawns live under the dynamics root and are the ones meant to survive
        // this teardown — drop their records, leave the entities alive.
        auto & oEM = GetSystem<EntityManager>();
        for (auto & r : vRecords) {
                if (!r.node.expired() && eLifetime != FlatBuffers::SpawnLifetime::Persistent) {
                        oEM.Destroy(r.node);
                }
        }
        vRecords.clear();
        vCommands.clear();

        GetSystem<SpawnerSystem>().RemoveComponent(this);
}

// ---------------------------------------------------------------------------
// world slot claim
// ---------------------------------------------------------------------------

bool Population::ClaimWorldSlot(StrID slot) {
        return id == slot;
}

std::string Population::Str() const {
        return "Population('" + id_string + "', " + std::to_string(oRuntime.ActiveTotal()) + " active)";
}

} // namespace SE

#endif
