
namespace SE  { 

template <class StoreStrategyList, class LoadStrategyList> template <class TConcreateSettings> bool Texture<StoreStategyList, LoadStrategyList>::Create(const std::string oName, TConcreateSettings & oSettings) {

  typedef Loki::typeAt<typename TConcreateSettings::TSettingsList, 0> TStoreStrategySettings;
  typedef Loki::typeAt<typename TConcreateSettings::TSettingsList, 1> TLoadStrategySettings;

  typedef MP::InnerSearch<StoreStrategyList, TStoreStrategySettings>::Result TStoreStrategy;
  typedef MP::InnerSearch<LoadStrategyList,  TLoadStrategySettings >::Result TLoadStrategy;


}


}
