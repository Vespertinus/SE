

#ifndef __APPLICATION_H__
#define __APPLICATION_H__ 1

// OpenGL include
//#include <GL/glut.h>
//#include <GL/gl.h>	
//#include <GL/glu.h>	

// C include
#include <unistd.h> 

// C++ include
#include <map>

// Loki include
//#include <Singleton.h>

// Internal include
#include <Global.h> 
#include <GlobalTypes.h>

#include <InputManager.h>
#include <Camera.h>
#include <FlyTransposer.h>
#include <X11Window.h>

#include <stl_extension.h>

namespace SE {

struct SysSettings_t {

	//int32_t 	width;
	//int32_t 	height;
	//float			fov;
	//bool			fullscreen;
	int				                clear_flag;

  Camera::CamSettings_t     oCamSettings;
  WindowSettings            oWindowSettings;

  std::string               sResourceDir;
};



template <class TLoop > class Application {

  //typedef Loki::SingletonHolder<InputManager>                                 TInputManager;
  //typedef std::mem_fun_t<void, Application<TLoop> >                           TRunFunctor;
  //typedef STD_EXT::mem_fun2_t<void, Application<TLoop>, int32_t, int32_t>     TResizeFunctor;
  typedef STD_EXT::GeneralFunctor<Application<TLoop>, void>                   TRunFunctor;
  typedef STD_EXT::GeneralFunctor<Application<TLoop>, void, int32_t, int32_t> TResizeFunctor;
//TODO write normal OS switch
//#ifdef linux  
  typedef X11Window<TResizeFunctor, TRunFunctor>                              TWindow;
//#else  
//# error "unsupported OS"
//#endif


	SysSettings_t						oSettings;
	int											window_id;
	FlyTransposer						oTranspose;
	Camera								 	oCamera;

  TRunFunctor             oRunFunctor;
  TResizeFunctor          oResizeFunctor;

  TWindow                 oMainWindow;
	TLoop										oLoop;

	public:
	Application(const SysSettings_t & oNewSettings, const typename TLoop::Settings & oLoopSettings);
	~Application() throw();

	//template <class T> Run(T & oLoop);

	//void AddViewPort

	//void UpdateSettings(const SysSettings_t & oNewSettings);
	//void Resize(const SysSettings_t & settings, const uint32_t uViewportNum = 0);

	public:		//TEMP
	void Init();
	//private:	//TEMP
	void ResizeViewport(const int32_t & new_width, const int32_t & new_height);
	void Run();

};

} //namespace SE

#include <application.tcc>

#endif
