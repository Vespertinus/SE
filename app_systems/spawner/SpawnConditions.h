
#ifndef APP_SPAWNER_SPAWN_CONDITIONS_H
#define APP_SPAWNER_SPAWN_CONDITIONS_H 1

// Pure condition evaluation (no engine systems) — the arming gate half of the
// spawner. Extensible by adding an EKind + an Evaluate branch; the spawner core
// never changes (the stage-plan "conditions are data" property).

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <StrID.h>
#include <world/SpatialIndex.h>   // pure query grid (glm/StrID only) — TagNear target

namespace SE {
class BlackboardCore;   // app_systems/ai/ — WorldFlag source; included by the .tcc

namespace SpawnConditions {

enum class EKind : uint8_t {
        FocusNear = 0,   // any registered focus point within radius of the evaluated origin
        TagNear   = 1,   // any spatial-index entry with the tag within radius (needs an index)
        AtTime    = 2,   // t (seconds since arm) >= at_s
        WorldFlag = 3,   // global blackboard bool key is true (needs BlackboardSystem)
};

struct SConditionDesc {
        EKind      kind     = EKind::FocusNear;
        float      radius   = 0.f;   // FocusNear/TagNear; must be > 0 (validated)
        float      at_s     = 0.f;   // AtTime; must be >= 0 (validated)
        std::string tag;             // TagNear, human-readable (debug/stats)
        uint64_t   tag_mask = 0;     // TagNear, resolved at Arm via SpatialIndex::TagBit; 0 = unresolved
        StrID      key;              // WorldFlag
        std::string s_key;           // WorldFlag, human-readable (debug/warns)
};

struct SEvalContext {
        glm::vec3        origin {};
        float            t_since_arm = 0.f;
        const glm::vec3 *vFocuses    = nullptr;  // registered focus positions
        uint32_t         focus_count = 0;
        SpatialIndex *   pIndex      = nullptr;  // TagNear; absent ⇒ false (one-shot warn at Arm)
        const BlackboardCore * pBlackboard = nullptr;  // WorldFlag; absent ⇒ false (one-shot warn at Arm)
};

/// Parse from authored strings (FB table dispatch); throws std::runtime_error on
/// bad authoring — load-time loudness, never a silently dead condition.
SConditionDesc Parse(EKind kind, float radius, float at_s, const std::string & tag,
                     const std::string & key = {});

/// Resolve TagNear tag → mask (SpatialIndex::TagBit is GetOrAdd; safe at Arm only).
void ResolveTag(SConditionDesc & desc, SpatialIndex & index);

/// Warn when the WorldFlag condition cannot resolve against the current global
/// blackboard (absent system, unresolvable key); call once at Arm.
void ResolveWorldFlag(SConditionDesc & desc, const BlackboardCore * pBlackboard);

bool Evaluate(const SConditionDesc & desc, const SEvalContext & ctx);

} // namespace SpawnConditions
} // namespace SE

#endif
