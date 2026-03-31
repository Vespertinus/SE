#ifndef __UI_INPUT_TRANSLATOR_H__
#define __UI_INPUT_TRANSLATOR_H__ 1

#ifdef SE_UI_ENABLED

#include <vector>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Input.h>
#include <InputEvents.h>

namespace SE {

class UIInputTranslator {

        std::vector<Rml::Context *> vContexts;

        void OnKeyDown           (const Event & oEvent);
        void OnKeyUp             (const Event & oEvent);
        void OnTextInput         (const Event & oEvent);
        void OnMouseMove         (const Event & oEvent);
        void OnMouseButtonDown   (const Event & oEvent);
        void OnMouseButtonUp     (const Event & oEvent);
        void OnMouseWheel        (const Event & oEvent);
        void OnGamepadButtonDown (const Event & oEvent);
        void OnGamepadButtonUp   (const Event & oEvent);
        void OnGamepadAxis       (const Event & oEvent);

        static Rml::Input::KeyIdentifier TranslateKey(Key key);
        static int                       TranslateModifiers(Keymod mod);

        // Left-stick d-pad simulation: [LEFT, RIGHT, UP, DOWN]
        bool axis_active[4]{};

        // -1 = no modal active (all contexts receive input)
        // >= 0 = contexts with index < modal_min_layer are skipped
        int  modal_min_layer{-1};

        template <typename Fn>
        void ForContexts(Fn && fn) const {
                for (int i = 0; i < static_cast<int>(vContexts.size()); ++i)
                        if (modal_min_layer < 0 || i >= modal_min_layer)
                                if (vContexts[static_cast<size_t>(i)])
                                        fn(vContexts[static_cast<size_t>(i)]);
        }

public:
        explicit UIInputTranslator(std::vector<Rml::Context *> contexts);
        ~UIInputTranslator();

        bool IsCapturingMouse()    const;
        bool IsCapturingKeyboard() const;

        // -1 = no modal; 0+ = only layers >= layer receive input.
        void SetModalMinLayer(int layer) { modal_min_layer = layer; }
};

} // namespace SE

#endif // SE_UI_ENABLED
#endif // __UI_INPUT_TRANSLATOR_H__
