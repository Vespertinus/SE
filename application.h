

#ifndef __APPLICATION_H__
#define __APPLICATION_H__ 1

// OpenGL include
#include <GL/glut.h>
#include <GL/gl.h>	
#include <GL/glu.h>	

// C include
#include <unistd.h> 

// C++ include
#include <map>

// Loki include
#include <Singleton.h>

// Internal include
#include <InputManager.h>
#include <Camera.h>
#include <FlyTransposer.h>
#include <X11Window.h>



namespace SE {

struct SysSettings_t {

	//int32_t 	width;
	//int32_t 	height;
	//float			fov;
	float 		near_clip;
	float			far_clip;
	bool			fullscreen;
	int				clear_flag;

  //TEMP ___Start___
  uint32_t  window_id;
  //TEMP ___End_____

	CamSettings_t oCamSettings;

};



template <class TLoop > class Application {

  typedef Loki::SingletonHolder<InputManager> TInputManager;

	SysSettings_t						oSettings;
	TLoop									&	oLoop;
	int											window_id;
	FlyTransposer						oTranspose;
	Camera								 	oCamera;
  //TInputManager           oInputManager;

	public:
	Application(const SysSettings_t & oNewSettings, TLoop & oNewLoop);
	~Application() throw();

	//template <class T> Run(T & oLoop);

	//void AddViewPort

	//void UpdateSettings(const SysSettings_t & oNewSettings);
	//void Resize(const SysSettings_t & settings, const uint32_t uViewportNum = 0);

	public:		//TEMP
	void Init();
	//private:	//TEMP
	void ResizeViewport(const int32_t new_width, const int32_t new_height);
	void Input(unsigned char key, const int x, const int y);
	void Run();

};

} //namespace SE

#include <application.tcc>

#endif
