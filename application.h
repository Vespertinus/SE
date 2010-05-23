

#ifndef __APPLICATION_H__
#define __APPLICATION_H__ 1

#include <GL/glut.h>
#include <GL/gl.h>	
#include <GL/glu.h>	
#include <unistd.h> 

#include <Camera.h>
#include <FlyTransposer.h>

namespace SD {

struct SysSettings_t {

	//int32_t 	width;
	//int32_t 	height;
	//float			fov;
	float 		near_clip;
	float			far_clip;
	bool			fullscreen;
	int				clear_flag;

	CamSettings_t oCamSettings;

};



template <class TLoop > class Application {

	SysSettings_t						oSettings;
	TLoop									&	oLoop;
	int											window_id;
	FlyTransposer						oTranspose;
	Camera								 	oCamera;

	public:
	Application(const SysSettings_t & oNewSettings, TLoop & oNewLoop);
	~Application() throw();

	//template <class T> Run(T & oLoop);

	//void AddViewPort

	//void UpdateSettings(const SysSettings_t & oNewSettings);
	//void Resize(const SysSettings_t & settings, const uint32_t uViewportNum = 0);

	private:

	public:		//TEMP
	void Init();
	//private:	//TEMP
	void ResizeViewport(const int32_t new_width, const int32_t new_height);
	void Input(unsigned char key, const int x, const int y);
	void Run();

};

} //namespace SD

#include <application.tcc>

#endif
