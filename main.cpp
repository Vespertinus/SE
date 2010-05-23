
#include <stdint.h>
#include <sys/time.h>
#include <stdio.h>

//#include <iostream>


#include <application.h>
#include <Scene.h>

//Temp code ___Start___

static SE::Application<SE::Scene> * pApp;

void DrawStub() {
	pApp->Run();
}

void ResizeStub(const int32_t new_width, const int32_t new_height) {
	pApp->ResizeViewport(new_height, new_height);
}

void InputStub(unsigned char key, const int x, const int y) {
	pApp->Input(key, x, y);
}

void MouseStub(const int x, const int y) {


}
//Temp code ___End_____


int main(int argc, char **argv) {

	SE::SysSettings_t oSettings;

	oSettings.oCamSettings.width 			= 1024;
	oSettings.oCamSettings.height			= 768;
	oSettings.oCamSettings.fov					= 45;
	oSettings.near_clip		=	0.1;
	oSettings.far_clip		= 100;
	oSettings.fullscreen	= 0;
	oSettings.clear_flag	= GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;

  try {

    SE::Scene oScene;

    //SE::Application<SE::Scene> App(oSettings, oScene);

    //Temp code ___Start___

    glutInit(&argc, argv);

    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_ALPHA | GLUT_DEPTH);

    glutInitWindowSize(oSettings.oCamSettings.width, oSettings.oCamSettings.height);

    glutInitWindowPosition(0, 0);
    
    oSettings.window_id = glutCreateWindow("Simple Engine DEMO (2010.05.13)");
    
    glutKeyboardFunc(0);
    
    pApp = new SE::Application<SE::Scene>(oSettings, oScene);

    glutDisplayFunc(&DrawStub);

    if (oSettings.fullscreen) { glutFullScreen(); }

    glutIdleFunc(&DrawStub);

    glutReshapeFunc(&ResizeStub);

    //glutKeyboardFunc(&InputStub);

    pApp->Init();

    glutMainLoop();

    delete pApp;
    //Temp code ___End_____

  }
  catch (std::exception & ex) {
    fprintf(stderr, "main: exception catched = %s\n", ex.what());
  }
  catch(...) {
    fprintf(stderr, "main: unknown exception catched\n");
  }

	return 0;
}
