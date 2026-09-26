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
void AllVisible<TRenderableComponents ...>::UpdateCameraView(const glm::mat4 & mViewProjection) {

        if (view_valid) {
                for (int c = 0; c < 4; ++c) {
                        for (int r = 0; r < 4; ++r) {
                                if (glm::abs(mViewProjection[c][r] - mLastViewProjection[c][r]) > view_epsilon) {
                                        changed = true;   // view-dependent renderables must rebuild
                                }
                        }
                }
        }
        else {
                changed = true;   // first view: unknown, force one build
        }
        mLastViewProjection = mViewProjection;
        view_valid          = true;
}

template <class ... TRenderableComponents>
typename AllVisible<TRenderableComponents ...>::VisibilityResult
AllVisible<TRenderableComponents ...>::GetVisible() {

        const bool need_rebuild = changed;

        if (need_rebuild) {
                // Full rebuild: re-gather all commands from renderables
                vOpaqueCommands.clear();
                vTransparentCommands.clear();

                for (auto & item : mActiveRenderables) {
                        TVariant * pVar = &item.second;
                        std::visit([this](auto * pRenderable) {
                                const auto AddCommand = [this](const RenderCommand * pCmd) {
                                        BlendMode mode = pCmd->State().GetBlendMode();
                                        if (mode == BlendMode::Opaque || mode == BlendMode::Masked) {
                                                vOpaqueCommands.emplace_back(pCmd);
                                        } else {
                                                vTransparentCommands.emplace_back(pCmd);
                                        }
                                };
                                ForEachRenderCommand(pRenderable->GetRenderCommands(), AddCommand);
                        }, *pVar);
                }

                changed = false;
        }

        return VisibilityResult {
                vOpaqueCommands,
                vTransparentCommands,
                need_rebuild
        };
}

}
