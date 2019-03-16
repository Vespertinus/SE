
#ifndef __OFF_SCREEN_APPLICATION_H__
#define __OFF_SCREEN_APPLICATION_H__ 1

// C include
#include <unistd.h>

// C++ include
#include <map>


// Internal include
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

        /** global initialization before user code (Loop)*/
        struct PreInit {

                PreInit();
        };

        SysSettings_t   oSettings;
        Camera          oCamera;
        OSMesa          oRenderingCtx;
        PreInit         oPreInit;
        TLoop		oLoop;
        DummyTransposer oTranspose;

        void Init();

        public:
        OffScreenApplication(const SysSettings_t & oNewSettings, const typename TLoop::Settings & oLoopSettings);
        ~OffScreenApplication() throw();

        void Run();
        void ResizeViewport(const int32_t new_width, const int32_t new_height);
        TLoop & GetAppLogic();

};

} //namespace SE

#ifdef SE_IMPL
#include <OffScreenApplication.tcc>
#endif

#endif

