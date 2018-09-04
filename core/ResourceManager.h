

#ifndef __RESOURCE_MANAGER_H_
#define __RESOURCE_MANAGER_H_ 1

#include <loki/HierarchyGenerators.h>
#include <unordered_map>

#include <ResourceHolder.h>
#include <Util.h>

namespace SE {


/**
 LRU
 Limit size for each resource type
 ref count
 */
template < class ResourceList > class ResourceManager {


  protected:

  template < class R> struct Holder : public std::unordered_map <rid_t, R *> { };

  typedef Loki::GenScatterHierarchy<ResourceList, Holder> TResourceStorage;


  TResourceStorage      oResourceStorage;



  template <class Resource> Holder<Resource> & Storage() {

    return Loki::Field<Resource>(oResourceStorage);
  }

  public:

  ResourceManager();
  virtual ~ResourceManager() noexcept;

  template <class Resource, class ... TConcreateSettings> Resource * Create (const std::string & oPath, const TConcreateSettings & ... oSettings);
  template <class Resource> void Destroy(const rid_t key);
  template <class Resource> bool IsLoaded(const rid_t key) const;
  template <class Resource> bool IsLoaded(const std::string & sPath) const;
  template <class Resource> void Clear();
  template <class Resource> size_t Size();
  /*TODO
                нужен Clone в ResourceManager
                Create empty и указание у Resource может ли он быть создан как empty
                автогенерация rid для empty Resource
                instance id помимо rid для instance'ов
  */

};

} // namespace SE

#include <ResourceManager.tcc>

#endif
