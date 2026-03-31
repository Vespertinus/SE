#ifdef SE_UI_ENABLED

#include <string>
#include <RmlUi/Core/ElementDocument.h>
#include <ui/UIDocumentManager.h>
#include <ui/UIAnimationController.h>

namespace SE {

void UIScreenManager::Init(UIDocumentManager & docMgr, UIAnimationController & animCtrl) {

        pDocMgr   = &docMgr;
        pAnimCtrl = &animCtrl;
}

UIDocumentId UIScreenManager::Top() const {

        if (vStack.empty()) return INVALID_UI_DOCUMENT;
        return vStack.back().id;
}

// Set the CSS initial state of `pDoc` for the incoming side of a transition.
// Must be called before Show() so there is no single-frame flash.
static void SetIncomingInitialState(Rml::ElementDocument * pDoc,
                                    ScreenTransition       transition,
                                    float                  viewport_w) {
        if (!pDoc) return;
        switch (transition) {
                case ScreenTransition::FADE:
                        pDoc->SetProperty("opacity", "0");
                        break;
                case ScreenTransition::SLIDE_LEFT:
                        pDoc->SetProperty("transform",
                                "translateX(" + std::to_string(viewport_w) + "px)");
                        break;
                case ScreenTransition::SLIDE_RIGHT:
                        pDoc->SetProperty("transform",
                                "translateX(" + std::to_string(-viewport_w) + "px)");
                        break;
                case ScreenTransition::SCALE:
                        pDoc->SetProperty("opacity", "0");
                        pDoc->SetProperty("transform", "scale(0.9)");
                        break;
                default: break;
        }
}

UIDocumentId UIScreenManager::Push(const std::string & path,
                                   ScreenTransition transition,
                                   float duration,
                                   AnimEasing easing) {

        const float vw = static_cast<float>(
                GetSystem<GraphicsState>().GetScreenSize().x);

        const UIDocumentId out_id = Top();
        const UIDocumentId in_id  = pDocMgr->Load(path);
        if (in_id == INVALID_UI_DOCUMENT) return INVALID_UI_DOCUMENT;

        Rml::ElementDocument * pDoc = pDocMgr->Get(in_id);
        SetIncomingInitialState(pDoc, transition, vw);
        if (pDoc) pDoc->Show();

        pAnimCtrl->Start(out_id, in_id, transition, duration, easing);
        vStack.push_back(ScreenEntry{in_id, path});
        return in_id;
}

void UIScreenManager::Pop(ScreenTransition transition, float duration, AnimEasing easing) {

        if (vStack.empty()) return;

        const float vw = static_cast<float>(
                GetSystem<GraphicsState>().GetScreenSize().x);

        const UIDocumentId out_id = vStack.back().id;
        vStack.pop_back();
        top_is_modal = false;  // popped doc may have been modal; new top is not
        const UIDocumentId in_id = Top();  // previous screen (was hidden when pushed below)

        // Restore the incoming document: set initial state and show it
        Rml::ElementDocument * pIn = pDocMgr->Get(in_id);
        SetIncomingInitialState(pIn, transition, vw);
        if (pIn) pIn->Show();

        // Release the outgoing document only after animation completes so it
        // remains visible and animatable for the full duration.
        auto fn_release = [this, out_id]() { pDocMgr->Release(out_id); };
        pAnimCtrl->Start(out_id, in_id, transition, duration, easing,
                         std::move(fn_release));
}

void UIScreenManager::Replace(const std::string & path,
                               ScreenTransition transition,
                               float duration,
                               AnimEasing easing) {

        const float vw = static_cast<float>(
                GetSystem<GraphicsState>().GetScreenSize().x);

        const UIDocumentId out_id = Top();
        const UIDocumentId in_id  = pDocMgr->Load(path);
        if (in_id == INVALID_UI_DOCUMENT) return;

        // If same document is already on top, undo the Load and do nothing
        if (in_id == out_id) {
                pDocMgr->Release(in_id);
                return;
        }

        Rml::ElementDocument * pDoc = pDocMgr->Get(in_id);
        SetIncomingInitialState(pDoc, transition, vw);
        if (pDoc) pDoc->Show();

        // Update stack entry before starting animation
        top_is_modal = false;  // Replace always produces a non-modal top
        if (!vStack.empty())
                vStack.back() = ScreenEntry{in_id, path};
        else
                vStack.push_back(ScreenEntry{in_id, path});

        // Defer release of outgoing doc until animation completes
        auto fn_release = [this, out_id]() {
                if (out_id != INVALID_UI_DOCUMENT)
                        pDocMgr->Release(out_id);
        };
        pAnimCtrl->Start(out_id, in_id, transition, duration, easing,
                         std::move(fn_release));
}

UIDocumentId UIScreenManager::PushModal(const std::string & path,
                                        ScreenTransition transition,
                                        float duration,
                                        AnimEasing easing) {

        const float vw = static_cast<float>(
                GetSystem<GraphicsState>().GetScreenSize().x);

        const UIDocumentId out_id = Top();
        const UIDocumentId in_id  = pDocMgr->Load(path);
        if (in_id == INVALID_UI_DOCUMENT) return INVALID_UI_DOCUMENT;

        Rml::ElementDocument * pDoc = pDocMgr->Get(in_id);
        SetIncomingInitialState(pDoc, transition, vw);
        if (pDoc) pDoc->Show(Rml::ModalFlag::Modal, Rml::FocusFlag::Auto);

        pAnimCtrl->Start(out_id, in_id, transition, duration, easing);
        vStack.push_back(ScreenEntry{in_id, path});
        top_is_modal = true;
        return in_id;
}

UIDocumentId UIScreenManager::ShowPersistent(const std::string & path) {

        // Idempotent: if already in persistent layer, return existing id
        for (UIDocumentId id : vPersistent) {
                Rml::ElementDocument * pDoc = pDocMgr->Get(id);
                if (pDoc && pDocMgr->GetPath(id) == path)
                        return id;
        }

        const UIDocumentId id = pDocMgr->Load(path);
        if (id == INVALID_UI_DOCUMENT) return INVALID_UI_DOCUMENT;
        Rml::ElementDocument * pDoc = pDocMgr->Get(id);
        if (pDoc) pDoc->Show();
        vPersistent.push_back(id);
        return id;
}

void UIScreenManager::HidePersistent(UIDocumentId id) {

        auto it = std::find(vPersistent.begin(), vPersistent.end(), id);
        if (it == vPersistent.end()) return;
        vPersistent.erase(it);
        Rml::ElementDocument * pDoc = pDocMgr->Get(id);
        if (pDoc) pDoc->Hide();
        pDocMgr->Release(id);
}

void UIScreenManager::PopTo(size_t target_size) {

        while (vStack.size() > target_size)
                Pop();
}

void UIScreenManager::Update(float dt) {

        pAnimCtrl->Update(dt);
}

} // namespace SE

#endif // SE_UI_ENABLED
