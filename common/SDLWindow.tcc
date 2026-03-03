namespace SE {

template <class ResizeHandler,  class DrawHandler> 
        SDLWindow<ResizeHandler, DrawHandler>::SDLWindow(
                ResizeHandler & oNewResizeHandler,
                DrawHandler & oNewDrawHandler,
                const WindowSettings & oNewSettings) :
        pWindow(nullptr),
        pGLContext(nullptr),
        oSettings(oNewSettings),
        oResizeHandler(oNewResizeHandler),
        oDrawHandler(oNewDrawHandler) {

        CreateWindow();

        log_d("Created");
}

template <class ResizeHandler,  class DrawHandler> 
        void SDLWindow<ResizeHandler, DrawHandler>::CreateWindow() {

        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
                log_e("failed to initialise SDL: '{}'", SDL_GetError());
                throw("failed to initialise SDL");
        }

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
        //TODO switch to SDL_GL_CONTEXT_PROFILE_CORE after removing all legacy OpenGL calls
        //SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

        uint32_t flags = SDL_WINDOW_OPENGL;
        //TODO input flags |grab, etc
        if (oSettings.fullscreen) {
                flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        }
        else {
                flags |= SDL_WINDOW_RESIZABLE;
        }

        pWindow = SDL_CreateWindow(
                        oSettings.title.c_str(), 
                        SDL_WINDOWPOS_CENTERED,
                        SDL_WINDOWPOS_CENTERED,
                        oSettings.width,
                        oSettings.height,
                        flags);

        pGLContext = SDL_GL_CreateContext(pWindow);

        //oResizeHandler(oSettings.width, oSettings.height);

        SDL_GL_MakeCurrent(pWindow, pGLContext);
        
        log_i("Running in {} mode", oSettings.fullscreen ? "fullscreen" : "window");
}

template <class ResizeHandler,  class DrawHandler> SDLWindow<ResizeHandler, DrawHandler>::~SDLWindow() throw() {

        log_i("destroy SDL pWindow");

        SDL_GL_DeleteContext(pGLContext);
        SDL_DestroyWindow(pWindow);
        SDL_Quit();
}

template <class ResizeHandler,  class DrawHandler> void SDLWindow<ResizeHandler, DrawHandler>::Loop() {

        bool            running{true};
        SDL_Event       event;

        auto & oInputManager = GetSystem<InputManager>();

        while (running) {

                while (SDL_PollEvent(&event)) {

                        oInputManager.HandleEvent(event);

                        switch (event.type) {

                                case SDL_QUIT:
                                        running = false;
                                        break;
                                case SDL_WINDOWEVENT:
                                        switch (event.window.event) {

                                                case SDL_WINDOWEVENT_CLOSE:
                                                        if (event.window.windowID == SDL_GetWindowID(pWindow)) {
                                                                running = false;
                                                        }
                                                        break;
                                                case SDL_WINDOWEVENT_RESIZED:
                                                        oSettings.width  = event.window.data1;
                                                        oSettings.height = event.window.data2;
                                                        log_d("resize event");
                                                        oResizeHandler(oSettings.width, oSettings.height);
                                                        break;
                                        }
                                        break;

                                case SDL_KEYDOWN:
                                        if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                                                log_i("quit application");
                                                running = false;
                                        }
                                        break;

                                default:
                                        break;
                        }
                }

                oDrawHandler();

                SDL_GL_SwapWindow(pWindow);
        }

        log_i("stop running main SDL loop");
}

//TEMP remove after switching from OIS
template <class ResizeHandler,  class DrawHandler> uint32_t SDLWindow<ResizeHandler, DrawHandler>::GetWindowID() const {

        return SDL_GetWindowID(pWindow);
}

} //namespace SE
