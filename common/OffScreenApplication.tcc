
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

                fprintf(stderr, "OffScreenApplication::OffScreenApplication: Inited\n");

}



template <class TLoop> OffScreenApplication<TLoop>::~OffScreenApplication() throw() { 

}



template <class TLoop> void OffScreenApplication<TLoop>::Init() { 

        fprintf(stderr, "OffScreenApplication::Init: \n");

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);   
        glClearDepth(1.0);        
        glDepthFunc(GL_LESS);       
        glEnable(GL_DEPTH_TEST);      
        glShadeModel(GL_SMOOTH);  

        glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
        
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_NORMAL_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);

        oCamera.UpdateProjection();
}



template <class TLoop> void OffScreenApplication<TLoop>::Run() { 

        glClear(oSettings.clear_flag);
        glLoadIdentity();
        glPushMatrix();

        oCamera.Adjust();

        oLoop.Process();

        glPopMatrix();
        glFinish();
}


} //namespace SE



