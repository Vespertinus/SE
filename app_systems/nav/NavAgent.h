
#ifndef APP_NAV_NAV_AGENT_H
#define APP_NAV_NAV_AGENT_H 1

#include <string>

#include <StrID.h>
#include <Component_generated.h>

namespace SE {

// ---------------------------------------------------------------------------
// NavAgentDesc — programmatic construction (tests, tools).
// ---------------------------------------------------------------------------

struct NavAgentDesc {
        std::string agent_class;
        float       radius    = 0.f;   // 0 = class default (query half-extents + crowd params)
        float       height    = 0.f;
        bool        avoidance = true;  // enroll into the crowd when a CrowdSystem is composed
};

// ---------------------------------------------------------------------------
// Posted by NavAgent::Enable()/Disable() (synchronously, before
// NavigationSystem registration so enrollees see the agent fully described).
// CrowdSystem listens to build its avoidance roster; apps without a crowd
// system never subscribe and the events are no-ops. Trivially copyable.
// ---------------------------------------------------------------------------

struct ENavAgentRegistered   { NavAgent * pAgent = nullptr; };
struct ENavAgentUnregistering{ NavAgent * pAgent = nullptr; };

// ---------------------------------------------------------------------------
// NavAgent — marks an entity as a navigation agent of one
// agent class (.senm / dtQueryFilter). Registers with NavigationSystem in
// Enable()/Disable() (StateMachine registration pattern — component lifecycle
// rule: no ctor-time registrations).
//
// Composition: apps putting NavAgent in TCustomComponents must compose
// NavigationSystem (compile-time GetSystem dependency). When SE_NAV_ENABLED is
// undefined the registration compiles out and the component is an inert data
// carrier (NavArea precedent).
// ---------------------------------------------------------------------------

class NavAgent {
public:
        using TSerialized = FlatBuffers::NavAgent;

        NavAgent(TSceneTree::TSceneNodeExact * pNode, const NavAgentDesc & desc);
        NavAgent(TSceneTree::TSceneNodeExact * pNode, const FlatBuffers::NavAgent * pFB);
        ~NavAgent() noexcept;

        void Enable();
        void Disable();

        const std::string & AgentClass() const { return oDesc.agent_class; }
        StrID               AgentClassID() const { return classID; }
        float               Radius() const { return oDesc.radius; }
        float               Height() const { return oDesc.height; }
        bool                Avoidance() const { return oDesc.avoidance; }
        std::string         Name() const { return pNode ? pNode->GetName() : std::string(); }
        TSceneTree::TSceneNodeExact * Node() const { return pNode; }

        std::string Str() const;

private:
        TSceneTree::TSceneNodeExact * pNode = nullptr;
        NavAgentDesc                  oDesc;
        StrID                         classID;
};

} // namespace SE

#endif
