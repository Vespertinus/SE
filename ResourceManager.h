

#ifndef __RESOURCE_MANAGER_H_
#define __RESOURCE_MANAGER_H_ 1

#include <loki/HierarchyGenerators.h>
#include <map>

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

  template < class R> struct Holder : public std::map <rid_t, R *> { };
  
  typedef Loki::GenScatterHierarchy<ResourceList, Holder> TResourceStorage;


  TResourceStorage      oResourceStorage;



  template <class Resource> Resource & Storage();

  public:

  ResourceManager();
  virtual ~ResourceManager() throw();

  template <class Resource, class TConcreateSettings> Resource * Create (const std::string & oPath, const TConcreateSettings & oSettings);
  template <class Resource> void Destroy(const rid_t key);
  template <class Resource> bool IsLoaded(const rid_t key) const;
  template <class Resource> bool IsLoaded(const std::string & sPath) const;
  template <class Resource> void Clear();

}; 

} // namespace SE

#include <ResourceManager.tcc>

#endif
