
#ifndef __OFF_SCREEN_APPLICATION_H__
#define __OFF_SCREEN_APPLICATION_H__ 1

// C include
#include <unistd.h> 

// C++ include
#include <map>


// Internal include
#include <Global.h> 
#include <GlobalTypes.h>

#include <GL/osmesa.h>

#include <Camera.h>
#include <DummyTransposer.h>

#include <stl_extension.h>

namespace SE {

struct SysSettings_t {
	int                     clear_flag;
        Camera::CamSettings_t   oCamSettings;
        std::string             sResourceDir;
};



template <class TLoop > class OffScreenApplication {


        SysSettings_t   oSettings;
        Camera          oCamera;
        TLoop		oLoop;
        DummyTransposer oTranspose;
        OSMesaContext   pMesaCtx;

        void Init();

        public:
        OffScreenApplication(const SysSettings_t & oNewSettings, const typename TLoop::Settings & oLoopSettings);
        ~OffScreenApplication() throw();

        void Run(std::vector<GLubyte> & vRenderBuffer);

};

} //namespace SE

#include <OffScreenApplication.tcc>

#endif

