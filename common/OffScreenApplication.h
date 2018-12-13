
#ifndef __OFF_SCREEN_APPLICATION_H__
#define __OFF_SCREEN_APPLICATION_H__ 1

// C include
#include <unistd.h>

// C++ include
#include <map>


// Internal include
#include <Global.h>
#include <GlobalTypes.h>

#include <OSMesa.h>
#include <Camera.h>
#include <DummyTransposer.h>

#include <stl_extension.h>

namespace SE {

struct SysSettings_t {
	int                     clear_flag;
        Camera::CamSettings_t   oCamSettings;
        std::string             sResourceDir;
        std::vector<GLubyte>  & vRenderBuffer;

        SysSettings_t(std::vector<GLubyte> & vNewRenderBuffer) : vRenderBuffer(vNewRenderBuffer) { ;; }
};



template <class TLoop > class OffScreenApplication {


        SysSettings_t   oSettings;
        Camera          oCamera;
        OSMesa          oRenderingCtx;
        PreInit         oPreInit;
        TLoop		oLoop;
        DummyTransposer oTranspose;

        /** global initialization before user code (Loop)*/
        struct PreInit {

                PreInit();
        };
        void Init();

        public:
        OffScreenApplication(const SysSettings_t & oNewSettings, const typename TLoop::Settings & oLoopSettings);
        ~OffScreenApplication() throw();

        void Run();

};

} //namespace SE

#ifdef SE_IMPL
#include <OffScreenApplication.tcc>
#endif

#endif

