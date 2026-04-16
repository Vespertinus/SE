
#include <CommonEvents.h>
#include <EventManager.h>
#include <PlatformClock.h>

namespace SE {

template <class TLoop> Application<TLoop>::PreInit::PreInit(const SysSettings_t & oSettings, const uint32_t window_id) {

        log_d("init remain subsystems");
        TEngine::Instance().Init();

        auto & oInputManager = GetSystem<InputManager>();
        oInputManager.Init(window_id, oSettings.grab_mouse, oSettings.hide_mouse);

#ifdef SE_PHYSICS_ENABLED
        GetSystem<PhysicsSystem>().Init(GetSystem<Config>().oPhysicsConfig);
#endif
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

        GetSystem<EventManager>().AddListener<EQuit, &Application<TLoop>::OnQuit>(this);

        TEngine::Instance().Get<EventManager>().TriggerEvent(EStartApp{});

        log_i("Start Loop");

        oMainWindow.Loop();

        log_i("Stop Loop");

}



template <class TLoop> Application<TLoop>::~Application() noexcept {

        GetSystem<EventManager>().RemoveListener<EQuit, &Application<TLoop>::OnQuit>(this);
}



template <class TLoop> void Application<TLoop>::OnQuit(const Event & /* oEvent */) {

        oMainWindow.RequestQuit();
}



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

        GetSystem<GraphicsState>().SetViewport(0, 0, new_width, new_height);

        GetSystem<InputManager>().SetWindowExtents(new_width, new_height);
        GetSystem<GraphicsState>().SetScreenSize(screen_size);
        GetSystem<TRenderer>().SetScreenSize(screen_size);
#ifdef SE_UI_ENABLED
        GetSystem<UISystem>().SetDimensions(new_width, new_height);
#endif
}



template <class TLoop> void Application<TLoop>::Run() {

        GetSystem<GraphicsState>().Clear(oSettings.clear_flag);

        auto & oEventManager = TEngine::Instance().Get<EventManager>();

        static double prev_wall = PlatformClock::Now();
        const double  cur_wall  = PlatformClock::Now();
        auto & oClock = GetSystem<AppClock>();
        oClock.Tick(cur_wall - prev_wall);
        prev_wall = cur_wall;
        const float last_frame_time = oClock.Delta();

        GetSystem<GraphicsState>().FrameStart();
        GetSystem<FpsTracker>().Update(oClock.RawDelta());

        oEventManager.TriggerEvent(EInputUpdate{last_frame_time});

        oEventManager.TriggerEvent(EUpdate{last_frame_time});

#ifdef SE_PHYSICS_ENABLED
        GetSystem<PhysicsSystem>().Update(last_frame_time);
#endif

        oEventManager.Process();
        oLoop.Process();
        oEventManager.Process();

        oEventManager.TriggerEvent(EPostUpdate{last_frame_time});

#ifdef SE_PHYSICS_ENABLED
        GetSystem<PhysicsSystem>().Interpolate();
#endif

        //Render
        TEngine::Instance().Get<TRenderer>().Render();

        oEventManager.TriggerEvent(EPostRenderUpdate{last_frame_time});

        TResourceManager::Instance().ProcessDeferred();

        GetSystem<InputManager>().Capture();

        GetSystem<FrameAllocator>().reset();

}

template <class TLoop> TLoop & Application<TLoop>::GetAppLogic() {

        return oLoop;
}


} //namespace SE


