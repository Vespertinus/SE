
#ifndef APP_CHARACTER_ANIMATION_SYSTEM_H
#define APP_CHARACTER_ANIMATION_SYSTEM_H 1

#include <vector>
#include <cstdint>

namespace SE {

class CharacterController;
class Animator;
class Event;

// ---------------------------------------------------------------------------
// CharacterAnimationSystem — the stateless movement→animation mapper.
//
// This is the only bridge between the locomotion layer (CharacterController /
// CharacterMovementSystem) and the pose layer (Animator / AnimGraph), analogous
// to Unreal's "Event Blueprint Update Animation". Each frame it reads a
// character's published movement state and writes AnimGraph parameters; it holds
// no state machine of its own.
//
// The CharacterController lives on the actor node while its Animator lives on a
// skinned child node, so the pairing cannot be auto-derived from a single
// component. The scene (the integrator) registers each pair via Register().
//
// Per frame, for every live link:
//   - SetFloat("speed", horizontal speed)
//   - SetBool ("grounded", grounded)
//   - SetFloat("vertical_velocity", v.y)
//   - SetPoseSource(EXTERNAL) while the controller is in RAGDOLL, else GRAPH.
// On ECharacterJumped / ECharacterLanded it fires the "jump" / "land" triggers.
// ---------------------------------------------------------------------------
class CharacterAnimationSystem {
public:
        CharacterAnimationSystem();
        ~CharacterAnimationSystem() noexcept;

        // Associate a character node's controller with the Animator on its visual
        // child. The link is pruned automatically once the character node OR the
        // animator's own node expires — the raw component pointers are only
        // dereferenced while both nodes are alive.
        void Register(TSceneTree::TSceneNodeWeak pCharNode,
                      CharacterController* pCC,
                      Animator* pAnimator);

private:
        void OnUpdate(const Event& e);
        void OnCharacterJumped(const Event& e);
        void OnCharacterLanded(const Event& e);

        struct Link {
                TSceneTree::TSceneNodeWeak pCharNode;
                // Node owning the Animator (visual child) — lifetime guard for pAnimator,
                // which is a component on a DIFFERENT node than the character.
                TSceneTree::TSceneNodeWeak pVisualNode;
                uint32_t                   char_id   = 0;
                CharacterController*        pCC       = nullptr;
                Animator*                  pAnimator = nullptr;
                // Last parameter values written to the animator — unchanged values are
                // not re-written (each write is an unordered_map operation in the
                // graph's parameter store)
                float last_speed    = 0.f;
                float last_vert_vel = 0.f;
                bool  last_grounded = false;
        };

        std::vector<Link> vLinks;
};

} // namespace SE

#endif
