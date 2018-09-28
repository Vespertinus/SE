
#include <string.h>
#include <GLUtil.h>

namespace SE {

template <class ResizeHandler,  class DrawHandler> X11Window<ResizeHandler, DrawHandler>::X11Window(
                ResizeHandler & oNewResizeHandler,
                DrawHandler & oNewDrawHandler,
                const WindowSettings & oNewSettings) :
        display(0),
        window(0),
        glx_context(0),
        screen(0),
        oSettings(oNewSettings),
        oResizeHandler(oNewResizeHandler),
        oDrawHandler(oNewDrawHandler) {

                CreateWindow();

                log_d("Created");
}



//TODO error handling
template <class ResizeHandler,  class DrawHandler> void X11Window<ResizeHandler, DrawHandler>::CreateWindow() {

        int                   video_mode_major,
        video_mode_minor;	
        XF86VidModeModeInfo **modes;
        int                   mode_num;
        int                   best_mode = -1;
        XVisualInfo         * visual;
        Colormap              color_map;
        int                   dpy_width,
                              dpy_height;
        Atom                  wm_delete;

        display = XOpenDisplay(0);
        if (!display) {
                log_e("can't open XDisplay");
                exit (-1);
        }

        screen  = DefaultScreen(display);
        XF86VidModeQueryVersion(display, &video_mode_major, &video_mode_minor);

        XF86VidModeGetAllModeLines(display, screen, &mode_num, &modes);

        video_mode = *modes[0];

        typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
        glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 0;
        glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc) glXGetProcAddressARB( (const GLubyte *) "glXCreateContextAttribsARB" );

        GLint glxAttribs[] = {
                GLX_X_RENDERABLE    , True,
                GLX_DRAWABLE_TYPE   , GLX_WINDOW_BIT,
                GLX_RENDER_TYPE     , GLX_RGBA_BIT,
                GLX_X_VISUAL_TYPE   , GLX_TRUE_COLOR,
                GLX_RED_SIZE        , 8,
                GLX_GREEN_SIZE      , 8,
                GLX_BLUE_SIZE       , 8,
                GLX_ALPHA_SIZE      , 8,
                GLX_DEPTH_SIZE      , 24,
                GLX_STENCIL_SIZE    , 8,
                GLX_DOUBLEBUFFER    , True,
                None
        };

        int fbcount;
        GLXFBConfig * fbc = glXChooseFBConfig(display, screen, glxAttribs, &fbcount);
        if (fbc == 0) {
                XCloseDisplay(display);
                throw(std::runtime_error("Failed to retrieve framebuffer."));
        }

        int best_fbc = -1, worst_fbc = -1, best_num_samp = -1, worst_num_samp = 999;
        for (int i = 0; i < fbcount; ++i) {
                XVisualInfo *vi = glXGetVisualFromFBConfig( display, fbc[i] );
                if ( vi != 0) {
                        int samp_buf, samples;
                        glXGetFBConfigAttrib( display, fbc[i], GLX_SAMPLE_BUFFERS, &samp_buf );
                        glXGetFBConfigAttrib( display, fbc[i], GLX_SAMPLES       , &samples  );

                        if ( best_fbc < 0 || (samp_buf && samples > best_num_samp) ) {
                                best_fbc = i;
                                best_num_samp = samples;
                        }
                        if ( worst_fbc < 0 || !samp_buf || samples < worst_num_samp )
                                worst_fbc = i;
                        worst_num_samp = samples;
                }
                XFree( vi );
        }
        GLXFBConfig bestFbc = fbc[ best_fbc ];
        XFree( fbc );

        visual = glXGetVisualFromFBConfig( display, bestFbc );
        if (visual == 0) {
                XCloseDisplay(display);
                throw(std::runtime_error("Could not create correct visual window."));
        }
        log_d("visual: 0x{:x}, default screen: {}, screen: {}", visual->visualid, screen, visual->screen);

        int context_attribs[] = {
                GLX_CONTEXT_MAJOR_VERSION_ARB,  3,
                GLX_CONTEXT_MINOR_VERSION_ARB,  3,
                GLX_CONTEXT_FLAGS_ARB,          GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
                GLX_CONTEXT_PROFILE_MASK_ARB,   GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
                None
        };

        glx_context = glXCreateContextAttribsARB( display, bestFbc, 0, true, context_attribs );


        color_map             = XCreateColormap(display, RootWindow(display, visual->screen), visual->visual, AllocNone);
        wnd_attr.colormap     = color_map;
        wnd_attr.border_pixel = 0;
        wnd_attr.cursor       = None;

