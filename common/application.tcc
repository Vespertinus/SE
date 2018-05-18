

namespace SE {

template <class TLoop> Application<TLoop>::Application(const SysSettings_t & oNewSettings, const typename TLoop::Settings  & oLoopSettings):
        oSettings(oNewSettings),
        oCamera(oTranspose,	oSettings.oCamSettings),
        oRunFunctor   (*this, &Application<TLoop>::Run),
        oResizeFunctor(*this, &Application<TLoop>::ResizeViewport),
        oMainWindow(oResizeFunctor, oRunFunctor, oSettings.oWindowSettings),
        oLoop(oLoopSettings, oCamera) {

                log_d("try to init OIS");

                //ResizeViewport(oSettings.oCamSettings.width, oSettings.oCamSettings.height);

                TInputManager::Instance().Initialise(oMainWindow.GetWindowID(), oSettings.oCamSettings.width, oSettings.oCamSettings.height);

                TInputManager::Instance().AddKeyListener   (&oTranspose, "Transpose");
                TInputManager::Instance().AddMouseListener (&oTranspose, "Transpose");


                Init();

                log_i("Start Loop");

                oMainWindow.Loop();

                log_i("Stop Loop");

}



template <class TLoop> Application<TLoop>::~Application() throw() { ;; }



template <class TLoop> void Application<TLoop>::Init() {

        log_d("basic OpenGL options");

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClearDepth(1.0);
        glDepthFunc(GL_LESS);
        glEnable(GL_DEPTH_TEST);
        //glShadeModel(GL_SMOOTH);
        //glLineWidth(4);
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc (GL_GREATER, 0.7);
        //glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

        oCamera.UpdateProjection();

        glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

        //glEnable(GL_LIGHTING);
}



template <class TLoop> void Application<TLoop>::ResizeViewport(const int32_t & new_width, const int32_t & new_height) {

        oSettings.oCamSettings.width 	= new_width;
        oSettings.oCamSettings.height	= (new_height) ? new_height : 1;

        oCamera.UpdateDimension(oSettings.oCamSettings.width, oSettings.oCamSettings.height);

        glViewport(0, 0, oSettings.oCamSettings.width, oSettings.oCamSettings.height);

        TInputManager::Instance().SetWindowExtents(oSettings.oCamSettings.width, oSettings.oCamSettings.height);

        oCamera.UpdateProjection();
}



template <class TLoop> void Application<TLoop>::Run() {

        glClear(oSettings.clear_flag);
        glLoadIdentity();
        glPushMatrix();
        oCamera.Adjust();

        SE::TRenderState::Instance().Reset();

        oLoop.Process();

        glPopMatrix();
        TInputManager::Instance().Capture();

        TSimpleFPS::Instance().Update();
}


} //namespace SE


