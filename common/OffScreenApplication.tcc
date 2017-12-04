
namespace SE {

template <class TLoop> OffScreenApplication<TLoop>::OffScreenApplication(const SysSettings_t & oNewSettings, const typename TLoop::Settings  & oLoopSettings):
        oSettings(oNewSettings), 
        oCamera(oTranspose, oSettings.oCamSettings),
        oRenderingCtx(WindowSettings { 
                        OSMESA_BGRA, 
                        16, 
                        0, 
                        0, 
                        oSettings.oCamSettings.width, 
                        oSettings.oCamSettings.height,
                        oSettings.vRenderBuffer } ),
        oLoop(oLoopSettings, oCamera) { 
                

                Init();

                log_i("Inited");

}



template <class TLoop> OffScreenApplication<TLoop>::~OffScreenApplication() throw() { 

}



template <class TLoop> void OffScreenApplication<TLoop>::Init() { 

        log_i("app init");

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);   
        glClearDepth(1.0);        
        glDepthFunc(GL_LESS);       
        glEnable(GL_DEPTH_TEST);      
        glShadeModel(GL_SMOOTH);  

        glEnable(GL_ALPHA_TEST);
        glAlphaFunc (GL_GREATER, 0.7);

        oCamera.UpdateProjection();
}



template <class TLoop> void OffScreenApplication<TLoop>::Run() {

        CalcDuration oLoopDuration;

        glClear(oSettings.clear_flag);
        glLoadIdentity();
        glPushMatrix();

        oCamera.Adjust();

        oLoop.Process();

        glPopMatrix();
        glFinish();

        log_i("duration = {} ms", oLoopDuration.Get());
}


} //namespace SE


