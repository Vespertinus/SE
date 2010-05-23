

#ifndef __Scene_H__
#define __Scene_H__ 1

namespace SD {

class Scene {


	public:
	
	Scene();
	~Scene() throw();
	
	void Process();
	//void Load(level);

};


} //namespace SD

#include <Scene.tcc>

#endif
