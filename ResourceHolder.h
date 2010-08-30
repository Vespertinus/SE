
#ifndef __RESOURCE_HOLDER_H_
#define __RESOURCE_HOLDER_H_ 1

namespace SE {

template <class Resource> struct ResourceHolder /*: public Resource*/ {

  uint32_t  size;

  private:
  template <class T> struct Holder : public T {};

  public:
  //template <class StoreStrategy, class LoadStrategy> struct SettingsType : publ
  template <class SettingsList> struct SettingsType : public Loki::GenScatterHierarchy <SettingsList, Holder> {
    typedef SettingsList TSettingsList;
  };
  template <class S, class TConcreateSettings> static S & Settings(TConcreateSettings & oSettings) {
    return Loki::Field<S>(oSettings);
  }

};


} //namespace SE


#endif
