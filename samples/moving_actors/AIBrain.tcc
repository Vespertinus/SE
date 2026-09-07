
#ifndef APP_AI_BRAIN_TCC
#define APP_AI_BRAIN_TCC 1

#include <AIBrain.h>
#include <InputState.h>
#include <hsm/StateMachine.h>
#include <hsm/ParameterStore.h>
#include <CommonEvents.h>
#include <Global.h>
#include <GlobalTypes.h>
#include <StrID.h>

#include <glm/geometric.hpp>
#include <cmath>
#include <cstdlib>

namespace SE {

AIBrain::AIBrain(TSceneTree::TSceneNodeExact* pOwner) : pNode(pOwner) {
        PickNewDirection();
        wander_timer = kWalkDuration;
        idle_timer   = kIdleDuration;

        GetSystem<EventManager>().AddListener<EUpdate, &AIBrain::OnUpdate>(this);
}

AIBrain::~AIBrain() noexcept {
        GetSystem<EventManager>().RemoveListener<EUpdate, &AIBrain::OnUpdate>(this);
}

void AIBrain::OnUpdate(const Event& e) {
        const float dt = e.Get<EUpdate>().last_frame_time;

        // Static StrIDs — constructed once, not re-hashed every frame per NPC.
        static const StrID sDistToPlayer("dist_to_player");
        static const StrID sChase("Chase");
        static const StrID sWander("Wander");
        static const StrID sWanderDone("wander_done");
        static const StrID sIdleDone("idle_done");

        // Resolve siblings per update (compile-time-typed O(k) scans) — cached
        // pointers would go stale if a component is destroyed and re-created
        // on this living node.
        auto* pInputState   = pNode->GetComponent<InputState>();
        auto* pStateMachine = pNode->GetComponent<StateMachine>();
        if (!pInputState || !pStateMachine) return;

        const glm::vec3 my_pos = pNode->GetTransform().GetWorldPos();

        // --- Perception → StateMachine params -------------------------------
        float dist = 1.0e6f;
        glm::vec2 to_target {0.f, 0.f};
        if (auto pT = pTarget.lock()) {
                const glm::vec3 tp = pT->GetTransform().GetWorldPos();
                to_target = {tp.x - my_pos.x, tp.z - my_pos.z};
                dist = glm::length(to_target);
        }

        auto& oParams = pStateMachine->GetParams();
        // Unchanged values are not re-written — each write is an unordered_map
        // operation in the parameter store. (The plain InputState field writes
        // below are POD stores; guarding those would cost more than it saves.)
        if (dist != last_dist_to_player) {
                oParams.SetFloat(sDistToPlayer, dist);
                last_dist_to_player = dist;
        }

        // --- Action per current behavior state ------------------------------
        const StrID state = pStateMachine->GetCurrentState();

        if (state == sChase) {
                pInputState->move_axis = (glm::length(to_target) > 1e-3f)
                        ? glm::normalize(to_target)
                        : glm::vec2{0.f, 0.f};
        } else if (state == sWander) {
                pInputState->move_axis = v_direction;
                wander_timer -= dt;
                if (wander_timer <= 0.f) {
                        oParams.SetTrigger(sWanderDone);
                        idle_timer = kIdleDuration;
                }
        } else { // Idle (and any unrecognised state)
                pInputState->move_axis = {0.f, 0.f};
                idle_timer -= dt;
                if (idle_timer <= 0.f) {
                        PickNewDirection();
                        wander_timer = kWalkDuration;
                        oParams.SetTrigger(sIdleDone);
                }
        }

        pInputState->sprint_held  = false;
        pInputState->jump_pressed = false;
        pInputState->jump_held    = false;
}

void AIBrain::PickNewDirection() {
        const float angle = (static_cast<float>(std::rand()) / RAND_MAX) * 6.2831853f;
        v_direction = {std::cos(angle), std::sin(angle)};
}

} // namespace SE

#endif
