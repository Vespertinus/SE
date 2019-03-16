
namespace SE {

template <class TLoop> OffScreenApplication<TLoop>::PreInit::PreInit() {

        log_d ("init remain subsytems");
        TEngine::Instance().Init();
}

template <class TLoop> OffScreenApplication<TLoop>::OffScreenApplication(const SysSettings_t & oNewSettings, const typename TLoop::Settings  & oLoopSettings):
        oSettings(oNewSettings),
        oCamera(oTranspose, oSettings.oCamSettings),
        oRenderingCtx(WindowSettings {
                        OSMESA_BGRA,
                        24,
                        0,
                        0,
                        oSettings.oCamSettings.width,
                        oSettings.oCamSettings.height,
                        oSettings.vRenderBuffer } ),
        oLoop(oLoopSettings, oCamera) {

                Init();

                log_i("Inited");

}



template <class TLoop> OffScreenApplication<TLoop>::~OffScreenApplication() throw() {

}



template <class TLoop> void OffScreenApplication<TLoop>::Init() {

        log_i("app init");

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClearDepth(1.0);
        glDepthFunc(GL_LESS);
        glEnable(GL_DEPTH_TEST);
}



template <class TLoop> void OffScreenApplication<TLoop>::Run() {

        CalcDuration oLoopDuration;

        glClear(oSettings.clear_flag);

        oCamera.Adjust();

        oLoop.Process();

        CalcDuration oRenderDuration;

        TEngine::Instance().Get<TRenderer>().Render();

        log_i("renderer duration = {} ms", oRenderDuration.Get());

        oLoop.PostRender();

        glFinish();

        log_i("duration = {} ms", oLoopDuration.Get());
}

template <class TLoop> TLoop & OffScreenApplication<TLoop>::GetAppLogic() {

        return oLoop;
}

template <class TLoop> void OffScreenApplication<TLoop>::ResizeViewport(const int32_t new_width, const int32_t new_height) {

        if (oSettings.oCamSettings.width == new_width && oSettings.oCamSettings.height == new_height) { return; }

        oSettings.oCamSettings.width 	= new_width;
        oSettings.oCamSettings.height	= (new_height) ? new_height : 1;

        oCamera.UpdateDimension(oSettings.oCamSettings.width, oSettings.oCamSettings.height);
        oRenderingCtx.UpdateDimension(oSettings.oCamSettings.width, oSettings.oCamSettings.height);

        SE::TGraphicsState::Instance().SetScreenSize(oSettings.oCamSettings.width, oSettings.oCamSettings.height);
}


} //namespace SE



