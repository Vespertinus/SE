
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

        GetSystem<GraphicsState>().SetViewProjection(pCamera->GetWorldMVP());

        for (auto * pRenderCommand : vRenderCommands) {
                pRenderCommand->Draw();
        }

        for (auto * pRenderCommand : vRenderInstantCommands) {
                pRenderCommand->Draw();
        }

        vRenderInstantCommands.clear();

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
        if (pCamera) {
                pCamera->UpdateDimension(screen_size);
        }
}

template <class TVisibilityManager> void Renderer<TVisibilityManager>::SetCamera(Camera * pCurCamera) {

        pCamera = pCurCamera;
        if (pCamera) {
                pCamera->UpdateDimension(screen_size);
        }
}

template <class TVisibilityManager> Camera * Renderer<TVisibilityManager>::GetCamera() const {
        return pCamera;
}

template <class TVisibilityManager> void Renderer<TVisibilityManager>::AddRenderCmd(RenderCommand const * pCmd) {

        vRenderInstantCommands.emplace_back(pCmd);
}

template <class TVisibilityManager> void Renderer<TVisibilityManager>::Print(const size_t indent) {

        log_d_clean("{:>{}} Renderer: resolution: ({}, {}), commands cnt: {}\n", ">", indent, screen_size[0], screen_size[1], vRenderCommands.size());
        for (const auto * pItem : vRenderCommands) {

                log_d_clean("{}", pItem->StrDump(indent));
        }
}

} //namespace SE
