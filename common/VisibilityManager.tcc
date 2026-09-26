
namespace SE {

template <class ... TRenderableComponents>
template <class TRenderable>
bool VisibilityManager<TRenderableComponents ...>::CullTest(
                const TRenderable * pRenderable, const SFrustum & oFrustum) const {

        if constexpr (HasCullSphere<TRenderable>::value) {
                glm::vec3 vCenter{0.f};
                float     fRadius = 0.f;
                if (pRenderable->GetCullSphere(vCenter, fRadius) && fRadius > 0.f) {
                        return oFrustum.IntersectsSphere(vCenter, fRadius);
                }
                return true;   // no bounds — cannot cull
        } else {
                (void)pRenderable; (void)oFrustum;
                return true;
        }
}

template <class ... TRenderableComponents>
template <class TRenderable >
void VisibilityManager<TRenderableComponents ...>::AddRenderable(TRenderable * pComponent) {

        mActiveRenderables.emplace(reinterpret_cast<std::uintptr_t>(pComponent), TVariant(pComponent));
        changed = true;
}

template <class ... TRenderableComponents>
template <class TRenderable >
void VisibilityManager<TRenderableComponents ...>::RemoveRenderable(TRenderable * pComponent) {

        mActiveRenderables.erase(reinterpret_cast<std::uintptr_t>(pComponent));
        mVisibleRenderables.erase(reinterpret_cast<std::uintptr_t>(pComponent));
        changed = true;
}

template <class ... TRenderableComponents>
void VisibilityManager<TRenderableComponents ...>::UpdateCameraView(const glm::mat4 & mViewProjection) {

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
typename VisibilityManager<TRenderableComponents ...>::VisibilityResult
VisibilityManager<TRenderableComponents ...>::GetVisible() {

        const bool need_rebuild = changed;

        if (need_rebuild) {
                const SFrustum oFrustum = SFrustum::FromVP(mLastViewProjection);

                vOpaqueCommands.clear();
                vTransparentCommands.clear();
                mVisibleRenderables.clear();

                for (auto & item : mActiveRenderables) {
                        TVariant * pVar = &item.second;
                        std::visit([this, &oFrustum](auto * pRenderable) {
                                if (!CullTest(pRenderable, oFrustum)) { return; }
                                mVisibleRenderables.insert(reinterpret_cast<std::uintptr_t>(pRenderable));
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
