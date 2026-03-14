

#include <loki/Typelist.h>

namespace SE {

namespace detail {

template <class TList, class RM>
struct ClearAllHelper {
        static void Do(RM & rm) {
                rm.template Clear<typename TList::Head>();
                ClearAllHelper<typename TList::Tail, RM>::Do(rm);
        }
};
template <class RM>
struct ClearAllHelper<Loki::NullType, RM> {
        static void Do(RM &) {}
};

template <class TList, class RM>
struct ProcessDeferredHelper {
        static void Do(RM & rm) {
                rm.template ProcessDeferredPool<typename TList::Head>();
                ProcessDeferredHelper<typename TList::Tail, RM>::Do(rm);
        }
};
template <class RM>
struct ProcessDeferredHelper<Loki::NullType, RM> {
        static void Do(RM &) {}
};

template <class TList, class RM>
struct DestroyUnretainedHelper {
        static void Do(RM & rm) {
                rm.template DestroyUnretainedPool<typename TList::Head>();
                DestroyUnretainedHelper<typename TList::Tail, RM>::Do(rm);
        }
};
template <class RM>
struct DestroyUnretainedHelper<Loki::NullType, RM> {
        static void Do(RM &) {}
};

template <class TList, class RM>
struct ClearAllRetainsHelper {
        static void Do(RM & rm) {
                rm.template ClearRetains<typename TList::Head>();
                ClearAllRetainsHelper<typename TList::Tail, RM>::Do(rm);
        }
};
template <class RM>
struct ClearAllRetainsHelper<Loki::NullType, RM> {
        static void Do(RM &) {}
};

} // namespace detail


template <class ResourceList>
ResourceManager<ResourceList>::ResourceManager() {}


template <class ResourceList>
ResourceManager<ResourceList>::~ResourceManager() noexcept {
        detail::ClearAllHelper<ResourceList, ResourceManager<ResourceList>>::Do(*this);
}


template <class ResourceList>
template <class Resource, class ... TConcreateSettings>
H<Resource> ResourceManager<ResourceList>::Create(
                const std::string & oPath,
                const TConcreateSettings & ... oSettings) {

        if (oPath.empty()) {
                log_e("empty resource name");
                return H<Resource>::Null();
        }

        rid_t key = Hash64(oPath.c_str(), oPath.length());
        Pool<Resource> & pool = GetPool<Resource>();

        auto it = pool.path_cache.find(key);
        if (it != pool.path_cache.end()) {
                if (Get(it->second) != nullptr) {
                        return it->second;
                }
                pool.path_cache.erase(it);
        }

        uint32_t idx;
        if (!pool.free_list.empty()) {
                idx = pool.free_list.back();
                pool.free_list.pop_back();
        } else {
                idx = static_cast<uint32_t>(pool.slots.size());
                pool.slots.emplace_back();
        }

        typename Pool<Resource>::Slot & slot = pool.slots[idx];

        try {
                CalcDuration oLoadDuration;
                slot.pResource = std::make_unique<Resource>(oPath, key, oSettings...);
                log_d("resource '{}' load duration = {} ms", oPath, oLoadDuration.Get());
        }
        catch (std::exception & ex) {
                log_e("got exception, description = '{}', name: '{}'", ex.what(), oPath);
                slot.pResource.reset();
                pool.free_list.push_back(idx);
                throw;
        }
        catch (...) {
                log_e("got unknown exception, name: '{}'", oPath);
                slot.pResource.reset();
                pool.free_list.push_back(idx);
                throw;
        }

        H<Resource> h { { idx, slot.generation } };
        pool.path_cache[key] = h;
        return h;
}


template <class ResourceList>
template <class Resource>
H<Resource> ResourceManager<ResourceList>::Get(const std::string & sPath) const {

        if (sPath.empty()) { return H<Resource>::Null(); }
        rid_t key = Hash64(sPath.c_str(), sPath.length());
        const Pool<Resource> & pool = GetPool<Resource>();

        auto it = pool.path_cache.find(key);
        if (it == pool.path_cache.end()) { return H<Resource>::Null(); }
        return it->second;
}


template <class ResourceList>
template <class Resource>
Resource * ResourceManager<ResourceList>::Get(H<Resource> h) const {

        if (!h.IsValid()) { return nullptr; }
        const Pool<Resource> & pool = GetPool<Resource>();
        if (h.raw.index >= pool.slots.size()) { return nullptr; }
        const auto & slot = pool.slots[h.raw.index];
        if (slot.generation != h.raw.generation) { return nullptr; }
        return slot.pResource.get();
}


template <class ResourceList>
template <class Resource>
void ResourceManager<ResourceList>::Destroy(H<Resource> h) {

        if (!h.IsValid()) { return; }
        Pool<Resource> & pool = GetPool<Resource>();
        // Remove from path_cache immediately so a subsequent Create() for the same
        // path allocates a fresh slot rather than returning the eviction-queued handle.
        if (h.raw.index < pool.slots.size()) {
                auto & slot = pool.slots[h.raw.index];
                if (slot.generation == h.raw.generation && slot.pResource) {
                        pool.path_cache.erase(slot.pResource->RID());
                }
        }
        pool.deferred_destroy.push_back(h);
}


template <class ResourceList>
template <class Resource>
bool ResourceManager<ResourceList>::Lock(H<Resource> h) {
        if (!h.IsValid()) {
                log_w("Lock called with invalid handle: {}", h);
                return false;
        }
        Pool<Resource> & pool = GetPool<Resource>();
        if (h.raw.index >= pool.slots.size()) { return false; }
        auto & slot = pool.slots[h.raw.index];
        if (slot.generation != h.raw.generation) { return false; }
        ++slot.lock_count;
        return true;
}


template <class ResourceList>
template <class Resource>
void ResourceManager<ResourceList>::Unlock(H<Resource> h) {
        if (!h.IsValid()) { return; }
        Pool<Resource> & pool = GetPool<Resource>();
        if (h.raw.index >= pool.slots.size()) { return; }
        auto & slot = pool.slots[h.raw.index];
        if (slot.generation != h.raw.generation) { return; }
        if (slot.lock_count > 0) { --slot.lock_count; }
}


template <class ResourceList>
template <class Resource>
void ResourceManager<ResourceList>::ProcessDeferredPool() {

        Pool<Resource> & pool = GetPool<Resource>();
        std::vector<H<Resource>> remaining;
        for (auto & h : pool.deferred_destroy) {
                if (!h.IsValid() || h.raw.index >= pool.slots.size()) { continue; }
                auto & slot = pool.slots[h.raw.index];
                if (slot.generation != h.raw.generation) { continue; } // already stale
                if (slot.lock_count > 0 || slot.retain_count > 0) {
                        remaining.push_back(h);                        // re-queue; try again later
                        continue;
                }
                slot.pResource.reset();
                slot.generation++;
                pool.free_list.push_back(h.raw.index);
        }
        pool.deferred_destroy = std::move(remaining);
}


template <class ResourceList>
void ResourceManager<ResourceList>::ProcessDeferred() {

        detail::ProcessDeferredHelper<ResourceList, ResourceManager<ResourceList>>::Do(*this);
}


template <class ResourceList>
template <class Resource>
bool ResourceManager<ResourceList>::IsLoaded(const std::string & sPath) const {

        if (sPath.empty()) { return false; }
        rid_t key = Hash64(sPath.c_str(), sPath.length());
        const Pool<Resource> & pool = GetPool<Resource>();
        auto it = pool.path_cache.find(key);
        if (it == pool.path_cache.end()) { return false; }
        return Get(it->second) != nullptr;
}


template <class ResourceList>
template <class Resource>
bool ResourceManager<ResourceList>::IsLoaded(H<Resource> h) const {

        return Get(h) != nullptr;
}


template <class ResourceList>
template <class Resource>
void ResourceManager<ResourceList>::Clear() {

        Pool<Resource> & pool = GetPool<Resource>();
        for (auto & slot : pool.slots) {
                slot.pResource.reset();
        }
        pool.slots.clear();
        pool.free_list.clear();
        pool.path_cache.clear();
        pool.deferred_destroy.clear();
}


template <class ResourceList>
template <class Resource>
size_t ResourceManager<ResourceList>::Size() const {

        const Pool<Resource> & pool = GetPool<Resource>();
        size_t count = 0;
        for (const auto & slot : pool.slots) {
                if (slot.pResource) { ++count; }
        }
        return count;
}


template <class ResourceList>
template <class Resource>
void ResourceManager<ResourceList>::Retain(H<Resource> h) {
        if (!h.IsValid()) { return; }
        Pool<Resource> & pool = GetPool<Resource>();
        if (h.raw.index >= pool.slots.size()) { return; }
        auto & slot = pool.slots[h.raw.index];
        if (slot.generation != h.raw.generation) { return; }
        ++slot.retain_count;
}


template <class ResourceList>
template <class Resource>
void ResourceManager<ResourceList>::Unretain(H<Resource> h) {
        if (!h.IsValid()) { return; }
        Pool<Resource> & pool = GetPool<Resource>();
        if (h.raw.index >= pool.slots.size()) { return; }
        auto & slot = pool.slots[h.raw.index];
        if (slot.generation != h.raw.generation) { return; }
        if (slot.retain_count > 0) { --slot.retain_count; }
}


template <class ResourceList>
template <class Resource>
void ResourceManager<ResourceList>::ClearRetains() {
        Pool<Resource> & pool = GetPool<Resource>();
        for (auto & slot : pool.slots) { slot.retain_count = 0; }
}


template <class ResourceList>
template <class Resource>
void ResourceManager<ResourceList>::DestroyUnretainedPool() {
        Pool<Resource> & pool = GetPool<Resource>();
        for (uint32_t i = 0; i < pool.slots.size(); ++i) {
                auto & slot = pool.slots[i];
                if (!slot.pResource || slot.retain_count > 0) { continue; }
                H<Resource> h { { i, slot.generation } };
                if (slot.lock_count == 0) {
                        pool.path_cache.erase(slot.pResource->RID());
                        slot.pResource.reset();
                        ++slot.generation;
                        pool.free_list.push_back(i);
                } else {
                        // locked this frame — fall back to deferred
                        Destroy(h);
                }
        }
}


template <class ResourceList>
void ResourceManager<ResourceList>::DestroyUnretained() {
        detail::DestroyUnretainedHelper<ResourceList, ResourceManager<ResourceList>>::Do(*this);
}


template <class ResourceList>
void ResourceManager<ResourceList>::ClearAllRetains() {
        detail::ClearAllRetainsHelper<ResourceList, ResourceManager<ResourceList>>::Do(*this);
}

} // namespace SE
