
namespace SE {

template <class TVisibilityManager>
        Renderer<TVisibilityManager>::Renderer(std::unique_ptr<TVisibilityManager> pNewManager) :
        pManager(pNewManager) {//Release

        vRenderCommands.reserve(1000);
}

template <class TVisibilityManager>
        Renderer<TVisibilityManager>::Renderer() :
                pManager(std::make_unique<TVisibilityManager>()) {

}

template <class TVisibilityManager> void Renderer<TVisibilityManager>::PrepareVisible() {

        bool changed = false;

        auto & vVisibleRenderables = pManager->GetVisible(changed);

        if (!changed) { return; }

        vRenderCommands.clear();

        for (auto * pRenderable : vVisibleRenderables) {
                std::visit([this](auto * pRenderableComponent) {
                        auto & vComponentRenderCommands = pRenderableComponent->GetRenderCommands();
                        for (auto & oRenderCommand : vComponentRenderCommands) {
                                vRenderCommands.emplace_back(&oRenderCommand);
                        }
                },
                *pRenderable);


        }

        //TODO sort vRenderCommands
}

template <class TVisibilityManager> void Renderer<TVisibilityManager>::Render() {

        CalcDuration oDuration;

        if (!pCamera) {
                log_e("main camera was not set");
                return;
        }

        PrepareVisible();

        TGraphicsState::Instance().SetViewProjection(pCamera->GetWorldMVP());

        for (auto * pRenderCommand : vRenderCommands) {
                pRenderCommand->Draw();
        }

        //log_d("rendered: {} cmd, duration = {} ms", vRenderCommands.size(), oDuration.Get());
}

template <class TVisibilityManager>
        template <class TRenderable >
                void Renderer<TVisibilityManager>::AddRenderable(TRenderable * pComponent) {

        pManager->AddRenderable(pComponent);
}

template <class TVisibilityManager>
        template <class TRenderable >
                void Renderer<TVisibilityManager>::RemoveRenderable(TRenderable * pComponent) {

        pManager->RemoveRenderable(pComponent);
}

template <class TVisibilityManager> void Renderer<TVisibilityManager>::SetScreenSize(const glm::uvec2 new_screen_size) {

        screen_size = new_screen_size;
        if (pCurCamera) {
                pCurCamera->UpdateDimension(screen_size);
        }
}

template <class TVisibilityManager> void Renderer<TVisibilityManager>::SetCamera(Camera * pCamera) {

        pCurCamera = pCamera;
        if (pCurCamera) {
                pCurCamera->UpdateDimension(screen_size);
        }
}

template <class TVisibilityManager> Camera * Renderer<TVisibilityManager>::GetCamera() const {
        return pCurCamera;
}

} //namespace SE
