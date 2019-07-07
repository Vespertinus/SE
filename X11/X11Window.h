

#ifndef __X11WINDOW_H__
#define __X11WINDOW_H__ 1

// OpenGL include
#include <GL/glx.h>
#include <GL/gl.h>

// X11 include
#include <X11/extensions/xf86vmode.h>
#include <X11/keysym.h>



namespace SE {

struct WindowSettings {

        int32_t     width,
                    height;
        int32_t     bpp;
        bool        fullscreen;
        std::string title;
};

template <class ResizeHandler,  class DrawHandler> class X11Window {

        Display             * display;
        Window                window;
        GLXContext            glx_context;
        XSetWindowAttributes  wnd_attr;
        XF86VidModeModeInfo   video_mode;
        int32_t               screen;

        WindowSettings        oSettings;

        ResizeHandler      & oResizeHandler;
        DrawHandler        & oDrawHandler;


        void CreateWindow();
        void DestroyWindow();

        public:

        // resize handler, draw handler, settings
        X11Window(ResizeHandler & oNewResizeHandler, DrawHandler & oNewDrawHandler, const WindowSettings & oSettings);
        ~X11Window() throw();

        uint32_t GetWindowID() const;
        void Loop();

};

} // namespace SE

#include <X11Window.tcc>

#endif
