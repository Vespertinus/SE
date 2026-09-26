
#ifdef SE_IMPL

#include <nav/NavAgent.h>

#ifdef SE_NAV_ENABLED
#include <nav/NavigationSystem.h>
#include <EventManager.h>
#include <GlobalTypes.h>
#endif

#include <Logging.h>

#include <stdexcept>

namespace SE {

// ---------------------------------------------------------------------------

NavAgent::NavAgent(TSceneTree::TSceneNodeExact * pNewNode, const NavAgentDesc & desc)
        : pNode(pNewNode)
        , oDesc(desc) {

        if (oDesc.agent_class.empty()) {
                throw std::runtime_error("NavAgent: agent_class is required");
        }
        if (oDesc.radius < 0.f || oDesc.height < 0.f) {
                throw std::runtime_error("NavAgent: radius/height overrides must be >= 0");
        }
        classID = StrID(oDesc.agent_class);
}

NavAgent::NavAgent(TSceneTree::TSceneNodeExact * pNewNode, const FlatBuffers::NavAgent * pFB)
        : pNode(pNewNode) {

        if (!pFB || !pFB->agent_class() || pFB->agent_class()->size() == 0) {
                throw std::runtime_error("NavAgent: agent_class is required");
        }

        oDesc.agent_class = pFB->agent_class()->str();
        oDesc.radius      = pFB->radius();
        oDesc.height      = pFB->height();
        oDesc.avoidance   = pFB->avoidance();

        if (oDesc.radius < 0.f || oDesc.height < 0.f) {
                throw std::runtime_error("NavAgent: radius/height overrides must be >= 0");
        }
        classID = StrID(oDesc.agent_class);
}

NavAgent::~NavAgent() noexcept {
        Disable();
}

// ---------------------------------------------------------------------------

void NavAgent::Enable() {
#ifdef SE_NAV_ENABLED
        GetSystem<EventManager>().TriggerEvent(ENavAgentRegistered{this});
        GetSystem<NavigationSystem>().AddComponent(this);
#endif
}

void NavAgent::Disable() {
#ifdef SE_NAV_ENABLED
        GetSystem<EventManager>().TriggerEvent(ENavAgentUnregistering{this});
        GetSystem<NavigationSystem>().RemoveComponent(this);
#endif
}

// ---------------------------------------------------------------------------

std::string NavAgent::Str() const {
        return fmt::format("NavAgent('{}' r={} h={} avoidance={})",
                           oDesc.agent_class, oDesc.radius, oDesc.height, oDesc.avoidance);
}

} // namespace SE

#endif // SE_IMPL
