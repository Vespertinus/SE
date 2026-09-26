
#ifdef SE_IMPL

#include <nav/CrowdSystem.h>
#include <nav/PathFollower.h>
#include <nav/NavAgent.h>
#include <nav/NavigationSystem.h>

#include <Logging.h>
#include <EventManager.h>
#include <CommonEvents.h>
#include <GlobalTypes.h>
#include <InputState.h>
#include <CharacterController.h>

#include <chrono>
#include <cmath>

namespace SE {

// ---------------------------------------------------------------------------

template <typename TBackend>
CrowdSystemT<TBackend>::CrowdSystemT() {
        auto & em = GetSystem<EventManager>();
        em.AddListener<EInputUpdate,          &CrowdSystemT::OnInputUpdate>(this);
        em.AddListener<EPostUpdate,           &CrowdSystemT::OnPostUpdate>(this);
        em.AddListener<ENavAgentRegistered,   &CrowdSystemT::OnNavAgentRegistered>(this);
        em.AddListener<ENavAgentUnregistering,&CrowdSystemT::OnNavAgentUnregistering>(this);
}

template <typename TBackend>
CrowdSystemT<TBackend>::~CrowdSystemT() noexcept {
        auto & em = GetSystem<EventManager>();
        em.RemoveListener<EInputUpdate,          &CrowdSystemT::OnInputUpdate>(this);
        em.RemoveListener<EPostUpdate,           &CrowdSystemT::OnPostUpdate>(this);
        em.RemoveListener<ENavAgentRegistered,   &CrowdSystemT::OnNavAgentRegistered>(this);
        em.RemoveListener<ENavAgentUnregistering,&CrowdSystemT::OnNavAgentUnregistering>(this);

        oBackend.Shutdown();
}

// ---------------------------------------------------------------------------

template <typename TBackend>
void CrowdSystemT<TBackend>::EnsureBackend() {

        if (backend_ready && !backend_dirty) { return; }

        // Re-init picks up pool resizing; per-class solver state is rebuilt
        // lazily by BindClass on the next enrollment pass.
        backend_ready = oBackend.Init(oConfig.max_agents, oConfig.max_agent_radius);
        backend_dirty = false;
        if (!backend_ready) {
                log_e("CrowdSystem: backend refused Init ({} agents, r={}) — everyone runs unavioded",
                      oConfig.max_agents, oConfig.max_agent_radius);
        }
}

// ---------------------------------------------------------------------------
// roster
// ---------------------------------------------------------------------------

template <typename TBackend>
void CrowdSystemT<TBackend>::OnNavAgentRegistered(const Event & oEvent) {

        NavAgent * pAgent = oEvent.Get<ENavAgentRegistered>().pAgent;
        if (!pAgent || !pAgent->Avoidance() || mAgentLookup.count(pAgent) > 0) { return; }

        SCrowdAgent a;
        a.pAgent         = pAgent;
        a.oParams.radius = pAgent->Radius() > 0.f ? pAgent->Radius() : oConfig.default_radius;
        a.oParams.height = pAgent->Height() > 0.f ? pAgent->Height() : oConfig.default_height;

        // Match the real body's limits so the avoidance model steers with the
        // same accelerations CharacterMovementSystem will apply.
        if (CharacterController * pCC = pAgent->Node()->template GetComponent<CharacterController>()) {
                a.oParams.max_speed = pCC->GetMaxSpeed();
                a.oParams.max_accel = pCC->GetAcceleration();
        } else {
                a.oParams.max_speed = oConfig.default_max_speed;
                a.oParams.max_accel = oConfig.default_max_accel;
        }

        mAgentLookup.emplace(pAgent, uint32_t(vAgents.size()));
        vAgents.push_back(a);
        TryEnroll(vAgents.back());
        oStats.avoided = uint32_t(oBackend.ActiveAgents());
}

template <typename TBackend>
void CrowdSystemT<TBackend>::OnNavAgentUnregistering(const Event & oEvent) {

        NavAgent * pAgent = oEvent.Get<ENavAgentUnregistering>().pAgent;
        const auto it = mAgentLookup.find(pAgent);
        if (it == mAgentLookup.end()) { return; }

        const uint32_t idx = it->second;
        oBackend.Unenroll(vAgents[idx].slot);

        vAgents[idx] = std::move(vAgents.back());
        if (idx < vAgents.size() - 1) { mAgentLookup[vAgents[idx].pAgent] = idx; }
        vAgents.pop_back();
        mAgentLookup.erase(it);
        oStats.avoided = uint32_t(oBackend.ActiveAgents());
}

template <typename TBackend>
void CrowdSystemT<TBackend>::TryEnroll(SCrowdAgent & oAgent) {

        if (oAgent.slot >= 0) { return; }
        EnsureBackend();   // enrollment events can arrive before the first tick
        if (!backend_ready) { return; }

        NavigationSystem & oNav = GetSystem<NavigationSystem>();
        const StrID className = oAgent.pAgent->AgentClassID();
        if (oNav.ClassIndex(className) < 0) { return; }   // class not (yet) registered — retry

        dtNavMesh *           pMesh   = oNav.GetClassMesh(className);
        const dtQueryFilter * pFilter = oNav.GetClassFilter(className);
        if (!pMesh || !pFilter) { return; }

        if (!oBackend.IsClassMeshCurrent(className, pMesh) &&
            !oBackend.BindClass(className, pMesh, pFilter)) {
                return;   // inert class — retry (cheap), rebinds when it returns
        }

        // Enrolling at an off-mesh position creates an INVALID dtCrowd agent —
        // the solver never computes velocity for it and the body freezes. Wait
        // for the node to land on (or near) the mesh; until then the follower
        // steers crowd-less on its raw desired velocity.
        const glm::vec3 vRaw = oAgent.pAgent->Node()->GetTransform().GetWorldPos();
        glm::vec3 vPos = vRaw;
        if (!oNav.ProjectToNavMesh(className, vRaw, 2.0f, vPos)) { return; }
        oAgent.slot = oBackend.Enroll(className, vPos, oAgent.oParams);
}

template <typename TBackend>
void CrowdSystemT<TBackend>::UpkeepRoster() {

        NavigationSystem & oNav = GetSystem<NavigationSystem>();

        for (SCrowdAgent & a : vAgents) {
                if (a.slot < 0) {
                        TryEnroll(a);
                        continue;
                }
                // A dtNavMesh may die under us (WorldNavBridge::UnregisterAll on
                // world unload). Detect and release — the retry path re-enrolls
                // when the class comes back. Revalidate only when the registry
                // generation moved; otherwise this is a per-agent-per-tick no-op.
                if (!a.mesh_checked || a.cached_mesh_gen != oNav.MeshGeneration()) {
                        a.cached_class_id = a.pAgent->AgentClassID();
                        a.cached_mesh_gen = oNav.MeshGeneration();
                        a.mesh_checked  = true;
                        if (!oBackend.IsClassMeshCurrent(a.cached_class_id, oNav.GetClassMesh(a.cached_class_id))) {
                                oBackend.Unenroll(a.slot);
                                a.slot = -1;
                        }
                }
        }
}

// ---------------------------------------------------------------------------
// followers
// ---------------------------------------------------------------------------

template <typename TBackend>
void CrowdSystemT<TBackend>::AddFollower(PathFollower * pFollower) {

        if (!pFollower || mFollowerLookup.count(pFollower) > 0) { return; }

        SFollowerRec rec;
        rec.pFollower = pFollower;

        mFollowerLookup.emplace(pFollower, uint32_t(vFollowers.size()));
        vFollowers.push_back(rec);
        oStats.followers = uint32_t(vFollowers.size());
}

template <typename TBackend>
void CrowdSystemT<TBackend>::RemoveFollower(PathFollower * pFollower) {

        const auto it = mFollowerLookup.find(pFollower);
        if (it == mFollowerLookup.end()) { return; }

        const uint32_t idx = it->second;
        vFollowers[idx] = std::move(vFollowers.back());
        if (idx < vFollowers.size() - 1) { mFollowerLookup[vFollowers[idx].pFollower] = idx; }
        vFollowers.pop_back();
        mFollowerLookup.erase(it);
        oStats.followers = uint32_t(vFollowers.size());
}

namespace {

float XZDist(const glm::vec3 & a, const glm::vec3 & b) {
        const float dx = a.x - b.x;
        const float dz = a.z - b.z;
        return std::sqrt(dx * dx + dz * dz);
}

} // namespace

// One follower's steering math: desired velocity from the corridor's first
// corner (or the goal directly when no corner lies ahead), with linear
// deceleration across the slow radius. CharacterMovementSystem honors
// move_axis magnitude, so this analog speed reaches the body unchanged.
template <typename TBackend>
void CrowdSystemT<TBackend>::ComputeDesired(SFollowerRec & oRec, NavAgent * pAgent,
                                            const glm::vec3 & vPos) {

        PathFollower * pF = oRec.pFollower;
        NavigationSystem & oNav = GetSystem<NavigationSystem>();

        glm::vec3 vTarget = pF->vGoal;
        glm::vec3 vCorners[NavigationSystem::kMaxAgentCorners];
        if (oNav.GetAgentCorners(pAgent, vCorners, nullptr) > 0) { vTarget = vCorners[0]; }

        glm::vec2 vDir{vTarget.x - vPos.x, vTarget.z - vPos.z};
        const float lenSq = glm::dot(vDir, vDir);
        if (lenSq > 1e-8f) { vDir = vDir * (1.0f / std::sqrt(lenSq)); }

        const float slowDenom  = std::max(pF->oDesc.slow_radius - pF->oDesc.arrival_radius, 0.01f);
        const float slowFactor = glm::clamp((oRec.dist_to_goal - pF->oDesc.arrival_radius) / slowDenom, 0.f, 1.f);
        oRec.vDesired = glm::vec3{vDir.x, 0.f, vDir.y} * (ResolveMaxSpeed(oRec) * slowFactor);
}

// Phase-gated repath checks; the re-path itself (a real findPath) is what the
// per-tick budget caps. MoveTo's request bypasses the phase gate but waits
// for budget. On failure the follower latches Failed (the game decides what
// "unreachable" means); on success desired velocity is recomputed from the
// fresh corners so this same tick steers along the new corridor.
template <typename TBackend>
bool CrowdSystemT<TBackend>::RepathIfNeeded(SFollowerRec & oRec, NavAgent * pAgent,
                                            const glm::vec3 & vPos, const bool bPhaseDue) {

        PathFollower * pF = oRec.pFollower;
        NavigationSystem & oNav = GetSystem<NavigationSystem>();

        bool bNeed = pF->request_path;
        if (!bNeed && bPhaseDue) {
                if (pF->oDesc.repath_interval > 0.f && oRec.repath_timer >= pF->oDesc.repath_interval) {
                        bNeed = true;
                }
                if (!bNeed && !oNav.IsAgentPathValid(pAgent, 8)) { bNeed = true; }
                if (!bNeed) {
                        if (const dtPathCorridor * pCorr = oNav.GetAgentCorridor(pAgent)) {
                                if (pCorr->getPathCount() > 0) {
                                        const float * p = pCorr->getPos();
                                        if (XZDist(vPos, {p[0], p[1], p[2]}) > oConfig.teleport_distance) {
                                                bNeed = true;
                                        }
                                }
                        }
                }
        }
        if (!bNeed) { return false; }

        pF->request_path  = false;
        oRec.repath_timer = 0.f;
        oStats.repaths++;

        if (!oNav.SetAgentPath(pAgent, vPos, pF->vGoal, pF->oDesc.accept_partial_paths)) {
                pF->eStatus   = PathStatus::Failed;
                oRec.vDesired = glm::vec3{0.f};
                oRec.fail_cooldown = std::max(pF->oDesc.repath_interval, 0.5f);
                log_d("CrowdSystem: path failed for '{}' to ({:.1f},{:.1f},{:.1f}) — backing off {:.1f}s",
                      pF->Name(), pF->vGoal.x, pF->vGoal.y, pF->vGoal.z, oRec.fail_cooldown);
                GetSystem<EventManager>().TriggerEvent(EPathFailed{pF->pNode->weak_from_this()});
        } else {
                oRec.fail_cooldown = 0.f;
                ComputeDesired(oRec, pAgent, vPos);   // steer by the new corners this tick
        }
        return true;
}

template <typename TBackend>
float CrowdSystemT<TBackend>::ResolveMaxSpeed(SFollowerRec & oRec) {
        PathFollower * pF = oRec.pFollower;
        if (!oRec.pCachedCC) {
                oRec.pCachedCC = pF->pNode->template GetComponent<CharacterController>();
        }
        return oRec.pCachedCC ? oRec.pCachedCC->GetMaxSpeed() : oConfig.default_max_speed;
}

template <typename TBackend>
void CrowdSystemT<TBackend>::WriteOutput(SFollowerRec & oRec) {

        PathFollower * pF = oRec.pFollower;
        if (!oRec.pCachedInput) {
                oRec.pCachedInput = pF->pNode->template GetComponent<InputState>();
        }
        InputState * pInput = oRec.pCachedInput;
        if (!pInput) { return; }

        // Adjusted velocity when avoidance drove this agent; the raw desired
        // velocity otherwise (crowd-less followers steer straight). A backend
        // agent that is not WALKING (invalid / off-mesh) never gets a solved
        // velocity out of dtCrowd — steering by its zero nvel would freeze the
        // body — so those fall back to the desired velocity too.
        glm::vec3 vVel = oRec.vDesired;
        if (!oRec.pCachedAgent) {
                oRec.pCachedAgent = pF->pNode->template GetComponent<NavAgent>();
        }
        if (NavAgent * pAgent = oRec.pCachedAgent) {
                const auto it = mAgentLookup.find(pAgent);
                if (it != mAgentLookup.end() && vAgents[it->second].slot >= 0 &&
                    oBackend.IsAgentWalking(vAgents[it->second].slot)) {
                        vVel = oBackend.AdjustedVelocity(vAgents[it->second].slot);
                }
        }
        oRec.vAdjusted = vVel;

        const float maxSpeed = ResolveMaxSpeed(oRec);

        // move_axis is analog: unit length = the body's max speed. Scale the
        // steering velocity into [0, 1] so slow-radius deceleration and the
        // solver's speed output both reach CharacterMovementSystem faithfully.
        glm::vec2 vAxis{vVel.x, vVel.z};
        if (glm::length(vVel) < oConfig.idle_speed_epsilon) {
                vAxis = glm::vec2{0.f};
        } else if (maxSpeed > 0.f) {
                vAxis = vAxis * (1.f / maxSpeed);
                const float mag = glm::length(vAxis);
                if (mag > 1.f) { vAxis = vAxis * (1.f / mag); }
        }

        pInput->move_axis    = vAxis;
        pInput->jump_pressed = false;
        pInput->jump_held    = false;
        pInput->sprint_held  = false;
}

// ---------------------------------------------------------------------------
// solver tick
// ---------------------------------------------------------------------------

template <typename TBackend>
void CrowdSystemT<TBackend>::RunCrowdStep(const float fDt) {

        const auto t0 = std::chrono::steady_clock::now();

        EnsureBackend();   // direct-drive seam (tests/tools) skips OnInputUpdate
        UpkeepRoster();

        NavigationSystem & oNav = GetSystem<NavigationSystem>();

        // --- 1. write-back: the solver samples the REAL bodies ---------------
        for (SCrowdAgent & a : vAgents) {
                if (a.slot < 0) { continue; }

                const glm::vec3 vPos = a.pAgent->Node()->GetTransform().GetWorldPos();
                if (!a.pCC) { a.pCC = a.pAgent->Node()->template GetComponent<CharacterController>(); }
                const glm::vec3 vVel = a.pCC ? a.pCC->GetVelocity() : glm::vec3{0.f};
                oBackend.SyncAgent(a.slot, vPos, vVel);
        }

        // --- 2. followers: arrival, desired velocity, repath upkeep ----------
        const uint32_t nBuckets = oConfig.repath_buckets > 0 ? oConfig.repath_buckets : 1;
        uint32_t nRepathsLeft   = oConfig.max_repaths_per_tick;

        for (SFollowerRec & rec : vFollowers) {
                PathFollower * pF = rec.pFollower;
                if (!pF || !pF->pNode) { continue; }

                if (!rec.pCachedAgent) { rec.pCachedAgent = pF->pNode->template GetComponent<NavAgent>(); }
                NavAgent * pAgent = rec.pCachedAgent;
                if (pF->eStatus == PathStatus::Following && !pAgent) {
                        // Corridors live on the NavAgent record — without one the
                        // follower cannot ever path. Fail loudly.
                        pF->eStatus = PathStatus::Failed;
                        log_e("CrowdSystem: follower '{}' has no NavAgent sibling", pF->Name());
                        GetSystem<EventManager>().TriggerEvent(EPathFailed{pF->pNode->weak_from_this()});
                }

                const auto itAgent  = pAgent ? mAgentLookup.find(pAgent) : mAgentLookup.end();
                const int32_t nSlot = (itAgent != mAgentLookup.end()) ? vAgents[itAgent->second].slot : -1;

                if (pF->eStatus != PathStatus::Following) {
                        // Idle / Arrived / Failed: hold the requested velocity at
                        // zero so the agent does not keep its last steering.
                        rec.vDesired     = glm::vec3{0.f};
                        rec.vAdjusted    = glm::vec3{0.f};
                        rec.dist_to_goal = XZDist(pF->pNode->GetTransform().GetWorldPos(), pF->vGoal);
                        if (nSlot >= 0) { oBackend.SetDesired(nSlot, glm::vec3{0.f}); }
                        continue;
                }

                const glm::vec3 vPos = pF->pNode->GetTransform().GetWorldPos();
                rec.dist_to_goal = XZDist(vPos, pF->vGoal);

                // Arrival is the follower's contract — checked every tick. The
                // radius alone is not reachable: the output deadzone freezes the
                // body once the slow-band speed drops below idle_speed_epsilon,
                // so the equivalent freeze band counts as arrived too.
                {
                        const float fSlowDenom  = std::max(pF->oDesc.slow_radius - pF->oDesc.arrival_radius, 0.01f);
                        const float fFreezeBand = (oConfig.idle_speed_epsilon / std::max(ResolveMaxSpeed(rec), 0.01f)) * fSlowDenom;
                        if (rec.dist_to_goal <= pF->oDesc.arrival_radius + fFreezeBand) {
                                pF->eStatus      = PathStatus::Arrived;
                                pF->request_path = false;
                                rec.vDesired     = glm::vec3{0.f};
                                oStats.arrivals++;
                                GetSystem<EventManager>().TriggerEvent(EPathArrived{pF->pNode->weak_from_this()});
                                if (nSlot >= 0) { oBackend.SetDesired(nSlot, glm::vec3{0.f}); }
                                continue;
                        }
                }

                // Repath checks are phase-gated; the re-path (a real findPath)
                // is capped per tick. MoveTo's request bypasses the phase but
                // waits for budget; the timer runs every tick regardless. The
                // checks must see the PRE-sync corridor — a teleport has to be
                // detected before movePosition swallows it — so they run first.
                rec.repath_timer += fDt;
                rec.fail_cooldown = std::max(rec.fail_cooldown - fDt, 0.f);
                const bool bPhaseDue = (nTickIndex % nBuckets) == (pF->pNode->GetID() % nBuckets);
                if ((pF->request_path || bPhaseDue) && nRepathsLeft > 0 && rec.fail_cooldown <= 0.f) {
                        const uint32_t nBefore = oStats.repaths;
                        RepathIfNeeded(rec, pAgent, vPos, bPhaseDue);
                        nRepathsLeft -= glm::min<uint32_t>(oStats.repaths - nBefore, nRepathsLeft);
                }
                // The corridor must track the real body (external locomotion):
                // without this movePosition the corners/validity would forever
                // answer for the path start. Until the first repath lands the
                // corridor is empty (movePosition asserts on that) — skip it.
                if (const dtPathCorridor * pCorr = oNav.GetAgentCorridor(pAgent)) {
                        if (pCorr->getPathCount() > 0) { oNav.MoveAgentAlong(pAgent, vPos); }
                }
                ComputeDesired(rec, pAgent, vPos);

                if (nSlot >= 0) { oBackend.SetDesired(nSlot, rec.vDesired); }
        }

        // --- 3. solve ---------------------------------------------------------
        oBackend.Update(fDt);

        // --- 4. output --------------------------------------------------------
        for (SFollowerRec & rec : vFollowers) {
                PathFollower * pF = rec.pFollower;
                if (!pF || !pF->pNode) { continue; }
                WriteOutput(rec);
        }

        nTickIndex++;
        oStats.solver_ticks++;
        oStats.solver_ms = std::chrono::duration<float, std::milli>(
                std::chrono::steady_clock::now() - t0).count();
}

// ---------------------------------------------------------------------------
// frame plumbing
// ---------------------------------------------------------------------------

template <typename TBackend>
void CrowdSystemT<TBackend>::OnInputUpdate(const Event & oEvent) {

        const float fDt = oEvent.Get<EInputUpdate>().last_frame_time;
        if (fDt <= 0.f) { return; }

        EnsureBackend();
        while (oClock.Step(fDt)) {
                RunCrowdStep(oClock.Dt());
        }
}

template <typename TBackend>
void CrowdSystemT<TBackend>::OnPostUpdate(const Event &) {
        oStats.solver_ticks = 0;
        oStats.arrivals     = 0;
        oStats.repaths      = 0;
        oStats.solver_ms    = 0.f;
}

template <typename TBackend>
void CrowdSystemT<TBackend>::GetDebugFollowers(std::vector<SFollowerDebug> & vOut) const {

        vOut.clear();
        vOut.reserve(vFollowers.size());

        for (const SFollowerRec & rec : vFollowers) {
                const PathFollower * pF = rec.pFollower;
                if (!pF) { continue; }

                SFollowerDebug dbg;
                dbg.sName       = pF->Name();
                dbg.status      = uint8_t(pF->eStatus);
                dbg.goal        = pF->vGoal;
                dbg.dist_to_goal = rec.dist_to_goal;
                dbg.vDesired    = rec.vDesired;
                dbg.vAdjusted   = rec.vAdjusted;

                if (NavAgent * pAgent = pF->pNode->template GetComponent<NavAgent>()) {
                        const auto it = mAgentLookup.find(pAgent);
                        dbg.avoided = (it != mAgentLookup.end() && vAgents[it->second].slot >= 0);
                }
                vOut.push_back(std::move(dbg));
        }
}

// Explicit instantiation: currently the only composition.
template class CrowdSystemT<DetourCrowdAvoidance>;

} // namespace SE

#endif // SE_IMPL
