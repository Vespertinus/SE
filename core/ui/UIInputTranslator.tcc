#ifdef SE_UI_ENABLED

#include <string>
#include <RmlUi/Core/Element.h>
#include <InputCodes.h>

namespace SE {

UIInputTranslator::UIInputTranslator(std::vector<Rml::Context *> contexts)
        : vContexts(std::move(contexts)) {

        auto & oEM = GetSystem<EventManager>();
        oEM.AddListener<EKeyDown,            &UIInputTranslator::OnKeyDown>           (this);
        oEM.AddListener<EKeyUp,              &UIInputTranslator::OnKeyUp>             (this);
        oEM.AddListener<ETextInput,          &UIInputTranslator::OnTextInput>         (this);
        oEM.AddListener<EMouseMove,          &UIInputTranslator::OnMouseMove>         (this);
        oEM.AddListener<EMouseButtonDown,    &UIInputTranslator::OnMouseButtonDown>   (this);
        oEM.AddListener<EMouseButtonUp,      &UIInputTranslator::OnMouseButtonUp>     (this);
        oEM.AddListener<EMouseWheel,         &UIInputTranslator::OnMouseWheel>        (this);
        oEM.AddListener<EGamepadButtonDown,  &UIInputTranslator::OnGamepadButtonDown> (this);
        oEM.AddListener<EGamepadButtonUp,    &UIInputTranslator::OnGamepadButtonUp>   (this);
        oEM.AddListener<EGamepadAxis,        &UIInputTranslator::OnGamepadAxis>       (this);
}

UIInputTranslator::~UIInputTranslator() {

        auto & oEM = GetSystem<EventManager>();
        oEM.RemoveListener<EKeyDown,            &UIInputTranslator::OnKeyDown>           (this);
        oEM.RemoveListener<EKeyUp,              &UIInputTranslator::OnKeyUp>             (this);
        oEM.RemoveListener<ETextInput,          &UIInputTranslator::OnTextInput>         (this);
        oEM.RemoveListener<EMouseMove,          &UIInputTranslator::OnMouseMove>         (this);
        oEM.RemoveListener<EMouseButtonDown,    &UIInputTranslator::OnMouseButtonDown>   (this);
        oEM.RemoveListener<EMouseButtonUp,      &UIInputTranslator::OnMouseButtonUp>     (this);
        oEM.RemoveListener<EMouseWheel,         &UIInputTranslator::OnMouseWheel>        (this);
        oEM.RemoveListener<EGamepadButtonDown,  &UIInputTranslator::OnGamepadButtonDown> (this);
        oEM.RemoveListener<EGamepadButtonUp,    &UIInputTranslator::OnGamepadButtonUp>   (this);
        oEM.RemoveListener<EGamepadAxis,        &UIInputTranslator::OnGamepadAxis>       (this);
}

bool UIInputTranslator::IsCapturingMouse() const {

        for (auto * ctx : vContexts)
                if (ctx && ctx->IsMouseInteracting()) return true;
        return false;
}

bool UIInputTranslator::IsCapturingKeyboard() const {

        for (auto * ctx : vContexts) {
                if (!ctx) continue;
                Rml::Element * pFocus = ctx->GetFocusElement();
                if (!pFocus) continue;
                const Rml::String tag = pFocus->GetTagName();
                if (tag == "input" || tag == "textarea" || tag == "select") return true;
        }
        return false;
}

void UIInputTranslator::OnKeyDown(const Event & oEvent) {

        auto & ev = oEvent.Get<EKeyDown>();
        const auto key = TranslateKey(ev.key);
        const int  mod = TranslateModifiers(ev.mod);
        ForContexts([&](Rml::Context * ctx) { ctx->ProcessKeyDown(key, mod); });
}

void UIInputTranslator::OnKeyUp(const Event & oEvent) {

        auto & ev = oEvent.Get<EKeyUp>();
        const auto key = TranslateKey(ev.key);
        const int  mod = TranslateModifiers(ev.mod);
        ForContexts([&](Rml::Context * ctx) { ctx->ProcessKeyUp(key, mod); });
}

void UIInputTranslator::OnTextInput(const Event & oEvent) {

        auto & ev = oEvent.Get<ETextInput>();
        Rml::String text(ev.text);
        ForContexts([&](Rml::Context * ctx) { ctx->ProcessTextInput(text); });
}

void UIInputTranslator::OnMouseMove(const Event & oEvent) {

        auto & ev = oEvent.Get<EMouseMove>();
        ForContexts([&](Rml::Context * ctx) { ctx->ProcessMouseMove(ev.pos.x, ev.pos.y, 0); });
}

void UIInputTranslator::OnMouseButtonDown(const Event & oEvent) {

        auto & ev = oEvent.Get<EMouseButtonDown>();
        int button = 0;
        switch (ev.button) {
                case MouseB::LEFT:   button = 0; break;
                case MouseB::MIDDLE: button = 2; break;
                case MouseB::RIGHT:  button = 1; break;
                default:             return;
        }
        ForContexts([&](Rml::Context * ctx) { ctx->ProcessMouseButtonDown(button, 0); });
}

void UIInputTranslator::OnMouseButtonUp(const Event & oEvent) {

        auto & ev = oEvent.Get<EMouseButtonUp>();
        int button = 0;
        switch (ev.button) {
                case MouseB::LEFT:   button = 0; break;
                case MouseB::MIDDLE: button = 2; break;
                case MouseB::RIGHT:  button = 1; break;
                default:             return;
        }
        ForContexts([&](Rml::Context * ctx) { ctx->ProcessMouseButtonUp(button, 0); });
}

void UIInputTranslator::OnMouseWheel(const Event & oEvent) {

        auto & ev = oEvent.Get<EMouseWheel>();
        const float delta = static_cast<float>(-ev.delta);
        ForContexts([&](Rml::Context * ctx) { ctx->ProcessMouseWheel(delta, 0); });
}

void UIInputTranslator::OnGamepadButtonDown(const Event & oEvent) {

        using namespace Rml::Input;
        auto & ev = oEvent.Get<EGamepadButtonDown>();
        KeyIdentifier ki = KI_UNKNOWN;
        switch (ev.button) {
                case GamepadButton::DPAD_UP:    ki = KI_UP;     break;
                case GamepadButton::DPAD_DOWN:  ki = KI_DOWN;   break;
                case GamepadButton::DPAD_LEFT:  ki = KI_LEFT;   break;
                case GamepadButton::DPAD_RIGHT: ki = KI_RIGHT;  break;
                case GamepadButton::A:          ki = KI_RETURN; break;
                case GamepadButton::B:          ki = KI_ESCAPE; break;
                default: return;
        }
        ForContexts([&](Rml::Context * ctx) { ctx->ProcessKeyDown(ki, 0); });
}

void UIInputTranslator::OnGamepadButtonUp(const Event & oEvent) {

        using namespace Rml::Input;
        auto & ev = oEvent.Get<EGamepadButtonUp>();
        KeyIdentifier ki = KI_UNKNOWN;
        switch (ev.button) {
                case GamepadButton::DPAD_UP:    ki = KI_UP;     break;
                case GamepadButton::DPAD_DOWN:  ki = KI_DOWN;   break;
                case GamepadButton::DPAD_LEFT:  ki = KI_LEFT;   break;
                case GamepadButton::DPAD_RIGHT: ki = KI_RIGHT;  break;
                case GamepadButton::A:          ki = KI_RETURN; break;
                case GamepadButton::B:          ki = KI_ESCAPE; break;
                default: return;
        }
        ForContexts([&](Rml::Context * ctx) { ctx->ProcessKeyUp(ki, 0); });
}

void UIInputTranslator::OnGamepadAxis(const Event & oEvent) {

        using namespace Rml::Input;
        auto & ev = oEvent.Get<EGamepadAxis>();

        // Only map the left stick (axis 0 = X, axis 1 = Y)
        if (ev.axis_id > 1) return;

        constexpr int16_t kDeadZone = 8000;

        auto sendDown = [&](KeyIdentifier ki) {
                ForContexts([&](Rml::Context * ctx) { ctx->ProcessKeyDown(ki, 0); });
        };
        auto sendUp = [&](KeyIdentifier ki) {
                ForContexts([&](Rml::Context * ctx) { ctx->ProcessKeyUp(ki, 0); });
        };

        if (ev.axis_id == 0) {
                const bool wantLeft  = ev.value < -kDeadZone;
                const bool wantRight = ev.value >  kDeadZone;
                if (wantLeft  && !axis_active[0]) { axis_active[0] = true;  sendDown(KI_LEFT);  }
                else if (!wantLeft  && axis_active[0]) { axis_active[0] = false; sendUp(KI_LEFT);   }
                if (wantRight && !axis_active[1]) { axis_active[1] = true;  sendDown(KI_RIGHT); }
                else if (!wantRight && axis_active[1]) { axis_active[1] = false; sendUp(KI_RIGHT);  }
        } else {
                const bool wantUp   = ev.value < -kDeadZone;
                const bool wantDown = ev.value >  kDeadZone;
                if (wantUp   && !axis_active[2]) { axis_active[2] = true;  sendDown(KI_UP);   }
                else if (!wantUp   && axis_active[2]) { axis_active[2] = false; sendUp(KI_UP);    }
                if (wantDown && !axis_active[3]) { axis_active[3] = true;  sendDown(KI_DOWN); }
                else if (!wantDown && axis_active[3]) { axis_active[3] = false; sendUp(KI_DOWN);  }
        }
}

// SE::Key values are SDL keycode-compatible (printable = Unicode, specials = (1<<30)|scancode)
Rml::Input::KeyIdentifier UIInputTranslator::TranslateKey(Key key) {

        using namespace Rml::Input;
        switch (key) {
                case Keys::A: return KI_A;
                case Keys::B: return KI_B;
                case Keys::C: return KI_C;
                case Keys::D: return KI_D;
                case Keys::E: return KI_E;
                case Keys::F: return KI_F;
                case Keys::G: return KI_G;
                case Keys::H: return KI_H;
                case Keys::I: return KI_I;
                case Keys::J: return KI_J;
                case Keys::K: return KI_K;
                case Keys::L: return KI_L;
                case Keys::M: return KI_M;
                case Keys::N: return KI_N;
                case Keys::O: return KI_O;
                case Keys::P: return KI_P;
                case Keys::Q: return KI_Q;
                case Keys::R: return KI_R;
                case Keys::S: return KI_S;
                case Keys::T: return KI_T;
                case Keys::U: return KI_U;
                case Keys::V: return KI_V;
                case Keys::W: return KI_W;
                case Keys::X: return KI_X;
                case Keys::Y: return KI_Y;
                case Keys::Z: return KI_Z;
                case Keys::RETURN:    return KI_RETURN;
                case Keys::ESCAPE:    return KI_ESCAPE;
                case Keys::BACKSPACE: return KI_BACK;
                case Keys::TAB:       return KI_TAB;
                case Keys::SPACE:     return KI_SPACE;
                case Keys::DELETE_:   return KI_DELETE;
                case Keys::INSERT:    return KI_INSERT;
                case Keys::HOME:      return KI_HOME;
                case Keys::END:       return KI_END;
                case Keys::PAGE_UP:   return KI_PRIOR;
                case Keys::PAGE_DOWN: return KI_NEXT;
                case Keys::LEFT:      return KI_LEFT;
                case Keys::RIGHT:     return KI_RIGHT;
                case Keys::UP:        return KI_UP;
                case Keys::DOWN:      return KI_DOWN;
                default:              return KI_UNKNOWN;
        }
}

int UIInputTranslator::TranslateModifiers(Keymod mod) {

        int rml_mod = 0;
        if (mod & Keymods::CTRL)  rml_mod |= Rml::Input::KM_CTRL;
        if (mod & Keymods::SHIFT) rml_mod |= Rml::Input::KM_SHIFT;
        if (mod & Keymods::ALT)   rml_mod |= Rml::Input::KM_ALT;
        return rml_mod;
}

} // namespace SE

#endif // SE_UI_ENABLED
