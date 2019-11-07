
#ifndef __OFF_SCREEN_APPLICATION_H__
#define __OFF_SCREEN_APPLICATION_H__ 1

// C include
#include <unistd.h>

// C++ include
#include <map>


// Internal include
#include <OSMesa.h>
#include <Camera.h>

#include <stl_extension.h>

namespace SE {

struct SysSettings_t {
	int                     clear_flag;
        std::string             sResourceDir;
        std::vector<GLubyte>  & vRenderBuffer;
        uint32_t                width;
        uint32_t                height;

        SysSettings_t(
                const int new_clear_flag,
                uint32_t new_width,
                uint32_t new_height,
                std::vector<GLubyte>  & vNewRenderBuffer) :
                        clear_flag(new_clear_flag),
                        vRenderBuffer(vNewRenderBuffer),
                        width(new_width),
                        height(new_height) {
        }

        SysSettings_t(std::vector<GLubyte> & vNewRenderBuffer) : vRenderBuffer(vNewRenderBuffer) { ;; }
};



template <class TLoop > class OffScreenApplication {

        /** global initialization before user code (Loop)*/
        struct PreInit {

                PreInit();
        };

        SysSettings_t   oSettings;
        OSMesa          oRenderingCtx;
        PreInit         oPreInit;
        TLoop		oLoop;

        void Init();

        public:
        OffScreenApplication(const SysSettings_t & oNewSettings, const typename TLoop::Settings & oLoopSettings);
        ~OffScreenApplication() noexcept;

        void Run();
        void ResizeViewport(const int32_t new_width, const int32_t new_height);
        TLoop & GetAppLogic();

};

} //namespace SE

#ifdef SE_IMPL
#include <OffScreenApplication.tcc>
#endif

#endif

