

namespace SE {

template <class TLoop> Application<TLoop>::Application(const SysSettings_t & oNewSettings, const typename TLoop::Settings  & oLoopSettings):
	oSettings(oNewSettings), 
	oCamera(oTranspose,	oSettings.oCamSettings),
  oRunFunctor   (*this, &Application<TLoop>::Run),
  oResizeFunctor(*this, &Application<TLoop>::ResizeViewport),
  oMainWindow(oResizeFunctor, oRunFunctor, oSettings.oWindowSettings),
	oLoop(oLoopSettings) { 

    fprintf(stderr, "Application::Application: try to init OIS\n");

    //ResizeViewport(oSettings.oCamSettings.width, oSettings.oCamSettings.height);

    TInputManager::Instance().Initialise(oMainWindow.GetWindowID(), oSettings.oCamSettings.width, oSettings.oCamSettings.height);

    TInputManager::Instance().AddKeyListener   (&oTranspose, "Transpose");
    TInputManager::Instance().AddMouseListener (&oTranspose, "Transpose");

    oCamera.SetPos(5, 1, 1);    
    //oCamera.LookAt(0, 1, 1);

    Init();
    
    fprintf(stderr, "Application::Application: Start Loop\n");

    oMainWindow.Loop();
    
    fprintf(stderr, "Application::Application: Stop Loop\n");

}



template <class TLoop> Application<TLoop>::~Application() throw() { ;; }



template <class TLoop> void Application<TLoop>::Init() { 

  fprintf(stderr, "Application::Init: \n");

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

  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

  //glEnable(GL_LIGHTING);
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
  glEnable(GL_TEXTURE_2D);
	glPushMatrix();
	oCamera.Adjust();

	oLoop.Process();

	glPopMatrix();
  glDisable(GL_TEXTURE_2D);
	//glutSwapBuffers();
  TInputManager::Instance().Capture();

  TSimpleFPS::Instance().Update();
}


} //namespace SE


