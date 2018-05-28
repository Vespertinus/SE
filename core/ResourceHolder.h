
#ifndef __RESOURCE_HOLDER_H_
#define __RESOURCE_HOLDER_H_ 1

namespace SE {

/** resource id type */
typedef uint64_t rid_t;

/*template <class Resource>*/ class ResourceHolder /*: public Resource*/ {

  protected:

  uint32_t      size;
  rid_t         rid;
  std::string   sName;

  private:
  template <class T> struct Holder : public T {};

  public:
  ResourceHolder(const rid_t new_rid, const std::string_view sNewName) : rid(new_rid), sName(sNewName) { ;; }
  template <class SettingsList> struct SettingsType : public Loki::GenScatterHierarchy <SettingsList, Holder> {
    typedef SettingsList TSettingsList;
  };
  template <class S, class TConcreateSettings> static S & Settings(TConcreateSettings & oSettings) {
    return Loki::Field<S>(oSettings);
  }
  uint32_t Size() const { return size ; }
  rid_t RID() const { return rid; }
  const std::string & Name() const { return sName; }
};


} //namespace SE


#endif
