
#ifdef SE_IMPL

#include <world/WorldSystem.h>
#include <world/WorldAsset.h>
#include <world/ResourcePins.h>
#include <EntityManager.h>
#include <Logging.h>
#include <MPUtil.h>

#include <flatbuffers/flatbuffers.h>
#include <SceneTree_generated.h>
#include <Resources_generated.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <map>
#include <thread>

namespace SE {

const std::string & ResourceDir() {
        return GetSystem<Config>().sResourceDir;
}

namespace {

template <class R>
void PinHandle(WorldSystem::SubLevelInstance & inst, H<R> h) {

        if (!h.IsValid()) { return; }

        constexpr void (*fn)(uint32_t, uint32_t) = &WorldSystem::UnpinResource<R>;

        for (const auto & p : inst.vPins) {
                if (p.fnUnpin == fn && p.index == h.raw.index && p.generation == h.raw.generation) {
                        return;   // same resource declared twice in this level
                }
        }

        inst.vPins.push_back(WorldSystem::Pin{ fn, h.raw.index, h.raw.generation });
        RetainResource(h);
}

// --- pin-adapter registration (the LoadFB loader-map pattern) ----------------

using TResourcePinFn  = void (*)(WorldSystem::SubLevelInstance &, const FlatBuffers::ResourceRef *);
using TResourcePinMap = std::map<FlatBuffers::ResourceU, TResourcePinFn>;

template <class R>
void PinViaAdapter(WorldSystem::SubLevelInstance & inst, const FlatBuffers::ResourceRef * pRef) {
        using TPin = TResourcePin<R>;
        PinHandle(inst, TPin::Create(static_cast<const typename TPin::THolder *>(pRef->resource())));
}

template <class R>
void RegisterPin(TResourcePinMap & m) {

        if constexpr (THasResourcePinVal<R>) {
                // TKeyHolder is the object type the generated union traits key on
                // (Resources_generated.h specializes on *HolderT, unlike
                // Component_generated.h which specializes on the native tables).
                constexpr FlatBuffers::ResourceU key =
                        FlatBuffers::ResourceUUnionTraits<typename TResourcePin<R>::TKeyHolder>::enum_value;
                m.emplace(key, &PinViaAdapter<R>);
        }
}

template <class ... TR>
TResourcePinMap BuildPinMap(MP::TypelistWrapper<TR...>) {
        TResourcePinMap m;
        (RegisterPin<TR>(m), ...);
        return m;
}

// The map depends only on the app's compile-time resource typelist: build once.
const TResourcePinMap & GetPinMap() {
        static const TResourcePinMap mPinners = BuildPinMap(TResourceList{});
        return mPinners;
}

std::string BasisName(const std::string & sPath) {
        const auto pos = sPath.find_last_of('/');
        return (pos == std::string::npos) ? sPath : sPath.substr(pos + 1);
}

} // namespace

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

WorldSystem::WorldSystem() {
        GetSystem<EventManager>().AddListener<EUpdate,          &WorldSystem::OnUpdate>(this);
        GetSystem<EventManager>().AddListener<EPostUpdate,      &WorldSystem::OnPostUpdate>(this);
        GetSystem<EventManager>().AddListener<EEntityDestroyed, &WorldSystem::OnEntityDestroyed>(this);
}

WorldSystem::~WorldSystem() noexcept {        GetSystem<EventManager>().RemoveListener<EUpdate,          &WorldSystem::OnUpdate>(this);
        GetSystem<EventManager>().RemoveListener<EPostUpdate,      &WorldSystem::OnPostUpdate>(this);
        GetSystem<EventManager>().RemoveListener<EEntityDestroyed, &WorldSystem::OnEntityDestroyed>(this);
}

void WorldSystem::SetSceneTree(TSceneTree * pTree) {

        if (pSceneTree == pTree) { return; }

        if (world_loaded) { UnloadWorld(); }

        pSceneTree = pTree;
}

const WorldAsset * WorldSystem::GetWorldAsset() const {
        return GetResource(hWorld);
}

// ---------------------------------------------------------------------------
// World load / unload
// ---------------------------------------------------------------------------

bool WorldSystem::LoadWorld(H<WorldAsset> hAsset) {

        if (!hAsset.IsValid() || !GetResource(hAsset)) {
                log_e("WorldSystem: LoadWorld called with an invalid world handle");
                return false;
        }

        hWorld = hAsset;
        return LoadWorldImpl();
}

bool WorldSystem::LoadWorld(const std::string & sWorldPath) {
        return LoadWorld(CreateResource<WorldAsset>(sWorldPath));
}

bool WorldSystem::LoadWorldImpl() {

        const auto * pWorldAsset = GetResource(hWorld);

        for (const auto & inst : vInstances) {
                if (inst.state != ELevelState::Unloaded) {
                        log_w("WorldSystem: previous world is still tearing down; LoadWorld refused (retry next frame) "
                              "[instance {} state {}]", inst.index, int(inst.state));
                        return false;
                }
        }

        const uint32_t count = uint32_t(pWorldAsset->GetSubLevels().size());

        vInstances.clear();
        vInstances.resize(count);
        for (uint32_t i = 0; i < count; ++i) { vInstances[i].index = i; }

        // New world epoch: any worker reads still in flight for the previous
        // world are dropped on arrival (serial/ticket mismatch).
        ++world_serial;
        oDirector.Reset(count);
        oLastPlan.Clear();
        vSnapLevels.clear();
        vSnapStatuses.clear();
        vSnapFocuses.clear();
        vDrainOrder.clear();
        last_budget_spent_ms = 0.f;

        auto pRootNode = pSceneTree->GetRoot();

        pWorldRoot    = pSceneTree->Create(pRootNode, "world");
        pDynamicsRoot = pSceneTree->Create(pRootNode, "dynamics");

        world_loaded = true;

        log_i("WorldSystem: world '{}' loaded ({} sub-levels, streaming {})",
                        pWorldAsset->Name(), count,
                        pWorldAsset->GetStreaming().enabled ? "on" : "off");

        GetSystem<EventManager>().TriggerEvent(EWorldLoaded{ pWorldAsset->GetId() });

        // Startup policy: streaming disabled => every non-Manual level (the
        // platformer path, same code); streaming enabled => AlwaysLoaded only.
        // Manual levels never auto-activate.
        const auto & oStreaming = pWorldAsset->GetStreaming();
        for (uint32_t i = 0; i < count; ++i) {
                const auto & entry = pWorldAsset->GetSubLevels()[i];
                if (entry.IsManual()) { continue; }
                if (!oStreaming.enabled || entry.IsAlwaysLoaded()) {
                        ActivateLevel(i);
                }
        }

        return true;
}

void WorldSystem::UnloadWorld() {

        if (!world_loaded) { return; }

        // Cancel any staged loads first: no events were fired for them, so
        // cancellation is invisible to gameplay (drops staged bytes, unpins
        // partially drained dependencies).
        for (auto & inst : vInstances) {
                if (inst.state == ELevelState::Requested || inst.state == ELevelState::Loading) {
                        CancelStagedLoad(inst.index);
                }
        }

        // World shutdown tears down everything, AlwaysLoaded included: BeginTeardown
        // directly (DeactivateLevel refuses AlwaysLoaded during normal play).
        for (auto & inst : vInstances) {
                if (inst.state == ELevelState::Active) { BeginTeardown(inst); }
        }

        auto & oEntityManager = GetSystem<EntityManager>();

        if (!pWorldRoot.expired())    { oEntityManager.Destroy(pWorldRoot); }
        if (!pDynamicsRoot.expired()) { oEntityManager.Destroy(pDynamicsRoot); }

        vFocuses.clear();
        vFocusHandles.clear();

        world_loaded = false;

        log_i("WorldSystem: world unloaded");

        GetSystem<EventManager>().TriggerEvent(EWorldUnloaded{
                        GetResource(hWorld) ? GetResource(hWorld)->GetId() : StrID{} });
}

// ---------------------------------------------------------------------------
// Sub-level activation
// ---------------------------------------------------------------------------

bool WorldSystem::ActivateLevel(const uint32_t index) {

        if (!world_loaded || index >= vInstances.size()) {
                log_w("WorldSystem: ActivateLevel({}) — no such sub-level", index);
                return false;
        }

        auto & inst = vInstances[index];

        if (inst.state != ELevelState::Unloaded) {
                log_w("WorldSystem: ActivateLevel({}) — level is not Unloaded (state {})",
                                index, int(inst.state));
                return false;
        }

        return ActivateLevelImpl(inst);
}

bool WorldSystem::RequestActivation(const uint32_t index) {

        if (!world_loaded || index >= vInstances.size()) {
                log_w("WorldSystem: RequestActivation({}) — no such sub-level", index);
                return false;
        }

        auto & inst = vInstances[index];

        if (inst.state != ELevelState::Unloaded) {
                log_w("WorldSystem: RequestActivation({}) — level is not Unloaded (state {})",
                                index, int(inst.state));
                return false;
        }

        const auto * pWorldAsset = GetResource(hWorld);
        const auto & entry       = pWorldAsset->GetSubLevels()[inst.index];

        if (!pWorldRoot.lock()) {
                log_e("WorldSystem: world root is gone; cannot stage '{}'", entry.path);
                return false;
        }

        try {
                // Stage synchronously (a local file read); the dependency drain +
                // instantiation then run inside TickStreaming's budget — the same
                // path a distance-streamed level takes through Loading. The
                // planner skips Manual levels entirely, so nothing here can be
                // cancelled out from under us.
                inst.pStagedBytes = std::make_shared<std::vector<char>>(
                                pSceneTree->ReadSceneFile(entry.path));
                inst.state    = ELevelState::Loading;
                inst.dep_done = 0;
                const auto * pScene = FlatBuffers::GetSceneTree(inst.pStagedBytes->data());
                inst.dep_total = pScene->resources() ? pScene->resources()->size() : 0;
                inst.ticket    = 0;
        }
        catch (const std::exception & ex) {
                log_e("WorldSystem: RequestActivation({}) — stage read failed: {}", index, ex.what());
                inst.pStagedBytes.reset();
                inst.dep_done = inst.dep_total = 0;
                inst.state = ELevelState::Unloaded;
                return false;
        }

        log_i("WorldSystem: '{}' staged for budgeted activation", entry.path);
        return true;
}

bool WorldSystem::ActivateLevelImpl(SubLevelInstance & inst) {

        const auto * pWorldAsset = GetResource(hWorld);
        const auto & entry       = pWorldAsset->GetSubLevels()[inst.index];

        auto pWorldRootNode = pWorldRoot.lock();
        if (!pWorldRootNode) {
                log_e("WorldSystem: world root is gone; cannot activate '{}'", entry.path);
                return false;
        }

        try {
                // Synchronous staging on the calling thread, then the SAME drain +
                // finalize path the streaming director uses (Manual levels, the
                // AlwaysLoaded startup policy, tests, explicit game requests).
                inst.pStagedBytes = std::make_shared<std::vector<char>>(
                                pSceneTree->ReadSceneFile(entry.path));
                inst.state = ELevelState::Loading;

                const auto * pScene = FlatBuffers::GetSceneTree(inst.pStagedBytes->data());
                inst.dep_total = pScene->resources() ? pScene->resources()->size() : 0;
                if (inst.dep_total == 0) {
                        log_w("WorldSystem: '{}' declares no resources; nothing pinned", entry.path);
                }
                while (!StepDependency(inst, pScene)) { ;; }

                FinalizeActivation(inst);
        }
        catch (...) {
                // StepDependency failures leave partial pins; FinalizeActivation
                // unwound its own state on throw — this catch covers both.
                UnpinAll(inst);
                inst.pStagedBytes.reset();
                inst.dep_done = inst.dep_total = 0;
                inst.state = ELevelState::Unloaded;
                throw;
        }

        return true;
}

void WorldSystem::FinalizeActivation(SubLevelInstance & inst) {

        const auto * pWorldAsset = GetResource(hWorld);
        const auto & entry       = pWorldAsset->GetSubLevels()[inst.index];

        auto pWorldRootNode = pWorldRoot.lock();
        if (!pWorldRootNode) {
                log_e("WorldSystem: world root is gone; cannot activate '{}'", entry.path);
                UnpinAll(inst);
                inst.pStagedBytes.reset();
                inst.state = ELevelState::Unloaded;
                throw std::runtime_error("WorldSystem: world root is gone");
        }

        // Wrapper node: unique per level id (basename#index), so two sub-levels may
        // safely instantiate the same .sesc; the ref transform places the instance.
        auto pWrapper = pSceneTree->Create(pWorldRootNode,
                        BasisName(entry.path) + "#" + std::to_string(inst.index));
        pWrapper->SetPos(entry.translation);
        pWrapper->SetRotation(entry.rotation);

        inst.pRoot = pWrapper;

        // Activating fires with the wrapper alive and immediately before
        // instantiation — consumers never observe a half-built subtree
        // (Activated always sees the fully constructed one).
        GetSystem<EventManager>().TriggerEvent(ESubLevelActivating{
                        inst.index, entry.name, inst.pRoot });

        try {
                // Staged bytes were verified by whoever read them (worker or
                // ReadSceneFile); component ctors hit the warm resource cache, so
                // activation is allocation-only.
                pSceneTree->InstantiateFromBuffer(*inst.pStagedBytes, pWrapper,
                                /*bPreVerified=*/true);
        }
        catch (...) {
                // Exception path (not an event callback), so direct Unlink is safe.
                pWrapper->Unlink();
                inst.pRoot.reset();
                UnpinAll(inst);
                inst.pStagedBytes.reset();
                inst.dep_done = inst.dep_total = 0;
                inst.state = ELevelState::Unloaded;
                throw;
        }

        inst.state = ELevelState::Active;
        ++active_count;

        log_d("WorldSystem: sub-level #{} ('{}') activated, {} resources pinned",
                        inst.index, entry.path, inst.vPins.size());

        // Staged bytes are no longer needed — drop them (bounded memory).
        inst.pStagedBytes.reset();
        inst.dep_done = inst.dep_total = 0;

        GetSystem<EventManager>().TriggerEvent(ESubLevelActivated{
                        inst.index, entry.name, inst.pRoot });
}

bool WorldSystem::StepDependency(SubLevelInstance & inst, const FlatBuffers::SceneTree * pScene) {

        const auto * pRefs = pScene->resources();
        if (!pRefs || inst.dep_done >= pRefs->size()) { return true; }

        const auto * pRef = pRefs->Get(inst.dep_done);

        if (pRef && pRef->resource_type() != FlatBuffers::ResourceU::NONE) {

                const auto & mPinners = GetPinMap();
                const auto   it = mPinners.find(pRef->resource_type());

                if (it == mPinners.end()) {
                        log_w("WorldSystem: scene declares resource type '{}' but this app's "
                              "composition provides no pin adapter for it",
                              FlatBuffers::EnumNameResourceU(pRef->resource_type()));
                }
                else {
                        it->second(inst, pRef);   // CreateResource + Retain (may throw)
                }
        }

        ++inst.dep_done;
        return inst.dep_done >= pRefs->size();
}

bool WorldSystem::CancelStagedLoad(const uint32_t index) {

        if (index >= vInstances.size()) { return false; }

        auto & inst = vInstances[index];

        if (inst.state != ELevelState::Requested && inst.state != ELevelState::Loading) {
                return false;
        }

        // Nothing gameplay-visible existed yet (no events, no nodes): unpin the
        // partial drain and drop the staged bytes. A worker result still in
        // flight is dropped on arrival (state/ticket mismatch).
        UnpinAll(inst);
        inst.pStagedBytes.reset();
        inst.dep_done = inst.dep_total = 0;
        inst.ticket = 0;
        inst.state = ELevelState::Unloaded;
        return true;
}

// ---------------------------------------------------------------------------
// Sub-level deactivation
// ---------------------------------------------------------------------------

bool WorldSystem::DeactivateLevel(const uint32_t index) {

        if (!world_loaded || index >= vInstances.size()) {
                log_w("WorldSystem: DeactivateLevel({}) — no such sub-level", index);
                return false;
        }

        const auto * pWorldAsset = GetResource(hWorld);
        if (pWorldAsset && pWorldAsset->GetSubLevels()[index].IsAlwaysLoaded()) {
                log_w("WorldSystem: sub-level #{} is AlwaysLoaded; explicit Deactivate refused "
                      "(it only goes away with UnloadWorld)", index);
                return false;
        }

        auto & inst = vInstances[index];

        if (inst.state != ELevelState::Active) {
                log_w("WorldSystem: DeactivateLevel({}) — level is not Active (state {})",
                                index, int(inst.state));
                return false;
        }

        BeginTeardown(inst);
        return true;
}

void WorldSystem::BeginTeardown(SubLevelInstance & inst) {

        const auto * pWorldAsset = GetResource(hWorld);
        const StrID name = pWorldAsset ? pWorldAsset->GetSubLevels()[inst.index].name : StrID{};

        // Consumers disarm while every node is still alive.
        GetSystem<EventManager>().TriggerEvent(ESubLevelDeactivating{
                        inst.index, name, inst.pRoot });

        inst.state = ELevelState::Unloading;
        if (active_count > 0) { --active_count; }

        inst.vTeardownExpect.clear();
        inst.mTeardownExpectIds.clear();
        if (!inst.pRoot.expired()) { inst.vTeardownExpect.push_back(inst.pRoot); }
        for (const auto & w : inst.vSpawns) {
                if (!w.expired()) { inst.vTeardownExpect.push_back(w); }
        }
        for (const auto & w : inst.vTeardownExpect) {
                if (auto p = w.lock()) { inst.mTeardownExpectIds.insert(p->GetID()); }
        }
        inst.vSpawns.clear();

        if (inst.vTeardownExpect.empty()) {
                FinishTeardown(inst);
                return;
        }

        // Deferred destruction (EntityManager flush at EPostUpdate); no node is
        // ever destroyed from inside an event callback.
        auto & oEntityManager = GetSystem<EntityManager>();
        for (const auto & w : inst.vTeardownExpect) { oEntityManager.Destroy(w); }
}

void WorldSystem::FinishTeardown(SubLevelInstance & inst) {

        UnpinAll(inst);

        const auto * pWorldAsset = GetResource(hWorld);
        const StrID name = pWorldAsset ? pWorldAsset->GetSubLevels()[inst.index].name : StrID{};

        inst.state = ELevelState::Unloaded;
        inst.pRoot.reset();
        inst.vTeardownExpect.clear();
        inst.mTeardownExpectIds.clear();

        GetSystem<EventManager>().TriggerEvent(ESubLevelDeactivated{ inst.index, name });
}

void WorldSystem::UnpinAll(SubLevelInstance & inst) noexcept {

        for (const auto & p : inst.vPins) { 
                p.fnUnpin(p.index, p.generation); 
        }
        inst.vPins.clear();
}

// ---------------------------------------------------------------------------
// Registries
// ---------------------------------------------------------------------------

void WorldSystem::RegisterSpawn(const uint32_t level, TSceneNodeWeak pNode) {

        if (!world_loaded || level >= vInstances.size()) {
                log_w("WorldSystem: RegisterSpawn({}) — no such sub-level", level);
                return;
        }

        auto & inst = vInstances[level];

        if (inst.state != ELevelState::Active) {
                log_w("WorldSystem: RegisterSpawn into non-active sub-level #{} — binding anyway", level);
        }

        inst.vSpawns.push_back(pNode);
}

WorldSystem::FocusHandle WorldSystem::RegisterFocus(TSceneNodeWeak pNode, const float radius, const float priority) {

        auto pNodeLocked = pNode.lock();
        if (!pNodeLocked) {
                log_e("WorldSystem: RegisterFocus — expired node");
                return 0;
        }

        FocusPoint fp;
        fp.node     = pNode;
        fp.pos      = glm::vec3(pNodeLocked->GetTransform().GetWorld()[3]);
        fp.radius   = radius;
        fp.priority = priority;

        const FocusHandle handle = (uint64_t(focus_serial++) << 32) | 1;

        vFocuses.push_back(fp);
        vFocusHandles.push_back(handle);

        return handle;
}

void WorldSystem::UnregisterFocus(const FocusHandle handle) {

        if (handle == 0) { return; }

        const auto it = std::find(vFocusHandles.begin(), vFocusHandles.end(), handle);
        if (it == vFocusHandles.end()) { return; }

        const auto idx = size_t(std::distance(vFocusHandles.begin(), it));

        vFocusHandles.erase(vFocusHandles.begin() + idx);
        vFocuses.erase(vFocuses.begin() + idx);
}

// ---------------------------------------------------------------------------
// Per-frame pass and deferred-teardown tracking
// ---------------------------------------------------------------------------

void WorldSystem::OnUpdate(const Event & oEvent) {

        const auto & e = oEvent.Get<EUpdate>();
        (void)e;

        PruneTeardowns();

        for (size_t i = 0; i < vFocuses.size(); ++i) {
                auto & f = vFocuses[i];
                auto pNode = f.node.lock();
                if (!pNode) { continue; }   // expired: position frozen at last known

                const glm::vec3 vPos(pNode->GetTransform().GetWorld()[3]);

                if (glm::any(glm::greaterThan(glm::abs(vPos - f.pos), glm::vec3(1e-4f)))) {
                        f.pos = vPos;
                        GetSystem<EventManager>().TriggerEvent(EFocusMoved{ f.node, vPos });
                }
        }

        // Streaming pass runs after the focus update so distances are fresh.
        // streaming.enabled=false (the platformer path) is this single check.
        if (world_loaded) {
                const auto * pWorldAsset = GetResource(hWorld);
                if (pWorldAsset && pWorldAsset->GetStreaming().enabled) {
                        TickStreaming(pWorldAsset->GetStreaming().main_budget_ms, false);
                }
                else {
                        DrainWorkerResults();   // keep the shared worker drained
                }
        }
        else {
                DrainWorkerResults();   // foreign content (terrain tiles) needs results
        }                                       // even with no world loaded
}

/// Pop the shared worker queue and route non-scene results to their registered
/// handlers. Scene results are meaningful only while a world is loaded (and its
/// streaming pass drains them in order) — outside that they are stale drops.
void WorldSystem::DrainWorkerResults() {

        StreamingDirector::SReadResult res;
        while (oDirector.TryPopResult(res)) {
                if (res.kind == StreamingDirector::EContentKind::SceneTree) { continue; }
                const size_t slot = static_cast<size_t>(res.kind);
                if (slot < vContentHandlers.size() && vContentHandlers[slot]) {
                        vContentHandlers[slot](res);
                }
        }
}

void WorldSystem::OnPostUpdate(const Event & oEvent) {

        const auto & e = oEvent.Get<EPostUpdate>();
        (void)e;

        // Runs after the EntityManager flush in the same phase when this system was
        // initialized after it; the OnUpdate pass is the order-independent fallback.
        PruneTeardowns();
}

void WorldSystem::PruneTeardowns() {

        for (auto & inst : vInstances) {

                if (inst.state != ELevelState::Unloading) { continue; }

                auto & v = inst.vTeardownExpect;
                const size_t before = v.size();
                v.erase(std::remove_if(v.begin(), v.end(),
                                [](const TSceneNodeWeak & w) { return w.expired(); }),
                        v.end());
                // expired nodes were destroyed outside OnEntityDestroyed's
                // dispatch (scene teardown) — drop their ids to stay in sync
                if (v.size() != before) {
                        for (auto idIt = inst.mTeardownExpectIds.begin(); idIt != inst.mTeardownExpectIds.end();) {
                                bool bFound = false;
                                for (const auto & w : v) {
                                        if (auto p = w.lock()) { if (p->GetID() == *idIt) { bFound = true; break; } }
                                }
                                if (!bFound) { idIt = inst.mTeardownExpectIds.erase(idIt); }
                                else { ++idIt; }
                        }
                }

                if (v.empty()) { FinishTeardown(inst); }
        }
}

void WorldSystem::OnEntityDestroyed(const Event & oEvent) {

        const auto & e = oEvent.Get<EEntityDestroyed>();

        auto pDestroyed = e.pNode.lock();
        if (!pDestroyed) { return; }

        for (auto & inst : vInstances) {

                if (inst.state != ELevelState::Unloading) { continue; }

                // Most nodes of an unloading subtree are NOT in the expect list
                // (only root + lifetime-bound spawns are) — the id set answers
                // that in O(1) with no weak_ptr locking.
                if (inst.mTeardownExpectIds.count(pDestroyed->GetID()) == 0) { continue; }

                auto & v = inst.vTeardownExpect;
                const auto it = std::find_if(v.begin(), v.end(),
                                [&pDestroyed](const TSceneNodeWeak & w) { return w.lock() == pDestroyed; });

                if (it != v.end()) {
                        inst.mTeardownExpectIds.erase(pDestroyed->GetID());
                        v.erase(it);
                        if (v.empty()) { FinishTeardown(inst); }
                        return;
                }
        }
}

// ---------------------------------------------------------------------------
// Streaming director pass
// ---------------------------------------------------------------------------

void WorldSystem::TickStreaming(const float budget_ms, const bool bFlushMode) {

        const auto * pWorldAsset = GetResource(hWorld);
        if (!pWorldAsset) { return; }

        const auto & oConfig = pWorldAsset->GetStreaming();
        const auto & vEntries = pWorldAsset->GetSubLevels();
        const uint32_t count = uint32_t(vInstances.size());

        // --- snapshots (reused scratch; planner is pure over values) -----------
        vSnapLevels.resize(count);
        vSnapStatuses.resize(count);
        for (uint32_t i = 0; i < count; ++i) {
                const auto & entry = vEntries[i];
                vSnapLevels[i].bounds_min   = entry.bounds_min;
                vSnapLevels[i].bounds_max   = entry.bounds_max;
                vSnapLevels[i].manual       = entry.IsManual();
                vSnapLevels[i].always_loaded = entry.IsAlwaysLoaded();

                using EDir = StreamingDirector::ELevelStatus;
                switch (vInstances[i].state) {
                        case ELevelState::Requested:
                        case ELevelState::Loading:  vSnapStatuses[i] = EDir::Loading;  break;
                        case ELevelState::Active:   vSnapStatuses[i] = EDir::Active;   break;
                        case ELevelState::Unloading:vSnapStatuses[i] = EDir::Unloading;break;
                        default:                    vSnapStatuses[i] = EDir::Unloaded; break;
                }
        }

        // Registered focuses, or the world's initial focus until the game
        // registers one (a fresh world streams in around spawn on its own).
        vSnapFocuses.clear();
        if (!vFocuses.empty()) {
                vSnapFocuses.reserve(vFocuses.size());
                for (const auto & f : vFocuses) {
                        vSnapFocuses.push_back({ f.pos, f.radius, f.priority });
                }
        }
        else {
                vSnapFocuses.push_back({ pWorldAsset->GetInitialFocus(), 0.f, 0.f });
        }

        StreamingDirector::SParams oParams;
        oParams.load_radius        = oConfig.load_radius;
        oParams.unload_radius      = oConfig.unload_radius;
        oParams.unload_frame_delay = std::max(1u, oConfig.unload_frame_delay);
        oParams.max_in_flight      = StreamingDirector::kMaxInFlightReads;

        oDirector.Plan(oParams, vSnapLevels.data(), vSnapStatuses.data(), count,
                       vSnapFocuses.data(), uint32_t(vSnapFocuses.size()),
                       oDirector.GetInFlightCount(), !bFlushMode, oLastPlan);

        // --- cancels: in-flight loads that left the wanted set -----------------
        for (const uint32_t idx : oLastPlan.vCancels) { CancelStagedLoad(idx); }

        // --- worker results (stale worlds/superseded tickets dropped) ----------
        StreamingDirector::SReadResult res;
        while (oDirector.TryPopResult(res)) {

                if (res.kind != StreamingDirector::EContentKind::SceneTree) {
                        const size_t slot = static_cast<size_t>(res.kind);
                        if (slot < vContentHandlers.size() && vContentHandlers[slot]) {
                                vContentHandlers[slot](res);   // foreign content (terrain tiles) routes here
                        }
                        continue;
                }

                if (res.world_serial != world_serial) { continue; }
                if (res.index >= vInstances.size())   { continue; }

                auto & inst = vInstances[res.index];
                if (inst.state != ELevelState::Requested || inst.ticket != res.ticket) {
                        continue;   // cancelled (or re-requested) while the read was out
                }

                if (!res.ok) {
                        inst.state  = ELevelState::Unloaded;
                        inst.ticket = 0;
                        oDirector.NotifyFailure(res.index);   // cooldown, no retry storm
                        continue;
                }

                inst.pStagedBytes = std::move(res.pBytes);
                inst.dep_done     = 0;
                const auto * pScene = FlatBuffers::GetSceneTree(inst.pStagedBytes->data());
                inst.dep_total = pScene->resources() ? pScene->resources()->size() : 0;
                if (inst.dep_total == 0) {
                        log_w("WorldSystem: '{}' declares no resources; nothing pinned",
                                        vEntries[res.index].path);
                }
                inst.state = ELevelState::Loading;
        }

        // --- dispatch read requests (nearest first, in-flight capped) ----------
        for (const uint32_t idx : oLastPlan.vRequests) {

                if (idx >= vInstances.size()) { continue; }
                auto & inst = vInstances[idx];
                if (inst.state != ELevelState::Unloaded) { continue; }

                ++dispatch_ticket;
                if (oDirector.DispatchRead(world_serial, dispatch_ticket, idx, vEntries[idx].path)) {
                        inst.state  = ELevelState::Requested;
                        inst.ticket = dispatch_ticket;
                }
                else {
                        --dispatch_ticket;   // queue full: retry next tick
                        break;
                }
        }

        // --- budgeted dependency drain + instantiation --------------------------
        // Wall clock (steady_clock), not AppClock: streaming I/O must not slow
        // down in bullet-time. Always at least one unit of work per pass.
        const auto tStart = std::chrono::steady_clock::now();
        auto ElapsedMs = [&tStart]() {
                return std::chrono::duration<float, std::milli>(
                                std::chrono::steady_clock::now() - tStart).count();
        };

        vDrainOrder.clear();
        for (uint32_t i = 0; i < count; ++i) {
                if (vInstances[i].state != ELevelState::Loading) { continue; }
                float best = std::numeric_limits<float>::max();
                for (const auto & f : vSnapFocuses) {
                        best = std::min(best, StreamingDirector::DistanceToBounds(
                                        f.pos, vEntries[i].bounds_min, vEntries[i].bounds_max));
                }
                vDrainOrder.emplace_back(best, i);
        }
        std::sort(vDrainOrder.begin(), vDrainOrder.end(),
                        [](const auto & a, const auto & b) {
                                if (a.first != b.first) { return a.first < b.first; }
                                return a.second < b.second;
                        });

        const uint32_t max_activations = std::max(1u, oConfig.max_activations_per_frame);
        uint32_t activations = 0;

        for (const auto & oOrder : vDrainOrder) {

                const uint32_t idx = oOrder.second;
                auto & inst = vInstances[idx];

                const auto * pScene = (inst.pStagedBytes)
                                ? FlatBuffers::GetSceneTree(inst.pStagedBytes->data())
                                : nullptr;
                if (!pScene) {
                        CancelStagedLoad(idx);   // defensive: Loading without bytes
                        continue;
                }

                while (inst.dep_done < inst.dep_total) {
                        try {
                                StepDependency(inst, pScene);
                        }
                        catch (const std::exception & ex) {
                                log_e("WorldSystem: dependency load failed for sub-level #{} ('{}'): {}",
                                                idx, vEntries[idx].path, ex.what());
                                CancelStagedLoad(idx);
                                oDirector.NotifyFailure(idx);
                                break;
                        }
                        if (inst.dep_done < inst.dep_total && ElapsedMs() >= budget_ms) {
                                break;   // budget spent: continue this table next frame
                        }
                }

                if (inst.state != ELevelState::Loading) { continue; }   // failed above
                if (inst.dep_done < inst.dep_total)     { continue; }   // budget split across frames
                if (activations >= max_activations)     { continue; }   // instantiation frame cap

                try {
                        FinalizeActivation(inst);
                        ++activations;
                }
                catch (const std::exception & ex) {
                        log_e("WorldSystem: instantiation failed for sub-level #{} ('{}'): {}",
                                        idx, vEntries[idx].path, ex.what());
                        oDirector.NotifyFailure(idx);
                }

                if (ElapsedMs() >= budget_ms) { break; }
        }

        // --- hysteresis unloads (the Step 1 deferred teardown path) -------------
        for (const uint32_t idx : oLastPlan.vUnloads) {
                if (idx < vInstances.size() && vInstances[idx].state == ELevelState::Active) {
                        BeginTeardown(vInstances[idx]);
                }
        }

        last_budget_spent_ms = ElapsedMs();
}

void WorldSystem::SetContentResultHandler(const StreamingDirector::EContentKind eKind,
                                          TContentResultHandler fn) {

        const size_t slot = static_cast<size_t>(eKind);
        if (eKind == StreamingDirector::EContentKind::SceneTree) {
                log_w("WorldSystem: scene results cannot be re-routed");
                return;
        }
        if (slot >= vContentHandlers.size()) { vContentHandlers.resize(slot + 1); }
        vContentHandlers[slot] = std::move(fn);
}

bool WorldSystem::FlushStreaming(const float timeout_ms,
                                 const std::function<void(uint32_t, uint32_t)> & fnPump) {

        if (!world_loaded) { return true; }

        const auto * pWorldAsset = GetResource(hWorld);
        if (!pWorldAsset || !pWorldAsset->GetStreaming().enabled) { return true; }

        const auto tStart = std::chrono::steady_clock::now();

        while (true) {

                // Unbudgeted (loading-screen / transition) pass; flush mode does
                // not advance unload timers or burn cooldowns.
                TickStreaming(std::numeric_limits<float>::max(), true);

                const auto oStats = GetStreamingStats();
                if (oStats.wanted == oStats.active &&
                    oStats.requested == 0 && oStats.loading == 0 &&
                    oStats.in_flight_reads == 0) {
                        return true;
                }

                if (fnPump) {
                        fnPump(oStats.wanted, oStats.active);
                }
                else {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }

                if (std::chrono::duration<float, std::milli>(
                                std::chrono::steady_clock::now() - tStart).count() >= timeout_ms) {
                        log_w("WorldSystem: FlushStreaming timed out (wanted {} / active {})",
                                        oStats.wanted, oStats.active);
                        return false;
                }
        }
}

// Wait for a single level (Manual included) to reach Active. Unbudgeted ticks,
// same flush discipline as FlushStreaming; bails out if the level falls back
// to Unloaded (its teardown completed — activation lost the race).
bool WorldSystem::FlushLevel(const uint32_t index, const float timeout_ms,
                             const std::function<void(uint32_t, uint32_t)> & fnPump) {

        if (index >= vInstances.size()) { return false; }

        const auto tStart = std::chrono::steady_clock::now();

        while (vInstances[index].state != ELevelState::Active) {

                if (vInstances[index].state == ELevelState::Unloaded) { return false; }

                TickStreaming(std::numeric_limits<float>::max(), true);

                if (vInstances[index].state == ELevelState::Active) { break; }

                if (fnPump) {
                        fnPump(1, 0);
                }
                else {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }

                if (std::chrono::duration<float, std::milli>(
                                std::chrono::steady_clock::now() - tStart).count() >= timeout_ms) {
                        log_w("WorldSystem: FlushLevel({}) timed out (state {})",
                                        index, int(vInstances[index].state));
                        return false;
                }
        }
        return true;
}

WorldSystem::StreamingStats WorldSystem::GetStreamingStats() const {

        StreamingStats oStats;

        oStats.wanted = oLastPlan.wanted;

        // The planner never counts Manual levels (explicit game control only),
        // so they must stay out of these counts too — otherwise a synchronous
        // FlushStreaming can never observe wanted == active while any Manual
        // level is up (e.g. an interior during a transition).
        const auto * pWorldAsset = GetResource(hWorld);
        const auto * pEntries    = pWorldAsset ? &pWorldAsset->GetSubLevels() : nullptr;

        for (const auto & inst : vInstances) {

                const bool bManual = pEntries && inst.index < pEntries->size()
                                  && (*pEntries)[inst.index].IsManual();

                if (!bManual) {
                        switch (inst.state) {
                                case ELevelState::Requested: ++oStats.requested; break;
                                case ELevelState::Loading:   ++oStats.loading;   break;
                                case ELevelState::Active:    ++oStats.active;    break;
                                case ELevelState::Unloading: ++oStats.unloading; break;
                                default: break;
                        }
                }
                oStats.pinned_total += uint32_t(inst.vPins.size());
        }

        oStats.in_flight_reads = oDirector.GetInFlightCount();
        oStats.budget_spent_ms = last_budget_spent_ms;

        if (pWorldAsset) {
                oStats.budget_limit_ms = pWorldAsset->GetStreaming().main_budget_ms;
        }

        return oStats;
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

bool WorldSystem::IsLevelActive(const uint32_t index) const {
        return index < vInstances.size() && vInstances[index].state == ELevelState::Active;
}

WorldSystem::ELevelState WorldSystem::GetLevelState(const uint32_t index) const {
        return index < vInstances.size() ? vInstances[index].state : ELevelState::Unloaded;
}

bool WorldSystem::GetLevelDepProgress(const uint32_t index, uint32_t & done, uint32_t & total) const {

        if (index >= vInstances.size())            { return false; }
        const auto & inst = vInstances[index];
        if (inst.state != ELevelState::Requested &&
            inst.state != ELevelState::Loading)    { return false; }

        done = inst.dep_done;
        total = inst.dep_total;
        return true;
}

TSceneTree::TSceneNodeWeak WorldSystem::GetLevelRoot(const uint32_t index) const {
        return index < vInstances.size() ? vInstances[index].pRoot : TSceneTree::TSceneNodeWeak{};
}

} // namespace SE

#endif
