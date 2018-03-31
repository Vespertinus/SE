
#ifndef __TEXTURE_H__
#define __TEXTURE_H__ 1

namespace SE {

typedef std::pair<uint16_t, uint16_t> Dimensions;



template <class StoreStrategyList, class LoadStrategyList> class Texture : public ResourceHolder {

        typedef typename  StoreStrategyList::Head TDefaultStoreStrategy;
        typedef typename  LoadStrategyList::Head  TDefaultLoadStrategy;

        Dimensions  oDimensions;
        uint32_t    id;


        template <class TStoreStrategySettings,  class TLoadStrategySettings> void
                Create(const std::string oName,
                       const TStoreStrategySettings & oStoreStrategySettings,
                       const TLoadStrategySettings & oLoadStrategySettings);

        template <class TStoreStrategySettings > void
                CreateHelper(const std::string oName,
                             const TStoreStrategySettings & oStoreStrategySettings);

        public:

        template <class StoreStrategy, class LoadStrategy> struct TSettings :
                public SettingsType <LOKI_TYPELIST_2(typename StoreStrategy::Settings, typename LoadStrategy::Settings) > {  };

        template <class TStoreStrategySettings,  class TLoadStrategySettings>
                Texture(const std::string oName,
                                const rid_t new_rid,
                                const TStoreStrategySettings & oStoreStrategySettings,
                                const TLoadStrategySettings & oLoadStrategySettings);

        Texture(const std::string oName,
                        const rid_t new_rid);

        template <class TConcreateSettings, std::enable_if_t< MP::InnerContain<StoreStrategyList, TConcreateSettings>::value, TConcreateSettings> * = nullptr>
                Texture(const std::string & oName,
                        const rid_t new_rid,
                        const TConcreateSettings & oSettings);

        template <class TConcreateSettings, std::enable_if_t< MP::InnerContain<LoadStrategyList, TConcreateSettings>::value, TConcreateSettings> * = nullptr>
                Texture(const std::string & oName,
                        const rid_t new_rid,
                        const TConcreateSettings & oSettings);

        ~Texture() noexcept;

        uint32_t GetID() const;
        const Dimensions & GetDimensions() const;


};

} //namespace SE

#include <Texture.tcc>

#endif
