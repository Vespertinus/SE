
#ifndef APP_SPRINT_STAMINA_H
#define APP_SPRINT_STAMINA_H 1

#include <string>

namespace SE {

// component tracking sprint stamina [0, 1].
// Scene/application code reads stamina and clears sprint_held on InputState when empty.
struct SprintStamina {
        SprintStamina() = default;
        explicit SprintStamina(TSceneTree::TSceneNodeExact*) {}

        float stamina      = 1.0f;
        float drain_rate   = 0.18f;  // depletion per second while sprinting
        float recovery_rate = 0.25f; // recovery per second when not sprinting

        void Enable()  {}
        void Disable() {}

        std::string Str()       const { return "SprintStamina"; }
        void        DrawDebug() const {}
};

} // namespace SE

#endif
