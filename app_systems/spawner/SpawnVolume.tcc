
#ifdef SE_IMPL

#include <spawner/SpawnVolume.h>

#include <Logging.h>
#include <stdexcept>

namespace SE {

SpawnVolume::SpawnVolume(TSceneTree::TSceneNodeExact * pNewNode, const FlatBuffers::SpawnVolume * pFB)
        : pNode(pNewNode) {

        if (pFB->size() == nullptr) {
                throw std::runtime_error("SpawnVolume: size is required");
        }
        oSize           = glm::vec3(pFB->size()->x(), pFB->size()->y(), pFB->size()->z());
        eDistribution   = pFB->distribution();
        candidate_count = pFB->candidate_count();
        seed_offset     = pFB->seed_offset();
}

void SpawnVolume::Enable() {

        if (enabled) { return; }
        enabled = true;

        vPicks.clear();      // re-arm = fresh spread
        pick_cursor = 0;
}

void SpawnVolume::Disable() {

        if (!enabled) { return; }
        enabled = false;

        vPicks.clear();
        pick_cursor = 0;
}

void SpawnVolume::RecordPick(const glm::vec3 & vPos) {
        if (vPicks.size() < kMaxPicks) {
                vPicks.push_back(vPos);
        } else {
                vPicks[pick_cursor] = vPos;   // ring: oldest pick ages out
                pick_cursor         = (pick_cursor + 1) % kMaxPicks;
        }
}

std::string SpawnVolume::Str() const {
        return "SpawnVolume(" + std::to_string(oSize.x) + "x" + std::to_string(oSize.y)
               + "x" + std::to_string(oSize.z) + ")";
}

} // namespace SE

#endif