        if (oSettings.fullscreen) {

                log_d("try to set fullscreen mode");

                for (int i = 0; i < mode_num; ++i) {
                        if ((modes[i]->hdisplay == oSettings.width) && (modes[i]->vdisplay == oSettings.height)) { best_mode = i; break; }
                }
                if (best_mode == -1) {
                        log_e("target mode unsupported, width = {}, height = {}", oSettings.width, oSettings.height);
                        exit(-1);
                }

                XF86VidModeSwitchToMode(display, screen, modes[best_mode]);
                XF86VidModeSetViewPort(display, screen, 0, 0);

                dpy_width   = modes[best_mode]->hdisplay;
                dpy_height  = modes[best_mode]->vdisplay;
                XFree(modes);

                wnd_attr.override_redirect = true;
                wnd_attr.event_mask = ExposureMask | StructureNotifyMask;

                window = XCreateWindow( display,
                                RootWindow(display, visual->screen),
                                0,
                                0,
                                dpy_width,
                                dpy_height,
                                0,
                                visual->depth,
                                InputOutput,
                                visual->visual,
                                CWBorderPixel | CWColormap | CWEventMask | CWOverrideRedirect,
                                &wnd_attr);

                XWarpPointer(display, None, window, 0, 0, 0, 0, 0, 0);
                XMapRaised(display, window);

                XGrabKeyboard (display, window, true, GrabModeAsync, GrabModeAsync, CurrentTime);
                XGrabPointer  (display, window, true, ButtonPressMask, GrabModeAsync, GrabModeAsync, window, None, CurrentTime);
        }
        else {
                log_d("try to set window mode");

                wnd_attr.event_mask = ExposureMask | StructureNotifyMask;
                wnd_attr.override_redirect = true;

                window = XCreateWindow( display,
                                RootWindow(display, visual->screen),
                                0,
                                0,
                                oSettings.width,
                                oSettings.height,
                                0,
                                visual->depth,
                                InputOutput,
                                visual->visual,
                                CWBorderPixel | CWColormap | CWEventMask,
                                &wnd_attr);

                wm_delete = XInternAtom(display, "WM_DELETE_WINDOW", true);

                XSetWMProtocols(display, window, &wm_delete, 1);
                XSetStandardProperties(display, window, oSettings.title.c_str(), oSettings.title.c_str(), None, NULL, 0, NULL);
                XMapRaised(display, window);
        }


        glXMakeCurrent(display, window, glx_context);

        bool direct_render = glXIsDirect(display, glx_context);

        log_i("Direct Rendering: {}", direct_render ? "Yes" : "No");
        if (!direct_render) { abort(); }

        log_i("Running in {} mode", oSettings.fullscreen ? "fullscreen" : "window");

        oResizeHandler(oSettings.width, oSettings.height);
}


//THINK do we need to get input events here
template <class ResizeHandler,  class DrawHandler> void X11Window<ResizeHandler, DrawHandler>::Loop() {

        XEvent event;

        bool running = 1;

        while(running) {

                while(XPending(display) > 0) {

                        XNextEvent(display, &event);

                        switch(event.type) {

                                case Expose:
                                        if (event.xexpose.count != 0)
                                                break;
                                        break;
                                case ConfigureNotify:
                                        if ((event.xconfigure.width != oSettings.width) || (event.xconfigure.height != oSettings.height)) {

                                                oSettings.width   = event.xconfigure.width;
                                                oSettings.height  = event.xconfigure.height;
                                                log_d("resize event");
                                                oResizeHandler(oSettings.width, oSettings.height);
                                        }
                                        break;

                                case KeyPress:

                                        switch(XLookupKeysym(&event.xkey, 0)) {

                                                case XK_Escape:									/* Quit application */
                                                        running = 0;
                                                        break;
                                                        /*
                                                           case XK_F1:										
                                                        //killGLWindow();
                                                        oSettings.fullscreen = !oSettings.fullscreen;
                                                        CreateWindow();
                                                        break;
                                                        */
                                        }
                                        break;

                                case KeyRelease:
                                        break;

                                case ClientMessage:
                                        if (*XGetAtomName(display, event.xclient.message_type) == *"WM_PROTOCOLS") {
                                                running = 0;
                                        }
                                        break;

                                default:
                                        break;
                        }
                }

                oDrawHandler();

                glXSwapBuffers(display, window);
        }

}



template <class ResizeHandler,  class DrawHandler> void X11Window<ResizeHandler, DrawHandler>::DestroyWindow() {

        log_i("destroy X11 window");
        if(glx_context) {
                if(!glXMakeCurrent(display, None, NULL)) {
                        log_e("error releasing drawing context");
                }
                glXDestroyContext(display, glx_context);
                glx_context = 0;
        }

        if(oSettings.fullscreen) {
                XF86VidModeSwitchToMode(display, screen, &video_mode);
                XF86VidModeSetViewPort(display, screen, 0, 0);
        }
        XDestroyWindow(display, window);
        XCloseDisplay(display);
}



template <class ResizeHandler,  class DrawHandler> X11Window<ResizeHandler, DrawHandler>::~X11Window() throw() {

        DestroyWindow();
}


template <class ResizeHandler,  class DrawHandler> uint32_t X11Window<ResizeHandler, DrawHandler>::GetWindowID() const {

        return window;
}


} //namespace SE
