
#include <GlobalTypes.h>

#include <InputMappingContext.h>
#include <EventManager.h>
#include <CommonEvents.h>

#include <glm/common.hpp>
#include <cstddef>

namespace SE {

InputMappingContext::InputMappingContext(TSceneTree::TSceneNodeExact* pOwner)
        : pNode(pOwner) {

        auto& em = GetSystem<EventManager>();
        em.AddListener<EKeyDown,           &InputMappingContext::OnKeyDown>(this);
        em.AddListener<EKeyUp,             &InputMappingContext::OnKeyUp>(this);
        em.AddListener<EMouseMove,         &InputMappingContext::OnMouseMove>(this);
        em.AddListener<EMouseButtonDown,   &InputMappingContext::OnMouseButtonDown>(this);
        em.AddListener<EMouseButtonUp,     &InputMappingContext::OnMouseButtonUp>(this);
        em.AddListener<EGamepadAxis,       &InputMappingContext::OnGamepadAxis>(this);
        em.AddListener<EGamepadButtonDown, &InputMappingContext::OnGamepadButtonDown>(this);
        em.AddListener<EGamepadButtonUp,   &InputMappingContext::OnGamepadButtonUp>(this);
        em.AddListener<EInputUpdate,       &InputMappingContext::OnInputUpdate>(this);
}

InputMappingContext::~InputMappingContext() noexcept {

        auto& em = GetSystem<EventManager>();
        em.RemoveListener<EKeyDown,           &InputMappingContext::OnKeyDown>(this);
        em.RemoveListener<EKeyUp,             &InputMappingContext::OnKeyUp>(this);
        em.RemoveListener<EMouseMove,         &InputMappingContext::OnMouseMove>(this);
        em.RemoveListener<EMouseButtonDown,   &InputMappingContext::OnMouseButtonDown>(this);
        em.RemoveListener<EMouseButtonUp,     &InputMappingContext::OnMouseButtonUp>(this);
        em.RemoveListener<EGamepadAxis,       &InputMappingContext::OnGamepadAxis>(this);
        em.RemoveListener<EGamepadButtonDown, &InputMappingContext::OnGamepadButtonDown>(this);
        em.RemoveListener<EGamepadButtonUp,   &InputMappingContext::OnGamepadButtonUp>(this);
        em.RemoveListener<EInputUpdate,       &InputMappingContext::OnInputUpdate>(this);
}

// ---------------------------------------------------------------------------
// Raw event handlers — update internal tracking state
// ---------------------------------------------------------------------------

void InputMappingContext::OnKeyDown(const Event& e) {

        const auto& oEvt = e.Get<EKeyDown>();
        sHeldKeys.insert(oEvt.key);
        sPressedKeys.insert(oEvt.key);
        sHeldScancodes.insert(oEvt.scancode);
        sPressedScancodes.insert(oEvt.scancode);
}

void InputMappingContext::OnKeyUp(const Event& e) {

        const auto& oEvt = e.Get<EKeyUp>();
        sHeldKeys.erase(oEvt.key);
        sHeldScancodes.erase(oEvt.scancode);
}

void InputMappingContext::OnMouseMove(const Event& e) {

        mouse_delta_acc += e.Get<EMouseMove>().delta;
}

void InputMappingContext::OnMouseButtonDown(const Event& e) {

        const uint32_t bit = static_cast<uint32_t>(e.Get<EMouseButtonDown>().button);
        held_mouse_buttons    |= bit;
        pressed_mouse_buttons |= bit;
}

void InputMappingContext::OnMouseButtonUp(const Event& e) {

        const uint32_t bit = static_cast<uint32_t>(e.Get<EMouseButtonUp>().button);
        held_mouse_buttons &= ~bit;
}

void InputMappingContext::OnGamepadAxis(const Event& e) {

        const auto& oEvt = e.Get<EGamepadAxis>();
        if (oEvt.axis_id >= gamepad_axes.size()) return;
        // Normalize from int16 [-32768, 32767] to float [-1, 1]
        constexpr float kScale = 1.f / 32767.f;
        gamepad_axes[oEvt.axis_id] = glm::clamp(oEvt.value * kScale, -1.f, 1.f);
}

void InputMappingContext::OnGamepadButtonDown(const Event& e) {

        const auto& oEvt = e.Get<EGamepadButtonDown>();
        const auto idx = static_cast<size_t>(oEvt.button);
        if (idx >= gamepad_buttons_held.size()) return;
        gamepad_buttons_held[idx]    = true;
        gamepad_buttons_pressed[idx] = true;
}

void InputMappingContext::OnGamepadButtonUp(const Event& e) {

        const auto& oEvt = e.Get<EGamepadButtonUp>();
        const auto idx = static_cast<size_t>(oEvt.button);
        if (idx >= gamepad_buttons_held.size()) return;
        gamepad_buttons_held[idx] = false;
}

// ---------------------------------------------------------------------------
// EInputUpdate — flush accumulated state to sibling InputState
// ---------------------------------------------------------------------------

void InputMappingContext::OnInputUpdate(const Event& /*e*/) {

        InputState* pInput = pNode->template GetComponent<InputState>();
        if (!pInput) return;

        // Start from a clean state; each binding OR/adds its contribution.
        *pInput = InputState{};

        float move_x = 0.f, move_y = 0.f;
        float look_x = 0.f, look_y = 0.f;

        for (const auto& b : vBindings) {
                float value = 0.f;

                switch (b.source) {
                case InputSource::KEY_DOWN:
                        value = sHeldKeys.count(static_cast<Key>(b.code)) ? 1.f : 0.f;
                        break;
                case InputSource::KEY_PRESS:
                        value = sPressedKeys.count(static_cast<Key>(b.code)) ? 1.f : 0.f;
                        break;
                case InputSource::SCANCODE_DOWN:
                        value = sHeldScancodes.count(static_cast<Scancode>(b.code)) ? 1.f : 0.f;
                        break;
                case InputSource::SCANCODE_PRESS:
                        value = sPressedScancodes.count(static_cast<Scancode>(b.code)) ? 1.f : 0.f;
                        break;
                case InputSource::MOUSE_AXIS_X:
                        value = static_cast<float>(mouse_delta_acc.x);
                        break;
                case InputSource::MOUSE_AXIS_Y:
                        value = static_cast<float>(mouse_delta_acc.y);
                        break;
                case InputSource::MOUSE_BUTTON_DOWN:
                        value = (held_mouse_buttons & static_cast<uint32_t>(b.code)) ? 1.f : 0.f;
                        break;
                case InputSource::MOUSE_BUTTON_PRESS:
                        value = (pressed_mouse_buttons & static_cast<uint32_t>(b.code)) ? 1.f : 0.f;
                        break;
                case InputSource::GAMEPAD_BUTTON_DOWN: {
                        const auto idx = static_cast<size_t>(b.code);
                        value = (idx < gamepad_buttons_held.size() && gamepad_buttons_held[idx]) ? 1.f : 0.f;
                        break;
                }
                case InputSource::GAMEPAD_BUTTON_PRESS: {
                        const auto idx = static_cast<size_t>(b.code);
                        value = (idx < gamepad_buttons_pressed.size() && gamepad_buttons_pressed[idx]) ? 1.f : 0.f;
                        break;
                }
                case InputSource::GAMEPAD_AXIS: {
                        const auto idx = static_cast<size_t>(b.code);
                        value = (idx < gamepad_axes.size()) ? gamepad_axes[idx] : 0.f;
                        break;
                }
                }

                const float scaled = value * b.scale;

                switch (b.action) {
                case InputAction::MOVE_AXIS_X:       move_x           += scaled; break;
                case InputAction::MOVE_AXIS_Y:       move_y           += scaled; break;
                case InputAction::LOOK_AXIS_X:       look_x           += scaled; break;
                case InputAction::LOOK_AXIS_Y:       look_y           += scaled; break;
                case InputAction::JUMP_PRESSED:      pInput->jump_pressed     |= (scaled > 0.f); break;
                case InputAction::JUMP_HELD:         pInput->jump_held        |= (scaled > 0.f); break;
                case InputAction::SPRINT_HELD:       pInput->sprint_held      |= (scaled > 0.f); break;
                case InputAction::CROUCH_HELD:       pInput->crouch_held      |= (scaled > 0.f); break;
                case InputAction::INTERACT_PRESSED:  pInput->interact_pressed |= (scaled > 0.f); break;
                }
        }

        pInput->move_axis = {glm::clamp(move_x, -1.f, 1.f), glm::clamp(move_y, -1.f, 1.f)};
        pInput->look_axis = {look_x, look_y};

        // Reset per-frame state
        mouse_delta_acc         = {0, 0};
        sPressedKeys.clear();
        sPressedScancodes.clear();
        pressed_mouse_buttons   = 0;
        gamepad_buttons_pressed.fill(false);
}

} // namespace SE
