
#ifndef APP_SPAWNER_SPAWN_VOLUME_H
#define APP_SPAWNER_SPAWN_VOLUME_H 1

#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <Component_generated.h>
#include <Spawner_generated.h>

namespace SE {

// ---------------------------------------------------------------------------
// SpawnVolume — placement provider: a box with a sampling distribution.
// Pure geometry: never spawns on its own; populations resolve it by node name
// and sample it (the population's RNG drives the sampling; the volume owns the
// best-candidate pick history, reset on Enable).
// ---------------------------------------------------------------------------

class SpawnVolume {
public:
        using TSerialized = FlatBuffers::SpawnVolume;

        SpawnVolume(TSceneTree::TSceneNodeExact * pNode, const FlatBuffers::SpawnVolume * pFB);
        ~SpawnVolume() noexcept = default;

        void Enable();
        void Disable();

        const glm::vec3 &               Size()       const { return oSize; }
        FlatBuffers::SpawnDistribution  Distribution() const { return eDistribution; }
        uint32_t                        Candidates() const { return candidate_count; }
        uint32_t                        SeedOffset() const { return seed_offset; }

        // Best-candidate pick history (caller records picks here).
        const std::vector<glm::vec3> & Picks() const { return vPicks; }
        void RecordPick(const glm::vec3 & vPos);
        static constexpr size_t kMaxPicks = 64;

        std::string Str() const;

private:
        TSceneTree::TSceneNodeExact * pNode = nullptr;
        glm::vec3                     oSize {2.f, 2.f, 2.f};
        FlatBuffers::SpawnDistribution eDistribution = FlatBuffers::SpawnDistribution::Uniform;
        uint32_t                      candidate_count = 8;
        uint32_t                      seed_offset     = 0;
        std::vector<glm::vec3>        vPicks;
        // Rotating overwrite cursor: `vPicks[size() % kMaxPicks]` is always
        // slot 0 once full, which collapsed best-candidate spacing to
        // "farthest from pick[0]".
        size_t                        pick_cursor = 0;
        bool                          enabled = false;
};

} // namespace SE

#endif
