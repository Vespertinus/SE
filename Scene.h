

#ifndef __Scene_H__
#define __Scene_H__ 1

#include <VisualHelpers.h>



namespace SE {

class Scene {

  //DEBUG code ___Start___
  HELPERS::Elipse   oSmallElipse;
  HELPERS::Elipse   oBigElipse;
  //DEBUG code ___End_____

  TTexture        * pTex01;


	public:
	
	Scene();
	~Scene() throw();
	
	void Process();
	//void Load(level);

};


} //namespace SE

#include <Scene.tcc>

#endif
