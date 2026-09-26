
#ifdef SE_IMPL

#include <nav/PathFollower.h>

#ifdef SE_NAV_ENABLED
#include <nav/CrowdSystem.h>
#include <EventManager.h>
#include <GlobalTypes.h>
#endif

#include <Logging.h>

#include <cmath>
#include <stdexcept>

namespace SE {

// ---------------------------------------------------------------------------

PathFollower::PathFollower(TSceneTree::TSceneNodeExact * pNewNode, const PathFollowerDesc & desc)
        : pNode(pNewNode)
        , oDesc(desc) {

        if (oDesc.arrival_radius < 0.f || oDesc.slow_radius < 0.f || oDesc.repath_interval < 0.f) {
                throw std::runtime_error("PathFollower: radii/interval must be >= 0");
        }
        if (oDesc.slow_radius < oDesc.arrival_radius) {
                throw std::runtime_error("PathFollower: slow_radius must be >= arrival_radius");
        }
}

PathFollower::PathFollower(TSceneTree::TSceneNodeExact * pNewNode, const FlatBuffers::PathFollower * pFB)
        : pNode(pNewNode) {

        if (pFB) {
                oDesc.arrival_radius       = pFB->arrival_radius();
                oDesc.slow_radius          = pFB->slow_radius();
                oDesc.repath_interval      = pFB->repath_interval();
                oDesc.accept_partial_paths = pFB->accept_partial_paths();
        }

        if (oDesc.arrival_radius < 0.f || oDesc.slow_radius < 0.f || oDesc.repath_interval < 0.f) {
                throw std::runtime_error("PathFollower: radii/interval must be >= 0");
        }
        if (oDesc.slow_radius < oDesc.arrival_radius) {
                throw std::runtime_error("PathFollower: slow_radius must be >= arrival_radius");
        }
}

PathFollower::~PathFollower() noexcept {
        Disable();
}

// ---------------------------------------------------------------------------

void PathFollower::Enable() {
#ifdef SE_NAV_ENABLED
        GetSystem<CrowdSystem>().AddFollower(this);
#endif
}

void PathFollower::Disable() {
#ifdef SE_NAV_ENABLED
        GetSystem<CrowdSystem>().RemoveFollower(this);
#endif
}

// ---------------------------------------------------------------------------

void PathFollower::MoveTo(const glm::vec3 & vNewGoal) {
        vGoal        = vNewGoal;
        eStatus      = PathStatus::Following;
        request_path = true;   // CrowdSystemT paths on its next solver tick
}

void PathFollower::Stop() {
        eStatus      = PathStatus::Idle;
        request_path = false;
}

// ---------------------------------------------------------------------------

std::string PathFollower::Str() const {
        static const char * names[] = { "Idle", "Following", "Arrived", "Failed" };
        return fmt::format("PathFollower('{}' {} goal=({:.1f},{:.1f},{:.1f}))",
                           Name(), names[int(eStatus)], vGoal.x, vGoal.y, vGoal.z);
}

} // namespace SE

#endif // SE_IMPL
