
#include <InputManager.h>
#include <CommonEvents.h>

namespace SE {

template <class TLoop> Application<TLoop>::PreInit::PreInit(const SysSettings_t & oSettings, const uint32_t window_id) {

        log_d ("init remain subsytems");
        TEngine::Instance().Init();

        log_d("try to init OIS");
/*
        TInputManager::Instance().Initialise(
                        window_id,
                        oSettings.oWindowSettings.width,
                        oSettings.oWindowSettings.height,
                        oSettings.grab_mouse,
                        oSettings.hide_mouse);
                        */

}

template <class TLoop> Application<TLoop>::Application(const SysSettings_t & oNewSettings, const typename TLoop::Settings  & oLoopSettings):
        oSettings(oNewSettings),
        oRunFunctor   (*this, &Application<TLoop>::Run),
        oResizeFunctor(*this, &Application<TLoop>::ResizeViewport),
        oMainWindow(oResizeFunctor, oRunFunctor, oSettings.oWindowSettings),
        oPreInit(oSettings, oMainWindow.GetWindowID()),
        oLoop(oLoopSettings) {

        /*TODO
         init all members as uniq pointers in specified order and destroy in reverse order
         or, all depend sub systems inside Engine<>
        */

        ResizeViewport(oSettings.oWindowSettings.width, oSettings.oWindowSettings.height);

        Init();

        TEngine::Instance().Get<EventManager>().TriggerEvent(EStartApp{});

        log_i("Start Loop");

        oMainWindow.Loop();

        log_i("Stop Loop");

}



template <class TLoop> Application<TLoop>::~Application() noexcept { ;; }



template <class TLoop> void Application<TLoop>::Init() {

        log_d("basic OpenGL options");

        GetSystem<GraphicsState>().SetClearColor(0.066f, 0.2235f, 0.3372f, 1.0f);
        GetSystem<GraphicsState>().SetClearDepth(1.0f);
        GetSystem<GraphicsState>().SetDepthFunc(DepthFunc::LESS);
        GetSystem<GraphicsState>().SetDepthTest(true);

        if (SE::CheckOpenGLError() != uSUCCESS) {
                throw("OpenGL Error after initial initialization");
        }
}



template <class TLoop> void Application<TLoop>::ResizeViewport(const int32_t & new_width, const int32_t & new_height) {

        glm::uvec2      screen_size(new_width, new_height);

        glViewport(0, 0, new_width, new_height);

        TInputManager::Instance().SetWindowExtents(new_width, new_height);
        GetSystem<GraphicsState>().SetScreenSize(screen_size);
        GetSystem<TRenderer>().SetScreenSize(screen_size);
}



template <class TLoop> void Application<TLoop>::Run() {

        glClear(oSettings.clear_flag);
        auto & oEventManager = TEngine::Instance().Get<EventManager>();

        float last_frame_time = GetSystem<GraphicsState>().FrameStart();

        oEventManager.TriggerEvent(EInputUpdate{last_frame_time});

        oEventManager.TriggerEvent(EUpdate{last_frame_time});

        oEventManager.Process();
        oLoop.Process();

        oEventManager.TriggerEvent(EPostUpdate{last_frame_time});

        //Render
        TEngine::Instance().Get<TRenderer>().Render();

        oEventManager.TriggerEvent(EPostRenderUpdate{last_frame_time});

        TInputManager::Instance().Capture();

        TSimpleFPS::Instance().Update();
}

template <class TLoop> TLoop & Application<TLoop>::GetAppLogic() {

        return oLoop;
}


} //namespace SE


