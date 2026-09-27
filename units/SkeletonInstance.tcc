
#ifdef SE_IMPL

#include <SkeletonInstance.h>

#include <unordered_map>

namespace SE {

std::shared_ptr<SkeletonInstance> SkeletonInstance::Acquire(const void * pRootKey) {

        static std::unordered_map<const void *, std::weak_ptr<SkeletonInstance>> sIndex;

        auto it = sIndex.find(pRootKey);
        if (it != sIndex.end()) {
                if (auto pLive = it->second.lock()) { return pLive; }
        }

        auto pInstance   = std::make_shared<SkeletonInstance>();
        sIndex[pRootKey] = pInstance;

        // drop entries whose last owner is gone (characters despawn)
        if (sIndex.size() > 256) {
                for (auto itDead = sIndex.begin(); itDead != sIndex.end();) {
                        if (itDead->second.expired()) { itDead = sIndex.erase(itDead); }
                        else { ++itDead; }
                }
        }
        return pInstance;
}

} // namespace SE

#endif
