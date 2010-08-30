

#ifndef __RESOURCE_MANAGER_H_
#define __RESOURCE_MANAGER_H_ 1

#include <loki/HierarchyGenerators.h>
#include <map>


namespace SE {

/** resource id type */
typedef uint64_t rid_t;

/**
 LRU
 Limit size for each resource type
 ref count 
 */
template < class ResourceList > class ResourceManager {


  protected:

  template < class R> struct Holder : public std::map <rid_t, R> { };
  
  typedef Loki::GenScatterHierarchy<ResourceList, Holder> TResourceStorage;


  TResourceStorage      oResourceStorage;



  template <class Resource> Resource & Storage();

  public:

  ResourceManager();
  virtual ~ResourceManager() throw();

  rid_t Create (const std::string & oPath, );
  //Destroy
  //IsLoaded(r_id_t);
  //<Resource type >Clear();

}; 

} // namespace SE

#include <ResourceManager.tcc>

#endif
