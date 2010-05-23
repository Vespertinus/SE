

namespace SE {

template <class TLoop> Application<TLoop>::Application(const SysSettings_t & oNewSettings, TLoop & oNewLoop):
	oSettings(oNewSettings), 
	oLoop(oNewLoop), 
	oCamera(oTranspose,	oSettings.oCamSettings) {

    TInputManager::Instance().Initialise(oSettings.window_id, oSettings.oCamSettings.width, oSettings.oCamSettings.height);

    TInputManager::Instance().AddKeyListener   (&oTranspose, "Transpose");
    TInputManager::Instance().AddMouseListener (&oTranspose, "Transpose");

/*
		glutInit(0, NULL);

		glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_ALPHA | GLUT_DEPTH);

		glutInitWindowSize(oSettings.width, oSettings.height);

		glutInitWindowPosition(0, 0);

		window_id = glutCreateWindow("Simple Engine DEMO (2010.05.13)");

		typedef void (Application<TLoop>::*Ptr_t)();

		//glutDisplayFunc( ((this)->*(  &Application<TLoop>::*Run)) );

		Ptr_t ptr = &Application<TLoop>::Run;

		glutDisplayFunc( ((this)->*( ptr)) );

		if (oSettings.fullscreen) { glutFullScreen(); }

		glutIdleFunc(&Run);

		glutReshapeFunc(&ResizeViewport);

		glutKeyboardFunc(&Input);

		Init();
		*/

}



template <class TLoop> Application<TLoop>::~Application() throw() { ;; }



template <class TLoop> void Application<TLoop>::Init() { 
	
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);   
	glClearDepth(1.0);        
	glDepthFunc(GL_LESS);       
	glEnable(GL_DEPTH_TEST);      
	glShadeModel(GL_SMOOTH);     
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();       
	gluPerspective(oSettings.oCamSettings.fov, 
								(GLfloat)oSettings.oCamSettings.width / (GLfloat) oSettings.oCamSettings.height,
								oSettings.near_clip,
								oSettings.far_clip);

	glMatrixMode(GL_MODELVIEW);
}



template <class TLoop> void Application<TLoop>::ResizeViewport(const int32_t new_width, const int32_t new_height) { 

	oSettings.oCamSettings.width 	= new_width;
	oSettings.oCamSettings.height	= (new_height) ? new_height : 1;

	oCamera.UpdateDimension(oSettings.oCamSettings.width, oSettings.oCamSettings.height);

  TInputManager::Instance().SetWindowExtents(oSettings.oCamSettings.width, oSettings.oCamSettings.height);
	
	glViewport(0, 0, oSettings.oCamSettings.width, oSettings.oCamSettings.height);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	
	gluPerspective(oSettings.oCamSettings.fov, 
								(GLfloat)oSettings.oCamSettings.width / (GLfloat) oSettings.oCamSettings.height,
								oSettings.near_clip,
								oSettings.far_clip);

	glMatrixMode(GL_MODELVIEW);
}



template <class TLoop> void Application<TLoop>::Run() { 

	glClear(oSettings.clear_flag);
	//gl clear texture buffer
	glLoadIdentity();
	glPushMatrix();
	oCamera.Adjust();

	oLoop.Process();

	glPopMatrix();
	glutSwapBuffers();
  TInputManager::Instance().Capture();
}



template <class TLoop> void Application<TLoop>::Input(unsigned char key, int x, int y) {

	//usleep(10);

	if (key == 27 /*ESCAPE*/) {
		//glutDestroyWindow(window_id);
		exit(0);
	}

	oTranspose.Operate(key, x, y);

}


} //namespace SE


