
#include <GlobalTypes.h>

#include <CharacterDebugger.h>
#include <CharacterController.h>
#include <MovementEvents.h>

#include <imgui.h>
#include <glm/common.hpp>

namespace SE {

namespace {
constexpr const char* ModeStr(MovementMode mode) {

        switch (mode) {
        case MovementMode::GROUNDED: return "GROUNDED";
        case MovementMode::AIRBORNE: return "AIRBORNE";
        case MovementMode::SWIMMING: return "SWIMMING";
        case MovementMode::CLIMBING: return "CLIMBING";
        case MovementMode::FLYING:   return "FLYING";
        default:                     return "CUSTOM";
        }
}
} // namespace

CharacterDebugger::CharacterDebugger() {
        GetSystem<EventManager>().AddListener<EPostRenderUpdate, &CharacterDebugger::OnPostRenderUpdate>(this);
}

CharacterDebugger::~CharacterDebugger() noexcept {
        GetSystem<EventManager>().RemoveListener<EPostRenderUpdate, &CharacterDebugger::OnPostRenderUpdate>(this);
}

void CharacterDebugger::SetSceneTree(TSceneTree* pTree) {

        mEntries.clear();
        if (!pTree) return;
        TSceneTree::TSceneNode pRoot = pTree->GetRoot();
        if (!pRoot) return;

        pRoot->DepthFirstWalk([this](auto& oNode) {
                if (oNode.template GetComponent<CharacterController>())
                        Register(oNode.GetShared());
                return true;
        });
}

void CharacterDebugger::Register(TSceneTree::TSceneNodeWeak pWeak) {

        auto pNode = pWeak.lock();
        if (!pNode) return;
        mEntries.try_emplace(pNode->GetID(), pWeak);
}

void CharacterDebugger::Unregister(TSceneTree::TSceneNodeWeak pWeak) {

        auto pNode = pWeak.lock();
        if (!pNode) return;
        mEntries.erase(pNode->GetID());
}

void CharacterDebugger::OnPostRenderUpdate(const Event& /*e*/) {

        if (!visible) return;   // panel hidden — no ImGui work at all
        if (!ImGui::Begin("Characters")) { ImGui::End(); return; }

        for (auto it = mEntries.begin(); it != mEntries.end(); ) {
                auto pNode = it->second.lock();
                if (!pNode) { it = mEntries.erase(it); continue; }
                ++it;

                auto* comp = pNode->GetComponent<CharacterController>();
                if (!comp) continue;

                const std::string label = fmt::format("{}###cc_{}", pNode->GetName(),
                        reinterpret_cast<uintptr_t>(comp));

                if (!ImGui::CollapsingHeader(label.c_str())) continue;

                ImGui::Text("Mode:     %s", ModeStr(comp->GetMovementMode()));
                ImGui::Text("Grounded: %s", comp->IsGrounded() ? "yes" : "no");

                const glm::vec3 vel     = comp->GetVelocity();
                const float     h_speed = glm::length(glm::vec2{vel.x, vel.z});
                ImGui::Text("Velocity: %.2f  %.2f  %.2f   |h|=%.2f",
                        static_cast<double>(vel.x),
                        static_cast<double>(vel.y),
                        static_cast<double>(vel.z),
                        static_cast<double>(h_speed));

                ImGui::Text("Coyote:   %.3f s", static_cast<double>(comp->GetCoyoteTimer()));
                ImGui::Text("JumpBuf:  %.3f s", static_cast<double>(comp->GetJumpBufferTimer()));
                ImGui::Text("Airborne: %.3f s", static_cast<double>(comp->GetTimeAirborne()));

                ImGui::Separator();
        }

        ImGui::End();
}

} // namespace SE
