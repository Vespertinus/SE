
#ifndef APP_NAV_DETOUR_CROWD_AVOIDANCE_H
#define APP_NAV_DETOUR_CROWD_AVOIDANCE_H 1

#include <cstdint>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include <StrID.h>
#include <nav/LocalAvoidance.h>

#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>
#include <DetourCrowd.h>

namespace SE {

// ---------------------------------------------------------------------------
// DetourCrowdAvoidance — the DetourCrowd (RVO-style sampling +
// proximity grid + separation) implementation of the LocalAvoidance contract.
//
// dtCrowd binds exactly one dtNavMesh, so one dtCrowd instance exists per nav
// agent class (created lazily in BindClass). System-wide slot ids map into
// (class crowd, crowd agent index); the class dtQueryFilter is copied into the
// crowd's filter slot 0 so area costs / include-exclude flags stay authoritative
// in NavigationSystem.
//
// Velocity mode only: agents are added with targetState VELOCITY driven by
// requestMoveVelocity (SetDesired). dtCrowd's own path queue, corridors and
// topology optimization stay unused — corridor following is CrowdSystem's job
// through NavigationSystem primitives. dtCrowd still integrates its internal
// npos every Update; CrowdSystem overwrites it from the real bodies via
// SyncAgent before the next solve, so integration output is discarded by
// construction (dtCrowd never moves a body).
// ---------------------------------------------------------------------------

class DetourCrowdAvoidance {
public:

        struct SConfig {
                float separation_weight     = 2.0f;   // dtCrowdAgentParams::separationWeight
                float collision_query_range = 0.f;    // 0 = radius * 8 (dtCrowd doc default)
                float horiz_time            = 2.5f;   // predictive horizon for velocity sampling
        };

        bool Init(uint32_t nMaxAgents, float fMaxAgentRadius);
        void Shutdown();

        bool BindClass(StrID sClass, dtNavMesh * pMesh, const dtQueryFilter * pFilter);
        bool IsClassMeshCurrent(StrID sClass, const dtNavMesh * pMesh) const;
        void DropClass(StrID sClass);

        int       Enroll(StrID sClass, const glm::vec3 & vPos, const SAvoidanceParams & oParams);
        void      Unenroll(int nSlot);
        void      SyncAgent(int nSlot, const glm::vec3 & vPos, const glm::vec3 & vVel);
        void      SetDesired(int nSlot, const glm::vec3 & vDesired);
        void      Update(float fDt);
        glm::vec3 AdjustedVelocity(int nSlot);
        /** False for invalid/off-mesh agents — dtCrowd never solves their
         *  velocity, so callers must not steer by AdjustedVelocity. */
        bool      IsAgentWalking(int nSlot) const;

        uint32_t MaxAgents()    const { return nMaxAgents; }
        size_t   ActiveAgents() const;
        size_t   ClassCount()   const { return vCrowds.size(); }

private:

        struct SClassCrowd {
                StrID       sClass{};
                dtNavMesh * pMesh  = nullptr;
                dtCrowd   * pCrowd = nullptr;
        };

        struct SSlot {
                bool    active    = false;
                int32_t class_idx = -1;   // index into vCrowds
                int32_t crowd_idx = -1;   // dtCrowd agent index
        };

        dtCrowdAgentParams MakeAgentParams(const SAvoidanceParams & oParams) const;

        SConfig oConfig;

        uint32_t nMaxAgents      = 0;
        float    fMaxAgentRadius = 1.0f;

        std::vector<SClassCrowd>                      vCrowds;
        std::unordered_map<StrID, size_t>             mClassLookup;
        std::vector<SSlot>                            vSlots;
        std::vector<int32_t>                          vFreeSlots;
};

} // namespace SE

#endif
