

namespace SE {


template < class ResourceList > ResourceManager<ResourceList>::ResourceManager() { ;; }

template < class ResourceList > ResourceManager<ResourceList>::~ResourceManager() throw() { ;; }

template < class ResourceList > template <class Resource> Resource & ResourceManager<ResourceList>::Storage() {

  return Loki::Field<Resource>(oResourceStorage);
}

} // namespace SE
