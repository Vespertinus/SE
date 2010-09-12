
namespace SE  { 

template <class StoreStrategyList, class LoadStrategyList> 
  template <class TStoreStrategySettings,  class TLoadStrategySettings> 
    Texture<StoreStrategyList, LoadStrategyList>::Texture(
        const std::string oName, 
        const rid_t new_rid,
        const TStoreStrategySettings & oStoreStrategySettings, 
        const TLoadStrategySettings & oLoadStrategySettings) :
          ResourceHolder(new_rid), oDimensions(0, 0), id(0) {
          
  Create(oName, oStoreStrategySettings, oLoadStrategySettings);
}

template <class StoreStrategyList, class LoadStrategyList> 
    Texture<StoreStrategyList, LoadStrategyList>::Texture(
        const std::string oName, 
        const rid_t new_rid) : 
          ResourceHolder(new_rid), oDimensions(0, 0), id(0) {
  

  Create(oName, typename TDefaultStoreStrategy::Settings(), typename TDefaultLoadStrategy::Settings());
}


template <class StoreStrategyList, class LoadStrategyList> Texture<StoreStrategyList, LoadStrategyList>::~Texture() throw() { ;; }



template <class StoreStrategyList, class LoadStrategyList> 
  template <class TStoreStrategySettings,  class TLoadStrategySettings> void 
    Texture<StoreStrategyList, LoadStrategyList>::Create(const std::string oName, 
                                                  const TStoreStrategySettings & oStoreStrategySettings, 
                                                  const TLoadStrategySettings & oLoadStrategySettings) {
 
  //typedef typename Loki::TL::TypeAt<typename TConcreateSettings::TSettingsList, 0 >::Result TStoreStrategySettings;
  //typedef typename Loki::TL::TypeAt<typename TConcreateSettings::TSettingsList, 1 >::Result TLoadStrategySettings;

  typedef typename MP::InnerSearch<StoreStrategyList, TStoreStrategySettings>::Result TStoreStrategy;
  typedef typename MP::InnerSearch<LoadStrategyList,  TLoadStrategySettings >::Result TLoadStrategy;

  TextureStock    oTextureStock;
  ret_code_t      err_code;

  //TLoadStrategy   oLoadStrategy(Settings<TLoadStrategySettings, TConcreateSettings>(oSettings));
  TLoadStrategy   oLoadStrategy(oLoadStrategySettings);

  //TStoreStrategy  oStoreStrategy(Settings<TStoreStrategySettings, TConcreateSettings>(oSettings));
  TStoreStrategy  oStoreStrategy(oStoreStrategySettings);

  err_code = oLoadStrategy.Load(oName, oTextureStock);
  if (err_code) {
    char buf[256];
    snprintf(buf, sizeof(buf), "Texture::Create: Loading failed, err_code = %u\n", err_code);
    fprintf(stderr, "%s", buf);
    throw (std::runtime_error(buf));
  }

  err_code = oStoreStrategy.Store(oTextureStock, id);
  if (err_code) {
    char buf[256];
    snprintf(buf, sizeof(buf), "Texture::Create: Storing failed, err_code = %u\n", err_code);
    fprintf(stderr, "%s", buf);
    throw (std::runtime_error(buf));
  }
  
  oDimensions.first   = oTextureStock.width;
  oDimensions.second  = oTextureStock.height;
}



template <class StoreStrategyList, class LoadStrategyList> uint32_t Texture<StoreStrategyList, LoadStrategyList>::GetID() const {
  return id;
}
 

template <class StoreStrategyList, class LoadStrategyList> const Dimensions & Texture<StoreStrategyList, LoadStrategyList>::GetDimensions() const {

  return oDimensions;
}

/*
template <class StoreStrategyList, class LoadStrategyList> uint32_t Texture<StoreStrategyList, LoadStrategyList>::Size() const {
  
  //TODO may be width * height * bpp
  return 0;
}
*/
}
