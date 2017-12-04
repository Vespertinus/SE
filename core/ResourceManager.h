

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

  //template <class Resource, class TConcreateSettings> Resource * Create (const std::string & oPath, const TConcreateSettings & oSettings);
  template <class Resource, class ... TConcreateSettings> Resource * Create (const std::string & oPath, const TConcreateSettings & ... oSettings);
  template <class Resource> void Destroy(const rid_t key);
  template <class Resource> bool IsLoaded(const rid_t key) const;
  template <class Resource> bool IsLoaded(const std::string & sPath) const;
  template <class Resource> void Clear();

}; 

} // namespace SE

#include <ResourceManager.tcc>

#endif
