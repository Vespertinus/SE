
namespace SE {

PhysicsDebugController::PhysicsDebugController() {
        auto & oEM = GetSystem<EventManager>();
        oEM.AddListener<EKeyDown, &PhysicsDebugController::OnKeyDown>(this);
}

PhysicsDebugController::~PhysicsDebugController() noexcept {
        auto & oEM = GetSystem<EventManager>();
        oEM.RemoveListener<EKeyDown, &PhysicsDebugController::OnKeyDown>(this);
        if (paused) GetSystem<PhysicsSystem>().SetPaused(false);
}

void PhysicsDebugController::OnKeyDown(const Event & oEvent) {
        auto & oPS = GetSystem<PhysicsSystem>();
        auto   key = oEvent.Get<EKeyDown>().key;

        if (key == Keys::P) {
                paused = !paused;
                oPS.SetPaused(paused);
                log_d("PhysicsDebugController: {}", paused ? "paused" : "resumed");
        } else if (key == Keys::N && paused) {
                oPS.StepOnce();
                log_d("PhysicsDebugController: step");
        }
}

void PhysicsDebugController::SetPaused(bool paused) {

        this->paused = paused;
        GetSystem<PhysicsSystem>().SetPaused(this->paused);
        log_d("PhysicsDebugController: {}", this->paused ? "paused" : "resumed");
}

} // namespace SE
