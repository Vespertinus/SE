// ---------------------------------------------------------------------------
// BlackboardTest — Stage 3 Step 9 (agent_workspace/artifacts/
// gameplay_systems_stage_3_step_9_001.md).
//
// Tier A/B: the .sebb asset and the BlackboardCore storage are exercised
// directly (asset bytes built in memory; cores initialized from live assets).
// Tier C: Blackboard/TriggerBinding components through the real scene-loading
// path (InstantiateFromBuffer), the world-declared global blackboard through
// WorldSystem, and trigger-binding dispatch over real node-registered bodies.
//
// Fixture: EnginePhysTestBase (engine-singleton PhysicsSystem — the
// component-level-suite rule; TriggerBinding dispatch resolves nodes through
// GetSystem<PhysicsSystem>::GetNode) plus WorldSystem + BlackboardSystem in
// SetUp (compose AFTER WorldSystem — the SpawnerSystem ctor-dependency
// precedent).
// ---------------------------------------------------------------------------

#define SE_IMPL
#include <GlobalTypes.h>
#include "EngineFixture.h"
#include "PhysicsHarness.h"
#include "BlackboardAssets.h"

#include <EventManager.h>
#include <TriggerVolume.h>
#include <world/WorldSystem.h>
#include <ai/Blackboard.h>
#include <ai/BlackboardSystem.h>
#include <ai/TriggerBinding.h>

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>

#include <glm/glm.hpp>

#include <string>
#include <vector>

using namespace SE;

namespace se_test {

// RAII sink for EBlackboardChanged (PhysicsHarness EventSink pattern, local to
// keep this suite decoupled from the physics fixture header).
template <class E>
struct BBSink {
        std::vector<E> hits;

        BBSink()  { SE::GetSystem<SE::EventManager>().template AddListener<E, &BBSink::On>(this); }
        ~BBSink() { SE::GetSystem<SE::EventManager>().template RemoveListener<E, &BBSink::On>(this); }
        void On(const SE::Event & e) { hits.push_back(e.template Get<E>()); }
};

} // namespace se_test

// ---------------------------------------------------------------------------

namespace {

class BlackboardSystemTest : public se_test::EnginePhysTestBase {

protected:
        // Component-level bodies go through the engine-singleton PhysicsSystem
        // (the PhysicsHarness rule: component-level suites use the singleton,
        // never a local Jolt instance). Bodies are node-registered, which is
        // what the TriggerBinding dispatch resolves via PhysicsSystem::GetNode.
        struct BodyRef {
                SE::BodyHandle                   handle;
                SE::TSceneTree::TSceneNodeExact* node = nullptr;
        };

        BodyRef MakeBody(const SE::RigidBodyDesc & d, const std::string & sName) {
                BodyRef r;
                auto pNode = scene->Create(sName);
                pNode->SetPos(d.vInitialPosition);
                r.handle = SE::GetSystem<SE::PhysicsSystem>().CreateRigidBody(d);
                if (r.handle.IsValid()) {
                        SE::GetSystem<SE::PhysicsSystem>().RegisterNode(r.handle, pNode.get());
                }
                r.node = pNode.get();
                return r;
        }

        BodyRef MakeStaticBox(glm::vec3 vCenter, const std::string & sName) {
                SE::RigidBodyDesc d;
                d.oCollider.type   = SE::ColliderDesc::Box;
                d.oCollider.vHalfExtents = glm::vec3(0.5f);
                d.vInitialPosition = vCenter;
                d.is_static        = true;
                return MakeBody(d, sName);
        }

        BodyRef MakeSphere(glm::vec3 vCenter, const std::string & sName) {
                SE::RigidBodyDesc d;
                d.oCollider.type   = SE::ColliderDesc::Sphere;
                d.oCollider.radius = 0.5f;
                d.vInitialPosition = vCenter;
                return MakeBody(d, sName);
        }


protected:
        void SetUp() override {
                EnginePhysTestBase::SetUp();

                static bool systems_constructed = []() {
                        auto & oEngine = TEngine::Instance();
                        oEngine.Init<EntityManager>();
                        oEngine.Init<WorldSystem>();
                        // after WorldSystem (ctor dependency)
                        oEngine.Init<BlackboardSystem>();
                        return true;
                }();
                (void)systems_constructed;

                GetSystem<EntityManager>().SetSceneTree(scene.get());
                GetSystem<WorldSystem>().SetSceneTree(scene.get());

                boost::filesystem::create_directories("resource/ai/gen");
                boost::filesystem::create_directories("resource/world/gen");
        }

        void TearDown() override {
                GetSystem<WorldSystem>().UnloadWorld();
                GetSystem<WorldSystem>().SetSceneTree(nullptr);
                GetSystem<EntityManager>().SetSceneTree(nullptr);

                // drain queued change events so they never leak across tests
                Events().Process();

                GetSystem<TResourceManager>().Clear<WorldAsset>();
                GetSystem<TResourceManager>().Clear<BlackboardAsset>();

                EnginePhysTestBase::TearDown();
        }

        // The schema most tests share: one key of each authorable type + entity.
        FlatBuffers::BlackboardDefinitionT MakeGuardDef() {
                auto oDef = se_test::MakeBBDef("guard_bb");
                se_test::AddKey(*oDef, "alert_level", FlatBuffers::EBType::Float);
                se_test::AddKey(*oDef, "alarm_raised", FlatBuffers::EBType::Bool);
                se_test::AddKey(*oDef, "patrol_index", FlatBuffers::EBType::Int);
                se_test::AddKey(*oDef, "home_position", FlatBuffers::EBType::Vec3);
                se_test::AddKey(*oDef, "target", FlatBuffers::EBType::Entity);
                return *oDef;
        }

