
#ifdef SE_IMPL

#include <spawner/SpawnConditions.h>
#include <world/SpatialIndex.h>
#include <ai/BlackboardCore.h>

// WorldFlag conditions evaluate through BlackboardCore — the method
// definitions must travel with this .tcc so every composition that evaluates
// spawner conditions links without per-app AI wiring. Both files are
// pin/resource-typelist-safe (the pin body lives in BlackboardPin.tcc).
#include <ai/BlackboardAsset.tcc>
#include <ai/BlackboardCore.tcc>

#include <Logging.h>
#include <stdexcept>

namespace SE {
namespace SpawnConditions {

SConditionDesc Parse(EKind kind, float radius, float at_s, const std::string & tag,
                     const std::string & key) {

        SConditionDesc d;
        d.kind = kind;

        switch (kind) {
        case EKind::FocusNear:
                if (!(radius > 0.f)) {
                        throw std::runtime_error("SpawnConditions: FocusNear requires radius > 0");
                }
                d.radius = radius;
                break;
        case EKind::TagNear:
                if (tag.empty()) {
                        throw std::runtime_error("SpawnConditions: TagNear requires a tag");
                }
                if (!(radius > 0.f)) {
                        throw std::runtime_error("SpawnConditions: TagNear requires radius > 0");
                }
                d.radius = radius;
                d.tag    = tag;
                break;
        case EKind::AtTime:
                if (!(at_s >= 0.f)) {
                        throw std::runtime_error("SpawnConditions: AtTime requires at_s >= 0");
                }
                d.at_s = at_s;
                break;
        case EKind::WorldFlag:
                if (key.empty()) {
                        throw std::runtime_error("SpawnConditions: WorldFlag requires a key");
                }
                d.s_key = key;
                d.key   = StrID(key);
                break;
        default:
                throw std::runtime_error("SpawnConditions: unknown condition kind");
        }
        return d;
}

void ResolveTag(SConditionDesc & desc, SpatialIndex & index) {

        if (desc.kind != EKind::TagNear) { return; }

        const uint32_t bit = index.TagBit(StrID(desc.tag));
        if (bit >= SpatialIndex::kMaxTags) {
                log_w("SpawnConditions: tag registry exhausted — TagNear('%s') never passes", desc.tag.c_str());
                desc.tag_mask = 0;
                return;
        }
        desc.tag_mask = 1ull << bit;
}

void ResolveWorldFlag(SConditionDesc & desc, const BlackboardCore * pBlackboard) {

        if (desc.kind != EKind::WorldFlag) { return; }

        if (!pBlackboard) {
                log_w("SpawnConditions: WorldFlag('%s') has no global blackboard — never passes",
                                desc.s_key.c_str());
                return;
        }

        SSlotRef slot;
        if (!pBlackboard->TryResolve(desc.key, EBBType::Bool, slot)) {
                log_w("SpawnConditions: WorldFlag('%s') does not resolve to a bool key — never passes",
                                desc.s_key.c_str());
        }
}

bool Evaluate(const SConditionDesc & desc, const SEvalContext & ctx) {

        switch (desc.kind) {
        case EKind::FocusNear: {
                const float r2 = desc.radius * desc.radius;
                for (uint32_t i = 0; i < ctx.focus_count; ++i) {
                        const glm::vec3 d = ctx.vFocuses[i] - ctx.origin;
                        if (d.x * d.x + d.y * d.y + d.z * d.z <= r2) {
                                return true;
                        }
                }
                return false;
        }
        case EKind::TagNear: {
                if (!ctx.pIndex || desc.tag_mask == 0) {
                        return false;
                }
                bool found = false;
                ctx.pIndex->ForEachInSphere(ctx.origin, desc.radius, desc.tag_mask,
                                [&found](const SpatialIndex::SEntry &) { found = true; });
                return found;
        }
        case EKind::AtTime:
                return ctx.t_since_arm >= desc.at_s;
        case EKind::WorldFlag: {
                if (!ctx.pBlackboard) {
                        return false;
                }
                SSlotRef slot;
                if (!ctx.pBlackboard->TryResolve(desc.key, EBBType::Bool, slot)) {
                        return false;   // unknown key / type mismatch — warned at Arm
                }
                return ctx.pBlackboard->GetBool(slot);
        }
        default:
                return false;
        }
}

} // namespace SpawnConditions
} // namespace SE

#endif
