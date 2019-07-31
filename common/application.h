
#ifndef __APPLICATION_H__
#define __APPLICATION_H__ 1

// C include
#include <unistd.h>

// C++ include
#include <map>

// Internal include
#include <X11Window.h>

#include <stl_extension.h>

namespace SE {

struct SysSettings_t {

        int                     clear_flag;
        bool                    grab_mouse;
        bool                    hide_mouse;

        WindowSettings          oWindowSettings;

        std::string             sResourceDir;

        SysSettings_t() :
                clear_flag(0),
                grab_mouse(true),
                hide_mouse(true)
        { ;; }
};



template <class TLoop > class Application {

        typedef STD_EXT::GeneralFunctor<Application<TLoop>, void>                   TRunFunctor;
        typedef STD_EXT::GeneralFunctor<Application<TLoop>, void, int32_t, int32_t> TResizeFunctor;
        //TODO write normal OS switch
        //#ifdef linux
        typedef X11Window<TResizeFunctor, TRunFunctor>                              TWindow;
        //#else
        //# error "unsupported OS"
        //#endif

        /** global initialization before user code (Loop)*/
        struct PreInit {

                PreInit(const SysSettings_t & oSettings, const uint32_t window_id);
        };


        SysSettings_t		oSettings;
        int                     window_id;

        TRunFunctor             oRunFunctor;
        TResizeFunctor          oResizeFunctor;

        TWindow                 oMainWindow;
        PreInit                 oPreInit;
        TLoop			oLoop;

        public:
        Application(const SysSettings_t & oNewSettings, const typename TLoop::Settings & oLoopSettings);
        ~Application() noexcept;


        public:		//TEMP
        void Init();
        //private:	//TEMP
        void ResizeViewport(const int32_t & new_width, const int32_t & new_height);
        void Run();
        TLoop & GetAppLogic();

};

} //namespace SE

#ifdef SE_IMPL
#include <application.tcc>
#endif

#endif
