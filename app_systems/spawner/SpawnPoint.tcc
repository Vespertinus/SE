
#ifdef SE_IMPL

#include <spawner/SpawnPoint.h>
#include <spawner/SpawnerSystem.h>

#include <Logging.h>
#include <stdexcept>

namespace SE {

SpawnPoint::SpawnPoint(TSceneTree::TSceneNodeExact * pNewNode, const FlatBuffers::SpawnPoint * pFB)
        : pNode(pNewNode) {

        if (pFB->template_id() == nullptr || pFB->template_id()->size() == 0) {
                throw std::runtime_error("SpawnPoint: template_id is required");
        }
        sTemplateId     = pFB->template_id()->str();
        initial_delay_s = pFB->initial_delay_s();
        max_spawns      = pFB->max_spawns();
        respawn_delay_s = pFB->respawn_delay_s();
        eSnap           = pFB->snap();

        if (pFB->condition() != nullptr && pFB->condition()->type_type() != FlatBuffers::SpawnConditionU::NONE) {
                const auto * pCond = pFB->condition();
                switch (pCond->type_type()) {
                case FlatBuffers::SpawnConditionU::FocusNear:
                        oCondition = SpawnConditions::Parse(SpawnConditions::EKind::FocusNear,
                                        pCond->value_as_FocusNear()->radius(), 0.f, {});
                        break;
                case FlatBuffers::SpawnConditionU::TagNear:
                        oCondition = SpawnConditions::Parse(SpawnConditions::EKind::TagNear,
                                        pCond->value_as_TagNear()->radius(), 0.f,
                                        pCond->value_as_TagNear()->tag()->str());
                        break;
                case FlatBuffers::SpawnConditionU::AtTime:
                        oCondition = SpawnConditions::Parse(SpawnConditions::EKind::AtTime,
                                        0.f, pCond->value_as_AtTime()->at_s(), {});
                        break;
                case FlatBuffers::SpawnConditionU::WorldFlag:
                        oCondition = SpawnConditions::Parse(SpawnConditions::EKind::WorldFlag,
                                        0.f, 0.f, {},
                                        pCond->value_as_WorldFlag()->key()->str());
                        break;
                default:
                        throw std::runtime_error("SpawnPoint: unknown condition type");
                }
                has_gate = true;
        }

        next_ready_t = initial_delay_s;
}

SpawnPoint::~SpawnPoint() noexcept {
        Disable();
}

void SpawnPoint::Enable() {

        if (enabled) { return; }
        enabled = true;

        age          = 0.f;
        next_ready_t = initial_delay_s;

        // resolve TagNear against the index (if composed) at arm
        if (has_gate && oCondition.kind == SpawnConditions::EKind::TagNear) {
                auto & oSystem = GetSystem<SpawnerSystem>();
                if (auto * pIndex = oSystem.GetIndex()) {
                        SpawnConditions::ResolveTag(oCondition, pIndex->Get());
                }
        }

        // resolve WorldFlag against the global blackboard (arm-time warn)
        if (has_gate && oCondition.kind == SpawnConditions::EKind::WorldFlag) {
                SpawnConditions::ResolveWorldFlag(oCondition, GetSystem<SpawnerSystem>().GetBlackboard());
        }

        GetSystem<SpawnerSystem>().AddComponent(this);
}

void SpawnPoint::Disable() {

        if (!enabled) { return; }
        enabled = false;

        GetSystem<SpawnerSystem>().RemoveComponent(this);
}

std::string SpawnPoint::Str() const {
        return "SpawnPoint('" + sTemplateId + "', " + std::to_string(spawned_total) + "/"
               + std::to_string(max_spawns) + ")";
}

} // namespace SE

#endif
