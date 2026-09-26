
#ifdef SE_IMPL

#include <nav/DetourCrowdAvoidance.h>

#include <Logging.h>

#include <DetourCommon.h>
#include <DetourCrowd.h>
#include <DetourObstacleAvoidance.h>

#include <algorithm>

namespace SE {

// ---------------------------------------------------------------------------

bool DetourCrowdAvoidance::Init(const uint32_t nNewMaxAgents, const float fNewMaxAgentRadius) {

        if (nNewMaxAgents == 0 || fNewMaxAgentRadius <= 0.f) {
                log_e("DetourCrowdAvoidance::Init: bad pool sizing ({} agents, r={})",
                      nNewMaxAgents, fNewMaxAgentRadius);
                return false;
        }

        Shutdown();
        nMaxAgents      = nNewMaxAgents;
        fMaxAgentRadius = fNewMaxAgentRadius;
        return true;
}

void DetourCrowdAvoidance::Shutdown() {

        for (SClassCrowd & c : vCrowds) {
                if (c.pCrowd) { dtFreeCrowd(c.pCrowd); }
                c.pCrowd = nullptr;
        }
        vCrowds.clear();
        mClassLookup.clear();
        vSlots.clear();
        vFreeSlots.clear();
        nMaxAgents = 0;
}

// ---------------------------------------------------------------------------

bool DetourCrowdAvoidance::BindClass(const StrID sClass, dtNavMesh * pMesh, const dtQueryFilter * pFilter) {

        const auto it = mClassLookup.find(sClass);
        if (it != mClassLookup.end()) {
                if (vCrowds[it->second].pMesh == pMesh) { return true; }   // idempotent
                DropClass(sClass);                                         // mesh replaced
        }

        // Guard dtCrowd's proximity-grid assert (poolSize = maxAgents * 4):
        // refusing an un-Init'ed backend degrades to unavioded agents, never
        // to a solver crash.
        if (!pMesh || !pFilter || nMaxAgents == 0) { return false; }

        SClassCrowd c;
        c.sClass = sClass;
        c.pMesh  = pMesh;
        c.pCrowd = dtAllocCrowd();
        if (!c.pCrowd) {
                log_e("DetourCrowdAvoidance: dtAllocCrowd failed for class '{}'", uint64_t(sClass));
                return false;
        }
        // 3-arg init in this vendored version: maxAgentRadius sizes the
        // proximity grid + collision queries.
        if (dtStatusFailed(c.pCrowd->init(int(nMaxAgents), fMaxAgentRadius, pMesh))) {
                log_e("DetourCrowdAvoidance: dtCrowd::init failed for class '{}'", uint64_t(sClass));
                dtFreeCrowd(c.pCrowd);
                return false;
        }

        // One tuned adaptive-sampling preset (slot 0; every agent uses it).
        dtObstacleAvoidanceParams oa;
        oa.velBias       = 0.4f;
        oa.weightDesVel  = 1.0f;
        oa.weightCurVel  = 0.2f;
        oa.weightSide    = 0.5f;
        oa.weightToi     = 2.5f;
        oa.horizTime     = oConfig.horiz_time;
        oa.gridSize      = 20;
        oa.adaptiveDivs  = 7;
        oa.adaptiveRings = 2;
        oa.adaptiveDepth = 3;
        c.pCrowd->setObstacleAvoidanceParams(0, &oa);

        // The nav class's filter is authoritative (area costs carry the bake's
        // +1 slot mapping; include/exclude flags default walk-everything).
        *c.pCrowd->getEditableFilter(0) = *pFilter;

        mClassLookup.emplace(sClass, vCrowds.size());
        vCrowds.push_back(c);
        return true;
}

bool DetourCrowdAvoidance::IsClassMeshCurrent(const StrID sClass, const dtNavMesh * pMesh) const {
        const auto it = mClassLookup.find(sClass);
        return it != mClassLookup.end() && vCrowds[it->second].pMesh == pMesh;
}

void DetourCrowdAvoidance::DropClass(const StrID sClass) {

        const auto it = mClassLookup.find(sClass);
        if (it == mClassLookup.end()) { return; }

        const size_t dead = it->second;

        // Release every slot pointing at the dropped crowd (their crowd agents
        // die with dtFreeCrowd; the slots must not dangle).
        for (SSlot & s : vSlots) {
                if (s.active && s.class_idx == int32_t(dead)) { s.active = false; }
        }

        if (vCrowds[dead].pCrowd) { dtFreeCrowd(vCrowds[dead].pCrowd); }

        // Swap-erase, fixing up slot + lookup indices.
        const size_t last = vCrowds.size() - 1;
        if (dead != last) {
                vCrowds[dead] = std::move(vCrowds[last]);
                mClassLookup[vCrowds[dead].sClass] = dead;
                for (SSlot & s : vSlots) {
                        if (s.active && s.class_idx == int32_t(last)) { s.class_idx = int32_t(dead); }
                }
        }
        vCrowds.pop_back();
        mClassLookup.erase(it);
}

// ---------------------------------------------------------------------------

dtCrowdAgentParams DetourCrowdAvoidance::MakeAgentParams(const SAvoidanceParams & oParams) const {

        dtCrowdAgentParams ap;
        ap.radius                = oParams.radius;
        ap.height                = oParams.height;
        ap.maxAcceleration       = oParams.max_accel;
        ap.maxSpeed              = oParams.max_speed;
        ap.collisionQueryRange   = oConfig.collision_query_range > 0.f
                                 ? oConfig.collision_query_range : oParams.radius * 8.f;
        ap.pathOptimizationRange = oParams.radius * 30.f;
        ap.separationWeight      = oConfig.separation_weight;
        // Velocity mode: no ANTICIPATE_TURNS / OPTIMIZE_TOPO — those steer by
        // dtCrowd's own corridors, which stay unused here.
        ap.updateFlags           = DT_CROWD_OBSTACLE_AVOIDANCE | DT_CROWD_SEPARATION;
        ap.obstacleAvoidanceType = 0;
        ap.queryFilterType       = 0;
        ap.userData              = nullptr;
        return ap;
}

int DetourCrowdAvoidance::Enroll(const StrID sClass, const glm::vec3 & vPos, const SAvoidanceParams & oParams) {

        const auto it = mClassLookup.find(sClass);
        if (it == mClassLookup.end()) { return -1; }
        SClassCrowd & c = vCrowds[it->second];

        const float pos[3] = {vPos.x, vPos.y, vPos.z};
        const dtCrowdAgentParams ap = MakeAgentParams(oParams);
        const int crowdIdx = c.pCrowd->addAgent(pos, &ap);
        if (crowdIdx == -1) { return -1; }

        SSlot s;
        s.active    = true;
        s.class_idx = int32_t(it->second);
        s.crowd_idx = crowdIdx;

        int nSlot;
        if (!vFreeSlots.empty()) {
                nSlot = vFreeSlots.back();
                vFreeSlots.pop_back();
                vSlots[size_t(nSlot)] = s;
        } else {
                nSlot = int(vSlots.size());
                vSlots.push_back(s);
        }
        return nSlot;
}

void DetourCrowdAvoidance::Unenroll(const int nSlot) {

        if (nSlot < 0 || size_t(nSlot) >= vSlots.size()) { return; }
        SSlot & s = vSlots[size_t(nSlot)];
        if (!s.active) { return; }

        if (s.class_idx >= 0 && size_t(s.class_idx) < vCrowds.size() && vCrowds[size_t(s.class_idx)].pCrowd) {
                vCrowds[size_t(s.class_idx)].pCrowd->removeAgent(s.crowd_idx);
        }
        s.active    = false;
        s.class_idx = -1;
        s.crowd_idx = -1;
        vFreeSlots.push_back(nSlot);
}

void DetourCrowdAvoidance::SyncAgent(const int nSlot, const glm::vec3 & vPos, const glm::vec3 & vVel) {

        if (nSlot < 0 || size_t(nSlot) >= vSlots.size()) { return; }
        const SSlot & s = vSlots[size_t(nSlot)];
        if (!s.active) { return; }

        dtCrowdAgent * ag = vCrowds[size_t(s.class_idx)].pCrowd->getEditableAgent(s.crowd_idx);
        if (!ag) { return; }
        ag->npos[0] = vPos.x;  ag->npos[1] = vPos.y;  ag->npos[2] = vPos.z;
        ag->vel[0]  = vVel.x;  ag->vel[1]  = vVel.y;  ag->vel[2]  = vVel.z;
}

void DetourCrowdAvoidance::SetDesired(const int nSlot, const glm::vec3 & vDesired) {

        if (nSlot < 0 || size_t(nSlot) >= vSlots.size()) { return; }
        const SSlot & s = vSlots[size_t(nSlot)];
        if (!s.active) { return; }

        const float vel[3] = {vDesired.x, vDesired.y, vDesired.z};
        vCrowds[size_t(s.class_idx)].pCrowd->requestMoveVelocity(s.crowd_idx, vel);
}

void DetourCrowdAvoidance::Update(const float fDt) {
        for (SClassCrowd & c : vCrowds) {
                if (c.pCrowd) { c.pCrowd->update(fDt, nullptr); }
        }
}

glm::vec3 DetourCrowdAvoidance::AdjustedVelocity(const int nSlot) {

        if (nSlot < 0 || size_t(nSlot) >= vSlots.size()) { return glm::vec3{0.f}; }
        const SSlot & s = vSlots[size_t(nSlot)];
        if (!s.active) { return glm::vec3{0.f}; }

        const dtCrowdAgent * ag = vCrowds[size_t(s.class_idx)].pCrowd->getAgent(s.crowd_idx);
        if (!ag) { return glm::vec3{0.f}; }
        return {ag->nvel[0], ag->nvel[1], ag->nvel[2]};
}

bool DetourCrowdAvoidance::IsAgentWalking(const int nSlot) const {

        if (nSlot < 0 || size_t(nSlot) >= vSlots.size()) { return false; }
        const SSlot & s = vSlots[size_t(nSlot)];
        if (!s.active) { return false; }

        const dtCrowdAgent * ag = vCrowds[size_t(s.class_idx)].pCrowd->getAgent(s.crowd_idx);
        return ag && ag->state == DT_CROWDAGENT_STATE_WALKING;
}

size_t DetourCrowdAvoidance::ActiveAgents() const {
        size_t n = 0;
        for (const SSlot & s : vSlots) { n += s.active ? 1u : 0u; }
        return n;
}

} // namespace SE

#endif // SE_IMPL
