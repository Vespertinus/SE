
#include <CharacterAnimationSystem.h>
#include <CharacterController.h>
#include <CharacterMovementSystem.h>
#include <Animator.h>
#include <MovementEvents.h>
#include <EventManager.h>
#include <CommonEvents.h>
#include <Global.h>
#include <StrID.h>
#include <Logging.h>

#include <glm/geometric.hpp>
#include <algorithm>

namespace SE {

CharacterAnimationSystem::CharacterAnimationSystem() {
        auto& em = GetSystem<EventManager>();
        em.AddListener<EUpdate,          &CharacterAnimationSystem::OnUpdate>(this);
        em.AddListener<ECharacterJumped, &CharacterAnimationSystem::OnCharacterJumped>(this);
        em.AddListener<ECharacterLanded, &CharacterAnimationSystem::OnCharacterLanded>(this);
}

CharacterAnimationSystem::~CharacterAnimationSystem() noexcept {
        auto& em = GetSystem<EventManager>();
        em.RemoveListener<EUpdate,          &CharacterAnimationSystem::OnUpdate>(this);
        em.RemoveListener<ECharacterJumped, &CharacterAnimationSystem::OnCharacterJumped>(this);
        em.RemoveListener<ECharacterLanded, &CharacterAnimationSystem::OnCharacterLanded>(this);
}

void CharacterAnimationSystem::Register(TSceneTree::TSceneNodeWeak pCharNode,
                                        CharacterController* pCC,
                                        Animator* pAnimator) {
        auto pNode = pCharNode.lock();
        if (!pNode || !pCC || !pAnimator) return;
        Link oLink;
        oLink.pCharNode   = pCharNode;
        oLink.pVisualNode = pAnimator->GetNodeWeak();
        oLink.char_id     = pNode->GetID();
        oLink.pCC         = pCC;
        oLink.pAnimator   = pAnimator;
        vLinks.push_back(std::move(oLink));
}

void CharacterAnimationSystem::OnUpdate(const Event& e) {

        const float dt = e.Get<EUpdate>().last_frame_time;

        static const StrID sSpeed   {"speed"};
        static const StrID sGrounded{"grounded"};
        static const StrID sVertVel {"vertical_velocity"};

        // Single pass: drive live links, erase expired ones in place (expired()
        // thus runs once per link, not twice as with a second remove_if pass).
        for (auto it = vLinks.begin(); it != vLinks.end(); ) {
                Link& link = *it;

                // pAnimator lives on the visual child node — dereference it only
                // while that node is alive too, not just the character node.
                if (link.pCharNode.expired() || link.pVisualNode.expired()) {
                        it = vLinks.erase(it);
                        continue;
                }

                const glm::vec3 vel = link.pCC->GetVelocity();

                // Pose-source seam: physics owns the skeleton while ragdolling.
                link.pAnimator->SetPoseSource(link.pCC->IsRagdoll()
                                ? Animator::PoseSource::EXTERNAL
                                : Animator::PoseSource::GRAPH);

                if (link.pCC->IsRagdoll()) {  // graph params irrelevant while external
                        ++it;
                        continue;
                }

                // Write params only when they changed — each write is an
                // unordered_map operation inside the graph's parameter store.
                const float  speed    = glm::length(glm::vec2{vel.x, vel.z});
                const bool   grounded = link.pCC->IsGrounded();
                const float  vert_vel = vel.y;
                if (speed != link.last_speed) {
                        link.pAnimator->SetFloat(sSpeed, speed);
                        link.last_speed = speed;
                }
                if (grounded != link.last_grounded) {
                        link.pAnimator->SetBool(sGrounded, grounded);
                        link.last_grounded = grounded;
                }
                if (vert_vel != link.last_vert_vel) {
                        link.pAnimator->SetFloat(sVertVel, vert_vel);
                        link.last_vert_vel = vert_vel;
                }

                // Root motion: forward the flag and hand the animator's extracted
                // root-bone delta to the locomotion layer as velocity. The delta was
                // produced by the previous frame's graph Update (Animators evaluate
                // after this system within EUpdate) — a constant one-frame latency.
                link.pAnimator->SetUseRootMotion(link.pCC->UseRootMotion());
                if (link.pCC->UseRootMotion()) {
                        GetSystem<CharacterMovementSystem>().ApplyRootMotionDelta(
                                        *link.pCC, link.pAnimator->GetRootMotionDelta().translation, dt);
                }

                ++it;
        }
}

void CharacterAnimationSystem::OnCharacterJumped(const Event& e) {
        static const StrID sJump{"jump"};
        auto pActor = e.Get<ECharacterJumped>().pActor.lock();
        if (!pActor) return;
        const uint32_t id = pActor->GetID();
        for (auto& link : vLinks)
                if (link.char_id == id && !link.pCharNode.expired()) {
                        link.pAnimator->SetTrigger(sJump);
                        return;
                }
}

void CharacterAnimationSystem::OnCharacterLanded(const Event& e) {
        static const StrID sLand{"land"};
        auto pActor = e.Get<ECharacterLanded>().pActor.lock();
        if (!pActor) return;
        const uint32_t id = pActor->GetID();
        for (auto& link : vLinks)
                if (link.char_id == id && !link.pCharNode.expired()) {
                        link.pAnimator->SetTrigger(sLand);
                        return;
                }
}

} // namespace SE
