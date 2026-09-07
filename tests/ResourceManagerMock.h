
#pragma once

#include <ResourceHandle.h>
#include <ResourceHolder.h>
#include <vector>
#include <cstdint>

namespace SE {

// Minimal ResourceManager substitute for unit tests.
// Allocates resources with `new Resource(name, rid, args...)`, mirroring the
// production ResourceManager constructor convention.  Stores raw pointers in a
// flat slot array indexed by H<T>.  Not thread-safe.
class ResourceManagerMock {

        struct Slot {
                void*        ptr     = nullptr;
                void(*deleter)(void*) = nullptr;
        };

        std::vector<Slot> vSlots;

public:

        template <class Resource, class ... TArgs>
        H<Resource> Create(const std::string& name, const TArgs& ... args) {

                const rid_t    rid = static_cast<rid_t>(vSlots.size());
                const uint32_t idx = static_cast<uint32_t>(vSlots.size());
                auto* p = new Resource(name, rid, args...);
                vSlots.push_back({ static_cast<void*>(p),
                                   [](void* raw) { delete static_cast<Resource*>(raw); } });
                return H<Resource>{{ idx, 0 }};
        }

        template <class Resource>
        Resource* Get(H<Resource> h) const {
                if (!h.IsValid() || h.raw.index >= static_cast<uint32_t>(vSlots.size()))
                        return nullptr;
                return static_cast<Resource*>(vSlots[h.raw.index].ptr);
        }

        template <class Resource>
        void Destroy(H<Resource> h) {
                if (!h.IsValid() || h.raw.index >= static_cast<uint32_t>(vSlots.size()))
                        return;
                auto& slot = vSlots[h.raw.index];
                if (slot.ptr && slot.deleter) slot.deleter(slot.ptr);
                slot = {};
        }

        ~ResourceManagerMock() noexcept {
                for (auto& slot : vSlots)
                        if (slot.ptr && slot.deleter) slot.deleter(slot.ptr);
        }
};

} // namespace SE