        Blackboard * LoadGuard(std::unique_ptr<FlatBuffers::BlackboardHolderT> pHolder,
                               std::vector<FlatBuffers::BlackboardInitialT> vInitial = {}) {
                auto oNode = se_test::MakeNode("Guard_" + std::to_string(++guard_counter));
                se_test::AddBlackboardComponent(oNode, std::move(pHolder), std::move(vInitial));
                const auto vBytes = se_test::BakeScene(std::move(oNode));
                const std::vector<char> vBuf(vBytes.begin(), vBytes.end());
                auto pNode = scene->InstantiateFromBuffer(vBuf);
                EXPECT_NE(pNode, nullptr);
                Blackboard * pBB = pNode ? pNode->GetComponent<Blackboard>() : nullptr;
                EXPECT_NE(pBB, nullptr);
                return pBB;
        }

        uint32_t guard_counter = 0;

        // Bake + load an in-memory world; returns whether the load succeeded.
        bool LoadWorldInline(const FlatBuffers::WorldT & oWorldT) {
                const auto vBytes = se_test::BakeWorld(oWorldT);
                auto hWorld = CreateResource<WorldAsset>(
                                "bb_world_under_test", FlatBuffers::GetWorld(vBytes.data()));
                return GetSystem<WorldSystem>().LoadWorld(hWorld);
        }

