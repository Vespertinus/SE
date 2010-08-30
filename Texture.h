
#ifndef __TEXTURE_H__
#define __TEXTURE_H__ 1

namespace SE {

template <class StoreStategyList, class LoadStrategyList> class Texture : public ResourceHolder<Texture<StoreStategyList, LoadStrategyList> > {

  uint16_t    width,
              height;

  /** bytes per pixel */
  //uint8_t     bpp;


  template <class TConcreateSettings> bool Create(const std::string oName, TConcreateSettings & oSettings);


  public:




};

#include <Texture.tcc>

} //namespace SE

#endif
