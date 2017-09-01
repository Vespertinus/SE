
namespace SE {

template <class TLoop> OffScreenApplication<TLoop>::OffScreenApplication(const SysSettings_t & oNewSettings, const typename TLoop::Settings  & oLoopSettings):
        oSettings(oNewSettings), 
        oCamera(oTranspose,	oSettings.oCamSettings),
        oLoop(oLoopSettings, oCamera) { 

                pMesaCtx = OSMesaCreateContextExt( OSMESA_RGBA, 16, 0, 0, NULL );
                if (!pMesaCtx) {
                        throw (std::runtime_error("OffScreenApplication::OffScreenApplication: OSMesaCreateContext failed"));
                }

                oCamera.SetPos(5, 1, 1);    

                Init();

                fprintf(stderr, "OffScreenApplication::OffScreenApplication: Inited\n");

}



template <class TLoop> OffScreenApplication<TLoop>::~OffScreenApplication() throw() { 

        OSMesaDestroyContext( pMesaCtx );
}



template <class TLoop> void OffScreenApplication<TLoop>::Init() { 

        fprintf(stderr, "OffScreenApplication::Init: \n");

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);   
        glClearDepth(1.0);        
        glDepthFunc(GL_LESS);       
        glEnable(GL_DEPTH_TEST);      
        glShadeModel(GL_SMOOTH);  

        glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

        oCamera.UpdateProjection();
}



template <class TLoop> void OffScreenApplication<TLoop>::Run(std::vector<GLubyte> & vRenderBuffer) { 

        vRenderBuffer.reserve(oSettings.oCamSettings.width * oSettings.oCamSettings.height * 4 /*RGBA*/ * sizeof(GLubyte));

        if (!OSMesaMakeCurrent( pMesaCtx, 
                                &vRenderBuffer[0] , 
                                GL_UNSIGNED_BYTE, 
                                oSettings.oCamSettings.width, 
                                oSettings.oCamSettings.height) ) {
                throw(std::runtime_error("OffScreenApplication::Run: OSMesaMakeCurrent failed!\n"));
                //TODO rewrite on err code handling
        }


        glClear(oSettings.clear_flag);
        //gl clear texture buffer
        glLoadIdentity();
        glEnable(GL_TEXTURE_2D);
        glPushMatrix();
        oCamera.Adjust();

        oLoop.Process();

        glPopMatrix();
        glDisable(GL_TEXTURE_2D);

        //fill vRenderBuffer
}


} //namespace SE



