
#ifndef __TEXTURE_H__
#define __TEXTURE_H__ 1

namespace SE {

typedef std::pair<uint16_t, uint16_t> Dimensions;



template <class StoreStrategyList, class LoadStrategyList> class Texture : public ResourceHolder/*<Texture<StoreStrategyList, LoadStrategyList> >*/ {

  typedef typename  StoreStrategyList::Head TDefaultStoreStrategy;
  typedef typename  LoadStrategyList::Head  TDefaultLoadStrategy;

  Dimensions  oDimensions;
  uint32_t    id;


  //template <class TConcreateSettings> void Create(const std::string oName, TConcreateSettings & oSettings);
  template <class TStoreStrategySettings,  class TLoadStrategySettings> void 
    Create(const std::string oName, 
           const TStoreStrategySettings & oStoreStrategySettings, 
           const TLoadStrategySettings & oLoadStrategySettings);
  
  public:

  template <class StoreStrategy, class LoadStrategy> struct TSettings : 
    public SettingsType <LOKI_TYPELIST_2(typename StoreStrategy::Settings, typename LoadStrategy::Settings) > {  };

  //template <class TConcreateSettings> Texture(const std::string oName, TConcreateSettings & oSettings, const rid_t new_rid);
  template <class TStoreStrategySettings,  class TLoadStrategySettings> 
    Texture(const std::string oName, 
            const rid_t new_rid,
            const TStoreStrategySettings & oStoreStrategySettings, 
            const TLoadStrategySettings & oLoadStrategySettings);
  
    Texture(const std::string oName, 
            const rid_t new_rid);
  ~Texture() throw();

  uint32_t GetID() const;
  const Dimensions & GetDimensions() const;


};

} //namespace SE

#include <Texture.tcc>

#endif
