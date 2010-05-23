

#ifndef __Scene_H__
#define __Scene_H__ 1

namespace SE {

class Scene {


	public:
	
	Scene();
	~Scene() throw();
	
	void Process();
	//void Load(level);

};


} //namespace SE

#include <Scene.tcc>

#endif
