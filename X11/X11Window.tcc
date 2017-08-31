
#include <string.h>

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

                fprintf(stderr, "X11Window::X11Window: Created\n");
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
        Window                wnd_dummy;
        uint32_t              border_dummy;
        int32_t               x,
                              y;

        int attr_list_single[] = {GLX_RGBA, GLX_RED_SIZE, 4,
                GLX_GREEN_SIZE, 4,
                GLX_BLUE_SIZE, 4,
                GLX_DEPTH_SIZE, 16,
                None};

        int attr_list_double[] = {GLX_RGBA, GLX_DOUBLEBUFFER,
                GLX_RED_SIZE, 4,
                GLX_GREEN_SIZE, 4,
                GLX_BLUE_SIZE, 4,
                GLX_DEPTH_SIZE, 16,
                None};

        display = XOpenDisplay(0);
        if (!display) {
                fprintf(stderr, "X11Window::CreateWindow: can't open XDisplay\n");
                exit (-1);
        }

        screen  = DefaultScreen(display);
        XF86VidModeQueryVersion(display, &video_mode_major, &video_mode_minor);

        XF86VidModeGetAllModeLines(display, screen, &mode_num, &modes);

        video_mode = *modes[0];


        visual = glXChooseVisual(display, screen, attr_list_double);
        if(!visual) {

                visual = glXChooseVisual(display, screen, attr_list_single);
                if (!visual) {
                        fprintf(stderr, "error: can't choose visual\n");
                        abort();
                }
                printf("set X11 single buffered window\n");
        }
        else { printf("set X11 double buffered window\n"); }

        glx_context           = glXCreateContext(display, visual, 0, true);

        color_map             = XCreateColormap(display, RootWindow(display, visual->screen), visual->visual, AllocNone);
        wnd_attr.colormap     = color_map;
        wnd_attr.border_pixel = 0;

        if (oSettings.fullscreen) {

                fprintf(stderr, "try to set fullscreen mode\n");

                for (int i = 0; i < mode_num; ++i) {
                        if ((modes[i]->hdisplay == oSettings.width) && (modes[i]->vdisplay == oSettings.height)) { best_mode = i; break; }
                }
                if (best_mode == -1) {
                        fprintf(stderr, "target mode unsupported, width = %u, height = %u\n", oSettings.width, oSettings.height);
                        exit(-1);  
                }

                XF86VidModeSwitchToMode(display, screen, modes[best_mode]);
                XF86VidModeSetViewPort(display, screen, 0, 0);

                dpy_width   = modes[best_mode]->hdisplay;
                dpy_height  = modes[best_mode]->vdisplay;
                XFree(modes);

                wnd_attr.override_redirect = true;
                //wnd_attr.event_mask = ExposureMask | KeyPressMask | ButtonPressMask | StructureNotifyMask;
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

                //wnd_attr.event_mask = ExposureMask | KeyPressMask | ButtonPressMask |	StructureNotifyMask;
                wnd_attr.event_mask = ExposureMask | StructureNotifyMask;

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

        uint32_t bpp;
        uint32_t width;
        uint32_t height;

        glXMakeCurrent(display, window, glx_context);
        XGetGeometry(display, window, &wnd_dummy, &x, &y, &width, &height, &border_dummy, &bpp);

        //fprintf get options

        bool direct_render = glXIsDirect(display, glx_context);

        fprintf(stderr, "Direct Rendering: %s\n", direct_render ? "Yes" : "No");
        if (!direct_render) { abort(); }

        fprintf(stderr, "Running in %s mode\n", oSettings.fullscreen ? "fullscreen" : "window");

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
                                                fprintf(stderr, "X11Window::StartLoop: resize event\n");
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

        if(glx_context) {
                if(!glXMakeCurrent(display, None, NULL)) {
                        fprintf(stderr, "X11Window::DestroyWindow: error releasing drawing context\n");
                }
                glXDestroyContext(display, glx_context);
                glx_context = 0;
        }

        if(oSettings.fullscreen) {
                XF86VidModeSwitchToMode(display, screen, &video_mode);
                XF86VidModeSetViewPort(display, screen, 0, 0);
        }
        XCloseDisplay(display);
}



template <class ResizeHandler,  class DrawHandler> X11Window<ResizeHandler, DrawHandler>::~X11Window() throw() {

        DestroyWindow();
}


template <class ResizeHandler,  class DrawHandler> uint32_t X11Window<ResizeHandler, DrawHandler>::GetWindowID() const {

        return window;
}


} //namespace SE