        BlackboardSystem & BB() { return GetSystem<BlackboardSystem>(); }
};

// ---------------------------------------------------------------------------
// asset (Tier B)
// ---------------------------------------------------------------------------

TEST_F(BlackboardSystemTest, AssetBakeLoadRoundTrip) {
        const auto vBytes = se_test::WriteBB("resource/ai/gen/bb_roundtrip.sebb", MakeGuardDef());
        ASSERT_FALSE(vBytes.empty());

        auto h = CreateResource<BlackboardAsset>("resource/ai/gen/bb_roundtrip.sebb");
        const BlackboardAsset * pAsset = GetResource(h);
        ASSERT_NE(pAsset, nullptr);
        EXPECT_EQ(pAsset->GetKeyCount(), 5u);
        EXPECT_EQ(pAsset->GetKeys()[0].s_name, "alert_level");
        EXPECT_EQ(pAsset->GetKeys()[0].slot.type, EBBType::Float);
        EXPECT_EQ(pAsset->GetKeys()[1].slot.type, EBBType::Bool);
        EXPECT_EQ(pAsset->GetKeys()[4].slot.type, EBBType::Entity);

        // slot indices partition per type, in declaration order
        EXPECT_EQ(pAsset->GetKeys()[0].slot.index, 0);
        EXPECT_EQ(pAsset->GetKeys()[3].slot.type, EBBType::Vec3);

        // inline-definition mode lands on the same schema
        flatbuffers::FlatBufferBuilder oFbbInline;
        auto hInline = CreateResource<BlackboardAsset>("bb_roundtrip_inline",
                        se_test::PackBBDef(oFbbInline, MakeGuardDef()));
        const BlackboardAsset * pInline = GetResource(hInline);
        ASSERT_NE(pInline, nullptr);
        EXPECT_EQ(pInline->GetKeyCount(), pAsset->GetKeyCount());
}

TEST_F(BlackboardSystemTest, AssetGarbageThrows) {
        std::vector<uint8_t> vGarbage{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
        EXPECT_ANY_THROW(CreateResource<BlackboardAsset>("bb_garbage",
                                                        vGarbage.data(), vGarbage.size()));

        // truncated real asset: same loudness
        const auto vBytes = se_test::BakeBB(MakeGuardDef());
        std::vector<uint8_t> vTruncated(vBytes.begin(), vBytes.begin() + vBytes.size() / 2);
        EXPECT_ANY_THROW(CreateResource<BlackboardAsset>("bb_truncated",
                                                        vTruncated.data(), vTruncated.size()));
}

TEST_F(BlackboardSystemTest, AssetValidationErrors) {
        flatbuffers::FlatBufferBuilder oFbb;
        // duplicate keys are load errors
        auto oDup = se_test::MakeBBDef("dup_bb");
        se_test::AddKey(*oDup, "alert_level", FlatBuffers::EBType::Float);
        se_test::AddKey(*oDup, "alert_level", FlatBuffers::EBType::Bool);
        EXPECT_ANY_THROW(CreateResource<BlackboardAsset>("bb_dup",
                                se_test::PackBBDef(oFbb, *oDup)));

        // an empty schema is an authoring bug
        auto oEmpty = se_test::MakeBBDef("empty_bb");
        EXPECT_ANY_THROW(CreateResource<BlackboardAsset>("bb_empty",
                                se_test::PackBBDef(oFbb, *oEmpty)));

        // a default whose type disagrees with the declared key type
        auto oMismatch = se_test::MakeBBDef("mismatch_bb");
        se_test::AddKey(*oMismatch, "alarm_raised", FlatBuffers::EBType::Bool)
                ->default_value.Set(FlatBuffers::Float(1.f));
        EXPECT_ANY_THROW(CreateResource<BlackboardAsset>("bb_mismatch",
                                se_test::PackBBDef(oFbb, *oMismatch)));
}

TEST_F(BlackboardSystemTest, ResolveTypeChecked) {
        const auto vBytes = se_test::BakeBB(MakeGuardDef());
        auto h = CreateResource<BlackboardAsset>("bb_resolve", vBytes.data(), vBytes.size());
        const BlackboardAsset * pAsset = GetResource(h);
        ASSERT_NE(pAsset, nullptr);

        const SSlotRef slot = pAsset->Resolve(StrID("alert_level"), EBBType::Float);
        EXPECT_EQ(slot.type,  EBBType::Float);
        EXPECT_EQ(slot.index, 0u);

        // wrong-type expectation throws (Resolve) / returns false (TryResolve)
        EXPECT_ANY_THROW(pAsset->Resolve(StrID("alert_level"), EBBType::Bool));
        SSlotRef out;
        EXPECT_FALSE(pAsset->TryResolve(StrID("no_such_key"), EBBType::Bool, out));
        EXPECT_FALSE(pAsset->TryResolve(StrID("alert_level"), EBBType::Bool, out));
        EXPECT_TRUE (pAsset->TryResolve(StrID("alarm_raised"), EBBType::Bool, out));
        EXPECT_EQ(out.index, 0u);   // first bool key
}

// ---------------------------------------------------------------------------
// core storage (Tier B)
// ---------------------------------------------------------------------------

TEST_F(BlackboardSystemTest, TypedRoundTrips) {
        const auto vBytes = se_test::BakeBB(MakeGuardDef());
        auto h = CreateResource<BlackboardAsset>("bb_core_types", vBytes.data(), vBytes.size());
        const BlackboardAsset * pAsset = GetResource(h);
        ASSERT_NE(pAsset, nullptr);

        BlackboardCore oCore;
        oCore.Init(pAsset);

        EXPECT_TRUE(oCore.SetFloat (StrID("alert_level"), 0.5f));
        EXPECT_TRUE(oCore.SetBool  (StrID("alarm_raised"), true));
        EXPECT_TRUE(oCore.SetInt   (StrID("patrol_index"), 3));
        EXPECT_TRUE(oCore.SetVec3  (StrID("home_position"), glm::vec3(1.f, 2.f, 3.f)));

        EXPECT_FLOAT_EQ(oCore.GetFloat(StrID("alert_level")), 0.5f);
        EXPECT_TRUE (oCore.GetBool(StrID("alarm_raised")));
        EXPECT_EQ   (oCore.GetInt(StrID("patrol_index")), 3);
        EXPECT_EQ   (oCore.GetVec3(StrID("home_position")), glm::vec3(1.f, 2.f, 3.f));

        // overwrites
        EXPECT_TRUE(oCore.SetFloat(StrID("alert_level"), 0.9f));
        EXPECT_FLOAT_EQ(oCore.GetFloat(StrID("alert_level")), 0.9f);

        // hot path agrees with the StrID convenience path
        const SSlotRef fSlot = oCore.Resolve(StrID("alert_level"), EBBType::Float);
        EXPECT_FLOAT_EQ(oCore.GetFloat(fSlot), 0.9f);

        // entity refs: runtime-only, set + read back
        auto hNode = scene->Create("ent_ref_target");
        EXPECT_TRUE(oCore.SetEntity(StrID("target"), hNode));
        EXPECT_EQ(oCore.GetEntity(StrID("target")).lock().get(), hNode.get());
}

TEST_F(BlackboardSystemTest, ChangeOnlyOnChange) {
        const auto vBytes = se_test::BakeBB(MakeGuardDef());
        auto h = CreateResource<BlackboardAsset>("bb_change", vBytes.data(), vBytes.size());
        const BlackboardAsset * pAsset = GetResource(h);
        ASSERT_NE(pAsset, nullptr);

        BlackboardCore oCore;
        oCore.Init(pAsset);

        se_test::BBSink<EBlackboardChanged> oSink;

        // same-value writes never notify
        oCore.SetBool(StrID("alarm_raised"), false);
        oCore.SetFloat(StrID("alert_level"), 0.f);
        Events().Process();
        EXPECT_TRUE(oSink.hits.empty());

        // a real change notifies exactly once, with the key
        oCore.SetBool(StrID("alarm_raised"), true);
        Events().Process();
        ASSERT_EQ(oSink.hits.size(), 1u);
        EXPECT_EQ(oSink.hits[0].key, StrID("alarm_raised"));

        // an ownerless (global) core posts an empty entity handle
        oCore.SetFloat(StrID("alert_level"), 2.f);
        Events().Process();
        ASSERT_EQ(oSink.hits.size(), 2u);
        EXPECT_TRUE(oSink.hits[1].entity.expired());
}

TEST_F(BlackboardSystemTest, DefaultsAndResetToDefaults) {
        auto oDef = se_test::MakeBBDef("defaults_bb");
        se_test::AddKey(*oDef, "alert_level", FlatBuffers::EBType::Float)->default_value.Set(FlatBuffers::Float(0.25f));
        se_test::AddKey(*oDef, "alarm_raised", FlatBuffers::EBType::Bool)->default_value.Set(FlatBuffers::Bool(true));
        se_test::AddKey(*oDef, "patrol_index", FlatBuffers::EBType::Int);   // no default → 0

        flatbuffers::FlatBufferBuilder oFbb;
        auto h = CreateResource<BlackboardAsset>("bb_defaults",
                        se_test::PackBBDef(oFbb, *oDef));
        const BlackboardAsset * pAsset = GetResource(h);
        ASSERT_NE(pAsset, nullptr);

        BlackboardCore oCore;
        oCore.Init(pAsset);
        EXPECT_FLOAT_EQ(oCore.GetFloat(StrID("alert_level")), 0.25f);
        EXPECT_TRUE (oCore.GetBool(StrID("alarm_raised")));
        EXPECT_EQ   (oCore.GetInt(StrID("patrol_index")), 0);

        oCore.SetFloat(StrID("alert_level"), 9.f);
        oCore.SetBool(StrID("alarm_raised"), false);
        oCore.ResetToDefaults();
        EXPECT_FLOAT_EQ(oCore.GetFloat(StrID("alert_level")), 0.25f);
        EXPECT_TRUE (oCore.GetBool(StrID("alarm_raised")));
}

TEST_F(BlackboardSystemTest, SnapshotRestoreScalars) {
        const auto vBytes = se_test::BakeBB(MakeGuardDef());
        auto h = CreateResource<BlackboardAsset>("bb_snap_scalars", vBytes.data(), vBytes.size());
        BlackboardCore oCore;
        oCore.Init(GetResource(h));

        oCore.SetFloat(StrID("alert_level"), 0.75f);
        oCore.SetBool(StrID("alarm_raised"), true);
        oCore.SetInt(StrID("patrol_index"), 7);
        oCore.SetVec3(StrID("home_position"), glm::vec3(5.f, 6.f, 7.f));

        const BlackboardCore::SSnapshot oSnap = oCore.Snapshot();

        // churn after the snapshot
        oCore.SetFloat(StrID("alert_level"), 0.f);
        oCore.SetBool(StrID("alarm_raised"), false);
        oCore.SetInt(StrID("patrol_index"), 0);
        oCore.SetVec3(StrID("home_position"), glm::vec3(0.f));

        // restore is bulk surgery: no change events
        se_test::BBSink<EBlackboardChanged> oSink;
        oCore.Restore(oSnap);
        Events().Process();
        EXPECT_TRUE(oSink.hits.empty());

        EXPECT_FLOAT_EQ(oCore.GetFloat(StrID("alert_level")), 0.75f);
        EXPECT_TRUE (oCore.GetBool(StrID("alarm_raised")));
        EXPECT_EQ   (oCore.GetInt(StrID("patrol_index")), 7);
        EXPECT_EQ   (oCore.GetVec3(StrID("home_position")), glm::vec3(5.f, 6.f, 7.f));
}

TEST_F(BlackboardSystemTest, SnapshotRestoreEntityRef) {
        const auto vBytes = se_test::BakeBB(MakeGuardDef());
        auto h = CreateResource<BlackboardAsset>("bb_snap_ent", vBytes.data(), vBytes.size());
        BlackboardCore oCore;
        oCore.Init(GetResource(h));

        auto hNode = scene->Create("snap_target");
        oCore.SetEntity(StrID("target"), hNode);

        const BlackboardCore::SSnapshot oSnap = oCore.Snapshot();
        oCore.SetEntity(StrID("target"), BlackboardCore::TNodeWeak{});
        EXPECT_TRUE(oCore.GetEntity(StrID("target")).expired());
        oCore.Restore(oSnap);

        // a live node re-locks after the round-trip (refcount-safe copies)
        EXPECT_EQ(oCore.GetEntity(StrID("target")).lock().get(), hNode.get());

        // a destroyed entity stays in the snapshot but locks null afterwards
        hNode->Unlink();
        hNode.reset();
        const BlackboardCore::SSnapshot oSnapDead = oCore.Snapshot();
        BlackboardCore oOther;
        oOther.Init(GetResource(h));
        oOther.Restore(oSnapDead);
        EXPECT_TRUE(oOther.GetEntity(StrID("target")).expired());

        // cross-schema restore is an authoring bug, thrown
        BlackboardCore::SSnapshot oAlien;   // wrong shapes (all empty)
        EXPECT_ANY_THROW(oOther.Restore(oAlien));
}

// ---------------------------------------------------------------------------
// component (Tier C: real scene loading)
// ---------------------------------------------------------------------------

TEST_F(BlackboardSystemTest, ComponentLoadFromSceneBuffer) {
        // path mode: .sebb on disk, scene references it
        se_test::WriteBB("resource/ai/gen/bb_guard.sebb", MakeGuardDef());

        Blackboard * pBB = LoadGuard(se_test::MakeBBHolderPath("resource/ai/gen/bb_guard.sebb"));
        ASSERT_NE(pBB, nullptr);
        EXPECT_FLOAT_EQ(pBB->GetFloat(StrID("alert_level")), 0.f);   // schema default

        // inline mode: definition embedded in the scene, name is the cache key
        auto oInline = se_test::MakeBBHolderInline(MakeGuardDef(), "guard_inline_bb");
        Blackboard * pInlineBB = LoadGuard(std::move(oInline));
        ASSERT_NE(pInlineBB, nullptr);
        EXPECT_EQ(pInlineBB->GetAsset()->GetKeyCount(), 5u);
}

TEST_F(BlackboardSystemTest, ComponentInitialOverrides) {
        std::vector<FlatBuffers::BlackboardInitialT> vInitial;
        vInitial.push_back(se_test::MakeInitial("home_position", glm::vec3(11.f, 22.f, 33.f)));
        vInitial.push_back(se_test::MakeInitial("patrol_index", int32_t(2)));

        Blackboard * pBB = LoadGuard(
                        se_test::MakeBBHolderInline(MakeGuardDef(), "guard_initial_bb"),
                        std::move(vInitial));
        ASSERT_NE(pBB, nullptr);
        EXPECT_EQ(pBB->GetVec3(StrID("home_position")), glm::vec3(11.f, 22.f, 33.f));
        EXPECT_EQ   (pBB->GetInt(StrID("patrol_index")), 2);

        // per-instance override does not touch the shared schema default
        Blackboard * pOther = LoadGuard(
                        se_test::MakeBBHolderInline(MakeGuardDef(), "guard_initial_bb2"));
        ASSERT_NE(pOther, nullptr);
        EXPECT_EQ(pOther->GetVec3(StrID("home_position")), glm::vec3(0.f));

        // unknown key override = load error
        std::vector<FlatBuffers::BlackboardInitialT> vBad;
        vBad.push_back(se_test::MakeInitial("no_such_key", 1.f));
        EXPECT_ANY_THROW(LoadGuard(
                        se_test::MakeBBHolderInline(MakeGuardDef(), "guard_bad_key_bb"),
                        std::move(vBad)));

        // type-mismatched override = load error
        std::vector<FlatBuffers::BlackboardInitialT> vWrong;
        vWrong.push_back(se_test::MakeInitial("alarm_raised", 3.5f));   // float into bool
        EXPECT_ANY_THROW(LoadGuard(
                        se_test::MakeBBHolderInline(MakeGuardDef(), "guard_bad_type_bb"),
                        std::move(vWrong)));
}

TEST_F(BlackboardSystemTest, ComponentChangeEventsCarryNode) {
        se_test::WriteBB("resource/ai/gen/bb_events.sebb", MakeGuardDef());
        Blackboard * pBB = LoadGuard(se_test::MakeBBHolderPath("resource/ai/gen/bb_events.sebb"));
        ASSERT_NE(pBB, nullptr);

        se_test::BBSink<EBlackboardChanged> oSink;
        pBB->SetBool(StrID("alarm_raised"), true);
        Events().Process();

        ASSERT_EQ(oSink.hits.size(), 1u);
        EXPECT_EQ(oSink.hits[0].key, StrID("alarm_raised"));
        // the component's node is the event identity (unlike the global core)
        EXPECT_FALSE(oSink.hits[0].entity.expired());
        EXPECT_EQ(oSink.hits[0].entity.lock().get(), pBB->Node());
}

TEST_F(BlackboardSystemTest, TryGetLocationVec3AndEntity) {
        se_test::WriteBB("resource/ai/gen/bb_loc.sebb", MakeGuardDef());
        Blackboard * pBB = LoadGuard(se_test::MakeBBHolderPath("resource/ai/gen/bb_loc.sebb"));
        ASSERT_NE(pBB, nullptr);

        // Vec3 key resolves directly
        glm::vec3 vOut;
        const SSlotRef homeSlot = pBB->Resolve(StrID("home_position"), EBBType::Vec3);
        EXPECT_TRUE(pBB->TryGetLocation(homeSlot, vOut));
        EXPECT_EQ(vOut, glm::vec3(0.f));

        // Entity key resolves through the node's world position
        auto hBody = MakeSphere(glm::vec3(3.f, 1.f, 4.f), "snap_target");
        pBB->SetEntity(StrID("target"), hBody.node->GetShared());
        const SSlotRef targetSlot = pBB->Resolve(StrID("target"), EBBType::Entity);
        ASSERT_TRUE(pBB->TryGetLocation(targetSlot, vOut));
        EXPECT_EQ(vOut, glm::vec3(3.f, 1.f, 4.f));

        // an unset entity resolves to nothing
        Blackboard * pOther = LoadGuard(se_test::MakeBBHolderInline(MakeGuardDef(), "guard_loc_bb2"));
        ASSERT_NE(pOther, nullptr);
        EXPECT_FALSE(pOther->TryGetLocation(pOther->Resolve(StrID("target"), EBBType::Entity), vOut));

        // non-locatable types (float) resolve to nothing
        EXPECT_FALSE(pBB->TryGetLocation(pBB->Resolve(StrID("alert_level"), EBBType::Float), vOut));
}

TEST_F(BlackboardSystemTest, ComponentRegistryLifecycle) {
        se_test::WriteBB("resource/ai/gen/bb_registry.sebb", MakeGuardDef());

        auto Count = [&]() {
                uint32_t n = 0;
                BB().ForEachComponent([&n](const Blackboard *) { ++n; });
                return n;
        };

        const uint32_t base = Count();

        Blackboard * pBB = LoadGuard(se_test::MakeBBHolderPath("resource/ai/gen/bb_registry.sebb"));
        ASSERT_NE(pBB, nullptr);
        EXPECT_EQ(Count(), base + 1);

        // Disable() deregisters; Enable() re-registers
        pBB->Disable();
        EXPECT_EQ(Count(), base);
        pBB->Enable();
        EXPECT_EQ(Count(), base + 1);

        // destroying the node destroys the component and drains the registry
        pBB->Node()->DestroyComponent<Blackboard>();
        EXPECT_EQ(Count(), base);
}

// ---------------------------------------------------------------------------
// world-level (global) blackboard
// ---------------------------------------------------------------------------

TEST_F(BlackboardSystemTest, GlobalBlackboardFromWorldPath) {
        // nothing loaded → null accessor
        EXPECT_EQ(GetSystem<WorldSystem>().GetGlobalBlackboard(), nullptr);

        se_test::WriteBB("resource/ai/gen/bb_global.sebb", MakeGuardDef());

        FlatBuffers::WorldT oWorldT = *se_test::MakeWorld("bb_global_world");
        se_test::SetGlobalBlackboard(oWorldT,
                        se_test::MakeBBHolderPath("resource/ai/gen/bb_global.sebb"));
        ASSERT_TRUE(LoadWorldInline(oWorldT));

        BlackboardCore * pGlobal = GetSystem<WorldSystem>().GetGlobalBlackboard();
        ASSERT_NE(pGlobal, nullptr);
        EXPECT_FALSE(pGlobal->GetBool(StrID("alarm_raised")));   // schema default

        // writes through the WorldSystem accessor (the spawner-condition path)
        EXPECT_TRUE(pGlobal->SetBool(StrID("alarm_raised"), true));
        EXPECT_TRUE(BB().HasGlobal());
}

TEST_F(BlackboardSystemTest, GlobalBlackboardFromWorldInline) {
        FlatBuffers::WorldT oWorldT = *se_test::MakeWorld("bb_global_inline_world");
        se_test::SetGlobalBlackboard(oWorldT,
                        se_test::MakeBBHolderInline(MakeGuardDef(), "global_inline_bb"));
        ASSERT_TRUE(LoadWorldInline(oWorldT));

        BlackboardCore * pGlobal = GetSystem<WorldSystem>().GetGlobalBlackboard();
        ASSERT_NE(pGlobal, nullptr);
        ASSERT_NE(pGlobal->GetAsset(), nullptr);
        EXPECT_EQ(pGlobal->GetAsset()->GetKeyCount(), 5u);
}

TEST_F(BlackboardSystemTest, GlobalAbsentNullAccessor) {
        FlatBuffers::WorldT oWorldT = *se_test::MakeWorld("bb_no_global_world");
        ASSERT_TRUE(LoadWorldInline(oWorldT));

        EXPECT_EQ(GetSystem<WorldSystem>().GetGlobalBlackboard(), nullptr);
        EXPECT_FALSE(BB().HasGlobal());
}

TEST_F(BlackboardSystemTest, GlobalResetOnWorldUnload) {
        FlatBuffers::WorldT oWorldT = *se_test::MakeWorld("bb_reset_world");
        se_test::SetGlobalBlackboard(oWorldT,
                        se_test::MakeBBHolderInline(MakeGuardDef(), "global_reset_bb"));
        ASSERT_TRUE(LoadWorldInline(oWorldT));

        BlackboardCore * pGlobal = GetSystem<WorldSystem>().GetGlobalBlackboard();
        ASSERT_NE(pGlobal, nullptr);
        pGlobal->SetBool(StrID("alarm_raised"), true);
        pGlobal->SetInt(StrID("patrol_index"), 4);
        EXPECT_TRUE(pGlobal->GetBool(StrID("alarm_raised")));

        // world-scoped state dies with the world: the accessor reads as absent
        GetSystem<WorldSystem>().UnloadWorld();
        Events().Process();

        EXPECT_EQ(GetSystem<WorldSystem>().GetGlobalBlackboard(), nullptr);

        // re-load re-initializes deterministically (defaults again)
        ASSERT_TRUE(LoadWorldInline(oWorldT));
        pGlobal = GetSystem<WorldSystem>().GetGlobalBlackboard();
        ASSERT_NE(pGlobal, nullptr);
        EXPECT_FALSE(pGlobal->GetBool(StrID("alarm_raised")));
        EXPECT_EQ   (pGlobal->GetInt(StrID("patrol_index")), 0);
}

TEST_F(BlackboardSystemTest, WorldVsInstanceIsolation) {
        FlatBuffers::WorldT oWorldT = *se_test::MakeWorld("bb_isolation_world");
        se_test::SetGlobalBlackboard(oWorldT,
                        se_test::MakeBBHolderInline(MakeGuardDef(), "global_isolation_bb"));
        ASSERT_TRUE(LoadWorldInline(oWorldT));

        BlackboardCore * pGlobal = GetSystem<WorldSystem>().GetGlobalBlackboard();
        Blackboard * pBB = LoadGuard(se_test::MakeBBHolderInline(MakeGuardDef(), "guard_iso_bb"));
        ASSERT_NE(pGlobal, nullptr);
        ASSERT_NE(pBB, nullptr);

        pGlobal->SetBool(StrID("alarm_raised"), true);
        pGlobal->SetInt(StrID("patrol_index"), 9);

        EXPECT_FALSE(pBB->GetBool(StrID("alarm_raised")));
        EXPECT_EQ   (pBB->GetInt(StrID("patrol_index")), 0);

        pBB->SetFloat(StrID("alert_level"), 5.f);
        EXPECT_FLOAT_EQ(pGlobal->GetFloat(StrID("alert_level")), 0.f);
}

// ---------------------------------------------------------------------------
// TriggerBinding dispatch (Tier C)
// ---------------------------------------------------------------------------

TEST_F(BlackboardSystemTest, BindingWritesGlobalOnEnter) {
        FlatBuffers::WorldT oWorldT = *se_test::MakeWorld("bb_binding_world");
        se_test::SetGlobalBlackboard(oWorldT,
                        se_test::MakeBBHolderInline(MakeGuardDef(), "global_binding_bb"));
        ASSERT_TRUE(LoadWorldInline(oWorldT));

        // lever node: registered body + authored binding (event → global bool)
        auto hLever = MakeStaticBox(glm::vec3(0.f), "lever");
        FlatBuffers::TriggerBindingDefT oDef =
                        se_test::MakeBindingDef("lever_pulled", "alarm_raised",
                                                se_test::BoolValue(true));
        std::vector<FlatBuffers::TriggerBindingDefT> vDefs{ oDef };
        flatbuffers::FlatBufferBuilder oFbb;
        hLever.node->CreateComponent<TriggerBinding>(se_test::PackTriggerBinding(oFbb, vDefs));
        ASSERT_NE(hLever.node->GetComponent<TriggerBinding>(), nullptr);

        auto hEnterer = MakeSphere(glm::vec3(1.f, 0.f, 0.f), "enterer");
        Events().TriggerEvent(ETriggerFired{ StrID("lever_pulled"), hLever.handle, hEnterer.handle });
        Events().Process();

        EXPECT_TRUE(BB().GetGlobal().GetBool(StrID("alarm_raised")));
}

TEST_F(BlackboardSystemTest, BindingSelfTarget) {
        se_test::WriteBB("resource/ai/gen/bb_self.sebb", MakeGuardDef());

        auto hLever = MakeStaticBox(glm::vec3(0.f), "lever");

        // the trigger node carries its own Blackboard + a Self-target binding
        flatbuffers::FlatBufferBuilder oFbbBB;
        hLever.node->CreateComponent<Blackboard>(
                        se_test::PackBlackboard(oFbbBB,
                                        se_test::MakeBBHolderInline(MakeGuardDef(), "self_bb")));
        Blackboard * pBB = hLever.node->GetComponent<Blackboard>();
        ASSERT_NE(pBB, nullptr);

        FlatBuffers::TriggerBindingDefT oDef =
                        se_test::MakeBindingDef("lever_pulled", "alert_level",
                                                se_test::FloatValue(1.5f),
                                                /*global=*/false);
        std::vector<FlatBuffers::TriggerBindingDefT> vDefs{ oDef };
        flatbuffers::FlatBufferBuilder oFbb;
        hLever.node->CreateComponent<TriggerBinding>(se_test::PackTriggerBinding(oFbb, vDefs));

        auto hEnterer = MakeSphere(glm::vec3(1.f, 0.f, 0.f), "enterer");
        Events().TriggerEvent(ETriggerFired{ StrID("lever_pulled"), hLever.handle, hEnterer.handle });
        Events().Process();

        // written into the trigger's own blackboard; no global is declared in
        // this test, so nothing else could have received the write
        EXPECT_FLOAT_EQ(pBB->GetFloat(StrID("alert_level")), 1.5f);
}

TEST_F(BlackboardSystemTest, BindingWritePosition) {
        FlatBuffers::WorldT oWorldT = *se_test::MakeWorld("bb_position_world");
        se_test::SetGlobalBlackboard(oWorldT,
                        se_test::MakeBBHolderInline(MakeGuardDef(), "global_pos_bb"));
        ASSERT_TRUE(LoadWorldInline(oWorldT));

        auto hLever = MakeStaticBox(glm::vec3(0.f), "lever");
        FlatBuffers::TriggerBindingDefT oDef =
                        se_test::MakeBindingDef("noise", "home_position",
                                                se_test::BBValue{}, /*global=*/true,
                                                /*write_position=*/true);
        std::vector<FlatBuffers::TriggerBindingDefT> vDefs{ oDef };
        {
                flatbuffers::FlatBufferBuilder oFbb;
                hLever.node->CreateComponent<TriggerBinding>(se_test::PackTriggerBinding(oFbb, vDefs));
        }

        auto hNoisemaker = MakeSphere(glm::vec3(7.f, 2.f, 9.f), "noisemaker");
        Events().TriggerEvent(ETriggerFired{ StrID("noise"), hLever.handle, hNoisemaker.handle });
        Events().Process();

        EXPECT_EQ(BB().GetGlobal().GetVec3(StrID("home_position")), glm::vec3(7.f, 2.f, 9.f));
}

TEST_F(BlackboardSystemTest, BindingOnExitEvent) {
        FlatBuffers::WorldT oWorldT = *se_test::MakeWorld("bb_exit_world");
        se_test::SetGlobalBlackboard(oWorldT,
                        se_test::MakeBBHolderInline(MakeGuardDef(), "global_exit_bb"));
        ASSERT_TRUE(LoadWorldInline(oWorldT));

        BB().GetGlobal().SetBool(StrID("alarm_raised"), true);

        auto hLever = MakeStaticBox(glm::vec3(0.f), "lever");
        FlatBuffers::TriggerBindingDefT oDef =
                        se_test::MakeBindingDef("lever_released", "alarm_raised",
                                                se_test::BoolValue(false));
        std::vector<FlatBuffers::TriggerBindingDefT> vDefs{ oDef };
        {
                flatbuffers::FlatBufferBuilder oFbb;
                hLever.node->CreateComponent<TriggerBinding>(se_test::PackTriggerBinding(oFbb, vDefs));
        }

        auto hEnterer = MakeSphere(glm::vec3(1.f, 0.f, 0.f), "enterer");
        Events().TriggerEvent(ETriggerFired{ StrID("lever_released"), hLever.handle, hEnterer.handle });
        Events().Process();

        EXPECT_FALSE(BB().GetGlobal().GetBool(StrID("alarm_raised")));
}

TEST_F(BlackboardSystemTest, BindingUnknownKeyWarnsNoWrite) {
        FlatBuffers::WorldT oWorldT = *se_test::MakeWorld("bb_unk_world");
        se_test::SetGlobalBlackboard(oWorldT,
                        se_test::MakeBBHolderInline(MakeGuardDef(), "global_unk_bb"));
        ASSERT_TRUE(LoadWorldInline(oWorldT));

        auto hLever = MakeStaticBox(glm::vec3(0.f), "lever");
        FlatBuffers::TriggerBindingDefT oDef =
                        se_test::MakeBindingDef("lever_pulled", "no_such_key",
                                                se_test::BoolValue(true));
        std::vector<FlatBuffers::TriggerBindingDefT> vDefs{ oDef };
        {
                flatbuffers::FlatBufferBuilder oFbb;
                hLever.node->CreateComponent<TriggerBinding>(se_test::PackTriggerBinding(oFbb, vDefs));
        }

        auto hEnterer = MakeSphere(glm::vec3(1.f, 0.f, 0.f), "enterer");
        EXPECT_NO_THROW({
                Events().TriggerEvent(ETriggerFired{ StrID("lever_pulled"), hLever.handle, hEnterer.handle });
                Events().Process();
        });

        // unrelated keys untouched; fire-time loudness instead of crashes
        EXPECT_FALSE(BB().GetGlobal().GetBool(StrID("alarm_raised")));
}

TEST_F(BlackboardSystemTest, BindingTypeMismatchWarns) {
        FlatBuffers::WorldT oWorldT = *se_test::MakeWorld("bb_mismatch_world");
        se_test::SetGlobalBlackboard(oWorldT,
                        se_test::MakeBBHolderInline(MakeGuardDef(), "global_mismatch_bb"));
        ASSERT_TRUE(LoadWorldInline(oWorldT));

        BB().GetGlobal().SetFloat(StrID("alert_level"), 0.5f);

        auto hLever = MakeStaticBox(glm::vec3(0.f), "lever");
        // bool value into a float key
        FlatBuffers::TriggerBindingDefT oDef =
                        se_test::MakeBindingDef("lever_pulled", "alert_level",
                                                se_test::BoolValue(true));
        std::vector<FlatBuffers::TriggerBindingDefT> vDefs{ oDef };
        {
                flatbuffers::FlatBufferBuilder oFbb;
                hLever.node->CreateComponent<TriggerBinding>(se_test::PackTriggerBinding(oFbb, vDefs));
        }

        auto hEnterer = MakeSphere(glm::vec3(1.f, 0.f, 0.f), "enterer");
        EXPECT_NO_THROW({
                Events().TriggerEvent(ETriggerFired{ StrID("lever_pulled"), hLever.handle, hEnterer.handle });
                Events().Process();
        });
        EXPECT_FLOAT_EQ(BB().GetGlobal().GetFloat(StrID("alert_level")), 0.5f);
}

TEST_F(BlackboardSystemTest, BindingIgnoresOtherEventIds) {
        FlatBuffers::WorldT oWorldT = *se_test::MakeWorld("bb_ids_world");
        se_test::SetGlobalBlackboard(oWorldT,
                        se_test::MakeBBHolderInline(MakeGuardDef(), "global_ids_bb"));
        ASSERT_TRUE(LoadWorldInline(oWorldT));

        auto hLever = MakeStaticBox(glm::vec3(0.f), "lever");
        std::vector<FlatBuffers::TriggerBindingDefT> vDefs{
                se_test::MakeBindingDef("event_a", "alarm_raised", se_test::BoolValue(true)),
                se_test::MakeBindingDef("event_b", "patrol_index", se_test::IntValue(5)),
        };
        {
                flatbuffers::FlatBufferBuilder oFbb;
                hLever.node->CreateComponent<TriggerBinding>(se_test::PackTriggerBinding(oFbb, vDefs));
        }

        auto hEnterer = MakeSphere(glm::vec3(1.f, 0.f, 0.f), "enterer");
        Events().TriggerEvent(ETriggerFired{ StrID("event_b"), hLever.handle, hEnterer.handle });
        Events().Process();

        // only event_b's binding fired
        EXPECT_EQ   (BB().GetGlobal().GetInt(StrID("patrol_index")), 5);
        EXPECT_FALSE(BB().GetGlobal().GetBool(StrID("alarm_raised")));
}

TEST_F(BlackboardSystemTest, BindingWithoutGlobalWarnsNoWrite) {
        // world without a declared global: Self-less (Global-target) bindings
        // degrade to a one-shot warn, no crash
        FlatBuffers::WorldT oWorldT = *se_test::MakeWorld("bb_noglobal_world");
        ASSERT_TRUE(LoadWorldInline(oWorldT));

        auto hLever = MakeStaticBox(glm::vec3(0.f), "lever");
        FlatBuffers::TriggerBindingDefT oDef =
                        se_test::MakeBindingDef("lever_pulled", "alarm_raised",
                                                se_test::BoolValue(true));
        std::vector<FlatBuffers::TriggerBindingDefT> vDefs{ oDef };
        {
                flatbuffers::FlatBufferBuilder oFbb;
                hLever.node->CreateComponent<TriggerBinding>(se_test::PackTriggerBinding(oFbb, vDefs));
        }

        auto hEnterer = MakeSphere(glm::vec3(1.f, 0.f, 0.f), "enterer");
        EXPECT_NO_THROW({
                Events().TriggerEvent(ETriggerFired{ StrID("lever_pulled"), hLever.handle, hEnterer.handle });
                Events().Process();
        });
        EXPECT_FALSE(BB().HasGlobal());
}

} // anonymous namespace
