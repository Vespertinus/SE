#ifdef SE_UI_ENABLED

#include <cmath>
#include <string>
#include <RmlUi/Core/ElementDocument.h>
#include <ui/UIDocumentManager.h>

namespace SE {

// Apply easing to normalized t ∈ [0, 1]
float UIAnimationController::Ease(float t, AnimEasing easing) {

        switch (easing) {
                case AnimEasing::EASE_IN:
                        return t * t;
                case AnimEasing::EASE_OUT:
                        return t * (2.0f - t);
                case AnimEasing::EASE_IN_OUT:
                        return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
                case AnimEasing::LINEAR:
                default:
                        return t;
        }
}

void UIAnimationController::Start(UIDocumentId out_id, UIDocumentId in_id,
                                  ScreenTransition type, float duration,
                                  AnimEasing easing,
                                  std::function<void()> on_complete) {

        oAnim.out_id     = out_id;
        oAnim.in_id      = in_id;
        oAnim.type       = type;
        oAnim.easing     = easing;
        oAnim.progress   = 0.0f;
        oAnim.duration   = (duration > 0.0f) ? duration : 0.001f;
        oAnim.viewport_w = static_cast<float>(GetSystem<GraphicsState>().GetScreenSize().x);

        if (type == ScreenTransition::NONE) {
                if (auto * pOut = pDocMgr ? pDocMgr->Get(out_id) : nullptr)
                        pOut->Hide();
                if (on_complete) on_complete();
                oAnim = ActiveAnim{};
                return;
        }

        oAnim.fnOnComplete = std::move(on_complete);
}

void UIAnimationController::Update(float dt) {

        if (oAnim.type == ScreenTransition::NONE) return;

        oAnim.progress += dt / oAnim.duration;
        if (oAnim.progress > 1.0f) oAnim.progress = 1.0f;

        const float t  = Ease(oAnim.progress, oAnim.easing);
        const float vw = oAnim.viewport_w;

        auto * pOut = pDocMgr ? pDocMgr->Get(oAnim.out_id) : nullptr;
        auto * pIn  = pDocMgr ? pDocMgr->Get(oAnim.in_id)  : nullptr;

        switch (oAnim.type) {

                case ScreenTransition::FADE: {
                        if (pOut) pOut->SetProperty("opacity", std::to_string(1.0f - t));
                        if (pIn)  pIn ->SetProperty("opacity", std::to_string(t));
                        break;
                }

                case ScreenTransition::SLIDE_LEFT: {
                        // Outgoing slides left off-screen; incoming enters from the right
                        if (pOut) pOut->SetProperty("transform",
                                "translateX(" + std::to_string(-vw * t) + "px)");
                        if (pIn)  pIn ->SetProperty("transform",
                                "translateX(" + std::to_string(vw * (1.0f - t)) + "px)");
                        break;
                }

                case ScreenTransition::SLIDE_RIGHT: {
                        // Outgoing slides right off-screen; incoming enters from the left
                        if (pOut) pOut->SetProperty("transform",
                                "translateX(" + std::to_string(vw * t) + "px)");
                        if (pIn)  pIn ->SetProperty("transform",
                                "translateX(" + std::to_string(-vw * (1.0f - t)) + "px)");
                        break;
                }

                case ScreenTransition::SCALE: {
                        // Outgoing shrinks and fades; incoming grows and fades in
                        if (pOut) {
                                pOut->SetProperty("opacity",   std::to_string(1.0f - t));
                                pOut->SetProperty("transform", "scale(" + std::to_string(1.0f - 0.1f * t) + ")");
                        }
                        if (pIn) {
                                pIn ->SetProperty("opacity",   std::to_string(t));
                                pIn ->SetProperty("transform", "scale(" + std::to_string(0.9f + 0.1f * t) + ")");
                        }
                        break;
                }

                default: break;
        }

        if (oAnim.progress >= 1.0f) {
                // Reset all animated properties, hide outgoing
                if (pOut) {
                        pOut->SetProperty("opacity", "1");
                        pOut->RemoveProperty("transform");
                        pOut->Hide();
                }
                if (pIn) {
                        pIn->SetProperty("opacity", "1");
                        pIn->RemoveProperty("transform");
                }
                // Move callback out before resetting so it fires cleanly
                auto fn = std::move(oAnim.fnOnComplete);
                oAnim = ActiveAnim{};
                if (fn) fn();
        }
}

} // namespace SE

#endif // SE_UI_ENABLED
