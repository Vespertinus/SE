
namespace SE {

template <class TLoop> OffScreenApplication<TLoop>::PreInit::PreInit() {

        log_d ("init remain subsytems");
        TEngine::Instance().Init();
}

template <class TLoop> OffScreenApplication<TLoop>::OffScreenApplication(const SysSettings_t & oNewSettings, const typename TLoop::Settings  & oLoopSettings):
        oSettings(oNewSettings),
        oRenderingCtx(WindowSettings {
                        OSMESA_BGRA,
                        24,
                        0,
                        0,
                        oSettings.width,
                        oSettings.height,
                        oSettings.vRenderBuffer } ),
        oLoop(oLoopSettings) {

        ResizeViewport(oSettings.width, oSettings.height);

        Init();

        GetSystem<EventManager>().TriggerEvent(EStartApp{});

        log_i("Inited");
}



template <class TLoop> OffScreenApplication<TLoop>::~OffScreenApplication() noexcept {

}



template <class TLoop> void OffScreenApplication<TLoop>::Init() {

        log_i("app init");

        GetSystem<GraphicsState>().SetClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        GetSystem<GraphicsState>().SetClearDepth(1.0f);
        GetSystem<GraphicsState>().SetDepthFunc(DepthFunc::LESS);
        GetSystem<GraphicsState>().SetDepthTest(true);
}



template <class TLoop> void OffScreenApplication<TLoop>::Run() {

        CalcDuration oLoopDuration;

        glClear(oSettings.clear_flag);

        auto & oEventManager = GetSystem<EventManager>();
        oEventManager.TriggerEvent(EUpdate{0.0f});

        oEventManager.Process();
        oLoop.Process();

        oEventManager.TriggerEvent(EPostUpdate{0.0f});

        CalcDuration oRenderDuration;

        TEngine::Instance().Get<TRenderer>().Render();

        log_i("renderer duration = {} ms", oRenderDuration.Get());

        oEventManager.TriggerEvent(EPostRenderUpdate{0.0f});

        glFinish();

        log_i("duration = {} ms", oLoopDuration.Get());
}

template <class TLoop> TLoop & OffScreenApplication<TLoop>::GetAppLogic() {

        return oLoop;
}

template <class TLoop> void OffScreenApplication<TLoop>::ResizeViewport(const int32_t new_width, const int32_t new_height) {

        oSettings.width 	= new_width;
        oSettings.height	= (new_height) ? new_height : 1;
        glm::uvec2 screen_size(oSettings.width, oSettings.height);

        oRenderingCtx.UpdateDimension(oSettings.width, oSettings.height);
        glViewport(0, 0, oSettings.width, oSettings.height);

        GetSystem<GraphicsState>().SetScreenSize(screen_size);
        GetSystem<TRenderer>().SetScreenSize(screen_size);
}


} //namespace SE



