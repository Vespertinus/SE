

#ifndef __RESOURCE_MANAGER_H_
#define __RESOURCE_MANAGER_H_ 1

#include <loki/HierarchyGenerators.h>
#include <unordered_map>
#include <vector>
#include <memory>

#include <ResourceHandle.h>
#include <ResourceHolder.h>
#include <Util.h>

namespace SE {

template <class ResourceList> class ResourceManager {

protected:

        template <class R> struct Pool {
                struct Slot {
                        uint32_t           generation   { 0 };
                        uint8_t            lock_count   { 0 };
                        uint8_t            retain_count { 0 };
                        std::unique_ptr<R> pResource;
                };

                std::vector<Slot>                    slots;
                std::vector<uint32_t>                free_list;
                std::unordered_map<rid_t, H<R>> path_cache;
                std::vector<H<R>>               deferred_destroy;
        };

        typedef Loki::GenScatterHierarchy<ResourceList, Pool> TResourceStorage;

        TResourceStorage oResourceStorage;

        template <class Resource> Pool<Resource> & GetPool() {
                return Loki::Field<Resource>(oResourceStorage);
        }
        template <class Resource> const Pool<Resource> & GetPool() const {
                return Loki::Field<Resource>(oResourceStorage);
        }

public:

        template <class Resource> void ProcessDeferredPool();

        ResourceManager();
        virtual ~ResourceManager() noexcept;

        template <class Resource, class ... TConcreateSettings>
                H<Resource> Create(const std::string & oPath, const TConcreateSettings & ... oSettings);

        template <class Resource>
                H<Resource> Get(const std::string & sPath) const;

        template <class Resource>
                Resource * Get(H<Resource> h) const;

        template <class Resource>
                void Destroy(H<Resource> h);

        template <class Resource> bool Lock  (H<Resource> h);
        template <class Resource> void Unlock(H<Resource> h);

        template <class Resource> void   Retain       (H<Resource> h);
        template <class Resource> void   Unretain     (H<Resource> h);
        template <class Resource> void   ClearRetains ();
        template <class Resource> void   DestroyUnretainedPool();
        void DestroyUnretained();
        void ClearAllRetains();

        void ProcessDeferred();

        template <class Resource> bool   IsLoaded(const std::string & sPath) const;
        template <class Resource> bool   IsLoaded(H<Resource> h) const;
        template <class Resource> void   Clear();
        template <class Resource> size_t Size() const;
};

} // namespace SE

#include <ResourceManager.tcc>

#endif
