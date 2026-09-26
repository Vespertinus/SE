
#ifdef SE_IMPL

#include <spawner/SpawnPlacement.h>

#include <cmath>

namespace SE {
namespace SpawnPlacement {

glm::vec3 SamplePoint(const SPointDesc & desc, DeterministicRng & rng, const glm::vec3 & vOrigin) {

        if (desc.jitter_radius <= 0.f) {
                return vOrigin;
        }

        // Uniform disc sample (sqrt for area-uniformity) on XZ; Y from the origin.
        const float angle = rng.Range(0.f, 6.2831853f);
        const float r     = desc.jitter_radius * std::sqrt(rng.Uniform01());
        return vOrigin + glm::vec3(r * std::cos(angle), 0.f, r * std::sin(angle));
}

namespace {

glm::vec3 SampleUniformInBox(const glm::vec3 & vHalf, DeterministicRng & rng, const glm::vec3 & vOrigin) {
        return vOrigin + glm::vec3(
                rng.Range(-vHalf.x, vHalf.x),
                rng.Range(-vHalf.y, vHalf.y),
                rng.Range(-vHalf.z, vHalf.z));
}

} // namespace

glm::vec3 SampleVolume(const SVolumeDesc & desc, DeterministicRng & rng, const glm::vec3 & vOrigin,
                const glm::vec3 * vPrevious, uint32_t previous_count) {

        const glm::vec3 vHalf = glm::max(desc.size * 0.5f, glm::vec3(0.f));

        if (desc.distribution != EDistribution::BestCandidate || previous_count == 0) {
                return SampleUniformInBox(vHalf, rng, vOrigin);
        }

        // Mitchell best-candidate: k uniform tries, keep the one with the largest
        // distance to the nearest previous pick — even spread, deterministic (the
        // rng draw order is fixed). The CALLER owns the pick history and appends
        // the returned position to it.
        const uint32_t k = desc.candidate_count > 0 ? desc.candidate_count : 1;

        glm::vec3 vBest        = SampleUniformInBox(vHalf, rng, vOrigin);
        float     best_dist2   = -1.f;

        for (uint32_t c = 0; c < k; ++c) {
                const glm::vec3 vCandidate = (c == 0) ? vBest : SampleUniformInBox(vHalf, rng, vOrigin);

                float min_dist2 = 3.4e38f;
                for (uint32_t p = 0; p < previous_count; ++p) {
                        const glm::vec3 d = vCandidate - vPrevious[p];
                        const float dist2 = d.x * d.x + d.y * d.y + d.z * d.z;
                        if (dist2 < min_dist2) { min_dist2 = dist2; }
                }

                if (min_dist2 > best_dist2) {
                        best_dist2 = min_dist2;
                        vBest      = vCandidate;
                }
        }
        return vBest;
}

} // namespace SpawnPlacement
} // namespace SE

#endif
