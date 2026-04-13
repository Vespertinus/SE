namespace SE {

template <class ... TRenderableComponents>
AllVisible<TRenderableComponents ...>::AllVisible() {

        mActiveRenderables.reserve(1000);
        vOpaqueCommands.reserve(1000);
        vTransparentCommands.reserve(200);
}

template <class ... TRenderableComponents>
template <class TRenderable >
void AllVisible<TRenderableComponents ...>::AddRenderable(TRenderable * pComponent) {

        mActiveRenderables.emplace(reinterpret_cast<uintptr_t>(pComponent), TVariant(pComponent));
        changed = true;
}

template <class ... TRenderableComponents>
template <class TRenderable >
void AllVisible<TRenderableComponents ...>::RemoveRenderable(TRenderable * pComponent) {

        mActiveRenderables.erase(reinterpret_cast<uintptr_t>(pComponent));
        changed = true;
}

template <class ... TRenderableComponents>
typename AllVisible<TRenderableComponents ...>::VisibilityResult
AllVisible<TRenderableComponents ...>::GetVisible(const glm::vec3 & cameraPos) {

        cameraChanged = (cameraPos != lastCameraPos);
        lastCameraPos = cameraPos;

        const bool needRebuild = changed || cameraChanged;

        if (needRebuild) {

                if (changed) {
                        // Full rebuild: re-gather all commands from renderables
                        vOpaqueCommands.clear();
                        vTransparentCommands.clear();

                        for (auto & item : mActiveRenderables) {
                                TVariant * pVar = &item.second;
                                std::visit([this](auto * pRenderable) {
                                        auto & cmds = pRenderable->GetRenderCommands();
                                        for (auto & oCmd : cmds) {
                                                BlendMode mode = oCmd.State().GetBlendMode();
                                                if (mode == BlendMode::Opaque || mode == BlendMode::Masked) {
                                                        vOpaqueCommands.emplace_back(&oCmd);
                                                } else {
                                                        vTransparentCommands.emplace_back(&oCmd);
                                                }
                                        }
                                }, *pVar);
                        }
                }

                changed = false;
        }

        return VisibilityResult {
                vOpaqueCommands,
                vTransparentCommands,
                needRebuild
        };
}

}
