
namespace SE  {

template <class StoreStrategyList, class LoadStrategyList> 
        template <class TStoreStrategySettings,  class TLoadStrategySettings> 
                Mesh<StoreStrategyList, LoadStrategyList>::Mesh(
                        const std::string oName, 
                        const rid_t new_rid,
                        const TStoreStrategySettings & oStoreStrategySettings, 
                        const TLoadStrategySettings & oLoadStrategySettings) :
                ResourceHolder(new_rid) {

                        Create(oName, oStoreStrategySettings, oLoadStrategySettings);
}

template <class StoreStrategyList, class LoadStrategyList> 
        Mesh<StoreStrategyList, LoadStrategyList>::Mesh(
                const std::string oName, 
                const rid_t new_rid) : 
                ResourceHolder(new_rid) {


                Create(oName, typename TDefaultStoreStrategy::Settings(), typename TDefaultLoadStrategy::Settings());
}


template <class StoreStrategyList, class LoadStrategyList> Mesh<StoreStrategyList, LoadStrategyList>::~Mesh() throw() { ;; }



template <class StoreStrategyList, class LoadStrategyList> 
        template <class TStoreStrategySettings,  class TLoadStrategySettings> void 
                Mesh<StoreStrategyList, LoadStrategyList>::Create(
                                const std::string oName, 
                                const TStoreStrategySettings & oStoreStrategySettings, 
                                const TLoadStrategySettings & oLoadStrategySettings) {
 
        typedef typename MP::InnerSearch<StoreStrategyList, TStoreStrategySettings>::Result TStoreStrategy;
        typedef typename MP::InnerSearch<LoadStrategyList,  TLoadStrategySettings >::Result TLoadStrategy;

        MeshStock    oMeshStock;
        ret_code_t   err_code;

        TLoadStrategy   oLoadStrategy(oLoadStrategySettings);
        TStoreStrategy  oStoreStrategy(oStoreStrategySettings);

        err_code = oLoadStrategy.Load(oName, oMeshStock);
        if (err_code) {
                char buf[256];
                snprintf(buf, sizeof(buf), "Mesh::Create: Loading failed, err_code = %u\n", err_code);
                fprintf(stderr, "%s", buf);
                throw (std::runtime_error(buf));
        }

        err_code = oStoreStrategy.Store(oMeshStock, vMeshData);
        if (err_code) {
                char buf[256];
                snprintf(buf, sizeof(buf), "Mesh::Create: Storing failed, err_code = %u\n", err_code);
                fprintf(stderr, "%s", buf);
                throw (std::runtime_error(buf));
        }

}

template <class StoreStrategyList, class LoadStrategyList> uint32_t Mesh<StoreStrategyList, LoadStrategyList>::GetShapesCnt() const {
        return vMeshData.size();
}

template <class StoreStrategyList, class LoadStrategyList> uint32_t Mesh<StoreStrategyList, LoadStrategyList>::GetTrianglesCnt() const {
        uint32_t total_triangles_cnt = 0;
        for (auto item : vMeshData) {
                total_triangles_cnt += item.triangles_cnt;
        }
        return total_triangles_cnt;
}

}

