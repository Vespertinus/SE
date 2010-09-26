
#include <stdint.h>
#include <sys/time.h>
#include <stdio.h>

//#include <iostream>


#include <application.h>
#include <Scene.h>



int main(int argc, char **argv) {

	SE::SysSettings_t oSettings;

	oSettings.oCamSettings.width 					= 1024;
	oSettings.oCamSettings.height					= 768;
	oSettings.oCamSettings.fov						= 45;
	oSettings.oCamSettings.near_clip			=	0.1;
	oSettings.oCamSettings.far_clip				= 1000;
  oSettings.oCamSettings.projection     = SE::Camera::uPERSPECTIVE;

	oSettings.clear_flag			            = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;

  oSettings.oWindowSettings.width       = oSettings.oCamSettings.width;
  oSettings.oWindowSettings.height      = oSettings.oCamSettings.height;
  oSettings.oWindowSettings.bpp         = 24;
	oSettings.oWindowSettings.fullscreen  = 0;
  oSettings.oWindowSettings.title       = "Simple Engine DEMO (2010.05.13)";
  oSettings.sResourceDir                = "resource/";



  try {

    SE::Application<SE::Scene> App(oSettings, SE::Scene::Settings());
  }
  catch (std::exception & ex) {
    fprintf(stderr, "main: exception catched = %s\n", ex.what());
  }
  catch(...) {
    fprintf(stderr, "main: unknown exception catched\n");
  }

	return 0;
}
