
#ifndef __TEXTURE_H__
#define __TEXTURE_H__ 1

namespace SE {

typedef std::pair<uint32_t, uint32_t> Dimensions;

//TODO store settings inside texture for updating later;


template <class StoreStrategyList, class LoadStrategyList> class Texture : public ResourceHolder {

        typedef typename  StoreStrategyList::Head TDefaultStoreStrategy;
        typedef typename  LoadStrategyList::Head  TDefaultLoadStrategy;

        Dimensions  oDimensions;
        uint32_t    id;
        uint32_t    gl_type;
        //format compressed type | uncompressed
        //gl_format GL_RGBA, GL_R32 etc
        //destination (unit index) from settings inside material


        template <class TStoreStrategySettings,  class TLoadStrategySettings> void
                Create(const TStoreStrategySettings & oStoreStrategySettings,
                       const TLoadStrategySettings & oLoadStrategySettings);

        template <class TStoreStrategySettings > void
                CreateHelper(const TStoreStrategySettings & oStoreStrategySettings);

        public:

        template <class StoreStrategy, class LoadStrategy> struct TSettings :
                public SettingsType <LOKI_TYPELIST_2(typename StoreStrategy::Settings, typename LoadStrategy::Settings) > {  };

        template <class TStoreStrategySettings,  class TLoadStrategySettings>
                Texture(const std::string sName,
                        const rid_t new_rid,
                        const TStoreStrategySettings & oStoreStrategySettings,
                        const TLoadStrategySettings & oLoadStrategySettings);

        Texture(const std::string sName,
                        const rid_t new_rid);

        template <class TConcreateSettings, std::enable_if_t< MP::InnerContain<StoreStrategyList, TConcreateSettings>::value, TConcreateSettings> * = nullptr>
                Texture(const std::string & sName,
                        const rid_t new_rid,
                        const TConcreateSettings & oSettings);

        template <class TConcreateSettings, std::enable_if_t< MP::InnerContain<LoadStrategyList, TConcreateSettings>::value, TConcreateSettings> * = nullptr>
                Texture(const std::string & sName,
                        const rid_t new_rid,
                        const TConcreateSettings & oSettings);

        template <class TStoreStrategySettings = typename TDefaultStoreStrategy::Settings>
                Texture(const std::string sName,
                        const rid_t new_rid,
                        const TextureStock & oTextureStock,
                        const TStoreStrategySettings & oStoreStrategySettings = TStoreStrategySettings());

        ~Texture() noexcept;

        uint32_t GetID() const;
        uint32_t Type() const;
        const Dimensions & GetDimensions() const;
        //Bind() const;


};

} //namespace SE

#include <Texture.tcc>

#endif
