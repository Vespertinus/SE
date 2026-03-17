
namespace SE {

BallController::BallController(TSceneTree::TSceneNodeExact * pNewNode, const glm::vec3 & vStartPos)
        : pNode(pNewNode), vStart(vStartPos) {

        auto * pRB = pNode->GetComponent<RigidBody>();
        hBody = pRB->GetHandle();
}

BallController::~BallController() noexcept {

        Disable();
}

void BallController::OnUpdate(const Event & oEvent) {

        auto & oIM = GetSystem<InputManager>();

        glm::vec3 oImpulse{0.0f};
        if (oIM.GetKeyDown(Keys::W)) oImpulse.z -= impulse;
        if (oIM.GetKeyDown(Keys::S)) oImpulse.z += impulse;
        if (oIM.GetKeyDown(Keys::A)) oImpulse.x -= impulse;
        if (oIM.GetKeyDown(Keys::D)) oImpulse.x += impulse;

        if (glm::length(oImpulse) > 0.0f) {
                GetSystem<PhysicsSystem>().ApplyImpulse(hBody, oImpulse);
                //log_d("apply impulse: ({}, {}, {})", oImpulse.x, oImpulse.y, oImpulse.z);
        }
}

void BallController::OnKeyDown(const Event & oEvent) {

        if (oEvent.Get<EKeyDown>().key == Keys::R) {
                auto & oPS = GetSystem<PhysicsSystem>();
                oPS.Teleport(hBody, vStart, glm::quat{1.0f, 0.0f, 0.0f, 0.0f});
                oPS.SetLinearVelocity(hBody, glm::vec3{0.0f});
        }
}

void BallController::Enable() {

        auto & oEM = TEngine::Instance().Get<EventManager>();
        oEM.AddListener<EInputUpdate, &BallController::OnUpdate >(this);
        oEM.AddListener<EKeyDown,     &BallController::OnKeyDown>(this);
}

void BallController::Disable() {

        auto & oEM = TEngine::Instance().Get<EventManager>();
        oEM.RemoveListener<EInputUpdate, &BallController::OnUpdate >(this);
        oEM.RemoveListener<EKeyDown,     &BallController::OnKeyDown>(this);
}

std::string BallController::Str() const { 
        return fmt::format("BallController: vStart: ({}, {}, {}), impulse: {}", 
                        vStart.x, 
                        vStart.y, 
                        vStart.z, 
                        impulse);
}

void BallController::DrawDebug() const { ;; }

} // namespace SE
