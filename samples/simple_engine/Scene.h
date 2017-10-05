

#ifndef __SCENE_H__
#define __SCENE_H__ 1

#include <VisualHelpers.h>



namespace SE {

class Scene {

  //DEBUG code ___Start___
  HELPERS::Elipse   oSmallElipse;
  HELPERS::Elipse   oBigElipse;
  //DEBUG code ___End_____

  TTexture        * pTex01;


	public:
  //empty settings
  struct Settings {};
	
	Scene(const Settings & oSettings, Camera & oCurCamera);
	~Scene() throw();
	
	void Process();
	//void Load(level);

};


} //namespace SE

#include "Scene.tcc"

#endif
