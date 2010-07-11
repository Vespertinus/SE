

namespace SE {

template <class TLoop> Application<TLoop>::Application(const SysSettings_t & oNewSettings, TLoop & oNewLoop):
	oSettings(oNewSettings), 
	oLoop(oNewLoop), 
	oCamera(oTranspose,	oSettings.oCamSettings),
  oRunFunctor   (*this, &Application<TLoop>::Run),
  oResizeFunctor(*this, &Application<TLoop>::ResizeViewport),
  oMainWindow(oResizeFunctor, oRunFunctor, oSettings.oWindowSettings) {

    fprintf(stderr, "Application::Application: try to init OIS\n");

    //ResizeViewport(oSettings.oCamSettings.width, oSettings.oCamSettings.height);

    TInputManager::Instance().Initialise(oMainWindow.GetWindowID(), oSettings.oCamSettings.width, oSettings.oCamSettings.height);

    TInputManager::Instance().AddKeyListener   (&oTranspose, "Transpose");
    TInputManager::Instance().AddMouseListener (&oTranspose, "Transpose");

    oCamera.SetPos(5, 1, 1);    
    //oCamera.LookAt(0, 1, 1);
    
    fprintf(stderr, "Application::Application: Start Loop\n");

    oMainWindow.Loop();
    
    fprintf(stderr, "Application::Application: Stop Loop\n");

}



template <class TLoop> Application<TLoop>::~Application() throw() { ;; }



template <class TLoop> void Application<TLoop>::Init() { 
	
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);   
	glClearDepth(1.0);        
	glDepthFunc(GL_LESS);       
	glEnable(GL_DEPTH_TEST);      
	glShadeModel(GL_SMOOTH);  
  //glLineWidth(4);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();       
	gluPerspective(oSettings.oCamSettings.fov, 
								(GLfloat)oSettings.oCamSettings.width / (GLfloat) oSettings.oCamSettings.height,
								oSettings.near_clip,
								oSettings.far_clip);

	glMatrixMode(GL_MODELVIEW);
}



template <class TLoop> void Application<TLoop>::ResizeViewport(const int32_t & new_width, const int32_t & new_height) { 

	oSettings.oCamSettings.width 	= new_width;
	oSettings.oCamSettings.height	= (new_height) ? new_height : 1;

	oCamera.UpdateDimension(oSettings.oCamSettings.width, oSettings.oCamSettings.height);
	
  glViewport(0, 0, oSettings.oCamSettings.width, oSettings.oCamSettings.height);
  
  TInputManager::Instance().SetWindowExtents(oSettings.oCamSettings.width, oSettings.oCamSettings.height);

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
	//glutSwapBuffers();
  TInputManager::Instance().Capture();
}


} //namespace SE


