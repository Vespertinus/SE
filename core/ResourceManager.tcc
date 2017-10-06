

namespace SE {


template < class ResourceList > ResourceManager<ResourceList>::ResourceManager() { ;; }



template < class ResourceList > ResourceManager<ResourceList>::~ResourceManager() throw() {

  //TODO clean all items in all storages
}


/*
template < class ResourceList > template <class Resource> Holder<Resource> & ResourceManager<ResourceList>::Storage() {

        return Loki::Field<Resource>(oResourceStorage);
}
*/


template < class ResourceList > template <class Resource, class ... TConcreateSettings> 
        Resource * ResourceManager<ResourceList>::
                Create (
                        const std::string        & oPath,
                        const TConcreateSettings & ... oSettings) {

        typedef Holder <Resource> TConcreateStorage;

        rid_t                         key       = Hash64(oPath.c_str(), oPath.length());
        TConcreateStorage           & oStorage  = Storage<Resource>();

        typename TConcreateStorage::iterator   itCheck   = oStorage.find(key);

        if (itCheck != oStorage.end()) { return itCheck->second; }

        Resource                    * pResource = 0;

        try {
                CalcDuration oLoadDuration;

                pResource = new Resource(oPath, key, oSettings ... );

                log_d("resource '{}' load duration = {} ms", oPath, oLoadDuration.Get());

                oStorage.insert(std::pair<rid_t, Resource *>(key, pResource));
        }
        catch(std::exception & ex) {
                log_e("got exception, description = '{}'", ex.what());
                if (pResource != 0) {
                        delete pResource;
                        pResource = 0;
                }
        }
        catch(...) {
                log_e("got unknown exception");
                if (pResource != 0) {
                        delete pResource;
                        pResource = 0;
                }
        }

        return pResource;
}



template < class ResourceList > template <class Resource> void ResourceManager<ResourceList>::Destroy(const rid_t key) {

        typedef Holder <Resource>             TConcreateStorage;

        TConcreateStorage                    & oStorage  = Storage<Resource>();
        typename TConcreateStorage::iterator   itCheck   = oStorage.find(key);

        if (itCheck == oStorage.end()) { return; }

        delete itCheck.second;
        oStorage.erase(itCheck);
}



template < class ResourceList > template <class Resource> bool ResourceManager<ResourceList>::IsLoaded(const rid_t key) const {

        typedef Holder <Resource>             TConcreateStorage;


        TConcreateStorage                    & oStorage  = Storage<Resource>();
        typename TConcreateStorage::iterator   itCheck   = oStorage.find(key);

        if (itCheck == oStorage.end()) { return false; }

        return true;
}



template < class ResourceList > template <class Resource> bool ResourceManager<ResourceList>::IsLoaded(const std::string & sPath) const {

        rid_t key = Hash64(sPath.c_str(), sPath.length());

        return IsLoaded<Resource>(key);
}
  


template < class ResourceList > template <class Resource> void ResourceManager<ResourceList>::Clear() {

        typedef Holder <Resource>             TConcreateStorage;

        TConcreateStorage & oStorage  = Storage<Resource>();

        for (typename TConcreateStorage::iterator itItem = oStorage.begin();
                        itItem != oStorage.end();
                        ++itItem) {

                delete itItem.second;
                oStorage.erase(itItem);
        }
}


} // namespace SE
