
#ifndef APP_SPAWNER_SPAWN_PLACEMENT_H
#define APP_SPAWNER_SPAWN_PLACEMENT_H 1

// Pure placement sampling (no engine includes) — the geometry half of the
// spawner. Deterministic: positions are pure functions of (desc, rng state).

#include <glm/glm.hpp>

#include <spawner/DeterministicRng.h>

namespace SE {
namespace SpawnPlacement {

// ---------------------------------------------------------------------------
// SpawnPoint sampling: a point with optional uniform disc jitter on XZ.
// ---------------------------------------------------------------------------

struct SPointDesc {
        float jitter_radius = 0.f;   // 0 = exact point
};

glm::vec3 SamplePoint(const SPointDesc & desc, DeterministicRng & rng, const glm::vec3 & vOrigin);

// ---------------------------------------------------------------------------
// SpawnVolume sampling: box centered on vOrigin with full extents size.
// ---------------------------------------------------------------------------

enum class EDistribution : uint8_t {
        Uniform       = 0,   // independent uniform sample in the box
        BestCandidate = 1,   // Mitchell k-candidate: k uniform samples, keep the one
                             // farthest from previous picks — even spread, deterministic
};

struct SVolumeDesc {
        glm::vec3    size              {2.f, 2.f, 2.f};  // full extents
        EDistribution distribution     = EDistribution::Uniform;
        uint32_t     candidate_count   = 8;              // best-candidate k
};

/// vPrevious/previous_count: the caller's pick history for best-candidate spread
/// (uniform ignores it). The caller appends the returned position to its history.
glm::vec3 SampleVolume(const SVolumeDesc & desc, DeterministicRng & rng, const glm::vec3 & vOrigin,
                const glm::vec3 * vPrevious = nullptr, uint32_t previous_count = 0);

} // namespace SpawnPlacement
} // namespace SE

#endif
